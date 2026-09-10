#pragma once
#include "../core/Globals.h"

bool InitOpenAL();
void UpdateOpenALListenerPos();
void ShutdownFileHooks();
void __cdecl hooked_alSourcePlay(ALuint source);
void InitOpenALSoundHook();
void ShutdownOpenALSoundHook();

extern alSourcePlay_fn p_alSourcePlay;
