#include "pch.h"

#include "imgui_layer.h"

#include <algorithm>

#include "directX.h"

namespace
{
    bool g_imguiInitialized = false;

    void DrawFpsOverlay(float fps)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const ImGuiStyle& style = ImGui::GetStyle();
        const float paddingX = std::max(8.0f, style.WindowPadding.x);
        const float paddingY = std::max(6.0f, style.WindowPadding.y);
        const float width = 100.0f;
        const float height = 18.0f + paddingY * 2.0f;
        const float x = 24.0f;
        const float y = io.DisplaySize.y * 0.5f - height * 0.5f;

        // ImGui supplies the HUD layout values; DxLib draws because no ImGui renderer backend is wired yet.
        const int left = static_cast<int>(std::round(x));
        const int top = static_cast<int>(std::round(y));
        const int right = static_cast<int>(std::round(x + width));
        const int bottom = static_cast<int>(std::round(y + height));
        const int textX = static_cast<int>(std::round(x + paddingX));
        const int textY = static_cast<int>(std::round(y + paddingY));

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
        DrawBox(left, top, right, bottom, GetColor(8, 10, 14), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawBox(left, top, right, bottom, GetColor(80, 90, 110), FALSE);
        DrawFormatString(textX, textY, GetColor(235, 242, 255), "FPS %.0f", fps);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

bool ImGuiLayer_Initialize(HWND hWnd, void* device, void* context)
{
    (void)hWnd;
    (void)device;
    (void)context;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(kVirtualScreenWidth), static_cast<float>(kVirtualScreenHeight));
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    g_imguiInitialized = true;
    return true;
}

void ImGuiLayer_Shutdown()
{
    if (!g_imguiInitialized)
    {
        return;
    }

    ImGui::DestroyContext();
    g_imguiInitialized = false;
}

void ImGuiLayer_BeginFrame()
{
    if (!g_imguiInitialized)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(kVirtualScreenWidth), static_cast<float>(kVirtualScreenHeight));
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
}

void ImGuiLayer_EndFrame()
{
    if (!g_imguiInitialized)
    {
        return;
    }

    ImGui::Render();
}

void ImGuiLayer_DrawFoundationWindow(float fps)
{
    if (!g_imguiInitialized)
    {
        return;
    }

    DrawFpsOverlay(std::max(0.0f, fps));
}

LRESULT ImGuiLayer_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;
    (void)message;
    (void)wParam;
    (void)lParam;
    return 0;
}
