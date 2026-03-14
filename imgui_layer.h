#pragma once

#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

bool ImGuiLayer_Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* context);
void ImGuiLayer_Shutdown();
void ImGuiLayer_BeginFrame();
void ImGuiLayer_EndFrame();
void ImGuiLayer_DrawFoundationWindow(float fps);
LRESULT ImGuiLayer_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
