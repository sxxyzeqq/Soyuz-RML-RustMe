#include "PlayerChams.h"
#include "ChamsHand.h"
#include <cmath>

bool  g_playerChamsEnabled  = false;
float g_playerChamsColorVis[4] = { 1.0f, 0.2f, 0.2f, 1.0f };
float g_playerChamsColorHid[4] = { 0.5f, 0.0f, 0.0f, 1.0f };
bool  g_playerChamsXray     = false;
int   g_playerChamsMode     = 1; // Rainbow по умолчанию
float g_playerChamsSpeed    = 1.0f;

// --- Состояние внутри одного "рендера игрока" ---
// Когда glScalef(0.9375) пришёл — Minecraft начинает рендер модели игрока.
// Внутри этого рендера каждая часть тела окружена парой push/pop matrix.
// Первый push после старта = голова. Включаем шейдер строго для первой части.
static bool s_inPlayerRender = false;
static int  s_pushDepth      = 0;
static int  s_partIndex      = 0;
static bool s_shaderActive   = false;
static float s_shaderTime    = 0.0f;

static void BeginShader() {
    if (s_shaderActive) return;
    
    if (g_playerChamsXray) {
        glDisable(GL_DEPTH_TEST);
        ChamsShaderBegin(g_playerChamsColorHid[0], g_playerChamsColorHid[1], g_playerChamsColorHid[2], g_playerChamsMode, s_shaderTime);
    } else {
        ChamsShaderBegin(g_playerChamsColorVis[0], g_playerChamsColorVis[1], g_playerChamsColorVis[2], g_playerChamsMode, s_shaderTime);
    }
    s_shaderActive = true;
}

static void EndShader() {
    if (!s_shaderActive) return;
    ChamsShaderEnd();
    if (g_playerChamsXray) glEnable(GL_DEPTH_TEST);
    s_shaderActive = false;
}

void PlayerChamsFrameReset() {
    EndShader();
    s_inPlayerRender = false;
    s_pushDepth      = 0;
    s_partIndex      = 0;
    s_shaderTime    += 0.016f * g_playerChamsSpeed;
}

void PlayerChamsForceEnd() {
    EndShader();
    s_inPlayerRender = false;
    s_pushDepth      = 0;
    s_partIndex      = 0;
}

bool PlayerChamsOnScale(GLfloat x, GLfloat y, GLfloat z) {
    if (!g_playerChamsEnabled) return false;

    // Скейл рендера игрока в Minecraft — 0.9375f во всех осях.
    bool isPlayerScale = (x == 0.9375f && y == 0.9375f && z == 0.9375f);
    if (!isPlayerScale) return false;

    // Старт нового рендера игрока. Сбросим состояние и приготовимся ловить голову.
    EndShader();
    s_inPlayerRender = true;
    s_pushDepth      = 0;
    s_partIndex      = 0;
    return true;
}

void PlayerChamsOnPushMatrix() {
    if (!s_inPlayerRender) return;

    s_pushDepth++;

    // Первый push на глубине 1 — это начало рендера body part.
    if (s_pushDepth == 1) {
        if (s_partIndex == 0) {
            BeginShader();
        } else {
            EndShader();
        }
    }
}

void PlayerChamsOnPopMatrix() {
    if (!s_inPlayerRender) return;
    if (s_pushDepth <= 0) return;

    if (s_pushDepth == 1) {
        // Закончилась первая часть тела (голова) — выключаем шейдер.
        if (s_partIndex == 0 && s_shaderActive) {
            EndShader();
        }
        s_partIndex++;
    }

    s_pushDepth--;
}
