#include "ESP.h"
#include "Aimbot.h"
#include "WallCheck.h"
#include "../hooks/OpenAL.h"

extern float g_espBreadcrumbsTime;

void DrawTracer(ImDrawList* drawList, const glm::mat4& modelview, const glm::mat4& projection,
    const GLint viewport[4], ImU32 color, float thickness)
{
    glm::vec2 playerScreen;
    ProjectToScreenOffscreen(glm::vec3(0.0f, 0.15f, 0.0f), modelview, projection, viewport, playerScreen);

    float centerX = (float)viewport[2] / 2.0f;
    float bottomY = (float)viewport[3];
    float screenW = (float)viewport[2];
    float screenH = (float)viewport[3];
    // Вычисляем направление от центра к игроку
    float dx = playerScreen.x - centerX;
    float dy = playerScreen.y - bottomY;

    // Если игрок за пределами экрана, находим точку пересечения с границей
    float targetX = playerScreen.x;
    float targetY = playerScreen.y;

    // Проверяем пересечение с каждой границей экрана
    if (targetX < 0 || targetX > screenW || targetY < 0 || targetY > screenH) {
        // Вычисляем параметр t для пересечения с границами
        float tMin = 0.0f;
        float tMax = 1.0f;

        // Левая граница
        if (dx < 0) tMax = (std::min)(tMax, (0.0f - centerX) / dx);
        // Правая граница  
        if (dx > 0) tMax = (std::min)(tMax, (screenW - centerX) / dx);
        // Верхняя граница
        if (dy < 0) tMax = (std::min)(tMax, (0.0f - bottomY) / dy);
        // Нижняя граница
        if (dy > 0) tMax = (std::min)(tMax, (screenH - bottomY) / dy);

        targetX = centerX + dx * tMax;
        targetY = bottomY + dy * tMax;
    }

    drawList->AddLine(ImVec2(centerX, bottomY), ImVec2(targetX, targetY), color, thickness);
}

void DrawOffscreenArrow(ImDrawList* drawList, const glm::vec3& targetWorldPos, const glm::vec3& cameraWorldPos, float radius, float size, ImU32 color) {
    // Вычисляем вектор от камеры к цели в мировых координатах
    glm::vec3 toTarget = targetWorldPos - cameraWorldPos;

    // Проецируем на плоскость XZ (игнорируем высоту)
    float x = toTarget.x;
    float z = toTarget.z;

    // Угол на плоскости XZ
    float angle = atan2f(x, -z);

    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 center = ImVec2(screenSize.x * 0.5f, screenSize.y * 0.5f);

    // Смещение угла на -90 градусов чтобы 0 был сверху
    float drawAngle = angle - 1.5708f;

    // Кончик стрелки
    ImVec2 tip = ImVec2(
        center.x + cosf(drawAngle) * radius,
        center.y + sinf(drawAngle) * radius
    );

    // Основание треугольника
    float spread = 0.45f;
    ImVec2 side1 = ImVec2(
        center.x + cosf(drawAngle - spread) * (radius - size),
        center.y + sinf(drawAngle - spread) * (radius - size)
    );
    ImVec2 side2 = ImVec2(
        center.x + cosf(drawAngle + spread) * (radius - size),
        center.y + sinf(drawAngle + spread) * (radius - size)
    );

    drawList->AddTriangleFilled(tip, side1, side2, color);
    drawList->AddTriangle(tip, side1, side2, IM_COL32(0, 0, 0, 200), 1.0f);
}

void Draw3DBox(ImDrawList* drawList, const glm::mat4& modelview, const glm::mat4& projection,
    const GLint viewport[4], float halfSize, ImU32 color, float thickness)
{
    glm::vec3 corners[8] = {
        {-halfSize,-halfSize,-halfSize}, { halfSize,-halfSize,-halfSize},
        { halfSize, halfSize,-halfSize}, {-halfSize, halfSize,-halfSize},
        {-halfSize,-halfSize, halfSize}, { halfSize,-halfSize, halfSize},
        { halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize},
    };
    glm::vec2 sc[8];
    for (int i = 0; i < 8; i++)
        if (!ProjectToScreen(corners[i], modelview, projection, viewport, sc[i])) return;
    int edges[12][2] = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };
    for (auto& e : edges)
        drawList->AddLine(ImVec2(sc[e[0]].x, sc[e[0]].y), ImVec2(sc[e[1]].x, sc[e[1]].y), color, thickness);
}

void RenderESP(double currentTime) {
    std::lock_guard<std::mutex> lock(g_entitiesMutex);
    if (g_entities.empty()) return;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    float screenW = (float)viewport[2];
    float screenH = (float)viewport[3];
    std::vector<glm::vec3> renderedAmmoWorldPositions;

    for (const auto& ent : g_entities) {

        // ── Ammo ESP ──────────────────────────────────────────────────
        if (g_ammoEspEnabled && ent.isAmmo) {
            // Не рисуем 2D ammo (иконки в инвентаре, прицел и т.п.) — у них ortho-projection.
            if (std::abs(ent.projection[3][3] - 1.0f) < 0.1f) continue;
            glm::vec2 bottomScreen, topScreen;
            ProjectToScreenOffscreen(glm::vec3(0.0f, 0.15f, 0.0f), ent.modelview, ent.projection, viewport, bottomScreen);
            ProjectToScreenOffscreen(glm::vec3(0.0f, -1.8f, 0.0f), ent.modelview, ent.projection, viewport, topScreen);
            float h = std::abs(topScreen.y - bottomScreen.y);
            float w = h / 2.0f;
            if (w < 2.0f || h < 4.0f) continue;
            float cx = (bottomScreen.x + topScreen.x) * 0.5f;
            float minY = (std::min)(bottomScreen.y, topScreen.y);

            glm::vec3 camPos(g_listenerPosX, g_listenerPosY, g_listenerPosZ);
            float dist = glm::length(ent.translatePos);

            if (g_ammoCometMode) {
                // ─── Капля по 3D-направлению полёта ─────────
                glm::vec3 ammoWorldPos = ent.hasWorldPos ? ent.worldPos : GetEntityRelativePos(ent.modelview);
                bool duplicateAmmo = false;
                for (const auto& renderedPos : renderedAmmoWorldPositions) {
                    if (glm::distance(renderedPos, ammoWorldPos) < 0.18f) {
                        duplicateAmmo = true;
                        break;
                    }
                }
                if (duplicateAmmo) continue;
                renderedAmmoWorldPositions.push_back(ammoWorldPos);

                auto projectAmmoWorld = [&](const glm::vec3& worldPos, glm::vec2& screen) {
                    glm::mat4 mv = g_cameraMatrix;
                    mv[3] = g_cameraMatrix * glm::vec4(worldPos, 1.0f);
                    return ProjectToScreen(glm::vec3(0.0f, 0.0f, 0.0f), mv, ent.projection, viewport, screen);
                    };

                glm::vec2 ammoCenterScreen;
                bool useWorldProjection = projectAmmoWorld(ammoWorldPos, ammoCenterScreen);
                if (!useWorldProjection)
                    ProjectToScreenOffscreen(glm::vec3(0.0f, 0.0f, 0.0f), ent.modelview, ent.projection, viewport, ammoCenterScreen);
                float coreX = ammoCenterScreen.x;
                float coreY = ammoCenterScreen.y;
                float coreR = (std::max)(2.25f, h * 0.16f);
                if (coreR > 10.5f) coreR = 10.5f;

                // Кэш предыдущей 3D-позиции пули.
                struct PrevSnap {
                    glm::vec2 screenPos;
                    glm::vec3 worldPos;     // в системе камеры (modelview[3] entity)
                    glm::vec3 smoothWorldPos;
                    glm::vec3 smoothVelocity;
                    double    time;
                };
                static std::vector<PrevSnap> s_prev;
                s_prev.erase(
                    std::remove_if(s_prev.begin(), s_prev.end(),
                        [&](const PrevSnap& p) { return (currentTime - p.time) > 0.5; }),
                    s_prev.end());

                // Текущая 3D позиция в view-space.
                glm::vec3 curView = useWorldProjection ? ammoWorldPos : glm::vec3(ent.modelview[3]);
                glm::vec2 headScreen(coreX, coreY);

                // Ищем ближайший предыдущий снимок (по 3D).
                glm::vec3 prevView = curView;
                glm::vec3 smoothView = curView;
                glm::vec3 smoothVelocity(0.0f);
                bool   havePrev = false;
                float  bestScore = 999999.0f;
                float  bestD = 4.0f; // порог 4 блока
                int    bestIdx = -1;
                (void)bestD;
                for (size_t i = 0; i < s_prev.size(); i++) {
                    if (std::abs(currentTime - s_prev[i].time) < 0.000001)
                        continue;

                    float worldD = glm::distance(s_prev[i].worldPos, curView);
                    float screenD = glm::distance(s_prev[i].screenPos, headScreen);
                    if (worldD > 28.0f && screenD > 280.0f)
                        continue;

                    float score = worldD * 100.0f + screenD * 0.05f;
                    if (score < bestScore) { bestScore = score; bestIdx = (int)i; }
                }
                if (bestIdx >= 0) {
                    PrevSnap& snap = s_prev[bestIdx];
                    prevView = snap.smoothWorldPos;
                    havePrev = true;
                    float rawStep = glm::length(curView - snap.worldPos);
                    float posAlpha = rawStep > 2.5f ? 0.65f : 0.38f;
                    smoothView = snap.smoothWorldPos + (curView - snap.smoothWorldPos) * posAlpha;
                    glm::vec3 rawVelocity = smoothView - snap.smoothWorldPos;
                    smoothVelocity = snap.smoothVelocity * 0.60f + rawVelocity * 0.40f;
                    snap.worldPos = curView;
                    snap.smoothWorldPos = smoothView;
                    snap.smoothVelocity = smoothVelocity;
                    snap.screenPos = headScreen;
                    snap.time = currentTime;
                }
                else {
                    PrevSnap np;
                    np.worldPos = curView;
                    np.smoothWorldPos = curView;
                    np.smoothVelocity = glm::vec3(0.0f);
                    np.screenPos = headScreen;
                    np.time = currentTime;
                    s_prev.push_back(np);
                }

                // 3D вектор движения.
                if (havePrev) {
                    glm::vec2 smoothHeadScreen;
                    bool smoothHeadOk = false;
                    if (useWorldProjection) {
                        smoothHeadOk = projectAmmoWorld(smoothView, smoothHeadScreen);
                    }
                    else {
                        glm::mat4 mvHead = ent.modelview;
                        mvHead[3] = glm::vec4(smoothView, 1.0f);
                        smoothHeadOk = ProjectToScreenOffscreen(glm::vec3(0.0f, 0.0f, 0.0f), mvHead, ent.projection, viewport, smoothHeadScreen);
                    }

                    if (smoothHeadOk) {
                        coreX = smoothHeadScreen.x;
                        coreY = smoothHeadScreen.y;
                        headScreen = smoothHeadScreen;
                        curView = smoothView;
                    }
                }

                glm::vec3 vel3D = havePrev ? smoothVelocity : (curView - prevView);
                float vlen3D = glm::length(vel3D);

                int rC = (int)(g_ammoCometColor[0] * 255);
                int gC = (int)(g_ammoCometColor[1] * 255);
                int bC = (int)(g_ammoCometColor[2] * 255);

                // Если движения нет (или первый кадр) — рисуем сферу с glow.
                if (!havePrev || vlen3D < 0.05f) {
                    drawList->AddCircleFilled({ coreX, coreY  }, coreR * 3.0f,
                        IM_COL32(rC, gC, bC, 25), 32);
                    drawList->AddCircleFilled({ coreX, coreY }, coreR * 2.2f,
                        IM_COL32(rC, gC, bC, 50), 32);
                    drawList->AddCircleFilled({ coreX, coreY }, coreR * 1.6f,
                        IM_COL32(rC, gC, bC, 90), 32);
                    drawList->AddCircleFilled({ coreX, coreY }, coreR * 1.25f,
                        IM_COL32(rC, gC, bC, 160), 32);
                    drawList->AddCircleFilled({ coreX, coreY }, coreR,
                        IM_COL32(rC, gC, bC, 255), 32);
                    drawList->AddCircleFilled(
                        { coreX - coreR * 0.30f, coreY - coreR * 0.30f },
                        coreR * 0.50f,
                        IM_COL32((std::min)(255, rC + 100), (std::min)(255, gC + 100), (std::min)(255, bC + 100), 255),
                        24);
                    continue;
                }

                // Вычисляем экранные позиции "head" (текущая) и "tail" (смещённая назад в мире).
                // Tail = curView - normalize(vel3D) * tailLen
                // tailLen в блоках — пропорционально скорости, но ограничено.
                float tailLen3D = (std::min)(8.5f, vlen3D * 6.5f);
                glm::vec3 dir3D = vel3D / vlen3D;
                glm::vec3 tailView = curView - dir3D * tailLen3D;

                // Проекция tailView через текущий projection. Берём modelview ent
                // и подменяем translation на tailView.
                glm::vec2 tailScreen;
                bool hasTail = false;
                if (useWorldProjection) {
                    hasTail = projectAmmoWorld(tailView, tailScreen);
                }
                else {
                    glm::mat4 mvTail = ent.modelview;
                    mvTail[3] = glm::vec4(tailView, 1.0f);
                    hasTail = ProjectToScreenOffscreen(glm::vec3(0.0f, 0.0f, 0.0f), mvTail, ent.projection, viewport, tailScreen);
                }

                if (!hasTail) {
                    // Запасной вариант — сфера.
                    drawList->AddCircleFilled({ coreX, coreY }, coreR,
                        IM_COL32(rC, gC, bC, 255), 32);
                    continue;
                }
                float tailX = tailScreen.x;
                float tailY = tailScreen.y;

                glm::vec2 rawDelta = headScreen - glm::vec2(tailX, tailY);
                float rawDeltaLen = glm::length(rawDelta);
                glm::vec2 rawDir(0.0f, 0.0f);
                if (rawDeltaLen >= 0.75f) {
                    rawDir = rawDelta / rawDeltaLen;
                }
                else {
                    glm::vec2 projectedDelta = headScreen - glm::vec2(tailX, tailY);
                    float projectedLen = glm::length(projectedDelta);
                    if (projectedLen >= 0.75f)
                        rawDir = projectedDelta / projectedLen;
                }

                if (glm::length(rawDir) > 0.01f) {
                    float trailLen = rawDeltaLen;
                    float minTrailLen = coreR * 5.5f;
                    float maxTrailLen = (std::min)(sqrtf(screenW * screenW + screenH * screenH) * 0.28f, 320.0f);
                    trailLen = (std::max)(minTrailLen, (std::min)(trailLen, maxTrailLen));

                    tailX = coreX - rawDir.x * trailLen;
                    tailY = coreY - rawDir.y * trailLen;
                }

                // Длина капсулы на экране.
                ImVec2 head(coreX, coreY);
                ImVec2 tail(tailX, tailY);
                ImVec2 sdir(head.x - tail.x, head.y - tail.y);
                float slen = sqrtf(sdir.x * sdir.x + sdir.y * sdir.y);
                if (slen < 1.0f) {
                    drawList->AddCircleFilled({ coreX, coreY }, coreR,
                        IM_COL32(rC, gC, bC, 255), 32);
                    continue;
                }
                ImVec2 dir(sdir.x / slen, sdir.y / slen);

                // Толстая часть — на голове (head — куда летит), узкая — на хвосте.
                float Rfront = coreR;          // куда летит — толсто
                float Rback = coreR * 0.30f;  // позади — узко

                Rfront *= 1.15f;
                Rback = coreR * 0.45f;

                auto drawCapsule = [&](float r1, float r2, ImU32 color) {
                    if (r1 < 0.5f) r1 = 0.5f;
                    if (r2 < 0.5f) r2 = 0.5f;
                    const int arcSteps = 12;
                    ImVec2 pts[arcSteps * 2 + 2];
                    int idx = 0;
                    float baseAng = atan2f(dir.y, dir.x);
                    // Полуокружность вокруг head (фронт).
                    for (int s = 0; s <= arcSteps; s++) {
                        float a = baseAng + (-3.14159265f * 0.5f) +
                            ((float)s / (float)arcSteps) * 3.14159265f;
                        pts[idx++] = ImVec2(head.x + cosf(a) * r1, head.y + sinf(a) * r1);
                    }
                    // Полуокружность вокруг tail (зад).
                    for (int s = 0; s <= arcSteps; s++) {
                        float a = baseAng + (3.14159265f * 0.5f) +
                            ((float)s / (float)arcSteps) * 3.14159265f;
                        pts[idx++] = ImVec2(tail.x + cosf(a) * r2, tail.y + sinf(a) * r2);
                    }
                    drawList->AddConvexPolyFilled(pts, idx, color);
                    };

                // Glow слои.
                drawCapsule(Rfront * 3.0f, Rback * 3.0f, IM_COL32(rC, gC, bC, 25));
                drawCapsule(Rfront * 2.2f, Rback * 2.2f, IM_COL32(rC, gC, bC, 50));
                drawCapsule(Rfront * 1.6f, Rback * 1.6f, IM_COL32(rC, gC, bC, 90));
                drawCapsule(Rfront * 1.25f, Rback * 1.25f, IM_COL32(rC, gC, bC, 160));
                drawCapsule(Rfront, Rback, IM_COL32(rC, gC, bC, 255));

                // Highlight на толстом конце (head).
                ImVec2 hlight(head.x - dir.x * Rfront * 0.2f, head.y - dir.y * Rfront * 0.2f);
                drawList->AddCircleFilled(hlight, Rfront * 0.5f,
                    IM_COL32((std::min)(255, rC + 100), (std::min)(255, gC + 100), (std::min)(255, bC + 100), 255),
                    24);

                continue;
            }

            // Старый режим — 3D box.
            Draw3DBox(drawList, ent.modelview, ent.projection, viewport, 0.25f, IM_COL32(255, 255, 255, 255), 2.0f);
            char primary[32], secondary[32];
            snprintf(primary, sizeof(primary), "Ammo");
            snprintf(secondary, sizeof(secondary), "%.0fm", dist);
            ImVec2 ps = ImGui::CalcTextSize(primary), ss = ImGui::CalcTextSize(secondary);
            float px = 6.f, py = 3.f, gap = 4.f;
            float pw = ps.x + px * 2, sw = ss.x + px * 2, rh = (std::max)(ps.y, ss.y) + py * 2;
            float sx = cx - (pw + gap + sw) * 0.5f, sy = minY - rh - 6.f;
            ImU32 bg = IM_COL32(10, 10, 10, 140), bd = IM_COL32(255, 255, 255, 210), tc = IM_COL32(255, 255, 255, 255);
            drawList->AddRectFilled({ sx,sy }, { sx + pw,sy + rh }, bg, 3.f);
            drawList->AddRect({ sx,sy }, { sx + pw,sy + rh }, bd, 3.f);
            drawList->AddText({ sx + px,sy + py }, tc, primary);
            drawList->AddRectFilled({ sx + pw + gap,sy }, { sx + pw + gap + sw,sy + rh }, bg, 3.f);
            drawList->AddRect({ sx + pw + gap,sy }, { sx + pw + gap + sw,sy + rh }, bd, 3.f);
            drawList->AddText({ sx + pw + gap + px,sy + py }, tc, secondary);
            continue;
        }

        if (!g_espEnabled && !g_tracerEnabled) continue;

        glm::vec3 camPos(g_listenerPosX, g_listenerPosY, g_listenerPosZ);
        glm::vec3 worldPos = camPos + ent.translatePos;
        float distance = glm::length(worldPos - camPos);
        bool isBot = (ent.scale.x > 0.4f && ent.scale.x < 0.5f &&
            ent.scale.y > 0.4f && ent.scale.y < 0.5f &&
            ent.scale.z > 0.4f && ent.scale.z < 0.5f);
        if (!(ent.isPlayer || ent.isBear || ent.isBot || isBot)) continue;

        ImU32 boxColor = isBot ? IM_COL32(0, 255, 255, 255)
            : ent.isPlayer ? IM_COL32(255, 255, 255, 255)
            : ent.isBear ? IM_COL32(200, 200, 200, 255)
            : IM_COL32(150, 150, 150, 255);

        // Color by visibility (depth-buffer wallcheck).
        if (g_espColorByVisibility && ent.isPlayer) {
            bool visible = WallCheckIsVisible(ent);
            const float* c = visible ? g_espVisibleColor : g_espHiddenColor;
            boxColor = IM_COL32(
                (int)(c[0] * 255),
                (int)(c[1] * 255),
                (int)(c[2] * 255),
                (int)(c[3] * 255));
        }

        // ── Трейсер ───────────────────────────────────────────────────
        if (g_tracerEnabled) {
            ImU32 col = IM_COL32(
                (int)(g_tracerColor[0] * 255),
                (int)(g_tracerColor[1] * 255),
                (int)(g_tracerColor[2] * 255),
                (int)(g_tracerColor[3] * 255));
            DrawTracer(drawList, ent.modelview, ent.projection, viewport, col, g_tracerThickness);
        }

        if (!g_espEnabled && !g_tracerEnabled) continue;

        // ── ESP бокс (только если на экране) ─────────────────────────
        float s = (ent.scale.x != 0.0f) ? ent.scale.x : 1.0f;
        // modelview захватывается ДО glScale → масштаб не включён в матрицу.
        // Константы 0.15/-1.8 откалиброваны для игрока (scale=0.9375).
        // Формула: model_coord * entity_scale, где model_coord = ref / 0.9375
        float boxScale = s / 0.9375f;
        glm::vec2 bottomScreen, topScreen;
        ProjectToScreenOffscreen(glm::vec3(0.0f, 0.15f * boxScale, 0.0f), ent.modelview, ent.projection, viewport, bottomScreen);
        ProjectToScreenOffscreen(glm::vec3(0.0f, -1.8f * boxScale, 0.0f), ent.modelview, ent.projection, viewport, topScreen);

        float h = std::abs(topScreen.y - bottomScreen.y);
        float w = h / 2.0f;
        float cx = (bottomScreen.x + topScreen.x) * 0.5f;
        float minX = cx - w / 2.f, maxX = cx + w / 2.f;
        float minY = (std::min)(bottomScreen.y, topScreen.y);
        float maxY = (std::max)(bottomScreen.y, topScreen.y);

        bool onScreen = (maxX > 0 && minX < screenW && maxY > 0 && minY < screenH && h >= 4.f && w >= 2.f);

        // Если игрок за спиной (z > 0), он гарантированно не на экране
        bool isBehind = ent.modelview[3][2] > 0;

        if (g_espArrows && (!onScreen || isBehind)) {
            // Используем мировые координаты для стрелок
            glm::vec3 targetWorld = ent.hasWorldPos ? ent.worldPos : (ent.capturedListenerPos + ent.translatePos);
            glm::vec3 cameraWorld = ent.capturedListenerPos;
            DrawOffscreenArrow(drawList, targetWorld, cameraWorld, g_espArrowsRadius, g_espArrowsSize, boxColor);
        }

        if (!onScreen || isBehind) continue;

        float hi = w * 0.12f, vi = h * 0.04f;
        minX += hi; maxX -= hi; minY += vi; maxY -= vi;

        drawList->AddRect({ minX - 0.75f,minY - 0.75f }, { maxX + 0.75f,maxY + 0.75f }, IM_COL32(20, 20, 20, 242));
        drawList->AddRect({ minX,      minY }, { maxX,      maxY }, boxColor);
        drawList->AddRect({ minX + 0.75f,minY + 0.75f }, { maxX - 0.75f,maxY - 0.75f }, IM_COL32(30, 30, 30, 216));

        char primary[64], secondary[32];
        if (isBot || ent.isBot) snprintf(primary, sizeof(primary), "Bot");
        else if (ent.isPlayer)  snprintf(primary, sizeof(primary), "Player");
        else                    snprintf(primary, sizeof(primary), "Bear");
        snprintf(secondary, sizeof(secondary), "%.0fm", distance);

        ImVec2 ps = ImGui::CalcTextSize(primary), ss = ImGui::CalcTextSize(secondary);
        float px = 6.f, py = 3.f, gap = 4.f;
        float pw = ps.x + px * 2, sw = ss.x + px * 2, rh = (std::max)(ps.y, ss.y) + py * 2;
        float sx = cx - (pw + gap + sw) * 0.5f, sy = minY - rh - 6.f;
        ImU32 bg = IM_COL32(10, 10, 10, 140), bd = IM_COL32(255, 255, 255, 210), tc = IM_COL32(255, 255, 255, 255);
        drawList->AddRectFilled({ sx,sy }, { sx + pw,sy + rh }, bg, 3.f);
        drawList->AddRect({ sx,sy }, { sx + pw,sy + rh }, bd, 3.f);
        drawList->AddText({ sx + px,sy + py }, tc, primary);
        drawList->AddRectFilled({ sx + pw + gap,sy }, { sx + pw + gap + sw,sy + rh }, bg, 3.f);
        drawList->AddRect({ sx + pw + gap,sy }, { sx + pw + gap + sw,sy + rh }, bd, 3.f);
        drawList->AddText({ sx + pw + gap + px,sy + py }, tc, secondary);
    }

    // ── Breadcrumbs ───────────────────────────────────────────────────
    if (g_espBreadcrumbs) {
        std::lock_guard<std::mutex> lockCb(g_cachedESPBoxesMutex);

        // Обновляем breadcrumbs на основе текущих игроков
        for (auto& cb : g_cachedESPBoxes) cb.isPlayer = false;
        for (const auto& ent : g_entities) {
            if (!ent.isPlayer) continue;
            glm::vec3 camPos(g_listenerPosX, g_listenerPosY, g_listenerPosZ);
            glm::vec3 wp = camPos + ent.translatePos;
            bool found = false;
            for (auto& cb : g_cachedESPBoxes) {
                if (glm::distance(cb.worldPos, wp) < 2.f) {
                    cb.worldPos = wp; cb.lastSeen = currentTime; cb.isPlayer = true; found = true; break;
                }
            }
            if (!found) {
                CachedESPBox nb{}; nb.worldPos = wp; nb.lastSeen = currentTime; nb.isPlayer = true;
                g_cachedESPBoxes.push_back(nb);
            }
        }

        // Рендерим ghost boxes для позиций где игроков сейчас нет
        glm::vec3 camPos(g_listenerPosX, g_listenerPosY, g_listenerPosZ);

        for (const auto& cb : g_cachedESPBoxes) {
            // Показываем только те позиции где игрока сейчас нет
            if (cb.isPlayer) continue;

            // Проверяем что позиция была видна недавно (используем g_espBreadcrumbsTime)
            double timeSinceLastSeen = currentTime - cb.lastSeen;
            if (timeSinceLastSeen > g_espBreadcrumbsTime) continue;

            // Вычисляем расстояние
            float distance = glm::length(cb.worldPos - camPos);

            // Создаем временную матрицу для ghost box
            glm::vec3 relativePos = cb.worldPos - camPos;
            glm::mat4 ghostModelview = g_cameraMatrix;
            ghostModelview[3] = glm::vec4(glm::mat3(g_cameraMatrix) * relativePos, 1.0f);

            // Используем projection из первой сущности
            if (g_entities.empty()) continue;
            const glm::mat4& projection = g_entities[0].projection;

            // Проверяем что позиция перед камерой (не за спиной)
            glm::vec4 clipPos = projection * ghostModelview * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            if (clipPos.w <= 0.0f) continue; // За камерой - не рендерим

            // Проекция на экран
            glm::vec2 bottomScreen, topScreen;
            if (!ProjectToScreen(glm::vec3(0.0f, 0.15f, 0.0f), ghostModelview, projection, viewport, bottomScreen)) continue;
            if (!ProjectToScreen(glm::vec3(0.0f, -1.8f, 0.0f), ghostModelview, projection, viewport, topScreen)) continue;

            float h = std::abs(topScreen.y - bottomScreen.y);
            float w = h / 2.0f;
            if (w < 2.0f || h < 4.0f) continue;

            float cx = (bottomScreen.x + topScreen.x) * 0.5f;
            float minX = cx - w / 2.f, maxX = cx + w / 2.f;
            float minY = (std::min)(bottomScreen.y, topScreen.y);
            float maxY = (std::max)(bottomScreen.y, topScreen.y);

            bool onScreen = (maxX > 0 && minX < screenW && maxY > 0 && minY < screenH && h >= 4.f && w >= 2.f);
            if (!onScreen) continue;

            float hi = w * 0.12f, vi = h * 0.04f;
            minX += hi; maxX -= hi; minY += vi; maxY -= vi;

            // Темно-желтый цвет с черной обводкой (как обычный бокс)
            ImU32 ghostColor = IM_COL32(180, 180, 0, 255); // Темно-желтый

            // Рисуем ghost box (3 линии как у обычного бокса)
            drawList->AddRect({ minX - 0.75f,minY - 0.75f }, { maxX + 0.75f,maxY + 0.75f }, IM_COL32(20, 20, 20, 242));
            drawList->AddRect({ minX,      minY }, { maxX,      maxY }, ghostColor);
            drawList->AddRect({ minX + 0.75f,minY + 0.75f }, { maxX - 0.75f,maxY - 0.75f }, IM_COL32(30, 30, 30, 216));

            // Текст "Ghost" и расстояние (как у обычного игрока)
            char primary[64], secondary[32];
            snprintf(primary, sizeof(primary), "Ghost");
            snprintf(secondary, sizeof(secondary), "%.0fm", distance);

            ImVec2 ps = ImGui::CalcTextSize(primary), ss = ImGui::CalcTextSize(secondary);
            float px = 6.f, py = 3.f, gap = 4.f;
            float pw = ps.x + px * 2, sw = ss.x + px * 2, rh = (std::max)(ps.y, ss.y) + py * 2;
            float sx = cx - (pw + gap + sw) * 0.5f, sy = minY - rh - 6.f;
            ImU32 bg = IM_COL32(10, 10, 10, 140), bd = IM_COL32(255, 255, 255, 210), tc = IM_COL32(255, 255, 255, 255);

            drawList->AddRectFilled({ sx,sy }, { sx + pw,sy + rh }, bg, 3.f);
            drawList->AddRect({ sx,sy }, { sx + pw,sy + rh }, bd, 3.f);
            drawList->AddText({ sx + px,sy + py }, tc, primary);
            drawList->AddRectFilled({ sx + pw + gap,sy }, { sx + pw + gap + sw,sy + rh }, bg, 3.f);
            drawList->AddRect({ sx + pw + gap,sy }, { sx + pw + gap + sw,sy + rh }, bd, 3.f);
            drawList->AddText({ sx + pw + gap + px,sy + py }, tc, secondary);
        }
    }
}

void RenderSoundESP() {
    if (!g_soundEspEnabled) return;
    std::lock_guard<std::mutex> lockEnt(g_entitiesMutex);
    if (g_entities.empty()) return;
    const auto& refEnt = g_entities[0];
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
    glm::vec3 listenerPos(g_listenerPosX, g_listenerPosY, g_listenerPosZ);
    std::lock_guard<std::mutex> lockSnd(g_soundsMutex);
    ULONGLONG now = GetTickCount64();
    auto* drawList = ImGui::GetBackgroundDrawList();
    for (auto it = g_sounds.begin(); it != g_sounds.end(); ) {
        if (now - it->time > (ULONGLONG)(g_soundEspDuration * 1000.f)) { it = g_sounds.erase(it); continue; }
        glm::vec3 dir = it->pos - listenerPos;
        glm::mat4 mv = g_cameraMatrix;
        mv[3] = glm::vec4(glm::mat3(g_cameraMatrix) * dir, 1.f);
        glm::vec2 sp;
        if (ProjectToScreen(glm::vec3(0.f), mv, refEnt.projection, viewport, sp)) {
            float t = (float)(now - it->time) / 1000.f;
            float alpha = 1.f - (t / g_soundEspDuration);
            if (alpha < 0.f) alpha = 0.f;
            if (g_soundEspVisual) {
                float maxR = 40.f;
                float r1 = fmodf(t * 30.f, maxR), r2 = fmodf(t * 30.f + maxR * 0.5f, maxR);
                drawList->AddCircleFilled({ sp.x,sp.y }, 4.f, IM_COL32(255, 255, 255, (int)(alpha * 255)));
                drawList->AddCircle({ sp.x,sp.y }, r1, IM_COL32(200, 200, 200, (int)(alpha * (1.f - r1 / maxR) * 255)), 0, 2.f);
                drawList->AddCircle({ sp.x,sp.y }, r2, IM_COL32(200, 200, 200, (int)(alpha * (1.f - r2 / maxR) * 255)), 0, 2.f);
            }
            char label[160];
            float dist = glm::length(it->pos - listenerPos);
            snprintf(label, sizeof(label), "sound [%.0fm]", dist);
            ImVec2 ls = ImGui::CalcTextSize(label);
            drawList->AddText({ sp.x - ls.x * 0.5f, sp.y + 8.f }, IM_COL32(255, 255, 255, (int)(alpha * 255)), label);
        }
        ++it;
    }
}


void RenderOpenALListenerInfo() {
    if (!g_openalEspEnabled) return;
    auto* drawList = ImGui::GetBackgroundDrawList();
    char text[128];
    snprintf(text, sizeof(text), "X=%.1f Y=%.1f Z=%.1f", g_listenerPosX, g_listenerPosY - 1.6f, g_listenerPosZ);
    ImVec2 ts = ImGui::CalcTextSize(text);
    float px = 8.f, py = 4.f;
    ImVec2 bgMin(10.f, ImGui::GetIO().DisplaySize.y - ts.y - py * 2 - 10.f);
    ImVec2 bgMax(10.f + ts.x + px * 2, bgMin.y + ts.y + py * 2);
    drawList->AddRectFilled(bgMin, bgMax, IM_COL32(10, 10, 10, 180), 4.f);
    drawList->AddRect(bgMin, bgMax, IM_COL32(200, 200, 200, 200), 4.f);
    drawList->AddText({ bgMin.x + px, bgMin.y + py }, IM_COL32(230, 230, 230, 255), text);
}
