#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>
#include <atomic>
#include <mutex>
#include "license.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

#pragma comment(lib, "C:\\Users\\unknown\\Desktop\\vmp 3.9.4 ultimate\\Lib\\Windows\\VMProtectSDK64.lib")

#include "C:\\Users\\unknown\\Desktop\\vmp 3.9.4 ultimate\\Include\\C\\VMProtectSDK.h"

static std::atomic<bool> g_licenseValid(false);
static std::atomic<bool> g_credentialsLoaded(false);
static std::mutex g_credentialsMutex;
static std::string g_apiUrl;
static std::string g_login;
static std::string g_uid;
static std::string g_hwid;
static std::string g_licenseToken;

static bool ReadCredentials() {
    if (g_credentialsLoaded.load(std::memory_order_acquire)) return true;

    HANDLE hMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\oyuz_shm_creds");
    if (!hMapping) return false;

    const char* pShm = (const char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 2048);
    if (!pShm) { CloseHandle(hMapping); return false; }

    std::istringstream f(std::string(pShm, strnlen(pShm, 2048)));
    UnmapViewOfFile(pShm);
    CloseHandle(hMapping);

    std::string login;
    std::string uid;
    std::string hwid;
    std::string licenseToken;
    std::string apiUrl;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "login") login = val;
        else if (key == "uid") uid = val;
        else if (key == "hwid") hwid = val;
        else if (key == "license_token") licenseToken = val;
        else if (key == "api_url") apiUrl = val;
    }

    if (login.empty() || hwid.empty() || licenseToken.empty() || apiUrl.empty()) return false;

    {
        std::lock_guard<std::mutex> lock(g_credentialsMutex);
        g_login = login;
        g_uid = uid;
        g_hwid = hwid;
        g_licenseToken = licenseToken;
        g_apiUrl = apiUrl;
    }
    g_credentialsLoaded.store(true, std::memory_order_release);
    return true;
}

static bool GetCredentialsSnapshot(std::string& login, std::string& hwid, std::string& licenseToken) {
    if (!ReadCredentials()) return false;

    std::lock_guard<std::mutex> lock(g_credentialsMutex);
    login = g_login;
    hwid = g_hwid;
    licenseToken = g_licenseToken;
    return !login.empty() && !hwid.empty() && !licenseToken.empty();
}

static std::string HttpPostLicense(const std::string& path, const std::string& body) {
    
    std::string result;
    std::string url;
    {
        std::lock_guard<std::mutex> lock(g_credentialsMutex);
        url = g_apiUrl;
    }

    // Determine scheme
    bool isHttps = false;
    size_t proto = url.find("://");
    if (proto != std::string::npos) {
        isHttps = (url.substr(0, proto) == "https");
        url = url.substr(proto + 3);
    }

    // Parse host, port, path
    int port = isHttps ? 443 : 80;
    size_t colon = url.find(':');
    size_t slash = url.find('/');
    std::string host;
    if (colon != std::string::npos) {
        host = url.substr(0, colon);
        size_t portEnd = (slash != std::string::npos) ? slash : url.size();
        port = std::stoi(url.substr(colon + 1, portEnd - colon - 1));
    } else if (slash != std::string::npos) {
        host = url.substr(0, slash);
    } else {
        host = url;
    }
    if (host.empty()) return result;

    HINTERNET hSession = WinHttpOpen(L"oyuz-cheat/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return result;
    DWORD timeout = 10000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_RESOLVE_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    wchar_t whost[256];
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, whost, 256);
    HINTERNET hConnect = WinHttpConnect(hSession, whost, port, 0);
    if (hConnect) {
        wchar_t wpath[1024];
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath, 1024);
        DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath, NULL, NULL, NULL, flags);
        if (hRequest) {
            if (isHttps) {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
            }
            WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
                if (WinHttpReceiveResponse(hRequest, NULL)) {
                    DWORD size = 0;
                    if (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
                        std::vector<char> buf(size + 1);
                        DWORD read = 0;
                        WinHttpReadData(hRequest, buf.data(), size, &read);
                        buf[read] = 0;
                        result.assign(buf.data(), read);
                    }
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    
    return result;
}

static std::string FindJsonStr(const std::string& json, const char* key) {
    std::string k = "\"" + std::string(key) + "\"";
    size_t p = json.find(k);
    if (p == std::string::npos) return "";
    p = json.find('"', p + k.size() + 1);
    if (p == std::string::npos) return "";
    size_t e = json.find('"', p + 1);
    if (e == std::string::npos) return "";
    return json.substr(p + 1, e - p - 1);
}

static DWORD WINAPI LicenseThread(LPVOID) {
    VMProtectBeginUltra("lic");
    std::string login;
    std::string hwid;
    std::string licenseToken;
    if (!GetCredentialsSnapshot(login, hwid, licenseToken)) {
        g_licenseValid = false;
        return 0;
    }
    
    // Initial verification
    std::string body = "{\"login\":\"" + login + "\",\"hwid\":\"" + hwid + "\",\"license_token\":\"" + licenseToken + "\"}";
    std::string resp = HttpPostLicense("/api/license/verify", body);
    std::string status = FindJsonStr(resp, "status");
    g_licenseValid = (status == "valid");
    

    // Periodic re-verification every 60s
    while (g_licenseValid) {
        Sleep(60000);
        resp = HttpPostLicense("/api/license/verify", body);
        status = FindJsonStr(resp, "status");
        g_licenseValid = (status == "valid");
    }
    VMProtectEnd();
    return 0;
}

void License_Init() {
    HANDLE hThread = CreateThread(NULL, 0, LicenseThread, NULL, 0, NULL);
    if (hThread) CloseHandle(hThread);
}

bool License_PreloadCredentials() {
    return ReadCredentials();
}

bool License_IsValid() {
    return g_licenseValid.load();
}

std::string License_GetLogin() {
    ReadCredentials();

    std::lock_guard<std::mutex> lock(g_credentialsMutex);
    return g_login;
}

std::string License_GetUid() {
    ReadCredentials();

    std::lock_guard<std::mutex> lock(g_credentialsMutex);
    return g_uid;
}
