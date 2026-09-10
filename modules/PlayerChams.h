#pragma once
#include "../core/Globals.h"

// Player Chams — рисует шейдерный эффект (как у Chams Hand) на голове игроков.

extern bool  g_playerChamsEnabled;
extern float g_playerChamsColorVis[4];
extern float g_playerChamsColorHid[4];
extern bool  g_playerChamsXray;
extern int   g_playerChamsMode;            // 0=Metallic, 1=Rainbow, 2=Galaxy, 3=Neon, 4=Lava, 5=Ice...
extern float g_playerChamsSpeed;

// Хуки скейла. Вызываются из hooked_glScalef.
bool PlayerChamsOnScale(GLfloat x, GLfloat y, GLfloat z);

// Сбрасывается каждый кадр.
void PlayerChamsFrameReset();

// Уведомления о PushMatrix/PopMatrix внутри рендера игрока.
void PlayerChamsOnPushMatrix();
void PlayerChamsOnPopMatrix();

// Завершает шейдер если он активен (для аварийного выхода).
void PlayerChamsForceEnd();
