#include "pch.h"

#include "imgui_layer.h"

#include <algorithm>

#include "directX.h"
#include "third_party/imgui/backends/imgui_impl_dx11.h"
#include "third_party/imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    bool g_imguiInitialized = false;
    bool g_imguiWin32Initialized = false;
    bool g_imguiDx11Initialized = false;
    int g_prevMouseButtons = 0;

    void SubmitDxLibMouseStateToImGui()
    {
        ImGuiIO& io = ImGui::GetIO();

        int mouseX = 0;
        int mouseY = 0;
        GetMousePoint(&mouseX, &mouseY);
        io.AddMousePosEvent(static_cast<float>(mouseX), static_cast<float>(mouseY));

        const int mouseButtons = GetMouseInput();
        const bool leftDown = (mouseButtons & MOUSE_INPUT_LEFT) != 0;
        const bool rightDown = (mouseButtons & MOUSE_INPUT_RIGHT) != 0;
        const bool middleDown = (mouseButtons & MOUSE_INPUT_MIDDLE) != 0;

        const bool prevLeftDown = (g_prevMouseButtons & MOUSE_INPUT_LEFT) != 0;
        const bool prevRightDown = (g_prevMouseButtons & MOUSE_INPUT_RIGHT) != 0;
        const bool prevMiddleDown = (g_prevMouseButtons & MOUSE_INPUT_MIDDLE) != 0;

        if (leftDown != prevLeftDown)
        {
            io.AddMouseButtonEvent(0, leftDown);
        }
        if (rightDown != prevRightDown)
        {
            io.AddMouseButtonEvent(1, rightDown);
        }
        if (middleDown != prevMiddleDown)
        {
            io.AddMouseButtonEvent(2, middleDown);
        }

        g_prevMouseButtons = mouseButtons;

        const int wheelDelta = GetMouseWheelRotVol();
        if (wheelDelta != 0)
        {
            io.AddMouseWheelEvent(0.0f, static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA));
        }
    }

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

        // ImGui supplies the HUD layout values; DxLib draws the overlay into the current render target.
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
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(kVirtualScreenWidth), static_cast<float>(kVirtualScreenHeight));
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    g_imguiWin32Initialized = ImGui_ImplWin32_Init(hWnd);
    g_imguiDx11Initialized = false;
    if (g_imguiWin32Initialized)
    {
        auto* d3dDevice = static_cast<ID3D11Device*>(device);
        auto* d3dContext = static_cast<ID3D11DeviceContext*>(context);
        if (d3dDevice != nullptr && d3dContext != nullptr)
        {
            g_imguiDx11Initialized = ImGui_ImplDX11_Init(d3dDevice, d3dContext);
        }
    }

    if (!g_imguiWin32Initialized || !g_imguiDx11Initialized)
    {
        if (g_imguiDx11Initialized)
        {
            ImGui_ImplDX11_Shutdown();
            g_imguiDx11Initialized = false;
        }
        if (g_imguiWin32Initialized)
        {
            ImGui_ImplWin32_Shutdown();
            g_imguiWin32Initialized = false;
        }
        ImGui::DestroyContext();
        g_imguiInitialized = false;
        return false;
    }

    ImGui_ImplDX11_CreateDeviceObjects();
    g_imguiInitialized = true;
    return true;
}

void ImGuiLayer_Shutdown()
{
    if (!g_imguiInitialized)
    {
        return;
    }

    if (g_imguiDx11Initialized)
    {
        ImGui_ImplDX11_Shutdown();
        g_imguiDx11Initialized = false;
    }
    if (g_imguiWin32Initialized)
    {
        ImGui_ImplWin32_Shutdown();
        g_imguiWin32Initialized = false;
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
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    SubmitDxLibMouseStateToImGui();
    ImGui::NewFrame();
}

void ImGuiLayer_EndFrame()
{
    if (!g_imguiInitialized)
    {
        return;
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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
    if (!g_imguiInitialized || !g_imguiWin32Initialized)
    {
        return 0;
    }

    return ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
}

bool ImGuiLayer_WantsCaptureMouse()
{
    if (!g_imguiInitialized || ImGui::GetCurrentContext() == nullptr)
    {
        return false;
    }

    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer_WantsCaptureKeyboard()
{
    if (!g_imguiInitialized || ImGui::GetCurrentContext() == nullptr)
    {
        return false;
    }

    return ImGui::GetIO().WantCaptureKeyboard;
}
