#pragma once
#include "../core/Globals.h"

extern bool g_chamsHandEnabled;
extern float g_chamsHandColorVis[4];
extern float g_chamsHandColorHid[4];
extern bool g_chamsHandXray;
extern int g_chamsHandMode; // 0=Metallic, 1=Rainbow, 2=Galaxy, 3=Neon, 4=Lava, 5=Ice, 6=Water, 7=Fire, 8=Hologram, 9=Crystal, 10=Toxic, 11=Glitch, 12=Electric, 13=Gold, 14=Ruby, 15=Obsidian
extern float g_chamsHandSpeed;

void ChamsHandInit();
void ChamsHandBegin();
void ChamsHandEnd();
void ChamsHandShutdown();
void ChamsEntityBegin();
void ChamsEntityEnd();

// Generic shader API for reuse by other chams modules (e.g. PlayerChams)
void ChamsShaderBegin(float r, float g, float b, int mode, float time);
void ChamsShaderEnd();
