#include "ChinaHat.h"
#include "Aimbot.h"
#include "WallCheck.h"
#include <vector>
#include <cmath>

static ImU32 ChinaHatColorU32(const float* c) {
    return IM_COL32(
        (int)(c[0] * 255.0f),
        (int)(c[1] * 255.0f),
        (int)(c[2] * 255.0f),
        (int)(c[3] * 255.0f));
}

static bool ProjectLocalPoint(const glm::vec3& local, const glm::mat4& modelview,
    const glm::mat4& projection, const GLint viewport[4], glm::vec2& screen)
{
    return ProjectToScreenOffscreen(local, modelview, projection, viewport, screen);
}

static void DrawProjectedRing(ImDrawList* drawList, const glm::mat4& modelview,
    const glm::mat4& projection, const GLint viewport[4], float y, float radius,
    ImU32 color, float thickness)
{
    const int segments = 64;
    std::vector<ImVec2> points;
    points.reserve(segments);

    for (int i = 0; i < segments; i++) {
        const float angle = (2.0f * 3.14159265f * (float)i) / (float)segments;
        glm::vec3 local(cosf(angle) * radius, y, sinf(angle) * radius);
        glm::vec2 screen;
        if (!ProjectLocalPoint(local, modelview, projection, viewport, screen))
            continue;
        points.emplace_back(screen.x, screen.y);
    }

    if (points.size() < 8)
        return;

    drawList->AddPolyline(points.data(), (int)points.size(), color, ImDrawFlags_Closed, thickness);
}

static void DrawChinaHatCone(ImDrawList* drawList, const glm::mat4& modelview,
    const glm::mat4& projection, const GLint viewport[4], ImU32 lineColor, ImU32 fillColor)
{
    const int segments = 48;
    const float y = g_chinaHatPosY;
    const float radius = g_chinaHatRadius;
    const float height = g_chinaHatHeight;

    glm::vec2 tipScreen;
    if (!ProjectLocalPoint(glm::vec3(0.0f, y - height, 0.0f), modelview, projection, viewport, tipScreen))
        return;

    std::vector<glm::vec2> baseScreen;
    baseScreen.reserve(segments);
    for (int i = 0; i < segments; i++) {
        const float angle = (2.0f * 3.14159265f * (float)i) / (float)segments;
        glm::vec3 local(cosf(angle) * radius, y, sinf(angle) * radius);
        glm::vec2 screen;
        if (ProjectLocalPoint(local, modelview, projection, viewport, screen))
            baseScreen.push_back(screen);
    }
    const int validBase = (int)baseScreen.size();
    if (validBase < 8)
        return;

    const ImVec2 tip(tipScreen.x, tipScreen.y);
    for (int i = 0; i < validBase; i++) {
        const int next = (i + 1) % validBase;
        const ImVec2 a(baseScreen[i].x, baseScreen[i].y);
        const ImVec2 b(baseScreen[next].x, baseScreen[next].y);
        if ((fillColor >> 24) != 0)
            drawList->AddTriangleFilled(tip, a, b, fillColor);
        drawList->AddLine(a, b, lineColor, g_chinaHatLineWidth);
        drawList->AddLine(a, tip, lineColor, g_chinaHatLineWidth * 0.75f);
    }
}

static void DrawNimb(ImDrawList* drawList, const glm::mat4& modelview,
    const glm::mat4& projection, const GLint viewport[4], ImU32 color, ImU32 glowColor)
{
    const float y = g_chinaHatPosY;
    const float radius = g_chinaHatRadius;
    const float glow = g_chinaHatHeight * 0.12f;

    DrawProjectedRing(drawList, modelview, projection, viewport, y, radius + glow, glowColor,
        g_chinaHatLineWidth * 2.2f);
    DrawProjectedRing(drawList, modelview, projection, viewport, y, radius, color,
        g_chinaHatLineWidth);
    DrawProjectedRing(drawList, modelview, projection, viewport, y, radius * 0.72f, glowColor,
        g_chinaHatLineWidth * 0.9f);
}

void RenderChinaHat() {
    if (!g_chinaHatEnabled)
        return;

    std::lock_guard<std::mutex> lock(g_entitiesMutex);
    if (g_entities.empty())
        return;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    ImU32 lineColor = ChinaHatColorU32(g_chinaHatColor);
    ImU32 fillColor = IM_COL32(
        (int)(g_chinaHatColor[0] * 255.0f),
        (int)(g_chinaHatColor[1] * 255.0f),
        (int)(g_chinaHatColor[2] * 255.0f),
        (int)(g_chinaHatFillAlpha * 255.0f));
    ImU32 glowColor = IM_COL32(
        (int)(g_chinaHatColor[0] * 255.0f),
        (int)(g_chinaHatColor[1] * 255.0f),
        (int)(g_chinaHatColor[2] * 255.0f),
        (int)(g_chinaHatColor[3] * 120.0f));

    for (const auto& ent : g_entities) {
        if (!ent.isPlayer)
            continue;
        if (ent.modelview[3][2] > 0.0f)
            continue;

        ImU32 drawLine = lineColor;
        ImU32 drawFill = fillColor;
        if (g_espColorByVisibility) {
            const bool visible = WallCheckIsVisible(ent);
            const float* c = visible ? g_espVisibleColor : g_espHiddenColor;
            drawLine = IM_COL32(
                (int)(c[0] * 255.0f),
                (int)(c[1] * 255.0f),
                (int)(c[2] * 255.0f),
                (int)(g_chinaHatColor[3] * 255.0f));
            drawFill = IM_COL32(
                (int)(c[0] * 255.0f),
                (int)(c[1] * 255.0f),
                (int)(c[2] * 255.0f),
                (int)(g_chinaHatFillAlpha * 255.0f));
            glowColor = IM_COL32(
                (int)(c[0] * 255.0f),
                (int)(c[1] * 255.0f),
                (int)(c[2] * 255.0f),
                120);
        }

        if (g_chinaHatMode == 0)
            DrawChinaHatCone(drawList, ent.modelview, ent.projection, viewport, drawLine, drawFill);
        else
            DrawNimb(drawList, ent.modelview, ent.projection, viewport, drawLine, glowColor);
    }
}
