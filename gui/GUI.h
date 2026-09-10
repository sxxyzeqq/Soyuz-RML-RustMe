#pragma once
#include "../core/Globals.h"

void InitImGui(HDC hdc);
void ShutdownImGui();
void RenderGUI();
void RenderWatermark(ImDrawList* drawList);
void RenderScopeInfo(ImDrawList* drawList);
void RenderModuleList(ImDrawList* drawList);
void RenderKeyBinds(ImDrawList* drawList);
void RenderCrosshair(ImDrawList* drawList);
void RenderAimFOV();
LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace GUI {
    bool GetInitializationState();
    bool GetDrawState();
    void SetDrawState(bool new_value);

}
