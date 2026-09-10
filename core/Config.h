#pragma once
#include "Globals.h"

void SaveConfig();
void LoadConfig();
void UpdateConfigFromVars();
void RefreshProfiles();
const char* GetKeyName(int vk);
std::string GetProfilePath(int index = 0);
