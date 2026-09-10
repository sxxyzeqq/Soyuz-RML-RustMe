#include "TracersNew.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {
    glm::mat4 s_lastModelview(1.0f);
    glm::mat4 s_lastProjection(1.0f);
    bool s_hasModelview = false;
    bool s_hasProjection = false;

    bool Near(float value, float expected, float eps = 0.0015f) {
        return std::fabs(value - expected) <= eps;
    }

    bool IsTracerScale(float x, float y, float z) {
        return (Near(x, 0.5f) && Near(y, 0.5f) && Near(z, 0.5f)) ||
            (Near(x, 0.68f) && Near(y, 0.68f) && Near(z, 0.68f));
    }

    bool IsTracerTranslate(float x, float y, float z) {
        return Near(x, 0.11875f) &&
            ((Near(y, 0.75f) && Near(z, 0.00625f)) ||
             (Near(y, 0.5625f) && Near(z, 0.25f)));
    }

    void RememberMatrix(GLenum pname, const GLfloat* params) {
        if (!params) return;

        if (pname == GL_MODELVIEW_MATRIX) {
            s_lastModelview = glm::make_mat4(params);
            s_hasModelview = true;
        }
        else if (pname == GL_PROJECTION_MATRIX) {
            s_lastProjection = glm::make_mat4(params);
            s_hasProjection = true;
        }
    }

    bool QueryMatrix(GLenum pname, glm::mat4& matrix) {
        GLfloat values[16] = {};

        if (original_glGetFloatv_addr) {
            ((GetFloatv_fn)original_glGetFloatv_addr)(pname, values);
        }
        else {
            glGetFloatv(pname, values);
        }

        matrix = glm::make_mat4(values);
        RememberMatrix(pname, values);
        return true;
    }

    void PushTarget(const glm::vec3& translate, const glm::vec3& scale) {
        if (!g_tracerNewEnabled) return;

        TracerNewTarget target{};
        target.translatePos = translate;
        target.scale = scale;

        if (!QueryMatrix(GL_MODELVIEW_MATRIX, target.modelview)) {
            if (!s_hasModelview) return;
            target.modelview = s_lastModelview;
        }

        if (!QueryMatrix(GL_PROJECTION_MATRIX, target.projection)) {
            if (!s_hasProjection) return;
            target.projection = s_lastProjection;
        }

        if (std::fabs(target.projection[3][3] - 1.0f) < 0.1f) return;

        const glm::vec3 viewPos(target.modelview[3]);
        std::lock_guard<std::mutex> lock(g_tracerNewTargetsMutex);

        for (const auto& existing : g_tracerNewTargets) {
            if (glm::distance(viewPos, glm::vec3(existing.modelview[3])) < 0.18f) {
                return;
            }
        }

        if (g_tracerNewTargets.size() < 512) {
            g_tracerNewTargets.push_back(target);
        }
    }

    ImVec2 ClampToViewport(const ImVec2& from, const ImVec2& target, float width, float height) {
        const float dx = target.x - from.x;
        const float dy = target.y - from.y;
        const float eps = 0.0001f;
        float best = (std::numeric_limits<float>::max)();

        auto consider = [&](float t) {
            if (t < 0.0f || t >= best) return;
            const float x = from.x + dx * t;
            const float y = from.y + dy * t;
            if (x >= -0.5f && x <= width + 0.5f && y >= -0.5f && y <= height + 0.5f) {
                best = t;
            }
        };

        if (dx > eps) consider((width - from.x) / dx);
        else if (dx < -eps) consider((0.0f - from.x) / dx);

        if (dy > eps) consider((height - from.y) / dy);
        else if (dy < -eps) consider((0.0f - from.y) / dy);

        if (best == (std::numeric_limits<float>::max)()) {
            return ImVec2(
                (std::clamp)(target.x, 0.0f, width),
                (std::clamp)(target.y, 0.0f, height));
        }

        return ImVec2(
            (std::clamp)(from.x + dx * best, 0.0f, width),
            (std::clamp)(from.y + dy * best, 0.0f, height));
    }

    bool ProjectTarget(const TracerNewTarget& target, const GLint viewport[4], glm::vec2& screenPoint, bool& behind) {
        glm::vec4 clip = target.projection * target.modelview * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        float w = clip.w;

        if (!std::isfinite(w)) return false;
        if (std::fabs(w) < 0.001f) w = (w < 0.0f) ? -0.001f : 0.001f;

        behind = w < 0.0f;
        if (behind) {
            w = -w;
            clip.x = -clip.x;
            clip.y = -clip.y;
        }

        const glm::vec3 ndc = glm::vec3(clip) / w;
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y)) return false;

        screenPoint.x = viewport[0] + (ndc.x + 1.0f) * 0.5f * viewport[2];
        screenPoint.y = viewport[1] + (1.0f - (ndc.y + 1.0f) * 0.5f) * viewport[3];
        return true;
    }
}

void TracerNewOnTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    if (IsTracerTranslate(x, y, z)) {
        PushTarget(glm::vec3(x, y, z), glm::vec3(1.0f));
    }
}

void TracerNewOnScalef(GLfloat x, GLfloat y, GLfloat z) {
    if (IsTracerScale(x, y, z)) {
        PushTarget(g_lastTranslate, glm::vec3(x, y, z));
    }
}

void TracerNewOnGetFloatv(GLenum pname, const GLfloat* params) {
    RememberMatrix(pname, params);
}

void RenderTracersNew() {
    if (!g_tracerNewEnabled) return;

    std::vector<TracerNewTarget> targets;
    {
        std::lock_guard<std::mutex> lock(g_tracerNewTargetsMutex);
        targets = g_tracerNewTargets;
    }

    if (targets.empty()) return;

    GLint viewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);

    const float screenW = static_cast<float>(viewport[2]);
    const float screenH = static_cast<float>(viewport[3]);
    if (screenW <= 1.0f || screenH <= 1.0f) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 start(screenW * 0.5f, screenH);
    const ImVec2 center(screenW * 0.5f, screenH * 0.5f);
    const float rayLen = (std::max)(screenW, screenH) * 2.0f;
    const float thickness = (std::max)(0.5f, g_tracerNewThickness);
    const ImU32 color = IM_COL32(
        static_cast<int>(g_tracerNewColor[0] * 255.0f),
        static_cast<int>(g_tracerNewColor[1] * 255.0f),
        static_cast<int>(g_tracerNewColor[2] * 255.0f),
        static_cast<int>(g_tracerNewColor[3] * 255.0f));

    for (const auto& target : targets) {
        glm::vec2 projected(0.0f);
        bool behind = false;
        if (!ProjectTarget(target, viewport, projected, behind)) continue;

        ImVec2 end(projected.x, projected.y);
        const bool outside = end.x < 0.0f || end.x > screenW || end.y < 0.0f || end.y > screenH;

        if (behind) {
            const glm::vec3 viewPos(target.modelview[3]);
            glm::vec2 dir(viewPos.x, -viewPos.y);
            const float len = glm::length(dir);
            if (len > 0.001f) dir /= len;
            else dir = glm::vec2(0.0f, -1.0f);
            end = ClampToViewport(center, ImVec2(center.x + dir.x * rayLen, center.y + dir.y * rayLen), screenW, screenH);
        }
        else if (outside) {
            end = ClampToViewport(start, end, screenW, screenH);
        }

        drawList->AddLine(start, end, color, thickness);
        if (behind || outside) {
            drawList->AddCircleFilled(end, (std::max)(2.0f, thickness + 1.0f), color, 16);
        }
    }
}

void ClearTracersNewTargets() {
    std::lock_guard<std::mutex> lock(g_tracerNewTargetsMutex);
    g_tracerNewTargets.clear();
}
