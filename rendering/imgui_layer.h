#pragma once

#include <windows.h>

bool ImGuiLayer_Initialize(HWND hWnd, void* device, void* context);
void ImGuiLayer_Shutdown();
void ImGuiLayer_BeginFrame();
void ImGuiLayer_EndFrame();
void ImGuiLayer_DrawFoundationWindow(float fps);
LRESULT ImGuiLayer_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool ImGuiLayer_WantsCaptureMouse();
bool ImGuiLayer_WantsCaptureKeyboard();
