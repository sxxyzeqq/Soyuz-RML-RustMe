#include "Aimbot.h"
#include "../core/Config.h"
#include <algorithm>
#include <cmath>
#include <limits>

bool IsGameplayCursor() {
    CURSORINFO cursorInfo = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&cursorInfo)) return true;
    return !((cursorInfo.flags & CURSOR_SHOWING) && cursorInfo.hCursor == LoadCursor(NULL, IDC_ARROW));
}

extern ToggleBind g_aimBind, g_espBind, g_sprintBind, g_timerBind, g_fogBind, g_wallhackBind, g_zoomBind, g_fullbrightBind;
extern float g_aimSens;
extern bool g_aimSensEnabled;

glm::vec3 GetEntityRelativePos(const glm::mat4& modelview) {
    glm::mat4 invCamera = glm::inverse(g_cameraMatrix);
    glm::mat4 entityLocalMatrix = invCamera * modelview;
    return glm::vec3(entityLocalMatrix[3]);
}

bool IsPointInCircle(const glm::vec2& point, const glm::vec2& center, float radius) {
    return glm::distance(point, center) <= radius;
}

bool ProjectToScreen(const glm::vec3& point, const glm::mat4& modelview, const glm::mat4& projection, const GLint viewport[4], glm::vec2& screenPoint) {
    const glm::vec4 clip = projection * modelview * glm::vec4(point, 1.0f);
    if (clip.w <= 0.001f) return false;
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    screenPoint.x = viewport[0] + (ndc.x + 1.0f) * 0.5f * viewport[2];
    screenPoint.y = viewport[1] + (1.0f - (ndc.y + 1.0f) * 0.5f) * viewport[3];
    return true;
}

// Проекция которая работает даже для объектов за спиной
bool ProjectToScreenOffscreen(const glm::vec3& point, const glm::mat4& modelview, const glm::mat4& projection, const GLint viewport[4], glm::vec2& screenPoint) {
    const glm::vec4 clip = projection * modelview * glm::vec4(point, 1.0f);
    
    float w = clip.w;
    if (std::abs(w) < 0.001f) w = 0.001f;
    
    // Если объект за камерой (w < 0), инвертируем координаты
    bool isBehind = w < 0.0f;
    if (isBehind) w = -w;
    
    glm::vec3 ndc = glm::vec3(clip) / w;
    
    // Если за камерой, инвертируем NDC координаты
    if (isBehind) {
        ndc.x = -ndc.x;
        ndc.y = -ndc.y;
    }
    
    screenPoint.x = viewport[0] + (ndc.x + 1.0f) * 0.5f * viewport[2];
    screenPoint.y = viewport[1] + (1.0f - (ndc.y + 1.0f) * 0.5f) * viewport[3];
    
    return true;
}

bool AimMcfShouldSkip(const EntityInfo& ent) {
    if (!g_aimMcfEnabled || g_nameTags.empty()) return false;

    auto isTagged = [](const glm::vec3& playerPos, const glm::vec3& tagPos) {
        const float horizontalDist = glm::distance(
            glm::vec2(playerPos.x, playerPos.z),
            glm::vec2(tagPos.x, tagPos.z)
        );
        const float verticalDist = std::fabs(playerPos.y - tagPos.y);

        return horizontalDist <= g_aimMcfMaxHorizontalDistance &&
            verticalDist <= g_aimMcfMaxVerticalDistance;
    };

    auto shouldSkipForSpace = [&](const glm::vec3& playerPos) {
        const float distanceToPlayer = glm::length(playerPos);
        int tagCount = 0;

        for (const auto& tag : g_nameTags) {
            if (isTagged(playerPos, glm::vec3(tag.modelview[3])) ||
                isTagged(playerPos, tag.relativePos) ||
                isTagged(playerPos, tag.translatePos)) {
                if (++tagCount >= 2) break;
            }
        }

        return (tagCount >= 2) || (distanceToPlayer > 4.16f && tagCount >= 1);
    };

    return shouldSkipForSpace(glm::vec3(ent.modelview[3])) ||
        shouldSkipForSpace(ent.worldPos) ||
        shouldSkipForSpace(ent.translatePos);
}

void AimRunLogic(HWND window) {
    double current = std::chrono::duration_cast<std::chrono::duration<double>>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    g_aimTracks.erase(
        std::remove_if(g_aimTracks.begin(), g_aimTracks.end(),
            [current](const AimTrack& t) { return current - t.lastUpdateTime > 0.5; }),
        g_aimTracks.end()
    );
    if (!g_aimEnabled || g_aimTargets.empty() || g_menuVisible) { g_aimTargets.clear(); return; }
    if (g_aimAutoDisable && !IsGameplayCursor()) { g_aimTargets.clear(); return; }
    if (!window || GetForegroundWindow() != window) { g_aimTargets.clear(); return; }
    RECT clientRect{};
    if (!GetClientRect(window, &clientRect)) { g_aimTargets.clear(); return; }
    glm::vec2 center((clientRect.right - clientRect.left) * 0.5f, (clientRect.bottom - clientRect.top) * 0.5f);
    AimTarget* bestTarget = nullptr;
    float bestScore = (std::numeric_limits<float>::max)();
    for (auto& target : g_aimTargets) {
        if (!target.valid) continue;
        if (!IsPointInCircle(target.screenPosition, center, g_aimCurrentFov)) continue;
        if (target.distance < bestScore) { bestScore = target.distance; bestTarget = &target; }
    }
    if (!bestTarget) { g_aimTargets.clear(); g_aimHasTarget = false; return; }
    g_aimTargetPoint = bestTarget->screenPosition;
    g_aimHasTarget = true;
    
    POINT targetPoint{ static_cast<LONG>(bestTarget->screenPosition.x), static_cast<LONG>(bestTarget->screenPosition.y) };
    ClientToScreen(window, &targetPoint);
    POINT currentPoint{};
    if (!GetCursorPos(&currentPoint)) { g_aimTargets.clear(); return; }
    float smoothX = (std::clamp)(g_aimSmoothX, 0.01f, 1.0f);
    float dx = (float)(targetPoint.x - currentPoint.x);
    float dy = (float)(targetPoint.y - currentPoint.y);
    if (g_zoomEnabled && g_zoomAmount > 1.001f) { dx /= g_zoomAmount; dy /= g_zoomAmount; }
    LONG newX, newY;
    if (g_aimSensEnabled) {
        float sensComp = (std::clamp)(g_aimSens, 0.01f, 1.0f);
        newX = currentPoint.x + static_cast<LONG>(dx * smoothX * sensComp);
        newY = currentPoint.y + static_cast<LONG>(dy * sensComp);
    } else {
        newX = currentPoint.x + static_cast<LONG>(dx * smoothX);
        newY = currentPoint.y + static_cast<LONG>(dy);
    }
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(newX * (65535.0f / (GetSystemMetrics(SM_CXSCREEN) - 1)));
    input.mi.dy = static_cast<LONG>(newY * (65535.0f / (GetSystemMetrics(SM_CYSCREEN) - 1)));
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
    g_aimTargets.clear();
}

void ProcessBinds() {
    if (g_menuVisible) return;
    if (!g_hwnd || GetForegroundWindow() != g_hwnd) return;
    if (GetAsyncKeyState('T') & 0x8000 || GetAsyncKeyState(VK_RETURN) & 0x8000) return;
    ToggleBind* binds[] = { &g_aimBind, &g_espBind, &g_sprintBind, &g_timerBind, &g_noFallBind, &g_fogBind, &g_wallhackBind, &g_zoomBind, &g_fullbrightBind, &g_ammoEspBind, &g_tracerBind, &g_tracerNewBind, &g_chinaHatBind, &g_nightModeBind, &g_coordsBind, &g_fovChangerBind, &g_rustmeEspBind };
    for (auto b : binds) {
        if (b->key != 0) {
            bool pressed = (GetAsyncKeyState(b->key) & 0x8000) != 0;
            if (b == &g_zoomBind && g_zoomHoldMode) {
                *(b->pEnabled) = pressed;
            } else {
                if (pressed && !b->wasPressed) *(b->pEnabled) = !*(b->pEnabled);
            }
            b->wasPressed = pressed;
        }
    }
}

bool HotkeyButton(const char* label, ToggleBind& bind) {
    ImGui::PushID(label);
    ImGui::SameLine();
    bool clicked = false;
    if (bind.waiting) {
        if (ImGui::Button("...", ImVec2(80, 0))) bind.waiting = false;
        for (int i = 1; i < 255; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                if (i != VK_RSHIFT) { bind.key = (i == VK_ESCAPE) ? 0 : i; bind.waiting = false; }
            }
        }
    } else {
        if (ImGui::Button(GetKeyName(bind.key), ImVec2(80, 0))) { bind.waiting = true; clicked = true; }
    }
    ImGui::PopID();
    return clicked;
}
