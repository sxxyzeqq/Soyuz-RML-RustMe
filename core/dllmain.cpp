#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wincrypt.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include "Globals.h"
#include "Config.h"
#include "hooks/Hooks.h"
#include "TrampolineHook.h"
#include "hooks/OpenGLHooks.h"
#include "hooks/OpenAL.h"
#include "gui/GUI.h"
#include "modules/ESP.h"
#include "modules/Aimbot.h"
#include "modules/Viewmodel.h"
#include "modules/PlayerChams.h"
#include "modules/WallCheck.h"
#include "modules/ChinaHat.h"
#include "modules/TracersNew.h"
#include "modules/NoFall.h"
#include "license.h"
#include "imgui/backends/imgui_impl_opengl2.h"
#include "imgui/backends/imgui_impl_win32.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "C:\\Users\\unknown\\Desktop\\vmp 3.9.4 ultimate\\Lib\\Windows\\VMProtectSDK64.lib")


#include "C:\\Users\\unknown\\Desktop\\vmp 3.9.4 ultimate\\Include\\C\\VMProtectSDK.h"



static bool VerifyLoaderHash() {
    VMProtectBeginUltra("LoaderHash");
    // Read injection_token and connection info from shared memory (no file on disk)
    HANDLE hMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\oyuz_shm_creds");
    if (!hMapping) return false;

    const char* pShm = (const char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 2048);
    if (!pShm) { CloseHandle(hMapping); return false; }

    std::string injection_token, hwid, api_url;
    std::istringstream ss(std::string(pShm, strnlen(pShm, 2048)));
    UnmapViewOfFile(pShm);
    CloseHandle(hMapping);

    std::string line;
    while (std::getline(ss, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.compare(0, 16, "injection_token=") == 0) injection_token = line.substr(16);
        else if (line.compare(0, 5, "hwid=") == 0) hwid = line.substr(5);
        else if (line.compare(0, 8, "api_url=") == 0) api_url = line.substr(8);
    }

    if (injection_token.empty() || hwid.empty() || api_url.empty()) return false;

    // Parse api_url to get host, port, https
    bool isHttps = false;
    std::string url = api_url;
    size_t proto = url.find("://");
    if (proto != std::string::npos) {
        isHttps = (url.substr(0, proto) == "https");
        url = url.substr(proto + 3);
    }
    int port = isHttps ? 443 : 80;
    size_t colon = url.find(':');
    std::string host;
    if (colon != std::string::npos) {
        host = url.substr(0, colon);
        size_t slash = url.find('/', colon);
        size_t portEnd = (slash != std::string::npos) ? slash : url.size();
        port = atoi(url.substr(colon + 1, portEnd - colon - 1).c_str());
    } else {
        size_t slash = url.find('/');
        host = (slash != std::string::npos) ? url.substr(0, slash) : url;
    }
    if (host.empty()) return false;

    // POST /api/inject-verify with injection_token + hwid
    std::string body = "{\"injection_token\":\"" + injection_token + "\",\"hwid\":\"" + hwid + "\"}";

    wchar_t whost[256] = {};
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, whost, 256);

    HINTERNET hSession = WinHttpOpen(L"oyuz-cheat/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return false;

    DWORD timeout = 10000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_RESOLVE_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConnect = WinHttpConnect(hSession, whost, port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/inject-verify", NULL, NULL, NULL, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    if (isHttps) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }

    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    bool valid = false;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD size = 0;
            if (WinHttpQueryDataAvailable(hRequest, &size) && size > 0) {
                std::vector<char> buf(size + 1);
                DWORD read = 0;
                WinHttpReadData(hRequest, buf.data(), size, &read);
                buf[read] = 0;
                std::string resp(buf.data(), read);
                // Check for "status":"valid"
                size_t pos = resp.find("\"status\"");
                if (pos != std::string::npos) {
                    size_t vp = resp.find("\"valid\"", pos);
                    if (vp != std::string::npos) valid = true;
                }
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    VMProtectEnd();
    return valid;
}

// ---------------------------------------------------------------------------
// Trampoline hook instances (one per hooked function)
// Analogous to aethelis xmmword_1802F6440 table entries.
// HWBP hooks (hwbp_clear, hwbp_clearcolor, hwbp_fogfv, hwbp_fogf) are
// intentionally left in hooks/Hooks.h — untouched per user request.
// ---------------------------------------------------------------------------
static TrampolineHook tramp_swapbuffers;
static TrampolineHook tramp_scalef;
static TrampolineHook tramp_ortho;
static TrampolineHook tramp_translatef;
static TrampolineHook tramp_rotatef;
static TrampolineHook tramp_loadidentity;
static TrampolineHook tramp_pushmatrix;
static TrampolineHook tramp_popmatrix;
static TrampolineHook tramp_enable;
static TrampolineHook tramp_disable;
static TrampolineHook tramp_translated;
static TrampolineHook tramp_frustum;
static TrampolineHook tramp_loadmatrixf;
static TrampolineHook tramp_loadmatrixd;
static TrampolineHook tramp_multmatrixf;
static TrampolineHook tramp_multmatrixd;
static TrampolineHook tramp_matrixmode;
static TrampolineHook tramp_getfloatv;
static TrampolineHook tramp_wglgetprocaddress;
static TrampolineHook tramp_color4f;
static TrampolineHook tramp_color3f;
static TrampolineHook tramp_alSourcePlay;
static TrampolineHook tramp_timegettime;

static void* ResolveGLAddress(const char* name) {
    void* result = nullptr;
    void* addr;
    HMODULE opengl;

    opengl = GetModuleHandleA("opengl32.dll");
    if (opengl) {
        addr = (void*)GetProcAddress(opengl, name);
        if (addr) { result = addr; goto _end; }
    }

    addr = (void*)wglGetProcAddress(name);
    if (addr && (INT_PTR)addr > 0x10000) { result = addr; goto _end; }
_end:
    return result;
}

static bool ResolveAllGLAddresses() {
    bool result = false;
    static const char* kClassName = "TempGLClass_Soyuz";
    HWND tempHwnd = nullptr;
    HDC tempHdc = nullptr;
    HGLRC tempRc = nullptr;
    bool classRegistered = false;
    PIXELFORMATDESCRIPTOR pfd = {};
    int pf = 0;

    WNDCLASSEXA wc = {};

    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = kClassName;
    UnregisterClassA(kClassName, g_hInstance);
    if (!RegisterClassExA(&wc)) goto _cleanup;
    classRegistered = true;

    tempHwnd = CreateWindowExA(0, kClassName, "", WS_POPUP | WS_DISABLED, 0, 0, 1, 1, NULL, NULL, g_hInstance, NULL);
    if (!tempHwnd) goto _cleanup;

    tempHdc = GetDC(tempHwnd);
    if (!tempHdc) goto _cleanup;

    pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    pf = ChoosePixelFormat(tempHdc, &pfd);
    if (!pf) {
        pf = ChoosePixelFormat(tempHdc, &pfd);
        if (!pf) goto _cleanup;
    }
    if (!SetPixelFormat(tempHdc, pf, &pfd)) goto _cleanup;

    tempRc = wglCreateContext(tempHdc);
    if (!tempRc) goto _cleanup;
    if (!wglMakeCurrent(tempHdc, tempRc)) goto _cleanup;

    original_glOrtho_addr      = ResolveGLAddress("glOrtho");
    original_glScalef_addr     = ResolveGLAddress("glScalef");
    original_glTranslatef_addr = ResolveGLAddress("glTranslatef");
    original_glRotatef_addr    = ResolveGLAddress("glRotatef");
    original_wglSwapBuffers_addr = ResolveGLAddress("wglSwapBuffers");
    original_glLoadIdentity_addr = ResolveGLAddress("glLoadIdentity");
    original_glPushMatrix_addr   = ResolveGLAddress("glPushMatrix");
    original_glPopMatrix_addr    = ResolveGLAddress("glPopMatrix");
    original_glEnable_addr       = ResolveGLAddress("glEnable");
    original_glDisable_addr      = ResolveGLAddress("glDisable");
    original_glColor4f_addr      = ResolveGLAddress("glColor4f");
    original_glColor3f_addr      = ResolveGLAddress("glColor3f");
    original_glTranslated_addr   = ResolveGLAddress("glTranslated");
    original_glClear_addr        = ResolveGLAddress("glClear");
    original_glClearColor_addr   = ResolveGLAddress("glClearColor");
    original_glFogfv_addr        = ResolveGLAddress("glFogfv");
    original_glFogf_addr         = ResolveGLAddress("glFogf");
    original_glFrustum_addr      = ResolveGLAddress("glFrustum");
    original_glLoadMatrixf_addr  = ResolveGLAddress("glLoadMatrixf");
    original_glLoadMatrixd_addr  = ResolveGLAddress("glLoadMatrixd");
    original_glMultMatrixf_addr  = ResolveGLAddress("glMultMatrixf");
    original_glMultMatrixd_addr  = ResolveGLAddress("glMultMatrixd");
    original_glMatrixMode_addr   = ResolveGLAddress("glMatrixMode");
    original_glGetFloatv_addr    = ResolveGLAddress("glGetFloatv");
    original_wglGetProcAddress_addr = ResolveGLAddress("wglGetProcAddress");
    if (original_wglGetProcAddress_addr) {
        PROC queryProc = ((wglGetProcAddress_fn)original_wglGetProcAddress_addr)("glGetQueryObjectiv");
        if (queryProc && reinterpret_cast<INT_PTR>(queryProc) > 0x10000) {
            original_glGetQueryObjectiv_addr = reinterpret_cast<void*>(queryProc);
        }
    }
    original_glLogicOp_addr      = ResolveGLAddress("glLogicOp");
    original_glBlendFunc_addr    = ResolveGLAddress("glBlendFunc");

    result = (original_glOrtho_addr && original_wglSwapBuffers_addr && original_glScalef_addr);

_cleanup:
    if (tempRc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(tempRc); }
    if (tempHdc && tempHwnd) ReleaseDC(tempHwnd, tempHdc);
    if (tempHwnd) DestroyWindow(tempHwnd);
    if (classRegistered) UnregisterClassA(kClassName, g_hInstance);
    return result;
}



bool IsImGuiMenuOpen() {
    return GUI::GetInitializationState() && GUI::GetDrawState();
}

bool IsTargetWindowActive()
{
    const char* targetTitles[] = { "RustMe Client" };
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) return false;
    char windowTitle[256];
    GetWindowTextA(foregroundWindow, windowTitle, sizeof(windowTitle));
    for (const char* title : targetTitles) {
        if (strstr(windowTitle, title) != nullptr) {
            return true;
        }
    }

    return false;


}

BOOL WINAPI hooked_wglSwapBuffers(HDC hdc) {
    BOOL swpResult = TRUE;
    float rScale = 1.0f, gScale = 1.0f, bScale = 1.0f;
    float rBias = 0.0f, gBias = 0.0f, bBias = 0.0f;
    if (g_unloading) {
        glPixelTransferf(GL_RED_SCALE, 1.0f);
        glPixelTransferf(GL_GREEN_SCALE, 1.0f);
        glPixelTransferf(GL_BLUE_SCALE, 1.0f);
        glPixelTransferf(GL_RED_BIAS, 0.0f);
        glPixelTransferf(GL_GREEN_BIAS, 0.0f);
        glPixelTransferf(GL_BLUE_BIAS, 0.0f);

        if (original_wglSwapBuffers_addr)
            swpResult = ((wglSwapBuffers_fn)original_wglSwapBuffers_addr)(hdc);
        goto _end;
    }
    
    g_renderingFrame.store(true);
    g_loadIdentityCount = 0;
    { static bool once = false; if (!once) { once = true; License_Init(); } }
    ProcessBinds();
    

    TimerModule::UpdateTime();
    NoFall_Update();


    g_frameCameraCaptured = false;
    g_cameraSetupState = -1;

    PlayerChamsFrameReset();

    if (g_aimAnimatedFov) {
        bool lmbPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        float targetFov = lmbPressed ? g_aimFov + g_fovAnim : g_aimFov;
        float smooth = lmbPressed ? 0.3f : 0.15f;
        g_aimCurrentFov += (targetFov - g_aimCurrentFov) * smooth;
    }
    else {
        g_aimCurrentFov = g_aimFov;
    }


    UpdateOpenALListenerPos();


    static glm::mat4 prevCameraMatrix = glm::mat4(1.0f);
    if (g_frameCameraCaptured) {
        float camDelta = glm::length(glm::vec3(g_cameraMatrix[3]) - glm::vec3(prevCameraMatrix[3]));
        float rotDelta = 0.0f;
        for (int i = 0; i < 3; i++) rotDelta += glm::length(glm::vec3(g_cameraMatrix[i]) - glm::vec3(prevCameraMatrix[i]));
        if (camDelta > 1.0f || rotDelta > 0.5f) g_aimTracks.clear();
        prevCameraMatrix = g_cameraMatrix;
    }

    if (!g_imguiInitialized) InitImGui(hdc);
    if (GetAsyncKeyState(VK_RSHIFT) & 1) g_menuVisible = !g_menuVisible;

    if (g_imguiInitialized) {
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        g_watermarkAnimTime += ImGui::GetIO().DeltaTime;
        float raw = (sinf(g_watermarkAnimTime * g_accentSpeed * 0.5f) + 1.0f) * 0.5f;
        // Smoothstep for smoother transition instead of harsh sin bounce
        float t = raw * raw * (3.0f - 2.0f * raw);
        g_currentAccentColor.x = g_accentColor1[0] + (g_accentColor2[0] - g_accentColor1[0]) * t;
        g_currentAccentColor.y = g_accentColor1[1] + (g_accentColor2[1] - g_accentColor1[1]) * t;
        g_currentAccentColor.z = g_accentColor1[2] + (g_accentColor2[2] - g_accentColor1[2]) * t;
        g_currentAccentColor.w = 1.0f;
        RenderGUI();
        RenderOpenALListenerInfo();


        if (g_espEnabled || g_ammoEspEnabled || g_tracerEnabled) {
            double currentTime = std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();
            RenderESP(currentTime);
        }
        if (g_tracerNewEnabled)
            RenderTracersNew();

        RenderSoundESP();

        if (g_chinaHatEnabled)
            RenderChinaHat();


        if (g_aimEnabled && (!g_aimAutoDisable || IsGameplayCursor())) {
            std::lock_guard<std::mutex> lock(g_entitiesMutex);
            if (!g_entities.empty()) {
                GLint viewport[4];
                glGetIntegerv(GL_VIEWPORT, viewport);
                for (const auto& ent : g_entities) {
                    if (ent.isPlayer) {
                        if (AimMcfShouldSkip(ent)) continue;

                        // Visible Only: пропускаем невидимых.
                        if (g_aimVisibleOnly) {
                            if (!WallCheckIsVisible(ent)) continue;
                        }
                        glm::vec3 camPos(g_listenerPosX, g_listenerPosY, g_listenerPosZ);
                        if (glm::length(camPos) < 0.001f) camPos = glm::vec3(g_cameraMatrix[3]);
                        glm::vec3 worldPosition = camPos + ent.translatePos;
                        float targetDistance = glm::length(worldPosition - camPos);
                        double currentTime = std::chrono::duration_cast<std::chrono::duration<double>>(
                            std::chrono::high_resolution_clock::now().time_since_epoch()
                        ).count();
                        glm::vec3 worldVelocity(0.0f);
                        AimTrack* bestTrack = nullptr;
                        float minDistance = 5.0f;
                        for (auto& track : g_aimTracks) {
                            if (!track.active) continue;
                            float d = glm::distance(track.lastWorldPos, worldPosition);
                            if (d < minDistance) { minDistance = d; bestTrack = &track; }
                        }
                        if (bestTrack) {
                            double dt = currentTime - bestTrack->lastUpdateTime;
                            if (dt > 0.005 && dt < 0.5) {
                                glm::vec3 rawDelta = worldPosition - bestTrack->lastWorldPos;
                                rawDelta.y = 0.0f;
                                glm::vec3 newVelocity = rawDelta * (0.05f / static_cast<float>(dt));

                                // Хард-снап если игрок практически не двигается за этот кадр.
                                // 0.05 блока за тик ≈ 1 блок/сек — медленнее любого реального движения.
                                float speedPerTick = glm::length(newVelocity);
                                float oldSpeedPerTick = glm::length(bestTrack->velocityPerTick);

                                if (speedPerTick < 0.05f) {
                                    // Игрок остановился — мгновенно гасим хвост.
                                    bestTrack->velocityPerTick = glm::vec3(0.0f);
                                }
                                else {
                                    // Асимметричный EMA: быстро реагируем на замедление,
                                    // медленнее — на ускорение (чтобы не дёргать на шуме).
                                    float smoothFactor = (speedPerTick < oldSpeedPerTick) ? 0.6f : 0.25f;
                                    bestTrack->velocityPerTick.x =
                                        bestTrack->velocityPerTick.x * (1.0f - smoothFactor) + newVelocity.x * smoothFactor;
                                    bestTrack->velocityPerTick.z =
                                        bestTrack->velocityPerTick.z * (1.0f - smoothFactor) + newVelocity.z * smoothFactor;
                                }

                                bestTrack->lastWorldPos = worldPosition;
                                bestTrack->lastUpdateTime = currentTime;
                            }
                            worldVelocity = bestTrack->velocityPerTick / 0.05f;
                        }
                        else {
                            AimTrack newTrack;
                            newTrack.lastWorldPos = worldPosition;
                            newTrack.lastUpdateTime = currentTime;
                            newTrack.velocity = glm::vec3(0.0f);
                            newTrack.velocityPerTick = glm::vec3(0.0f);
                            newTrack.active = true;
                            g_aimTracks.push_back(newTrack);
                        }

                        glm::mat4 predictedModelView = ent.modelview;
                        if (ent.hasWorldPos) {
                            // Новая логика предикта от пользователя
                            double pingTicks = ceil(g_aimPing / 50.0 + 2.0);
                            double currentDistance = glm::distance(camPos, worldPosition);
                            double flightSeconds = currentDistance / (double)g_aimBulletSpeed;
                            double flightTicks = ceil(flightSeconds * 20.0);

                            double predictTicks = pingTicks + flightTicks;

                            glm::vec3 predictionOffset(0.0f);
                            if (bestTrack) {
                                predictionOffset = bestTrack->velocityPerTick * (float)predictTicks;
                            }

                            // Вычисляем падение пули (drop) на основе горизонтального расстояния до предсказанной цели
                            glm::vec3 predictedWorldPos = worldPosition + predictionOffset;
                            double dx_p = (double)predictedWorldPos.x - (double)camPos.x;
                            double dz_p = (double)predictedWorldPos.z - (double)camPos.z;
                            double horizontalDistance = sqrt(dx_p * dx_p + dz_p * dz_p);
                            
                            double t_flight = horizontalDistance / (double)g_aimBulletSpeed;
                            double drop = 0.5 * 9.15 * t_flight * t_flight;
                            
                            predictionOffset.y += (float)drop;

                            glm::vec3 viewSpaceOffset = glm::mat3(g_cameraMatrix) * predictionOffset;
                            predictedModelView[3] += glm::vec4(viewSpaceOffset, 0.0f);
                        }

                        float aimHeight = -1.588f;
                        predictedModelView = glm::translate(predictedModelView, glm::vec3(0.0f, aimHeight, 0.0f));
                        glm::vec2 sp;
                        if (ProjectToScreen(glm::vec3(0.0f), predictedModelView, ent.projection, viewport, sp)) {
                            AimTarget t;
                            t.screenPosition = sp;
                            t.worldPosition = worldPosition;
                            t.worldVelocity = worldVelocity;
                            t.distance = targetDistance;
                            t.timestamp = currentTime;
                            t.valid = true;
                            g_aimTargets.push_back(t);
                        }
                    }
                }
            }

            RenderAimFOV();
        }

        {
            std::lock_guard<std::mutex> lock(g_entitiesMutex);
            // We clear entities AFTER everything that uses them is rendered.
            // TESTESP::Render() should be called BEFORE clearing.
        }
        glPixelTransferf(GL_RED_SCALE, 1.0f);
        glPixelTransferf(GL_GREEN_SCALE, 1.0f);
        glPixelTransferf(GL_BLUE_SCALE, 1.0f);
        glPixelTransferf(GL_RED_BIAS, 0.0f);
        glPixelTransferf(GL_GREEN_BIAS, 0.0f);
        glPixelTransferf(GL_BLUE_BIAS, 0.0f);

        g_uiRenderingStarted = true;
        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        { std::lock_guard<std::mutex> lock(g_entitiesMutex); g_entities.clear(); g_nameTags.clear(); }
        ClearTracersNewTargets();
        { std::lock_guard<std::mutex> lock(g_objectsMutex); g_objects.clear(); }
    }

    if (g_autoSprintEnabled) {
        bool wPressed = (GetAsyncKeyState('W') & 0x8000) != 0;
        if (wPressed && !g_sprintKeyHeld) {
            keybd_event(VK_LCONTROL, 0, 0, 0);
            g_sprintKeyHeld = true;
        }
        else if (!wPressed && g_sprintKeyHeld) {
            keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0);
            g_sprintKeyHeld = false;
        }
    }
    else if (g_sprintKeyHeld) {
        keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0);
        g_sprintKeyHeld = false;
    }

    if (g_hwnd) AimRunLogic(g_hwnd);
    g_renderingFrame.store(false);

    // Logic for Fullbright, NightMode, and Ambience using glPixelTransfer
    // Apply color tints first
    if (g_nightModeEnabled) {
        rScale *= g_nightColor[0];
        gScale *= g_nightColor[1];
        bScale *= g_nightColor[2];
    }

    if (g_ambince && g_world) {
        rScale *= g_ambinceColor[0];
        gScale *= g_ambinceColor[1];
        bScale *= g_ambinceColor[2];
    }

    if (g_fullbrightEnabled) {
        // Use BIAS for Fullbright to ensure minimum visibility without washing out the tint
        // We set bias to a large portion of the target color and scale to the rest
        rBias = rScale * 0.85f;
        gBias = gScale * 0.85f;
        bBias = bScale * 0.85f;

        rScale *= 0.15f;
        gScale *= 0.15f;
        bScale *= 0.15f;
    }

    glPixelTransferf(GL_RED_SCALE, rScale);
    glPixelTransferf(GL_GREEN_SCALE, gScale);
    glPixelTransferf(GL_BLUE_SCALE, bScale);
    glPixelTransferf(GL_RED_BIAS, rBias);
    glPixelTransferf(GL_GREEN_BIAS, gBias);
    glPixelTransferf(GL_BLUE_BIAS, bBias);

    swpResult = ((wglSwapBuffers_fn)original_wglSwapBuffers_addr)(hdc);
_end:
    return swpResult;
}

DWORD WINAPI UnloadThread(LPVOID lpParam) {
    g_unloading = true;
    g_world = false;
    g_ambince = false;
    for (int i = 0; i < 200 && g_renderingFrame.load(); i++) Sleep(10);
    Sleep(50);
    TrampolineHook_Remove(&tramp_swapbuffers);
    Sleep(50);
    for (int i = 0; i < 100 && g_renderingFrame.load(); i++) Sleep(10);
    Sleep(32);
    ShutdownImGui();
    ShutdownNoFallHook();
    ShutdownOpenALSoundHook();
    if (g_sprintKeyHeld) { keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0); g_sprintKeyHeld = false; }
    // HWBP hooks — untouched
    remove_hwbp_hook(&hwbp_clearcolor);
    remove_hwbp_hook(&hwbp_clear);
    remove_hwbp_hook(&hwbp_fogfv);
    remove_hwbp_hook(&hwbp_fogf);
    PVOID veh = GetVEHHandle();
    if (veh) { RemoveVectoredExceptionHandler(veh); SetVEHHandle(nullptr); }
    // Trampoline hooks — removed via aethelis-ported TrampolineHook_Remove
    TrampolineHook_Remove(&tramp_color4f);
    TrampolineHook_Remove(&tramp_color3f);
    TrampolineHook_Remove(&tramp_ortho);
    TrampolineHook_Remove(&tramp_scalef);
    TrampolineHook_Remove(&tramp_translatef);
    TrampolineHook_Remove(&tramp_rotatef);
    TrampolineHook_Remove(&tramp_loadidentity);
    TrampolineHook_Remove(&tramp_pushmatrix);
    TrampolineHook_Remove(&tramp_popmatrix);
    TrampolineHook_Remove(&tramp_timegettime);
    TrampolineHook_Remove(&tramp_enable);
    TrampolineHook_Remove(&tramp_disable);
    TrampolineHook_Remove(&tramp_translated);
    TrampolineHook_Remove(&tramp_alSourcePlay);
    ShutdownFileHooks();
    TrampolineHook_Remove(&tramp_frustum);
    TrampolineHook_Remove(&tramp_loadmatrixf);
    TrampolineHook_Remove(&tramp_loadmatrixd);
    TrampolineHook_Remove(&tramp_multmatrixf);
    TrampolineHook_Remove(&tramp_multmatrixd);
    TrampolineHook_Remove(&tramp_matrixmode);
    { std::lock_guard<std::mutex> lock(g_entitiesMutex); g_entities.clear(); g_nameTags.clear(); }
    ClearTracersNewTargets();
    { std::lock_guard<std::mutex> lock(g_soundsMutex); g_sounds.clear(); }
    { std::lock_guard<std::mutex> lock(g_cachedESPBoxesMutex); g_cachedESPBoxes.clear(); }
    Sleep(32);
    UninitializeBuffer();
    FreeLibraryAndExitThread(g_hInstance, 0);
    return 0;
}

static bool InitBufferSafe() {
    __try {
        InitializeBuffer();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static DWORD WINAPI SelfDeleteThread(LPVOID param) {
    VMProtectBeginUltra("destruct");
    Sleep(3000);
    HINSTANCE hMod = (HINSTANCE)param;
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(hMod, path, MAX_PATH)) {
        wchar_t delPath[MAX_PATH + 8] = {};
        wcscpy_s(delPath, path);
        wcscat_s(delPath, L".deleted");
        MoveFileExW(path, delPath, MOVEFILE_REPLACE_EXISTING);

        wchar_t cmd[1024];
        swprintf_s(cmd, L"cmd.exe /c timeout /t 3 >nul & del /f /q \"%s\"", delPath);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
    }
    VMProtectEnd();
    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD reason, LPVOID reserved) {
    VMProtectBeginUltra("main");
    if (reason == DLL_PROCESS_ATTACH) {
        
        g_hInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);


       CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
            License_PreloadCredentials();

            if (!VerifyLoaderHash()) {
                MessageBoxA(NULL, "Ti 4eto syka borshish", "$oyuz", MB_OK | MB_ICONERROR);
                TerminateProcess(GetCurrentProcess(), 0);
                return 0;
            }

            bool initOk = true;

            if (initOk) {
                InitializeBuffer();


                HMODULE opengl = nullptr;
                for (int i = 0; i < 50 && !opengl; i++) { opengl = GetModuleHandleA("opengl32.dll"); if (!opengl) Sleep(100); }

                if (opengl) {
                    g_openglLoaded = true;

                    if (ResolveAllGLAddresses()) {
                        install_hwbp_hook(original_glClear_addr, (void*)hooked_glClear, &original_glClear_addr, &hwbp_clear);
                        install_hwbp_hook(original_glClearColor_addr, (void*)hooked_glClearColor, &original_glClearColor_addr, &hwbp_clearcolor);
                        install_hwbp_hook(original_glFogfv_addr, (void*)hooked_glFogfv, &original_glFogfv_addr, &hwbp_fogfv);
                        // Trampoline hooks — installed via aethelis-ported TrampolineHook_Install
                        // (sub_18003F2F0 equivalent: LDE copy + FF25-patch + trampoline JMP-back)
                        TrampolineHook_Install(original_glScalef_addr,     (void*)hooked_glScalef,     &original_glScalef_addr,     &tramp_scalef);
                        TrampolineHook_Install(original_glOrtho_addr,      (void*)hooked_glOrtho,      &original_glOrtho_addr,      &tramp_ortho);
                        TrampolineHook_Install(original_glTranslatef_addr, (void*)hooked_glTranslatef, &original_glTranslatef_addr, &tramp_translatef);
                        TrampolineHook_Install(original_glRotatef_addr,    (void*)hooked_glRotatef,    &original_glRotatef_addr,    &tramp_rotatef);
                        TrampolineHook_Install(original_wglSwapBuffers_addr,(void*)hooked_wglSwapBuffers,&original_wglSwapBuffers_addr,&tramp_swapbuffers);
                        TrampolineHook_Install(original_glLoadIdentity_addr,(void*)hooked_glLoadIdentity,&original_glLoadIdentity_addr,&tramp_loadidentity);
                        TrampolineHook_Install(original_glPushMatrix_addr,  (void*)hooked_glPushMatrix, &original_glPushMatrix_addr, &tramp_pushmatrix);
                        TrampolineHook_Install(original_glPopMatrix_addr,   (void*)hooked_glPopMatrix,  &original_glPopMatrix_addr,  &tramp_popmatrix);
                        TrampolineHook_Install(original_glEnable_addr,      (void*)hooked_glEnable,     &original_glEnable_addr,     &tramp_enable);
                        TrampolineHook_Install(original_glDisable_addr,     (void*)hooked_glDisable,    &original_glDisable_addr,    &tramp_disable);
                        install_hwbp_hook(original_glFogf_addr, (void*)hooked_glFogf, &original_glFogf_addr, &hwbp_fogf);
                        TrampolineHook_Install(original_glTranslated_addr,  (void*)hooked_glTranslated, &original_glTranslated_addr, &tramp_translated);
                        TrampolineHook_Install(original_glFrustum_addr,     (void*)hooked_glFrustum,    &original_glFrustum_addr,    &tramp_frustum);
                        TrampolineHook_Install(original_glLoadMatrixf_addr, (void*)hooked_glLoadMatrixf,&original_glLoadMatrixf_addr,&tramp_loadmatrixf);
                        TrampolineHook_Install(original_glLoadMatrixd_addr, (void*)hooked_glLoadMatrixd,&original_glLoadMatrixd_addr,&tramp_loadmatrixd);
                        TrampolineHook_Install(original_glMultMatrixf_addr, (void*)hooked_glMultMatrixf,&original_glMultMatrixf_addr,&tramp_multmatrixf);
                        TrampolineHook_Install(original_glMultMatrixd_addr, (void*)hooked_glMultMatrixd,&original_glMultMatrixd_addr,&tramp_multmatrixd);
                        TrampolineHook_Install(original_glMatrixMode_addr,  (void*)hooked_glMatrixMode, &original_glMatrixMode_addr, &tramp_matrixmode);
                   
                        TrampolineHook_Install(original_glColor4f_addr,     (void*)hooked_glColor4f,    &original_glColor4f_addr,    &tramp_color4f);
                        TrampolineHook_Install(original_glColor3f_addr,     (void*)hooked_glColor3f,    &original_glColor3f_addr,    &tramp_color3f);
                        g_glHooksInstalled = true;
                        g_openglFuncsLoaded = true;
                    }
                }
                

                TimerModule::InitSpeedhack();
                LoadConfig();
                InitNoFallHook();

                HMODULE winmm = GetModuleHandleA("winmm.dll");
                if (winmm) g_winmmLoaded = true;

                InitOpenAL();
                InitOpenALSoundHook();
                if (alGetListener3f) {
                    g_openalLoaded = true;
                    original_alSourcePlay_addr = (void*)p_alSourcePlay;
                    if (original_alSourcePlay_addr) {
                        g_openalFuncsLoaded = true;
                        TrampolineHook_Install(original_alSourcePlay_addr, (void*)hooked_alSourcePlay, &original_alSourcePlay_addr, &tramp_alSourcePlay);
                    }
                }

                if (winmm) {
                    void* pTimeGetTime = GetProcAddress(winmm, "timeGetTime");
                    if (pTimeGetTime) {
                        g_timeGetTimeHooked = true;
                        TrampolineHook_Install(pTimeGetTime, TimerModule::h_timeGetTime, (void**)&TimerModule::o_timeGetTime, &tramp_timegettime);
                    }
                }
            }

            if (initOk) {
                CreateThread(NULL, 0, SelfDeleteThread, g_hInstance, 0, NULL);
            }

            return 0;
        }, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        ShutdownImGui();
        ShutdownNoFallHook();
        // HWBP hooks — untouched
        remove_hwbp_hook(&hwbp_clearcolor);
        remove_hwbp_hook(&hwbp_clear);
        remove_hwbp_hook(&hwbp_fogfv);
        remove_hwbp_hook(&hwbp_fogf);
        PVOID veh = GetVEHHandle();
        if (veh) { RemoveVectoredExceptionHandler(veh); SetVEHHandle(nullptr); }
        // Trampoline hooks — removed via aethelis-ported TrampolineHook_Remove
        TrampolineHook_Remove(&tramp_color4f);
        TrampolineHook_Remove(&tramp_color3f);
        TrampolineHook_Remove(&tramp_ortho);
        TrampolineHook_Remove(&tramp_scalef);
        TrampolineHook_Remove(&tramp_translatef);
        TrampolineHook_Remove(&tramp_rotatef);
        TrampolineHook_Remove(&tramp_swapbuffers);
        TrampolineHook_Remove(&tramp_loadidentity);
        TrampolineHook_Remove(&tramp_pushmatrix);
        TrampolineHook_Remove(&tramp_popmatrix);
        TrampolineHook_Remove(&tramp_timegettime);
        TrampolineHook_Remove(&tramp_enable);
        TrampolineHook_Remove(&tramp_disable);
        TrampolineHook_Remove(&tramp_translated);
        TrampolineHook_Remove(&tramp_frustum);
        TrampolineHook_Remove(&tramp_loadmatrixf);
        TrampolineHook_Remove(&tramp_loadmatrixd);
        TrampolineHook_Remove(&tramp_multmatrixf);
        TrampolineHook_Remove(&tramp_multmatrixd);
        TrampolineHook_Remove(&tramp_matrixmode);
        TrampolineHook_Remove(&tramp_getfloatv);
        TrampolineHook_Remove(&tramp_wglgetprocaddress);
        UninitializeBuffer();
    }
    VMProtectEnd();
    return TRUE;
}
