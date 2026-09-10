#include "WallCheck.h"
#include <chrono>
#include <algorithm>

bool  g_aimVisibleOnly         = false;
bool  g_espColorByVisibility   = false;
float g_espVisibleColor[4]     = { 0.2f, 1.0f, 0.2f, 1.0f };
float g_espHiddenColor[4]      = { 1.0f, 0.2f, 0.2f, 1.0f };

// Точки на модели игрока: 12 равномерных от макушки (-2.0) до ступней (0.0).
// Этого достаточно для устойчивости — больше точек практически не улучшают точность,
// но сильно бьют по fps (каждая точка = синхронный glReadPixels).
static constexpr int kNumPoints = 12;
static float kPointsY[kNumPoints];
static bool  kPointsInit = false;

static void InitPoints() {
    if (kPointsInit) return;
    for (int i = 0; i < kNumPoints; i++) {
        kPointsY[i] = -2.0f + (2.0f * i) / (float)(kNumPoints - 1);
    }
    kPointsInit = true;
}

// Конвертация window-depth [0..1] в eye-space distance (расстояние в блоках от камеры).
static float DepthToWorldDistance(float depth, const glm::mat4& proj) {
    float zNdc = depth * 2.0f - 1.0f;
    float A = proj[2][2];
    float B = proj[3][2];
    float zEye = B / (-zNdc - A);
    return -zEye;
}

static bool ProjectAndSample(const glm::mat4& modelview, const glm::mat4& projection,
                             float localY, GLint vp[4],
                             float& outBufDist, float& outTargetDist) {
    glm::vec4 clip = projection * modelview * glm::vec4(0.0f, localY, 0.0f, 1.0f);
    if (clip.w <= 0.001f) return false;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) return false;

    int sx = (int)((ndc.x * 0.5f + 0.5f) * vp[2]) + vp[0];
    int sy = (int)((ndc.y * 0.5f + 0.5f) * vp[3]) + vp[1];

    if (sx < vp[0] + 2 || sx > vp[0] + vp[2] - 3) return false;
    if (sy < vp[1] + 2 || sy > vp[1] + vp[3] - 3) return false;

    float targetDepth = ndc.z * 0.5f + 0.5f;
    outTargetDist = DepthToWorldDistance(targetDepth, projection);

    // 5x5 сэмплинг — компромисс точности и fps.
    const int R = 2;
    const int N = (R*2+1) * (R*2+1);
    float buf[N] = {};
    glReadPixels(sx - R, sy - R, R*2+1, R*2+1, GL_DEPTH_COMPONENT, GL_FLOAT, buf);

    float dists[N];
    int n = 0;
    bool hasSky = false;
    for (int i = 0; i < N; i++) {
        if (buf[i] >= 0.999f) { hasSky = true; continue; }
        float d = DepthToWorldDistance(buf[i], projection);
        if (d <= 0.5f) continue;
        dists[n++] = d;
    }

    if (hasSky) { outBufDist = 99999.0f; return true; }
    if (n == 0) { outBufDist = 99999.0f; return true; }

    float minD = dists[0], maxD = dists[0];
    for (int i = 1; i < n; i++) {
        if (dists[i] < minD) minD = dists[i];
        if (dists[i] > maxD) maxD = dists[i];
    }

    // Ветка "max" — для тонкой растительности с явными просветами:
    // абсолютный разброс 1.5..5 блоков (типично для травы/листьев).
    // На больших дистанциях стены тоже могут давать большой разброс,
    // поэтому ограничиваем сверху чтобы не путать стену с растительностью.
    float spread = maxD - minD;
    if (spread > 1.5f && spread < 5.0f) {
        outBufDist = maxD;
        return true;
    }

    // Иначе — медиана (плотная поверхность).
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (dists[j] < dists[i]) { float t = dists[i]; dists[i] = dists[j]; dists[j] = t; }
        }
    }
    outBufDist = dists[n / 2];
    return true;
}

static bool ComputeVisibility(EntityInfo& ent) {
    InitPoints();
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    GLint prevReadBuffer = 0;
    glGetIntegerv(GL_READ_BUFFER, &prevReadBuffer);
    glReadBuffer(GL_BACK);

    int visibleHits = 0;
    int neededHits = 2;        // достаточно 2 точек чтобы считать "видим"
    for (int i = 0; i < kNumPoints; i++) {
        float bufD = 0.0f, tgtD = 0.0f;
        if (!ProjectAndSample(ent.modelview, ent.projection, kPointsY[i], vp, bufD, tgtD)) continue;

        if (bufD + 0.5f >= tgtD) {
            visibleHits++;
            if (visibleHits >= neededHits) break; // early-exit — не считаем зря
        }
    }
    ent.wallCheckRawHits = visibleHits;

    if (prevReadBuffer != 0) glReadBuffer((GLenum)prevReadBuffer);

    // Сырое решение по 2 успешным точкам.
    return (visibleHits >= 2);
}

// Трекинг "стабильной" видимости каждого игрока между кадрами.
// Мы держим счётчик подряд-VIS кадров и подряд-HID кадров.
// Переключение происходит только когда новое состояние подтверждено N кадрами подряд.
struct VisTrack {
    glm::vec3 lastPos;
    int  visStreak = 0;
    int  hidStreak = 0;
    bool stableVisible = true;
    double lastSeen = 0.0;
    double lastChecked = 0.0;
};
static std::vector<VisTrack> g_visTracks;

static const int    kStreakToFlip = 2;     // сколько подряд кадров для переключения
static const double kCheckInterval = 0.06; // интервал между чеками одного игрока (сек)

static VisTrack* FindOrCreateTrack(const glm::vec3& worldPos, double now) {
    VisTrack* best = nullptr;
    float bestD = 2.5f; // блока — допуск на сопоставление трека
    for (auto& t : g_visTracks) {
        float d = glm::distance(t.lastPos, worldPos);
        if (d < bestD) { bestD = d; best = &t; }
    }
    if (best) return best;

    g_visTracks.push_back({});
    VisTrack* nt = &g_visTracks.back();
    nt->lastPos = worldPos;
    nt->lastSeen = now;
    return nt;
}

static void PruneTracks(double now) {
    // Удаляем треки которые не обновлялись больше 1 секунды.
    g_visTracks.erase(
        std::remove_if(g_visTracks.begin(), g_visTracks.end(),
            [now](const VisTrack& t) { return (now - t.lastSeen) > 1.0; }),
        g_visTracks.end());
}

void WallCheckProcessEntities() {
    // Активируем только если кому-то нужно.
    if (!g_aimVisibleOnly && !g_espColorByVisibility) return;

    std::lock_guard<std::mutex> lock(g_entitiesMutex);
    if (g_entities.empty()) return;

    double now = std::chrono::duration_cast<std::chrono::duration<double>>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();

    PruneTracks(now);

    for (auto& ent : g_entities) {
        if (ent.wallCheckDone) continue;
        if (!ent.isPlayer) {
            ent.wallCheckDone = true;
            ent.visibleFromCamera = true;
            continue;
        }

        glm::vec3 wp = glm::vec3(ent.modelview[3]);
        VisTrack* tr = FindOrCreateTrack(wp, now);
        tr->lastPos = wp;
        tr->lastSeen = now;

        // Throttle: чек тяжёлый, делаем не каждый кадр.
        if (now - tr->lastChecked < kCheckInterval) {
            ent.visibleFromCamera = tr->stableVisible;
            ent.wallCheckDone = true;
            continue;
        }
        tr->lastChecked = now;

        bool rawVisible = ComputeVisibility(ent);

        if (rawVisible) {
            tr->visStreak++;
            tr->hidStreak = 0;
            if (tr->visStreak >= kStreakToFlip) tr->stableVisible = true;
        } else {
            tr->hidStreak++;
            tr->visStreak = 0;
            if (tr->hidStreak >= kStreakToFlip) tr->stableVisible = false;
        }

        ent.visibleFromCamera = tr->stableVisible;
        ent.wallCheckDone = true;
    }
}

bool WallCheckIsVisible(const EntityInfo& ent) {
    if (!ent.wallCheckDone) return true;
    return ent.visibleFromCamera;
}
