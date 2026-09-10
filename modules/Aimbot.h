#pragma once
#include "../core/Globals.h"

bool IsGameplayCursor();
void AimRunLogic(HWND window);
void ProcessBinds();
bool HotkeyButton(const char* label, ToggleBind& bind);
glm::vec3 GetEntityRelativePos(const glm::mat4& modelview);
bool IsPointInCircle(const glm::vec2& point, const glm::vec2& center, float radius);
bool ProjectToScreen(const glm::vec3& point, const glm::mat4& modelview, const glm::mat4& projection, const GLint viewport[4], glm::vec2& screenPoint);
bool ProjectToScreenOffscreen(const glm::vec3& point, const glm::mat4& modelview, const glm::mat4& projection, const GLint viewport[4], glm::vec2& screenPoint);
bool AimMcfShouldSkip(const EntityInfo& ent);
