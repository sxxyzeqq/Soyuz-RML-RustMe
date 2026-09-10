#define _CRT_SECURE_NO_WARNINGS
#include "GUI.h"
#include "../core/Config.h"
#include "../modules/Aimbot.h"
#include "../modules/ESP.h"
#include "../modules/AspectRatio.h"
#include "../modules/ChamsHand.h"
#include "../modules/PlayerChams.h"
#include "../modules/WallCheck.h"
#include "../hooks/Hooks.h"
#include "../hooks/OpenGLHooks.h"
#include "../hooks/OpenAL.h"
#include "../core/license.h"
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <string>
#include "imgui/backends/imgui_impl_opengl2.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "../fonts/PoppinsBoldFont.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern ToggleBind g_aimBind, g_espBind, g_sprintBind, g_timerBind, g_fogBind, g_wallhackBind, g_zoomBind, g_fullbrightBind, g_chinaHatBind, g_tracerNewBind;
extern float g_aimSens;
extern bool g_aimSensEnabled;

bool g_guiInitialized = false;
bool g_guiDrawState = false;

namespace GUI {
    bool GetInitializationState() { return g_guiInitialized; }
    bool GetDrawState() { return g_guiDrawState; }
    void SetDrawState(bool new_value) { g_guiDrawState = new_value; }
}

static const ImVec4 COLOR_TEXT_NORMAL = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);

LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_unloading) return CallWindowProcA(g_originalWndProc, hWnd, msg, wParam, lParam);
    if (g_menuVisible) {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        if (msg == WM_SETCURSOR) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
        if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
            msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_MBUTTONDOWN ||
            msg == WM_MBUTTONUP || msg == WM_MOUSEWHEEL) return TRUE;
    }
    return CallWindowProcA(g_originalWndProc, hWnd, msg, wParam, lParam);
}

void InitImGui(HDC hdc) {
    g_hwnd = WindowFromDC(hdc);
    if (!g_hwnd) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    
    // Загружаем встроенный шрифт из памяти
    ImFontConfig cfg;
    cfg.OversampleH = 3; cfg.OversampleV = 2; cfg.PixelSnapH = false;
    cfg.FontDataOwnedByAtlas = false; // Важно! Данные шрифта статические, не удалять
    
    g_fontSmall = io.Fonts->AddFontFromMemoryTTF((void*)g_PoppinsBoldFont, sizeof(g_PoppinsBoldFont), 14.0f, &cfg);
    g_fontMain = io.Fonts->AddFontFromMemoryTTF((void*)g_PoppinsBoldFont, sizeof(g_PoppinsBoldFont), 16.0f, &cfg);
    g_fontMedium = io.Fonts->AddFontFromMemoryTTF((void*)g_PoppinsBoldFont, sizeof(g_PoppinsBoldFont), 18.0f, &cfg);
    
    // Если не загрузился - используем дефолтный
    if (!g_fontMain) { 
        g_fontMain = io.Fonts->AddFontDefault(); 
        g_fontSmall = g_fontMain; 
        g_fontMedium = g_fontMain; 
    }
    
    // Устанавливаем дефолтный шрифт для всего ImGui
    if (g_fontMain) io.FontDefault = g_fontMain;
    
    // Используем стандартный стиль ImGui (Dark)
    ImGui::StyleColorsDark();
    
    ImGui_ImplWin32_InitForOpenGL(g_hwnd);
    ImGui_ImplOpenGL2_Init();
    g_originalWndProc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
    g_imguiInitialized = true;
}

void ShutdownImGui() {
    if (!g_imguiInitialized) return;
    if (g_hwnd && g_originalWndProc)
        SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_imguiInitialized = false;
}

void RenderWatermark(ImDrawList* drawList) {
    if (!g_showWatermark) return;
    
    // Обновляем FPS раз в секунду
    static float lastFps = 0.0f;
    static double lastFpsUpdateTime = 0.0;
    double currentTime = ImGui::GetTime();
    
    if (currentTime - lastFpsUpdateTime >= 1.0) {
        lastFps = ImGui::GetIO().Framerate;
        lastFpsUpdateTime = currentTime;
    }
    
    // Получаем время
    char timeStr[16];
    { time_t t = time(nullptr); struct tm lt {}; localtime_s(&lt, &t); snprintf(timeStr, sizeof(timeStr), "%02d:%02d", lt.tm_hour, lt.tm_min); }
    
    float fontSize = 16.0f;
    float padX = 12.0f, padY = 4.0f;
    
    // Рисуем компоненты отдельно для точного расчета ширины
    const char* titleText = "$oyuz";
    std::string userLogin = License_GetLogin();
    std::string userUid = License_GetUid();
    std::string uidText = userUid.empty() || userUid == "0" ? "" : "[" + userUid + "]";
    char fpsText[32];
    snprintf(fpsText, sizeof(fpsText), "%.0ffps", lastFps);
    
    ImVec2 titleSize = ImGui::CalcTextSize(titleText);
    ImVec2 loginSize = ImGui::CalcTextSize(userLogin.c_str());
    ImVec2 uidSize = ImGui::CalcTextSize(uidText.c_str());
    ImVec2 fpsSize = ImGui::CalcTextSize(fpsText);
    ImVec2 timeSize = ImGui::CalcTextSize(timeStr);
    bool hasUser = !userLogin.empty();
    
    // Вычисляем общую ширину: текст + точки + отступы
    float dotSpacing = 14.0f;
    float userWidth = hasUser ? loginSize.x + uidSize.x + dotSpacing : 0.0f;
    float totalTextWidth = titleSize.x + dotSpacing + userWidth + fpsSize.x + dotSpacing + timeSize.x;
    float totalW = totalTextWidth + padX * 2.0f;
    float totalH = titleSize.y + padY * 2.0f;
    
    float wx = 10.0f, wy = 10.0f;
    
    // Рисуем фон с закругленными углами
    drawList->AddRectFilled(ImVec2(wx, wy), ImVec2(wx + totalW, wy + totalH), IM_COL32(20, 20, 20, 220), 4.0f);
    
    // Рисуем текст (опущен на 0.5 пикселя)
    float tx = wx + padX, ty = wy + padY + 0.5f;
    
    // Рисуем "$oyuz" цветом из Theme Engine
    ImU32 accentColor = IM_COL32(
        (int)(g_currentAccentColor.x * 255),
        (int)(g_currentAccentColor.y * 255),
        (int)(g_currentAccentColor.z * 255),
        255
    );
    ImU32 loginColor = IM_COL32(245, 245, 245, 255);
    ImU32 dimColor = IM_COL32(160, 160, 160, 255);
    ImU32 infoColor = IM_COL32(200, 200, 200, 255);

    drawList->AddText(ImVec2(tx, ty), accentColor, titleText);
    tx += titleSize.x;

    float dotRadius = 1.8f;
    float dotY = ty + titleSize.y * 0.5f;
    auto drawDot = [&]() {
        tx += dotSpacing * 0.5f;
        drawList->AddCircleFilled(ImVec2(tx, dotY), dotRadius, dimColor);
        tx += dotSpacing * 0.5f;
    };

    drawDot();

    if (hasUser) {
        drawList->AddText(ImVec2(tx, ty), loginColor, userLogin.c_str());
        tx += loginSize.x;

        if (!uidText.empty()) {
            drawList->AddText(ImVec2(tx, ty), dimColor, uidText.c_str());
            tx += uidSize.x;
        }

        drawDot();
    }

    drawList->AddText(ImVec2(tx, ty), infoColor, fpsText);
    tx += fpsSize.x;

    drawDot();

    drawList->AddText(ImVec2(tx, ty), infoColor, timeStr);

}



void RenderModuleList(ImDrawList* drawList) {
    if (!g_showArrayList) return;
    
    struct ModInfo { std::string name; bool active; };
    std::vector<ModInfo> modules = {
        { "AutoSprint", g_autoSprintEnabled },
        { "Ambince", g_ambince },
        { "ESP", g_espEnabled },
        { "AimBot", g_aimEnabled },
        { "SoundESP", g_soundEspEnabled },
        { "NightMode", g_nightModeEnabled },
        { "Zoom", g_zoomEnabled },
        { "NoFall", g_noFallEnabled },
        { "Timer", TimerModule::g_timerEnabled },
        { "FovChanger", g_fovChangerEnabled },
        { "NoHurtCam", g_noHurtCamEnabled },
        { "ViewModel", g_viewModelEnabled },
        { "FogColor", g_fogEnabled },
        { "CustomCrosshair", g_crosshairEnabled },
        { "Fullbright", g_fullbrightEnabled },
        { "AmmoESP", g_ammoEspEnabled },
        { "Tracers", g_tracerEnabled },
        { "TracerNew", g_tracerNewEnabled },
        { "ChinaHat", g_chinaHatEnabled },
        { "WallHack", g_wallHackEnabled },
        { "NightMode", g_nightModeEnabled },
        { "ShowCoords", g_openalEspEnabled },
        { "AspectRatio", g_aspectRatioEnabled },
        { "ChamsHand", g_chamsHandEnabled },
        { "PlayerChams", g_playerChamsEnabled }
    };
    
    std::vector<std::string> activeNames;
    for (const auto& mod : modules) if (mod.active) activeNames.push_back(mod.name);
    
    if (activeNames.empty()) return;
    
    // Сортируем по длине текста (от длинного к короткому)
    std::sort(activeNames.begin(), activeNames.end(), [](const std::string& a, const std::string& b) {
        ImVec2 sizeA = ImGui::CalcTextSize(a.c_str());
        ImVec2 sizeB = ImGui::CalcTextSize(b.c_str());
        if (std::abs(sizeA.x - sizeB.x) < 0.5f) return a < b;
        return sizeA.x > sizeB.x;
    });
    
    float fontSize = 15.0f;
    float itemH = fontSize + 8.0f; // Уменьшил отступы внутри элемента
    float padX = 10.0f; // Уменьшил горизонтальные отступы
    float padY = 4.0f;
    
    // Вычисляем высоту Watermark
    float watermarkH = fontSize + padY * 2.0f;
    
    // Начинаем ArrayList ближе к Watermark (уменьшил отступ)
    float startY = 10.0f + watermarkH + 4.0f;
    float lx = 10.0f;
    
    for (const auto& name : activeNames) {
        ImVec2 ts = ImGui::CalcTextSize(name.c_str());
        float w = ts.x + padX * 2.0f;
        float x0 = lx, x1 = lx + w, y0 = startY, y1 = y0 + itemH;
        
        // Рисуем фон модуля
        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(20, 20, 20, 220), 4.0f);
        
        // Рисуем текст (поднят на 0.5 пикселя выше)
        float tx = x0 + padX, ty = y0 + (itemH - ts.y) * 0.5f - 0.5f;
        drawList->AddText(ImVec2(tx, ty), IM_COL32(220, 220, 220, 255), name.c_str());
        
        startY += itemH + 2.0f; // Уменьшил отступ между элементами
    }
}

void RenderKeyBinds(ImDrawList* drawList) {
    if (!g_showKeyBinds) return;
    
    // Собираем активные бинды
    struct BindInfo {
        std::string name;
        std::string key;
        bool active;
    };
    
    std::vector<BindInfo> binds;
    
    if (g_aimEnabled && g_aimBind.key != 0) {
        binds.push_back({"Aim", GetKeyName(g_aimBind.key), true});
    }
    if (g_espEnabled && g_espBind.key != 0) {
        binds.push_back({"ESP", GetKeyName(g_espBind.key), true});
    }
    if (g_autoSprintEnabled && g_sprintBind.key != 0) {
        binds.push_back({"Sprint", GetKeyName(g_sprintBind.key), true});
    }
    if (g_noFallEnabled && g_noFallBind.key != 0) {
        binds.push_back({"NoFall", GetKeyName(g_noFallBind.key), true});
    }
    if (TimerModule::g_timerEnabled && g_timerBind.key != 0) {
        binds.push_back({"Timer", GetKeyName(g_timerBind.key), true});
    }
    if (g_fogEnabled && g_fogBind.key != 0) {
        binds.push_back({"Fog", GetKeyName(g_fogBind.key), true});
    }
    if (g_wallHackEnabled && g_wallhackBind.key != 0) {
        binds.push_back({"WallHack", GetKeyName(g_wallhackBind.key), true});
    }
    if (g_zoomEnabled && g_zoomBind.key != 0) {
        binds.push_back({"Zoom", GetKeyName(g_zoomBind.key), true});
    }
    if (g_fullbrightEnabled && g_fullbrightBind.key != 0) {
        binds.push_back({"Fullbright", GetKeyName(g_fullbrightBind.key), true});
    }
    if (g_tracerNewEnabled && g_tracerNewBind.key != 0) {
        binds.push_back({"TracerNew", GetKeyName(g_tracerNewBind.key), true});
    }
    
    if (binds.empty()) {
        binds.push_back({"No active binds", "", false});
    }
    // Сортируем по алфавиту
    std::sort(binds.begin(), binds.end(), [](const BindInfo& a, const BindInfo& b) {
        return a.name < b.name;
    });
    
    // Вычисляем динамическую ширину на основе самого длинного элемента
    float maxWidth = 0.0f;
    for (const auto& bind : binds) {
        ImVec2 nameSize = ImGui::CalcTextSize(bind.name.c_str());
        ImVec2 keySize = ImGui::CalcTextSize(bind.key.c_str());
        float lineWidth = nameSize.x + 40.0f + keySize.x; // 40px между названием и клавишей
        if (lineWidth > maxWidth) maxWidth = lineWidth;
    }
    
    // Параметры окна
    const char* title = "KeyBinds";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float minWidth = titleSize.x + 60.0f; // Минимальная ширина для заголовка + иконка
    float windowW = (std::max)(maxWidth + 30.0f, minWidth); // 30px = padX * 2
    float headerH = 25.0f;
    float itemH = 35.0f;
    float padX = 15.0f;
    ImU32 accentColor = IM_COL32(
        (int)(g_currentAccentColor.x * 255),
        (int)(g_currentAccentColor.y * 255),
        (int)(g_currentAccentColor.z * 255),
        255
    );
    
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    
    // Инициализируем позицию по центру экрана при первом запуске
    if (!g_keyBindsInitialized) {
        g_keyBindsX = screenSize.x / 2.0f - windowW / 2.0f;
        g_keyBindsY = screenSize.y - 200.0f;
        g_keyBindsInitialized = true;
    }
    
    float totalH = headerH + binds.size() * itemH;
    
    // Проверяем перетаскивание
    static bool isDragging = false;
    static ImVec2 dragOffset;
    
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool mouseDown = ImGui::IsMouseDown(0);
    
    // Область заголовка для перетаскивания
    ImVec2 headerMin(g_keyBindsX, g_keyBindsY);
    ImVec2 headerMax(g_keyBindsX + windowW, g_keyBindsY + headerH);
    
    // Перетаскивание работает всегда (и когда меню открыто, и когда закрыто)
    if (mouseDown) {
        if (!isDragging) {
            // Начинаем перетаскивание если кликнули на заголовок
            if (mousePos.x >= headerMin.x && mousePos.x <= headerMax.x &&
                mousePos.y >= headerMin.y && mousePos.y <= headerMax.y) {
                isDragging = true;
                dragOffset = ImVec2(mousePos.x - g_keyBindsX, mousePos.y - g_keyBindsY);
            }
        }
        
        if (isDragging) {
            g_keyBindsX = mousePos.x - dragOffset.x;
            g_keyBindsY = mousePos.y - dragOffset.y;
            
            // Ограничиваем позицию экраном
            g_keyBindsX = (std::max)(0.0f, (std::min)(g_keyBindsX, screenSize.x - windowW));
            g_keyBindsY = (std::max)(0.0f, (std::min)(g_keyBindsY, screenSize.y - totalH));
        }
    } else {
        isDragging = false;
    }
    
    float wx = g_keyBindsX;
    float wy = g_keyBindsY;
    
    // Рисуем фон окна
    drawList->AddRectFilled(ImVec2(wx, wy), ImVec2(wx + windowW, wy + totalH), IM_COL32(20, 20, 20, 220), 4.0f);
    
    // Рисуем заголовок "KeyBinds"
    float titleX = wx + padX;
    float titleY = wy + (headerH - titleSize.y) * 0.5f;
    drawList->AddText(ImVec2(titleX, titleY + 1.0f), accentColor, title);
    
    
    
    // Рисуем разделитель после заголовка
    drawList->AddLine(
        ImVec2(wx + padX, wy + headerH),
        ImVec2(wx + windowW - padX, wy + headerH),
        IM_COL32(50, 50, 50, 180),
        1.0f
    );
    
    // Рисуем биндыы
    float currentY = wy + headerH;
    for (const auto& bind : binds) {
        float textY = currentY + (itemH - ImGui::CalcTextSize(bind.name.c_str()).y) * 0.5f;
        
        // Название модуля слева
        drawList->AddText(ImVec2(wx + padX, textY), IM_COL32(220, 220, 220, 255), bind.name.c_str());
        
        // Клавиша справа
        ImVec2 keySize = ImGui::CalcTextSize(bind.key.c_str());
        float keyX = wx + windowW - padX - keySize.x;
        drawList->AddText(ImVec2(keyX, textY), IM_COL32(180, 180, 180, 255), bind.key.c_str());
        
        currentY += itemH;
    }
}

/*
void RenderCrosshair(ImDrawList* drawList) {
    if (!g_crosshairEnabled) return;
    
    ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImU32 col = ImColor(g_crosshairColor[0], g_crosshairColor[1], g_crosshairColor[2], g_crosshairColor[3]);
    float time = (float)ImGui::GetTime();
    
    // Pulsing effect
    float pulse = sinf(time * 3.0f) * 0.2f + 1.0f;
    float size = g_crosshairSize * pulse;
    
    if (g_crosshairType == 0) {
        drawList->AddCircleFilled(center, g_crosshairThickness + 1.0f * pulse, col);
    }
    else if (g_crosshairType == 1) {
        float s = size * 0.5f, t = g_crosshairThickness;
        float angle = time * 1.5f; // Rotation speed
        
        // Animated Cross (rotating)
        for (int i = 0; i < 4; i++) {
            float a = angle + i * (3.14159f * 0.5f);
            ImVec2 p1 = ImVec2(center.x + cosf(a) * 2.0f, center.y + sinf(a) * 2.0f);
            ImVec2 p2 = ImVec2(center.x + cosf(a) * s, center.y + sinf(a) * s);
            drawList->AddLine(p1, p2, col, t);
        }
    }
}
*/

void RenderAimFOV() {
    float thickness = std::max(0.1f, g_aimFovSize);
    if (!g_aimEnabled || !g_hwnd) return;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    RECT clientRect{};
    GetClientRect(g_hwnd, &clientRect);
    ImVec2 center((clientRect.right - clientRect.left) * 0.5f, (clientRect.bottom - clientRect.top) * 0.5f);
    drawList->AddCircle(center, g_aimCurrentFov, IM_COL32((int)(g_fovColor[0] * 255), (int)(g_fovColor[1] * 255), (int)(g_fovColor[2] * 255), (int)(g_fovColor[3] * 255)), 96, thickness);
}

#include "imgui/imgui_internal.h"

bool CustomCheckbox(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    const float square_sz = 18.0f; // Размер квадратика как на первом фото
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? 10.0f + label_size.x : 0.0f), (std::max)(square_sz, label_size.y)));
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }

    const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
    
    ImU32 accentColor = IM_COL32((int)(g_currentAccentColor.x * 255), (int)(g_currentAccentColor.y * 255), (int)(g_currentAccentColor.z * 255), 255);
    
    // Рисуем квадратик
    window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, *v ? accentColor : IM_COL32(40, 40, 45, 255), 4.0f);
    
    // Рисуем текст
    if (label_size.x > 0.0f) {
        ImGui::RenderText(ImVec2(check_bb.Max.x + 10.0f, check_bb.Min.y + (square_sz - label_size.y) * 0.5f), label);
    }
    return pressed;
}

bool CustomSliderFloat(const char* label, float* v, float v_min, float v_max) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    // Слайдер занимает фиксированную ширину, как на первом фото
    const float slider_width = 180.0f; 
    const float slider_height = 14.0f; // Увеличил высоту для текста
    const float item_height = 20.0f;
    
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect frame_bb(pos, pos + ImVec2(slider_width, item_height));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? 10.0f + label_size.x : 0.0f, 0.0f));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb)) return false;

    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = hovered && ImGui::IsMouseClicked(0);
    if (clicked || g.ActiveId == id) {
        g.ActiveId = id;
        ImGui::SetFocusID(id, window);
        ImGui::FocusWindow(window);
    }
    
    if (g.ActiveId == id) {
        float mouse_x = ImGui::GetIO().MousePos.x;
        float ratio = ImClamp((mouse_x - frame_bb.Min.x) / (frame_bb.Max.x - frame_bb.Min.x), 0.0f, 1.0f);
        *v = v_min + ratio * (v_max - v_min);
        if (!ImGui::IsMouseDown(0)) ImGui::ClearActiveID();
    }
    
    float fraction = ImClamp((*v - v_min) / (v_max - v_min), 0.0f, 1.0f);
    
    ImU32 accentColor = IM_COL32((int)(g_currentAccentColor.x * 255), (int)(g_currentAccentColor.y * 255), (int)(g_currentAccentColor.z * 255), 255);
    
    // Рисуем фон слайдера (сильно скругленный)
    ImVec2 slider_start = ImVec2(frame_bb.Min.x, frame_bb.Min.y + (item_height - slider_height) * 0.5f);
    ImVec2 slider_end = ImVec2(frame_bb.Max.x, slider_start.y + slider_height);
    
    float rounding = slider_height * 0.5f; // Полностью скругленные края
    window->DrawList->AddRectFilled(slider_start, slider_end, IM_COL32(30, 30, 35, 255), rounding);
    
    // Рисуем заполненную часть
    ImVec2 fill_end = ImVec2(slider_start.x + (slider_end.x - slider_start.x) * fraction, slider_end.y);
    if (fraction > 0.0f) {
        window->DrawList->AddRectFilled(slider_start, fill_end, accentColor, rounding);
    }

    // Текст значения по центру слайдера
    char value_buf[64];
    snprintf(value_buf, sizeof(value_buf), "%.1f", *v);
    
    // Используем самый маленький/основной шрифт
    ImVec2 value_size = ImGui::CalcTextSize(value_buf);
    ImVec2 value_pos = ImVec2(
        slider_start.x + (slider_width - value_size.x) * 0.5f,
        slider_start.y + (slider_height - value_size.y) * 0.5f
    );
    
    // Тень для значения
    window->DrawList->AddText(ImVec2(value_pos.x + 1, value_pos.y + 1), IM_COL32(0, 0, 0, 180), value_buf);
    // Основной цвет текста (белый)
    window->DrawList->AddText(value_pos, IM_COL32(255, 255, 255, 255), value_buf);
    
    // Рисуем текст (название) справа
    if (label_size.x > 0.0f) {
        ImGui::RenderText(ImVec2(frame_bb.Max.x + 10.0f, frame_bb.Min.y + (item_height - label_size.y) * 0.5f), label);
    }
    
    return clicked;
}

void RenderGUI() {
    auto* drawList = ImGui::GetForegroundDrawList();
    RenderWatermark(drawList);
    RenderModuleList(drawList);
    RenderKeyBinds(drawList);
    // RenderCrosshair(drawList);

    if (g_menuVisible) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 20, 20, 220));
        ImGui::PushStyleColor(ImGuiCol_ResizeGrip, IM_COL32(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(35, 35, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(45, 45, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(55, 55, 60, 255));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(25, 25, 30, 255));
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(45, 45, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(55, 55, 60, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(65, 65, 70, 255));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(35, 35, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(45, 45, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(55, 55, 60, 255));
        
        ImGui::SetNextWindowSize(ImVec2(870, 560), ImGuiCond_Always);
        
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | 
                                       ImGuiWindowFlags_NoResize | 
                                       ImGuiWindowFlags_NoScrollbar | 
                                       ImGuiWindowFlags_NoScrollWithMouse | 
                                       ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("##MainWindow", &g_menuVisible, windowFlags);
        
        ImU32 accentColor = IM_COL32(
            (int)(g_currentAccentColor.x * 255),
            (int)(g_currentAccentColor.y * 255),
            (int)(g_currentAccentColor.z * 255),
            255
        );
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        
        // === HEADER ===
        float headerH = 48.0f;
        dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + headerH),
            IM_COL32(20, 20, 20, 255), 10.0f, ImDrawFlags_RoundCornersTop);
        dl->AddRectFilled(ImVec2(wp.x, wp.y + headerH - 10.0f),
            ImVec2(wp.x + ws.x, wp.y + headerH), IM_COL32(20, 20, 20, 255));
        dl->AddLine(ImVec2(wp.x, wp.y + headerH), ImVec2(wp.x + ws.x, wp.y + headerH),
            IM_COL32(50, 50, 50, 255), 1.0f);
        
        // Название — рисуем с явным размером 28px
        float titleFontSize = 22.0f;
        ImFont* titleFont = g_fontMedium ? g_fontMedium : ImGui::GetDefaultFont();
        dl->AddText(titleFont, titleFontSize, ImVec2(wp.x + 20, wp.y + (headerH - titleFontSize) * 0.5f), accentColor, "$oyuz");
        
        // Время — выровнено по центру хидера
        char timeStr[16];
        time_t t = time(nullptr);
        struct tm lt;
        localtime_s(&lt, &t);
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", lt.tm_hour, lt.tm_min);
        ImVec2 timeSize = ImGui::CalcTextSize(timeStr);
        dl->AddText(ImVec2(wp.x + ws.x - timeSize.x - 20, wp.y + (headerH - timeSize.y) * 0.5f),
            IM_COL32(180, 180, 190, 255), timeStr);

        // === TABS ===
        static int currentTab = 0;
        float tabY = headerH + 12.0f;
        ImGui::SetCursorPos(ImVec2(20, tabY));
        
        ImGui::BeginGroup();
        auto TabButton = [&](const char* lbl, int idx) {
            bool sel = (currentTab == idx);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            if (sel) {
                ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentColor);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(20, 20, 20, 220));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(58, 58, 65, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(44, 44, 50, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(140, 140, 150, 255));
            }
            if (ImGui::Button(lbl, ImVec2(105, 32))) currentTab = idx;
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        };
        TabButton("Combat",  0); ImGui::SameLine(0, 8);
        TabButton("Move",    1); ImGui::SameLine(0, 8);
        TabButton("Misc",    2); ImGui::SameLine(0, 8);
        TabButton("Theme",   3); ImGui::SameLine(0, 8);
        TabButton("Config",  4);
        ImGui::EndGroup();

        static int lastTab = -1;
        if (currentTab == 4 && lastTab != 4) {
            RefreshProfiles();
        }
        lastTab = currentTab;

        // === CONTENT AREA ===
        float contentY = tabY + 32.0f + 12.0f;
        float pad = 20.0f;
        float colGap = 12.0f;
        float colW = (ws.x - pad * 2 - colGap) / 2.0f;
        float contentH = ws.y - contentY - pad;

        ImGui::SetCursorPos(ImVec2(pad, contentY));
        // Общий контейнер со скроллом (scrollbar hidden but scroll works)
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
        ImGui::BeginChild("##content", ImVec2(ws.x - pad * 2, contentH), false, 0);
        
        // Колонка 1 (Группа)
        ImGui::BeginGroup();

        static std::string s_curCard;
        static ImVec2 s_curCardPos;

        auto BeginCard = [&](const char* title, ToggleBind* bind = nullptr) {
            s_curCard = title;
            s_curCardPos = ImGui::GetCursorScreenPos();
            
            ImGui::PushID(title);
            ImGui::GetWindowDrawList()->ChannelsSplit(2);
            ImGui::GetWindowDrawList()->ChannelsSetCurrent(1); // Передний план (контент)
            
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(0, 12));
            ImGui::Indent(14.0f);
            
            if (g_fontMain) ImGui::PushFont(g_fontMain);
            ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.95f, 1.0f), "%s", title);
            if (g_fontMain) ImGui::PopFont();
            
            if (bind) {
                std::string bindText;
                if (bind->waiting) bindText = "...";
                else if (bind->key != 0) bindText = GetKeyName(bind->key);
                else bindText = "None";
                
                ImVec2 textSize = ImGui::CalcTextSize(bindText.c_str());
                float btnWidth = textSize.x + 16.0f;
                float btnHeight = 22.0f;
                
                ImGui::SameLine(colW - btnWidth - 14);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
                
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(30, 30, 35, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(45, 45, 55, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(60, 60, 70, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, bind->waiting ? IM_COL32(255, 50, 50, 255) : IM_COL32(150, 150, 160, 255));
                
                if (ImGui::Button((bindText + "##bind_" + title).c_str(), ImVec2(btnWidth, btnHeight))) {
                    bind->waiting = true;
                }
                
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
                
                if (bind->waiting) {
                    for (int i = 1; i < 255; i++) {
                        if (i == VK_LBUTTON) continue;
                        if (GetAsyncKeyState(i) & 0x8000) {
                            if (i == VK_ESCAPE) {
                                bind->key = 0;
                            } else {
                                bind->key = i;
                            }
                            bind->waiting = false;
                            break;
                        }
                    }
                }
            }
            
            ImGui::Dummy(ImVec2(0, 4));
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            ImVec2 sp = ImGui::GetCursorScreenPos();
            cdl->AddLine(ImVec2(sp.x, sp.y), ImVec2(sp.x + colW - 28.0f, sp.y), IM_COL32(50, 50, 50, 255), 1.0f);
            ImGui::Dummy(ImVec2(0, 10));
            
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));
        };
        
        auto EndCard = [&]() {
            ImGui::PopStyleVar(); // ItemSpacing
            ImGui::Unindent(14.0f);
            ImGui::Dummy(ImVec2(0, 12));
            ImGui::EndGroup();
            
            ImVec2 min = s_curCardPos;
            ImVec2 max = ImVec2(min.x + colW, ImGui::GetItemRectMax().y);
            
            ImGui::GetWindowDrawList()->ChannelsSetCurrent(0); // Задний план (фон)
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, IM_COL32(20, 20, 20, 220), 10.0f);
            ImGui::GetWindowDrawList()->ChannelsMerge();
            
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0, 10));
        };

        if (currentTab == 0) {
            BeginCard("AimBot", &g_aimBind);
            CustomCheckbox("AimBot", &g_aimEnabled);
            CustomSliderFloat("Bullet Speed", &g_aimBulletSpeed, 10.0f, 500.0f);
            CustomSliderFloat("Ping",         &g_aimPing,        0.0f,  500.0f);
            CustomSliderFloat("FOV",          &g_aimFov,         10.0f, 500.0f);
            CustomSliderFloat("Smooth X",     &g_aimSmoothX,     0.01f, 1.0f);
            CustomCheckbox("Sens",           &g_aimSensEnabled);
            if (g_aimSensEnabled) CustomSliderFloat("Sens Value", &g_aimSens, 0.1f, 1.0f);
            CustomCheckbox("Bow Assist",    &g_aimBow);
            CustomCheckbox("Animated FOV",  &g_aimAnimatedFov);
            CustomSliderFloat("FOV Thickness",  &g_aimFovSize, 0.1f, 10.0f);
            CustomCheckbox("Visible Only",  &g_aimVisibleOnly);
            CustomCheckbox("MCF",           &g_aimMcfEnabled);
            if (g_aimMcfEnabled) {
                CustomSliderFloat("MCF Horizontal", &g_aimMcfMaxHorizontalDistance, 0.1f, 30.0f);
                CustomSliderFloat("MCF Vertical",   &g_aimMcfMaxVerticalDistance,   1.0f, 250.0f);
            }
            CustomCheckbox("Auto Disable",  &g_aimAutoDisable);
            EndCard();

        }
        else if (currentTab == 1) {
            BeginCard("Timer", &g_timerBind);
            CustomCheckbox("Timer", &TimerModule::g_timerEnabled);
            CustomSliderFloat("Speed", &TimerModule::g_SpeedMultiplier, 0.1f, 5.0f);
            CustomSliderFloat("Duration", &TimerModule::g_smartTimerDuration, 0.1f, 2.0f);
            CustomCheckbox("Smart", &TimerModule::g_smartTimerEnabled);
            EndCard();
        }
          else if (currentTab == 2) {
            BeginCard("ESP", &g_espBind);
            CustomCheckbox("Enabled",   &g_espEnabled);
            CustomCheckbox("Breadcrumbs",  &g_espBreadcrumbs);
            CustomSliderFloat("Duration",  &g_espBreadcrumbsTime, 1.0f, 30.0f);
            CustomCheckbox("Color By Visibility", &g_espColorByVisibility);
            if (g_espColorByVisibility) {
                ImGui::ColorEdit4("Visible##esp", g_espVisibleColor, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Hidden##esp",  g_espHiddenColor,  ImGuiColorEditFlags_NoInputs);
            }
            EndCard();

            BeginCard("RustMe ESP", &g_rustmeEspBind);
            CustomCheckbox("Enabled", &g_rustmeESP);
            EndCard();
            
            BeginCard("Sound ESP");
            CustomCheckbox("Enabled",        &g_soundEspEnabled);
            CustomCheckbox("Visual Circles", &g_soundEspVisual);
            CustomSliderFloat("Duration",    &g_soundEspDuration, 0.5f, 10.0f);
            EndCard();
            
            BeginCard("Ambience");
            CustomCheckbox("Enabled", &g_ambince);
            if (g_ambince) {
                CustomCheckbox("Sky",   &g_sky);
                CustomCheckbox("World", &g_world);
                CustomSliderFloat("Density", &g_density, -25.0f, 25.0f);
                ImGui::ColorEdit4("Color##amb", g_ambinceColor, ImGuiColorEditFlags_NoInputs);
            }
            EndCard();
            
            BeginCard("Ammo ESP", &g_ammoEspBind);
            CustomCheckbox("Enabled", &g_ammoEspEnabled);
            if (g_ammoEspEnabled) {
                CustomCheckbox("Comet Mode", &g_ammoCometMode);
                if (g_ammoCometMode) {
                    ImGui::ColorEdit4("Color##comet", g_ammoCometColor, ImGuiColorEditFlags_NoInputs);
                }
            }
            EndCard();

            BeginCard("Tracer", &g_tracerBind);
            CustomCheckbox("Enabled", &g_tracerEnabled);
            if (g_tracerEnabled) {
                CustomSliderFloat("Thickness", &g_tracerThickness, 0.5f, 5.0f);
                ImGui::ColorEdit4("Tracer Color", g_tracerColor, ImGuiColorEditFlags_NoInputs);
            }
            EndCard();


            BeginCard("ChinaHat", &g_chinaHatBind);
            CustomCheckbox("Enabled", &g_chinaHatEnabled);
            if (g_chinaHatEnabled) {
                const char* styles[] = { "China Hat", "Nimb" };
                ImGui::PushItemWidth(colW - 28);
                ImGui::Combo("Style", &g_chinaHatMode, styles, 2);
                ImGui::PopItemWidth();
                CustomSliderFloat("Height", &g_chinaHatHeight, 0.05f, 1.5f);
                CustomSliderFloat("Radius", &g_chinaHatRadius, 0.1f, 2.0f);
                CustomSliderFloat("Position Y", &g_chinaHatPosY, -3.0f, 0.5f);
                CustomSliderFloat("Line Width", &g_chinaHatLineWidth, 0.5f, 5.0f);
                if (g_chinaHatMode == 0)
                    CustomSliderFloat("Fill Alpha", &g_chinaHatFillAlpha, 0.0f, 1.0f);
                ImGui::ColorEdit4("Color##chinahat", g_chinaHatColor, ImGuiColorEditFlags_NoInputs);
            }
            EndCard();

            BeginCard("WallHack", &g_wallhackBind);
            CustomCheckbox("Enabled", &g_wallHackEnabled);
            EndCard();

            BeginCard("Night Mode", &g_nightModeBind);
            CustomCheckbox("Enabled", &g_nightModeEnabled);
            if (g_nightModeEnabled) {
                ImGui::ColorEdit3("Night Color", g_nightColor, ImGuiColorEditFlags_NoInputs);
            }
            EndCard();

            BeginCard("Show Coords", &g_coordsBind);
            CustomCheckbox("Enabled", &g_openalEspEnabled);
            EndCard();

            BeginCard("Fullbright", &g_fullbrightBind);
            CustomCheckbox("Enabled", &g_fullbrightEnabled);
            EndCard();

            BeginCard("Zoom", &g_zoomBind);
            CustomCheckbox("Enabled", &g_zoomEnabled);
            CustomCheckbox("Hold Mode",   &g_zoomHoldMode);
            CustomSliderFloat("Level", &g_zoomAmount, 1.0f, 10.0f);
            EndCard();
        } else if (currentTab == 4) {
            BeginCard("Config");
            // RefreshProfiles is now called once on tab switch at the top of RenderGUI
            std::vector<const char*> profiles;
            for (const auto& p : g_profileList) profiles.push_back(p.c_str());
            ImGui::PushItemWidth(180);
            ImGui::Combo("Profile", &g_currentProfile, profiles.data(), (int)profiles.size());
            ImGui::PopItemWidth();
            ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32((int)(g_currentAccentColor.x * 200), (int)(g_currentAccentColor.y * 200), (int)(g_currentAccentColor.z * 200), 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32((int)(g_currentAccentColor.x * 150), (int)(g_currentAccentColor.y * 150), (int)(g_currentAccentColor.z * 150), 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            if (ImGui::Button("Save Config", ImVec2(180, 0))) { UpdateConfigFromVars(); SaveConfig(); }
            if (ImGui::Button("Load Config", ImVec2(180, 0))) { LoadConfig(); }
            if (ImGui::Button("Create New Profile", ImVec2(180, 0))) {
                for (int i = 1; i <= 10; i++) {
                    std::string path = GetProfilePath(i);
                    std::ifstream f(path);
                    if (!f.good()) { g_currentProfile = i; UpdateConfigFromVars(); SaveConfig(); break; }
                }
            }
            if (ImGui::Button("UNLOAD", ImVec2(180, 0))) { UpdateConfigFromVars(); SaveConfig(); CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr); }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            EndCard();
        } else if (currentTab == 3) {
            // Theme tab - full width, no col2
            float fullW = colW * 2 + colGap;
            
            // Theme presets - gradient color buttons
            struct ThemePreset {
                const char* name;
                float c1[4];
                float c2[4];
            };
            
            static ThemePreset presets[] = {
                { "Red",      {1.0f, 0.1f, 0.1f, 1.0f}, {0.6f, 0.0f, 0.0f, 1.0f} },
                { "Green",    {0.1f, 1.0f, 0.3f, 1.0f}, {0.0f, 0.6f, 0.2f, 1.0f} },
                { "Purple",   {0.8f, 0.1f, 0.9f, 1.0f}, {0.5f, 0.0f, 0.6f, 1.0f} },
                { "Cyan",     {0.1f, 0.9f, 0.8f, 1.0f}, {0.0f, 0.5f, 0.5f, 1.0f} },
                { "Gold",     {1.0f, 0.85f, 0.0f, 1.0f}, {0.7f, 0.55f, 0.0f, 1.0f} },
                { "Orange",   {1.0f, 0.5f, 0.0f, 1.0f}, {0.7f, 0.3f, 0.0f, 1.0f} },
                { "Blue",     {0.1f, 0.2f, 1.0f, 1.0f}, {0.0f, 0.1f, 0.6f, 1.0f} },
                { "Emerald",  {0.2f, 0.9f, 0.4f, 1.0f}, {0.1f, 0.5f, 0.3f, 1.0f} },
                { "Pink",     {1.0f, 0.1f, 0.5f, 1.0f}, {0.7f, 0.0f, 0.3f, 1.0f} },
            };
            
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 16));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 20, 20, 220));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::BeginChild("##themePresets", ImVec2(fullW, 0), true, ImGuiWindowFlags_NoScrollbar);
            
            if (g_fontMedium) ImGui::PushFont(g_fontMedium);
            ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.95f, 1.0f), "Color Presets");
            if (g_fontMedium) ImGui::PopFont();
            
            ImDrawList* tdl = ImGui::GetWindowDrawList();
            ImVec2 sep = ImGui::GetCursorScreenPos();
            tdl->AddLine(ImVec2(sep.x, sep.y + 2), ImVec2(sep.x + ImGui::GetContentRegionAvail().x, sep.y + 2),
                IM_COL32(50, 50, 50, 255), 1.0f);
            ImGui::Dummy(ImVec2(0, 14));
            
            int cols = 3;
            float spacing = 12.0f;
            float btnW = (ImGui::GetContentRegionAvail().x - spacing * (cols - 1)) / cols;
            float btnH = 56.0f;
            
            for (int i = 0; i < 9; i++) {
                if (i > 0 && i % cols != 0) ImGui::SameLine(0, spacing);
                
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                ImVec2 btnMin = cursor;
                ImVec2 btnMax = ImVec2(cursor.x + btnW, cursor.y + btnH);
                float rounding = 10.0f;
                float inset = 3.0f;
                
                ImU32 colL = IM_COL32((int)(presets[i].c1[0]*255), (int)(presets[i].c1[1]*255), (int)(presets[i].c1[2]*255), 255);
                ImU32 colR = IM_COL32((int)(presets[i].c2[0]*255), (int)(presets[i].c2[1]*255), (int)(presets[i].c2[2]*255), 255);
                
                // Draw rounded background (mid color between c1 and c2)
                int mr = (int)((presets[i].c1[0] + presets[i].c2[0]) * 0.5f * 255);
                int mg = (int)((presets[i].c1[1] + presets[i].c2[1]) * 0.5f * 255);
                int mb = (int)((presets[i].c1[2] + presets[i].c2[2]) * 0.5f * 255);
                tdl->AddRectFilled(btnMin, btnMax, IM_COL32(mr, mg, mb, 255), rounding);
                
                // Draw gradient inside (inset so corners of gradient are hidden by rounded fill)
                ImVec2 gMin = ImVec2(btnMin.x + inset, btnMin.y + inset);
                ImVec2 gMax = ImVec2(btnMax.x - inset, btnMax.y - inset);
                tdl->AddRectFilledMultiColor(gMin, gMax, colL, colR, colR, colL);
                
                // Invisible button
                char btnId[32];
                snprintf(btnId, sizeof(btnId), "##preset_%d", i);
                ImGui::SetCursorScreenPos(cursor);
                if (ImGui::InvisibleButton(btnId, ImVec2(btnW, btnH))) {
                    memcpy(g_accentColor1, presets[i].c1, sizeof(float) * 4);
                    memcpy(g_accentColor2, presets[i].c2, sizeof(float) * 4);
                }
                
                if (ImGui::IsItemHovered()) {
                    tdl->AddRect(btnMin, btnMax, IM_COL32(255, 255, 255, 180), rounding, 0, 2.0f);
                }
                
                if (i % cols == cols - 1) ImGui::Dummy(ImVec2(0, spacing));
            }
            
            ImGui::Dummy(ImVec2(0, 20));
            
            // Separator
            ImVec2 sep2 = ImGui::GetCursorScreenPos();
            tdl->AddLine(ImVec2(sep2.x, sep2.y), ImVec2(sep2.x + ImGui::GetContentRegionAvail().x, sep2.y),
                IM_COL32(50, 50, 50, 255), 1.0f);
            ImGui::Dummy(ImVec2(0, 14));
            
            if (g_fontMedium) ImGui::PushFont(g_fontMedium);
            ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.95f, 1.0f), "Custom Colors");
            if (g_fontMedium) ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 10));
            ImGui::ColorEdit4("Accent 1", g_accentColor1, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::SameLine(0, 20);
            ImGui::ColorEdit4("Accent 2", g_accentColor2, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::Dummy(ImVec2(0, 8));
            CustomSliderFloat("Flow Speed", &g_accentSpeed, 0.1f, 10.0f);
            
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }

        ImGui::EndGroup(); // col1

        // Переходим ко второй колонке в том же ряду
        ImGui::SetCursorPosY(0.0f);
        ImGui::SetCursorPosX(colW + colGap);

        // Колонка 2 (Группа)
        ImGui::BeginGroup();

        if (currentTab == 0) {
            // Empty Col 2 for Combat
        } else if (currentTab == 1) {

            BeginCard("Movement", &g_sprintBind);
            CustomCheckbox("Auto Sprint", &g_autoSprintEnabled);
            EndCard();

            BeginCard("NoFall", &g_noFallBind);
            CustomCheckbox("Enabled", &g_noFallEnabled);
            EndCard();

        } else if (currentTab == 2) {
            BeginCard("ViewModel");
            CustomCheckbox("Enabled",    &g_viewModelEnabled);
            CustomCheckbox("Glass Hand", &g_glassHand);
            CustomSliderFloat("X",     &g_viewModelX,     -10.0f, 10.0f);
            CustomSliderFloat("Y",     &g_viewModelY,     -10.0f, 10.0f);
            CustomSliderFloat("Z",     &g_viewModelZ,     -10.0f, 10.0f);
            CustomSliderFloat("Rotate X", &g_viewModelRotX, -360.0f, 360.0f);
            CustomSliderFloat("Rotate Y", &g_viewModelRotY, -360.0f, 360.0f);
            CustomSliderFloat("Rotate Z", &g_viewModelRotZ, -360.0f, 360.0f);
            CustomSliderFloat("Scale X", &g_viewModelScaleX, -10.0f, 10.0f);
            CustomSliderFloat("Scale Y", &g_viewModelScaleY, -10.0f, 10.0f);
            CustomSliderFloat("Scale Z", &g_viewModelScaleZ, -10.0f, 10.0f);
            if (g_glassHand) CustomSliderFloat("Glass Amount", &g_glassAmount, 0.1f, 1.0f);
            EndCard();

            BeginCard("FOV Changer", &g_fovChangerBind);
            CustomCheckbox("Enabled", &g_fovChangerEnabled);
            if (g_fovChangerEnabled) CustomSliderFloat("FOV Mult", &g_fovMultiplier, 0.5f, 3.0f);
            EndCard();

            BeginCard("Chams Hand");
            CustomCheckbox("Enabled", &g_chamsHandEnabled);
            if (g_chamsHandEnabled) {
                const char* modes[] = { "Metallic", "Rainbow", "Galaxy", "Neon", "Lava", "Ice", "Toxic"};
                ImGui::PushItemWidth(colW - 28);
                ImGui::Combo("Mode##chams", &g_chamsHandMode, modes, 7);
                ImGui::PopItemWidth();
                CustomSliderFloat("Speed##chamshand", &g_chamsHandSpeed, 0.0f, 5.0f);
                CustomCheckbox("X-Ray##chamshand", &g_chamsHandXray);
                ImGui::Text("Colors:");
                ImGui::ColorEdit4("Visible##chamshand", g_chamsHandColorVis, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Hidden##chamshand",  g_chamsHandColorHid, ImGuiColorEditFlags_NoInputs);
            }
            EndCard();

            BeginCard("Player Chams");
            CustomCheckbox("Enabled", &g_playerChamsEnabled);
            if (g_playerChamsEnabled) {
                const char* pcModes[] = { "Metallic", "Rainbow", "Galaxy", "Neon", "Lava", "Ice", "Toxic" };
                ImGui::PushItemWidth(colW - 28);
                ImGui::Combo("Mode##playerchams", &g_playerChamsMode, pcModes, 7);
                ImGui::PopItemWidth();
                CustomSliderFloat("Speed##playerchams", &g_playerChamsSpeed, 0.0f, 5.0f);
                CustomCheckbox("X-Ray##playerchams", &g_playerChamsXray);
                ImGui::Text("Colors:");
                ImGui::ColorEdit4("Visible##playerchams", g_playerChamsColorVis, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Hidden##playerchams",  g_playerChamsColorHid, ImGuiColorEditFlags_NoInputs);
            }
            EndCard();

            BeginCard("Aspect Ratio");
            CustomCheckbox("Enabled", &g_aspectRatioEnabled);
            if (g_aspectRatioEnabled) {
                CustomSliderFloat("Stretch", &g_aspectRatioValue, 0.5f, 2.0f);
            }
            EndCard();

            BeginCard("Misc Settings");
            CustomCheckbox("Watermark",  &g_showWatermark);
            CustomCheckbox("ArrayList",  &g_showArrayList);
            CustomCheckbox("KeyBinds",   &g_showKeyBinds);
            CustomCheckbox("No Hurt Cam",&g_noHurtCamEnabled);
            EndCard();
        } else if (currentTab == 4) {
            // Config tab col2 is empty
        }

        ImGui::EndGroup(); // col2
        ImGui::EndChild(); // content
        ImGui::PopStyleVar(); // ScrollbarSize
        
        ImGui::End();
        ImGui::PopStyleColor(12);
        ImGui::PopStyleVar(4);
    }
}     
