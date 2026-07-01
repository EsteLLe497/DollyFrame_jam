#include "pch.h"

#include "imgui_layer.h"

#include <algorithm>
#include <array>

#include "directX.h"
#include "third_party/imgui/backends/imgui_impl_dx11.h"
#include "third_party/imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    bool g_imguiInitialized = false;
    bool g_imguiWin32Initialized = false;
    bool g_imguiDx11Initialized = false;
    bool g_foundationOverlayVisible = true;
    int g_prevMouseButtons = 0;
    std::array<DX_CHAR, 256> g_prevKeyState{};

    struct DxImGuiKeyMapping
    {
        int dxKey = 0;
        ImGuiKey imguiKey = ImGuiKey_None;
    };

    constexpr std::array<DxImGuiKeyMapping, 78> kDxImGuiKeyMappings =
    {{
        { KEY_INPUT_TAB, ImGuiKey_Tab },
        { KEY_INPUT_LEFT, ImGuiKey_LeftArrow },
        { KEY_INPUT_RIGHT, ImGuiKey_RightArrow },
        { KEY_INPUT_UP, ImGuiKey_UpArrow },
        { KEY_INPUT_DOWN, ImGuiKey_DownArrow },
        { KEY_INPUT_PGUP, ImGuiKey_PageUp },
        { KEY_INPUT_PGDN, ImGuiKey_PageDown },
        { KEY_INPUT_HOME, ImGuiKey_Home },
        { KEY_INPUT_END, ImGuiKey_End },
        { KEY_INPUT_INSERT, ImGuiKey_Insert },
        { KEY_INPUT_DELETE, ImGuiKey_Delete },
        { KEY_INPUT_BACK, ImGuiKey_Backspace },
        { KEY_INPUT_SPACE, ImGuiKey_Space },
        { KEY_INPUT_RETURN, ImGuiKey_Enter },
        { KEY_INPUT_NUMPADENTER, ImGuiKey_KeypadEnter },
        { KEY_INPUT_ESCAPE, ImGuiKey_Escape },
        { KEY_INPUT_LCONTROL, ImGuiKey_LeftCtrl },
        { KEY_INPUT_RCONTROL, ImGuiKey_RightCtrl },
        { KEY_INPUT_LSHIFT, ImGuiKey_LeftShift },
        { KEY_INPUT_RSHIFT, ImGuiKey_RightShift },
        { KEY_INPUT_LALT, ImGuiKey_LeftAlt },
        { KEY_INPUT_RALT, ImGuiKey_RightAlt },
        { KEY_INPUT_LWIN, ImGuiKey_LeftSuper },
        { KEY_INPUT_RWIN, ImGuiKey_RightSuper },
        { KEY_INPUT_0, ImGuiKey_0 },
        { KEY_INPUT_1, ImGuiKey_1 },
        { KEY_INPUT_2, ImGuiKey_2 },
        { KEY_INPUT_3, ImGuiKey_3 },
        { KEY_INPUT_4, ImGuiKey_4 },
        { KEY_INPUT_5, ImGuiKey_5 },
        { KEY_INPUT_6, ImGuiKey_6 },
        { KEY_INPUT_7, ImGuiKey_7 },
        { KEY_INPUT_8, ImGuiKey_8 },
        { KEY_INPUT_9, ImGuiKey_9 },
        { KEY_INPUT_A, ImGuiKey_A },
        { KEY_INPUT_B, ImGuiKey_B },
        { KEY_INPUT_C, ImGuiKey_C },
        { KEY_INPUT_D, ImGuiKey_D },
        { KEY_INPUT_E, ImGuiKey_E },
        { KEY_INPUT_F, ImGuiKey_F },
        { KEY_INPUT_G, ImGuiKey_G },
        { KEY_INPUT_H, ImGuiKey_H },
        { KEY_INPUT_I, ImGuiKey_I },
        { KEY_INPUT_J, ImGuiKey_J },
        { KEY_INPUT_K, ImGuiKey_K },
        { KEY_INPUT_L, ImGuiKey_L },
        { KEY_INPUT_M, ImGuiKey_M },
        { KEY_INPUT_N, ImGuiKey_N },
        { KEY_INPUT_O, ImGuiKey_O },
        { KEY_INPUT_P, ImGuiKey_P },
        { KEY_INPUT_Q, ImGuiKey_Q },
        { KEY_INPUT_R, ImGuiKey_R },
        { KEY_INPUT_S, ImGuiKey_S },
        { KEY_INPUT_T, ImGuiKey_T },
        { KEY_INPUT_U, ImGuiKey_U },
        { KEY_INPUT_V, ImGuiKey_V },
        { KEY_INPUT_W, ImGuiKey_W },
        { KEY_INPUT_X, ImGuiKey_X },
        { KEY_INPUT_Y, ImGuiKey_Y },
        { KEY_INPUT_Z, ImGuiKey_Z },
        { KEY_INPUT_NUMPAD0, ImGuiKey_Keypad0 },
        { KEY_INPUT_NUMPAD1, ImGuiKey_Keypad1 },
        { KEY_INPUT_NUMPAD2, ImGuiKey_Keypad2 },
        { KEY_INPUT_NUMPAD3, ImGuiKey_Keypad3 },
        { KEY_INPUT_NUMPAD4, ImGuiKey_Keypad4 },
        { KEY_INPUT_NUMPAD5, ImGuiKey_Keypad5 },
        { KEY_INPUT_NUMPAD6, ImGuiKey_Keypad6 },
        { KEY_INPUT_NUMPAD7, ImGuiKey_Keypad7 },
        { KEY_INPUT_NUMPAD8, ImGuiKey_Keypad8 },
        { KEY_INPUT_NUMPAD9, ImGuiKey_Keypad9 },
        { KEY_INPUT_DECIMAL, ImGuiKey_KeypadDecimal },
        { KEY_INPUT_DIVIDE, ImGuiKey_KeypadDivide },
        { KEY_INPUT_MULTIPLY, ImGuiKey_KeypadMultiply },
        { KEY_INPUT_SUBTRACT, ImGuiKey_KeypadSubtract },
        { KEY_INPUT_ADD, ImGuiKey_KeypadAdd },
        { KEY_INPUT_MINUS, ImGuiKey_Minus },
        { KEY_INPUT_PERIOD, ImGuiKey_Period },
        { KEY_INPUT_COMMA, ImGuiKey_Comma },
    }};

    bool LoadJapaneseFont(ImGuiIO& io)
    {
        ImFontConfig config{};
        config.OversampleH = 3;
        config.OversampleV = 3;
        config.PixelSnapH = false;

        const ImWchar* ranges = io.Fonts->GetGlyphRangesJapanese();
        constexpr float kFontSize = 18.0f;

        if (ImFont* font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/NotoSansJP-Regular.otf", kFontSize, &config, ranges))
        {
            io.FontDefault = font;
            return true;
        }

        if (ImFont* font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", kFontSize, &config, ranges))
        {
            io.FontDefault = font;
            return true;
        }

        return false;
    }

    bool IsDxKeyDown(const std::array<DX_CHAR, 256>& keyState, int dxKey)
    {
        return dxKey >= 0 && dxKey < static_cast<int>(keyState.size()) && keyState[static_cast<size_t>(dxKey)] != 0;
    }

    bool WasDxKeyPressed(const std::array<DX_CHAR, 256>& keyState, int dxKey)
    {
        return IsDxKeyDown(keyState, dxKey) && !IsDxKeyDown(g_prevKeyState, dxKey);
    }

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

    void SubmitDxLibKeyboardStateToImGui()
    {
        ImGuiIO& io = ImGui::GetIO();

        std::array<DX_CHAR, 256> keyState{};
        GetHitKeyStateAll(keyState.data());

        for (const auto& mapping : kDxImGuiKeyMappings)
        {
            io.AddKeyEvent(mapping.imguiKey, IsDxKeyDown(keyState, mapping.dxKey));
        }

        const bool ctrlDown = IsDxKeyDown(keyState, KEY_INPUT_LCONTROL) || IsDxKeyDown(keyState, KEY_INPUT_RCONTROL);
        const bool shiftDown = IsDxKeyDown(keyState, KEY_INPUT_LSHIFT) || IsDxKeyDown(keyState, KEY_INPUT_RSHIFT);
        const bool altDown = IsDxKeyDown(keyState, KEY_INPUT_LALT) || IsDxKeyDown(keyState, KEY_INPUT_RALT);
        const bool superDown = IsDxKeyDown(keyState, KEY_INPUT_LWIN) || IsDxKeyDown(keyState, KEY_INPUT_RWIN);
        io.AddKeyEvent(ImGuiMod_Ctrl, ctrlDown);
        io.AddKeyEvent(ImGuiMod_Shift, shiftDown);
        io.AddKeyEvent(ImGuiMod_Alt, altDown);
        io.AddKeyEvent(ImGuiMod_Super, superDown);

        if (!ctrlDown && !altDown && !superDown)
        {
            for (int digit = 0; digit <= 9; ++digit)
            {
                const int rowKey = digit == 0 ? KEY_INPUT_0 : KEY_INPUT_1 + digit - 1;
                if (WasDxKeyPressed(keyState, rowKey))
                {
                    io.AddInputCharacter(static_cast<unsigned int>('0' + digit));
                }
            }

            constexpr std::array<int, 10> kNumpadDigitKeys =
            {{
                KEY_INPUT_NUMPAD0,
                KEY_INPUT_NUMPAD1,
                KEY_INPUT_NUMPAD2,
                KEY_INPUT_NUMPAD3,
                KEY_INPUT_NUMPAD4,
                KEY_INPUT_NUMPAD5,
                KEY_INPUT_NUMPAD6,
                KEY_INPUT_NUMPAD7,
                KEY_INPUT_NUMPAD8,
                KEY_INPUT_NUMPAD9,
            }};
            for (int digit = 0; digit <= 9; ++digit)
            {
                if (WasDxKeyPressed(keyState, kNumpadDigitKeys[static_cast<size_t>(digit)]))
                {
                    io.AddInputCharacter(static_cast<unsigned int>('0' + digit));
                }
            }

            if (WasDxKeyPressed(keyState, KEY_INPUT_MINUS) || WasDxKeyPressed(keyState, KEY_INPUT_SUBTRACT))
            {
                io.AddInputCharacter('-');
            }
            if (WasDxKeyPressed(keyState, KEY_INPUT_PERIOD) || WasDxKeyPressed(keyState, KEY_INPUT_DECIMAL))
            {
                io.AddInputCharacter('.');
            }
        }

        g_prevKeyState = keyState;
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
        DrawFormatString(textX, textY, GetColor(235, 242, 255), "FPS: %.0f", fps);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

bool ImGuiLayer_Initialize(HWND hWnd, void* device, void* context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(kVirtualScreenWidth), static_cast<float>(kVirtualScreenHeight));
    LoadJapaneseFont(io);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigDragClickToInputText = true;

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
    SubmitDxLibKeyboardStateToImGui();
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

void ImGuiLayer_SetFoundationOverlayVisible(bool visible)
{
    g_foundationOverlayVisible = visible;
}

void ImGuiLayer_DrawFoundationWindow(float fps)
{
    if (!g_imguiInitialized || !g_foundationOverlayVisible)
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
