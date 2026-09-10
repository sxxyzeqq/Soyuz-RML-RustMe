#pragma once
#include <string>

void License_Init();
bool License_PreloadCredentials();
bool License_IsValid();
std::string License_GetLogin();
std::string License_GetUid();
