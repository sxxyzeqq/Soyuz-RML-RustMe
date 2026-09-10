#pragma once
#include "../core/Globals.h"

// Внутренний модуль видимости через depth-buffer. Сам по себе настроек не имеет —
// активируется автоматически когда нужны g_aimVisibleOnly или g_espColorByVisibility.

extern bool  g_aimVisibleOnly;
extern bool  g_espColorByVisibility;
extern float g_espVisibleColor[4];
extern float g_espHiddenColor[4];

// Должно вызываться строго до очистки depth-буфера в кадре (из glClear с DEPTH_BIT).
void WallCheckProcessEntities();

// Кэшированный результат для сущности.
bool WallCheckIsVisible(const EntityInfo& ent);
