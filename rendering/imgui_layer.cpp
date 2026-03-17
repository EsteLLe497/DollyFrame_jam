#include "imgui_layer.h"

bool ImGuiLayer_Initialize(HWND hWnd, void* device, void* context)
{
    (void)hWnd;
    (void)device;
    (void)context;
    return true;
}

void ImGuiLayer_Shutdown()
{
}

void ImGuiLayer_BeginFrame()
{
}

void ImGuiLayer_EndFrame()
{
}

void ImGuiLayer_DrawFoundationWindow(float fps)
{
    (void)fps;
}

LRESULT ImGuiLayer_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;
    (void)message;
    (void)wParam;
    (void)lParam;
    return 0;
}
