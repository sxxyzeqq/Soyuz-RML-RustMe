#pragma once
#include "../core/Globals.h"

void DrawTracer(ImDrawList* drawList, const glm::mat4& modelview, const glm::mat4& projection, const GLint viewport[4], ImU32 color, float thickness);
void Draw3DBox(ImDrawList* drawList, const glm::mat4& modelview, const glm::mat4& projection, const GLint viewport[4], float halfSize, ImU32 color, float thickness);
void RenderESP(double currentTime);
void RenderSoundESP();
void RenderOpenALListenerInfo();
