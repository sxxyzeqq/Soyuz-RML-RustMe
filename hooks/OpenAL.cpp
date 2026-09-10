#include "OpenAL.h"

#include "Hooks.h"

#include <cstdio>

#include <cctype>

#include <algorithm>

#include <shlobj.h>



extern void* original_alSourcePlay_addr;

extern bool g_openalLoaded, g_openalFuncsLoaded;



alGetListener3f_t alGetListener3f = nullptr;

static alGetError_fn p_alGetError = nullptr;

alSourcePlay_fn p_alSourcePlay = nullptr;

static alGetSource3f_fn p_alGetSource3f = nullptr;



static std::unordered_map<ALuint, std::string> g_bufferNames;

static std::mutex g_bufferNamesMutex;

static std::unordered_map<int, std::string> g_sizeToName;

static std::string g_lastZipPath;

// Глобальная очередь "недавно открытых звуковых файлов".
struct PendingSound {
    std::string name;
    ULONGLONG   time;
};
static std::vector<PendingSound> g_pendingSounds;
static std::mutex g_pendingSoundsMutex;
static const ULONGLONG kPendingTTLms = 3000;

static std::unordered_map<std::string, std::string> g_pathToEvent; // relPath -> event name

static std::mutex g_pathToEventMutex;

static HANDLE(WINAPI* original_CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = nullptr;

static HookInfo hook_createFile;



static std::unordered_map<std::string, std::string> ParseSoundsJsonContent(const std::string& jsonContent) {
    std::unordered_map<std::string, std::string> fileToEvent;
    if (jsonContent.empty()) return fileToEvent;
    const char* p = jsonContent.c_str();
    std::string curEvent;
    while (*p) {
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++;
        const char* ss = p;
        while (*p && *p != '"') p++;
        if (!*p) break;
        std::string str(ss, p - ss);
        p++;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p == ':') {
            p++;
            while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
            if (*p == '{') { curEvent = str; p++; }
            else if (*p == '[' && str == "sounds") {
                p++;
                while (*p && *p != ']') {
                    if (*p == '"') {
                        p++;
                        const char* vs = p;
                        while (*p && *p != '"') p++;
                        std::string sp(vs, p - vs);
                        if (*p) p++;
                        if (!curEvent.empty() && sp != "none")
                            fileToEvent[sp] = curEvent;
                    } else p++;
                }
                if (*p) p++;
            } else if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p) p++; }
        }
    }
    return fileToEvent;
}

static void ExtractSoundNamesFromZip(const std::string& zipPath) {
    FILE* f = nullptr;
    fopen_s(&f, zipPath.c_str(), "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> allData(fsz);
    fread(allData.data(), 1, fsz, f);
    fclose(f);

    long eocd = -1;
    for (long i = fsz - 22; i >= 0 && i >= fsz - 65536; i--) {
        if (allData[i] == 0x50 && allData[i + 1] == 0x4B && allData[i + 2] == 0x05 && allData[i + 3] == 0x06) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) return;

    uint32_t cdOffset = allData[eocd + 16] | (allData[eocd + 17] << 8) | (allData[eocd + 18] << 16) | (allData[eocd + 19] << 24);
    uint16_t cdCount = allData[eocd + 8] | (allData[eocd + 9] << 8);

    // First pass: find and extract sounds.json
    std::string soundsJsonContent;
    long pos = (long)cdOffset;
    for (int e = 0; e < cdCount && pos + 46 < fsz; e++) {
        if (allData[pos] != 'P' || allData[pos + 1] != 'K' || allData[pos + 2] != 0x01 || allData[pos + 3] != 0x02) break;
        uint32_t uSize = allData[pos + 24] | (allData[pos + 25] << 8) | (allData[pos + 26] << 16) | (allData[pos + 27] << 24);
        uint16_t fnLen = allData[pos + 28] | (allData[pos + 29] << 8);
        uint16_t exLen = allData[pos + 30] | (allData[pos + 31] << 8);
        uint16_t cmLen = allData[pos + 32] | (allData[pos + 33] << 8);
        uint32_t localOff = allData[pos + 42] | (allData[pos + 43] << 8) | (allData[pos + 44] << 16) | (allData[pos + 45] << 24);
        uint16_t method = allData[pos + 10] | (allData[pos + 11] << 8);

        if (fnLen > 0 && fnLen < 512 && pos + 46 + fnLen <= fsz) {
            std::string entry((const char*)&allData[pos + 46], fnLen);
            std::string entryLower = entry;
            for (auto& c : entryLower) c = (char)tolower((unsigned char)c);
            if (entryLower.find("sounds.json") != std::string::npos && method == 0 && uSize > 0) {
                long lhPos = (long)localOff;
                if (lhPos + 30 < fsz) {
                    uint16_t lfnLen = allData[lhPos + 26] | (allData[lhPos + 27] << 8);
                    uint16_t lexLen = allData[lhPos + 28] | (allData[lhPos + 29] << 8);
                    long dataStart = lhPos + 30 + lfnLen + lexLen;
                    if (dataStart + (long)uSize <= fsz) {
                        soundsJsonContent = std::string((const char*)&allData[dataStart], uSize);
                    }
                }
            }
        }
        pos += 46 + fnLen + exLen + cmLen;
    }

    // Parse sounds.json to get file->event mapping
    std::unordered_map<std::string, std::string> fileToEvent = ParseSoundsJsonContent(soundsJsonContent);

    // Store path->event mapping for CreateFileW hook lookups
    if (!fileToEvent.empty()) {
        std::lock_guard<std::mutex> lk(g_pathToEventMutex);
        for (const auto& kv : fileToEvent) {
            g_pathToEvent[kv.first] = kv.second;
        }
    }

    // Second pass: map sound files to event names
    pos = (long)cdOffset;
    for (int e = 0; e < cdCount && pos + 46 < fsz; e++) {
        if (allData[pos] != 'P' || allData[pos + 1] != 'K' || allData[pos + 2] != 0x01 || allData[pos + 3] != 0x02) break;

        uint32_t uSize = allData[pos + 24] | (allData[pos + 25] << 8) | (allData[pos + 26] << 16) | (allData[pos + 27] << 24);
        uint16_t fnLen = allData[pos + 28] | (allData[pos + 29] << 8);
        uint16_t exLen = allData[pos + 30] | (allData[pos + 31] << 8);
        uint16_t cmLen = allData[pos + 32] | (allData[pos + 33] << 8);
        uint16_t method = allData[pos + 10] | (allData[pos + 11] << 8);
        uint32_t localOff = allData[pos + 42] | (allData[pos + 43] << 8) | (allData[pos + 44] << 16) | (allData[pos + 45] << 24);

        if (fnLen > 0 && fnLen < 512 && pos + 46 + fnLen <= fsz) {
            std::string entry((const char*)&allData[pos + 46], fnLen);
            std::string lower = entry;
            for (auto& c : lower) c = (char)tolower((unsigned char)c);

            size_t sPos = lower.find("sounds/");
            if (sPos != std::string::npos &&
                (lower.find(".ogg") != std::string::npos || lower.find(".wav") != std::string::npos)) {

                // Get relative path after "sounds/" without extension
                std::string relPath = entry.substr(sPos + 7);
                if (relPath.size() > 4) relPath = relPath.substr(0, relPath.size() - 4);
                for (auto& c : relPath) if (c == '\\') c = '/';

                // Try to find event name from sounds.json mapping
                std::string label;
                auto it = fileToEvent.find(relPath);
                if (it != fileToEvent.end()) {
                    label = it->second;
                } else {
                    // Try lowercase
                    std::string relLower = relPath;
                    for (auto& c : relLower) c = (char)tolower((unsigned char)c);
                    auto it2 = fileToEvent.find(relLower);
                    if (it2 != fileToEvent.end()) {
                        label = it2->second;
                    } else {
                        // Fallback: use path with dots
                        label = relPath;
                        for (auto& c : label) if (c == '/') c = '.';
                    }
                }

                if (uSize > 0 && !label.empty()) {
                    g_sizeToName[(int)uSize] = label;
                    // For stored .ogg files, compute PCM size from ogg data
                    std::string ext = lower.substr(lower.size() - 4);
                    if (ext == ".ogg" && method == 0 && uSize > 100) {
                        long lhPos = (long)localOff;
                        if (lhPos + 30 < fsz) {
                            uint16_t lfnLen = allData[lhPos + 26] | (allData[lhPos + 27] << 8);
                            uint16_t lexLen = allData[lhPos + 28] | (allData[lhPos + 29] << 8);
                            long dataStart = lhPos + 30 + lfnLen + lexLen;
                            if (dataStart + (long)uSize <= fsz) {
                                const unsigned char* oggData = &allData[dataStart];
                                int channels = 0;
                                for (int i = 0; i < 100 && i < (int)uSize - 40; i++) {
                                    if (oggData[i] == 'v' && oggData[i+1] == 'o' && oggData[i+2] == 'r' && oggData[i+3] == 'b') {
                                        channels = oggData[i + 10];
                                        break;
                                    }
                                }
                                long long totalSamples = 0;
                                for (int i = (int)uSize - 14; i > (int)uSize - 4096 && i >= 0; i--) {
                                    if (oggData[i] == 'O' && oggData[i+1] == 'g' && oggData[i+2] == 'g' && oggData[i+3] == 'S') {
                                        totalSamples = *(long long*)(&oggData[i + 6]);
                                        break;
                                    }
                                }
                                int pcmSize = 0;
                                if (channels > 0 && channels <= 2 && totalSamples > 0)
                                    pcmSize = (int)(totalSamples * channels * 2);
                                if (pcmSize > 0) {
                                    for (int d = -4; d <= 4; d++) g_sizeToName[pcmSize + d] = label;
                                }
                            }
                        }
                    }
                }
            }
        }
        pos += 46 + fnLen + exLen + cmLen;
    }
}



static void ScanSoundsDir(const std::string& dir, const std::string& baseDir,

    const std::unordered_map<std::string, std::string>& fileToEvent) {

    WIN32_FIND_DATAA fd;

    HANDLE hFind = FindFirstFileA((dir + "\\*").c_str(), &fd);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {

        std::string name = fd.cFileName;

        if (name == "." || name == "..") continue;

        std::string fullPath = dir + "\\" + name;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

            ScanSoundsDir(fullPath, baseDir, fileToEvent);

            continue;

        }

        std::string lower = name;

        for (auto& c : lower) c = (char)tolower((unsigned char)c);

        bool isOgg = lower.size() > 4 && lower.substr(lower.size() - 4) == ".ogg";

        bool isWav = lower.size() > 4 && lower.substr(lower.size() - 4) == ".wav";

        if (!isOgg && !isWav) continue;

        int fileSize = (int)((ULONGLONG)fd.nFileSizeHigh << 32 | fd.nFileSizeLow);

        if (fileSize <= 0) continue;

        std::string relPath = fullPath.substr(baseDir.size() + 1);

        std::string relNoExt = relPath.substr(0, relPath.size() - 4);

        for (auto& c : relNoExt) if (c == '\\') c = '/';

        std::string label;

        auto it = fileToEvent.find(relNoExt);

        if (it != fileToEvent.end()) {

            label = it->second;

        }

        else {

            // heuristic: step/wood6 -> step.wood, mob/creeper/say1 -> mob.creeper.say

            label = relNoExt;

            for (auto& c : label) if (c == '/') c = '.';

            // strip trailing digits from last component

            size_t lastDot = label.find_last_of('.');

            if (lastDot != std::string::npos) {

                size_t i = label.size();

                while (i > lastDot + 1 && isdigit((unsigned char)label[i - 1])) i--;

                if (i < label.size()) label = label.substr(0, i);

            }

        }

        g_sizeToName[fileSize] = label;

        if (isOgg && fileSize > 100) {

            FILE* f = nullptr;

            fopen_s(&f, fullPath.c_str(), "rb");

            if (f) {

                std::vector<unsigned char> data(fileSize);

                fread(data.data(), 1, fileSize, f);

                fclose(f);

                int channels = 0;

                for (int i = 0; i < 100 && i < fileSize - 40; i++) {

                    if (data[i] == 'v' && data[i + 1] == 'o' && data[i + 2] == 'r' && data[i + 3] == 'b') { channels = data[i + 10]; break; }

                }

                long long totalSamples = 0;

                for (int i = fileSize - 14; i > fileSize - 4096 && i >= 0; i--) {

                    if (data[i] == 'O' && data[i + 1] == 'g' && data[i + 2] == 'g' && data[i + 3] == 'S') { totalSamples = *(long long*)(&data[i + 6]); break; }

                }

                int pcmSize = 0;

                if (channels > 0 && channels <= 2 && totalSamples > 0) pcmSize = (int)(totalSamples * channels * 2);

                if (pcmSize > 0) {

                    for (int d = -4; d <= 4; d++) g_sizeToName[pcmSize + d] = label;

                }

            }

        }

    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

}



// Загружает sounds.json напрямую (без ZIP), наполняет g_pathToEvent.
// Полезно если файл просто лежит рядом с DLL или по известному пути.
static void LoadSoundsJsonFile(const std::string& path) {
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 50 * 1024 * 1024) { fclose(f); return; }
    std::string content;
    content.resize(sz);
    fread(&content[0], 1, sz, f);
    fclose(f);

    auto fileToEvent = ParseSoundsJsonContent(content);
    if (fileToEvent.empty()) return;

    std::lock_guard<std::mutex> lk(g_pathToEventMutex);
    for (const auto& kv : fileToEvent) {
        // Ключ может быть с префиксом "rustme/" — кладём оба варианта.
        const std::string& key = kv.first;
        g_pathToEvent[key] = kv.second;
        if (key.size() > 7 && key.substr(0, 7) == "rustme/") {
            g_pathToEvent[key.substr(7)] = kv.second;
        } else {
            g_pathToEvent[std::string("rustme/") + key] = kv.second;
        }
    }
}

static void ExtractSoundNamesFromDir(const std::string& rpDir) {    std::string rustmeDir = rpDir + "\\assets\\minecraft\\sounds\\rustme";

    DWORD attr = GetFileAttributesA(rustmeDir.c_str());

    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return;

    std::unordered_map<std::string, std::string> fileToEvent;

    std::string jsonPath = rpDir + "\\assets\\minecraft\\sounds.json";

    FILE* jf = nullptr;

    fopen_s(&jf, jsonPath.c_str(), "rb");

    if (jf) {

        fseek(jf, 0, SEEK_END);

        long sz = ftell(jf);

        fseek(jf, 0, SEEK_SET);

        std::vector<char> buf(sz + 1);

        fread(buf.data(), 1, sz, jf);

        buf[sz] = 0;

        fclose(jf);

        const char* p = buf.data();

        std::string curEvent;

        while (*p) {

            while (*p && *p != '"') p++;

            if (!*p) break;

            p++;

            const char* ss = p;

            while (*p && *p != '"') p++;

            if (!*p) break;

            std::string str(ss, p - ss);

            p++;

            while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

            if (*p == ':') {

                p++;

                while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

                if (*p == '{') { curEvent = str; p++; }

                else if (*p == '[' && str == "sounds") {

                    p++;

                    while (*p && *p != ']') {

                        if (*p == '"') {

                            p++;

                            const char* vs = p;

                            while (*p && *p != '"') p++;

                            std::string sp(vs, p - vs);

                            if (*p) p++;

                            if (!curEvent.empty() && sp != "none") {

                                if (sp.size() > 7 && sp.substr(0, 7) == "rustme/")

                                    fileToEvent[sp.substr(7)] = curEvent;

                                else

                                    fileToEvent[sp] = curEvent;

                            }

                        }

                        else p++;

                    }

                    if (*p) p++;

                }

                else if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p) p++; }

            }

        }

    }

    // Store path->event mapping for CreateFileW hook lookups
    if (!fileToEvent.empty()) {
        std::lock_guard<std::mutex> lk(g_pathToEventMutex);
        for (const auto& kv : fileToEvent) {
            g_pathToEvent[kv.first] = kv.second;
        }
    }

    ScanSoundsDir(rustmeDir, rustmeDir, fileToEvent);

}



static void StoreSoundFromPath(const wchar_t* wpath) {

    if (!wpath) return;

    char buf[1024];

    wcstombs_s(nullptr, buf, sizeof(buf), wpath, _TRUNCATE);

    std::string p(buf);

    std::string lower = p;

    for (auto& c : lower) c = (char)tolower((unsigned char)c);

    // Check if it's a sound file being opened

    if (lower.find(".ogg") == std::string::npos && lower.find(".wav") == std::string::npos) return;

    if (lower.find("sounds") == std::string::npos) return;

    // Extract the relative path after "sounds/"

    size_t sPos = lower.find("sounds/");

    if (sPos == std::string::npos) sPos = lower.find("sounds\\");

    if (sPos == std::string::npos) return;

    std::string relPath = p.substr(sPos + 7);

    if (relPath.size() > 4) relPath = relPath.substr(0, relPath.size() - 4);

    for (auto& c : relPath) if (c == '\\') c = '/';

    // Look up event name

    std::string label;

    {

        std::lock_guard<std::mutex> lk(g_pathToEventMutex);

        auto it = g_pathToEvent.find(relPath);

        if (it != g_pathToEvent.end()) {

            label = it->second;

        } else {

            // Try lowercase

            std::string relLower = relPath;

            for (auto& c : relLower) c = (char)tolower((unsigned char)c);

            auto it2 = g_pathToEvent.find(relLower);

            if (it2 != g_pathToEvent.end()) {

                label = it2->second;

            }

        }

    }

    if (label.empty()) {

        // Fallback: use path with dots

        label = relPath;

        for (auto& c : label) if (c == '/') c = '.';

    }

    // Кладём в глобальную очередь — alBufferData может прийти в другом потоке.
    {
        std::lock_guard<std::mutex> lk(g_pendingSoundsMutex);
        ULONGLONG now = GetTickCount64();
        // Чистим протухшие.
        g_pendingSounds.erase(
            std::remove_if(g_pendingSounds.begin(), g_pendingSounds.end(),
                [now](const PendingSound& p) { return (now - p.time) > kPendingTTLms; }),
            g_pendingSounds.end());
        g_pendingSounds.push_back({ label, now });
        // Ограничим размер очереди.
        if (g_pendingSounds.size() > 64) {
            g_pendingSounds.erase(g_pendingSounds.begin(),
                g_pendingSounds.begin() + (g_pendingSounds.size() - 64));
        }
    }

}







void InitOpenALSoundHook() {

    // Hook CreateFileW to track sound file opens

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

    if (!kernel32) kernel32 = LoadLibraryA("kernel32.dll");

    if (kernel32) {

        original_CreateFileW = (HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE))
        GetProcAddress(kernel32, "CreateFileW");

    }

    // Локальные кандидаты в текущей рабочей директории.
    const char* candidates[] = {
        "rustme-rp.zip",
        "C:\\Users\\Public\\Documents\\RustMeRP\\rustme-rp.zip",
        "C:\\RustMeRP\\rustme-rp.zip",
    };

    // Получаем актуальный %APPDATA% для текущего пользователя.
    char appdata[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        // Все профили лаунчера rustme.
        std::string profilesRoot = std::string(appdata) + "\\rustme-launcher\\profiles";
        WIN32_FIND_DATAA pfd;
        HANDLE hProf = FindFirstFileA((profilesRoot + "\\*").c_str(), &pfd);
        if (hProf != INVALID_HANDLE_VALUE) {
            do {
                if (!(pfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (pfd.cFileName[0] == '.') continue;
                std::string rpDir = profilesRoot + "\\" + pfd.cFileName + "\\resourcepacks";
                WIN32_FIND_DATAA fd;
                HANDLE hFind = FindFirstFileA((rpDir + "\\*.zip").c_str(), &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        std::string zipPath = rpDir + "\\" + fd.cFileName;
                        ExtractSoundNamesFromZip(zipPath);
                    } while (FindNextFileA(hFind, &fd));
                    FindClose(hFind);
                }
                // Также распакованные ресурс-паки (директории).
                ExtractSoundNamesFromDir(rpDir);
            } while (FindNextFileA(hProf, &pfd));
            FindClose(hProf);
        }

        // Стандартный Minecraft Bedrock/Java тоже можно зацепить мимоходом.
        // .minecraft\resourcepacks
        std::string mcRp = std::string(appdata) + "\\.minecraft\\resourcepacks";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA((mcRp + "\\*.zip").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                ExtractSoundNamesFromZip(mcRp + "\\" + fd.cFileName);
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }

    for (const char* path : candidates) {

        ExtractSoundNamesFromZip(path);

    }

    // Распакованные ресурс-паки — пути по умолчанию.
    {
        const char* dirCandidates[] = {
            "C:\\Users\\admin\\Desktop\\rp",
            "C:\\Users\\admin\\Desktop\\newHOOKS\\rp",
            "rp",
        };
        for (const char* d : dirCandidates) {
            DWORD attr = GetFileAttributesA(d);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                ExtractSoundNamesFromDir(d);
            }
        }
    }

    // Прямая загрузка sounds.json — несколько кандидатов.
    {
        const char* jsonCandidates[] = {
            "sounds.json",
            "C:\\Users\\admin\\Desktop\\newHOOKS\\sounds.json",
            "C:\\Users\\admin\\Desktop\\rp\\assets\\minecraft\\sounds.json",
        };
        for (const char* p : jsonCandidates) LoadSoundsJsonFile(p);

        // Рядом с DLL.
        char dllPath[MAX_PATH] = {};
        HMODULE hMod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&LoadSoundsJsonFile, &hMod);
        if (hMod) {
            GetModuleFileNameA(hMod, dllPath, MAX_PATH);
            std::string p(dllPath);
            size_t slash = p.find_last_of("\\/");
            if (slash != std::string::npos) {
                LoadSoundsJsonFile(p.substr(0, slash + 1) + "sounds.json");
            }
        }

        char appdata[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
            LoadSoundsJsonFile(std::string(appdata) + "\\soyuz\\sounds.json");
        }
    }

}



void ShutdownOpenALSoundHook() {

    remove_hook(&hook_createFile);

}



bool InitOpenAL() {

    if (alGetListener3f) return true;

    HMODULE hMod = GetModuleHandleA("openal64.dll");

    if (!hMod) hMod = LoadLibraryA("openal64.dll");

    if (hMod) {

        alGetListener3f = (alGetListener3f_t)GetProcAddress(hMod, "alGetListener3f");

        p_alGetError = (alGetError_fn)GetProcAddress(hMod, "alGetError");

        p_alSourcePlay = (alSourcePlay_fn)GetProcAddress(hMod, "alSourcePlay");

        p_alGetSource3f = (alGetSource3f_fn)GetProcAddress(hMod, "alGetSource3f");

        if (alGetListener3f) {

            g_openalLoaded = true;

            g_openalFuncsLoaded = true;

            return true;

        }

    }

    return false;

}



void UpdateOpenALListenerPos() {

    if (!alGetListener3f) return;

    ALfloat posX = 0.0f, posY = 0.0f, posZ = 0.0f;

    alGetListener3f(AL_POSITION, &posX, &posY, &posZ);

    g_listenerPosX = posX;

    g_listenerPosY = posY;

    g_listenerPosZ = posZ;

}



void __cdecl hooked_alSourcePlay(ALuint source) {

    if (g_soundEspEnabled && p_alGetSource3f) {

        ALfloat posX = 0.0f, posY = 0.0f, posZ = 0.0f;

        p_alGetSource3f(source, AL_POSITION, &posX, &posY, &posZ);

        if (p_alGetError() == AL_NO_ERROR) {

            SoundInfo s;

            s.pos = glm::vec3(posX, posY, posZ);

            s.time = GetTickCount64();

            s.source = source;

            static alGetSourcei_fn p_gsi = nullptr;

            if (!p_gsi) { HMODULE h = GetModuleHandleA("openal64.dll"); if (h) p_gsi = (alGetSourcei_fn)GetProcAddress(h, "alGetSourcei"); }

            int bufID = 0;

            if (p_gsi) p_gsi(source, 0x1009, &bufID);

            int bSize = 0, bFreq = 0;

            if (bufID != 0) {

                static alGetBufferi_fn p_gbi = nullptr;

                if (!p_gbi) { HMODULE h = GetModuleHandleA("openal64.dll"); if (h) p_gbi = (alGetBufferi_fn)GetProcAddress(h, "alGetBufferi"); }

                if (p_gbi) {

                    p_gbi((ALuint)bufID, 0x2004, &bSize);

                    p_gbi((ALuint)bufID, 0x2002, &bFreq);

                }

                {

                    std::lock_guard<std::mutex> bn(g_bufferNamesMutex);

                    auto it = g_bufferNames.find((ALuint)bufID);

                    if (it != g_bufferNames.end()) s.name = it->second;

                }

                // Точный size match.
                if (s.name.empty() && bSize > 0) {

                    auto it = g_sizeToName.find(bSize);

                    if (it != g_sizeToName.end()) s.name = it->second;

                }

                if (s.name.empty())

                    s.name = "unknown";

            }

            std::lock_guard<std::mutex> lock(g_soundsMutex);

            bool found = false;
            for (auto& existing : g_sounds) {
                if (glm::distance(existing.pos, s.pos) < 1.0f) {
                    existing.time = GetTickCount64();
                    found = true;
                    break;
                }
            }
            if (!found) {
                g_sounds.push_back(s);
            }

        }

    }

    if (original_alSourcePlay_addr) ((alSourcePlay_fn)original_alSourcePlay_addr)(source);

}



static void LoadSoundNamesFromZip(const char* zipPath, const char* soundsFolder) {
    // Delegates to ExtractSoundNamesFromZip which handles sounds.json parsing
    ExtractSoundNamesFromZip(std::string(zipPath));
}

void ShutdownFileHooks() {
}