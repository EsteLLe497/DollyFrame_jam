#include "input.h"

#include <Xinput.h>

#pragma comment(lib, "xinput.lib")

namespace
{
    XINPUT_STATE g_state{};
    XINPUT_STATE g_prevState{};
    SHORT g_keyState[256]{};
    SHORT g_prevKeyState[256]{};
    bool g_connected = false;

    float NormalizeThumb(SHORT value, SHORT deadZone)
    {
        if (value > deadZone)
        {
            return static_cast<float>(value - deadZone) / static_cast<float>(32767 - deadZone);
        }
        if (value < -deadZone)
        {
            return static_cast<float>(value + deadZone) / static_cast<float>(32768 - deadZone);
        }
        return 0.0f;
    }

    float NormalizeTrigger(BYTE value)
    {
        if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        {
            return 0.0f;
        }
        return static_cast<float>(value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
            static_cast<float>(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
    }
}

bool Input_Initialize()
{
    ZeroMemory(&g_state, sizeof(g_state));
    ZeroMemory(&g_prevState, sizeof(g_prevState));
    ZeroMemory(g_keyState, sizeof(g_keyState));
    ZeroMemory(g_prevKeyState, sizeof(g_prevKeyState));
    g_connected = false;
    return true;
}

void Input_Update()
{
    g_prevState = g_state;
    CopyMemory(g_prevKeyState, g_keyState, sizeof(g_keyState));
    for (int key = 0; key < 256; ++key)
    {
        g_keyState[key] = GetAsyncKeyState(key);
    }
    ZeroMemory(&g_state, sizeof(g_state));
    g_connected = (XInputGetState(0, &g_state) == ERROR_SUCCESS);
}

bool Input_IsKeyDown(int virtualKey)
{
    return virtualKey >= 0 &&
        virtualKey < 256 &&
        (g_keyState[virtualKey] & 0x8000) != 0;
}

bool Input_IsKeyPressed(int virtualKey)
{
    return virtualKey >= 0 &&
        virtualKey < 256 &&
        (g_keyState[virtualKey] & 0x8000) != 0 &&
        (g_prevKeyState[virtualKey] & 0x8000) == 0;
}

bool Input_IsMouseLeftPressed()
{
    return Input_IsKeyPressed(VK_LBUTTON);
}

bool Input_IsGamepadConnected()
{
    return g_connected;
}

float Input_GetMoveX()
{
    return g_connected ? NormalizeThumb(g_state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) : 0.0f;
}

float Input_GetMoveY()
{
    return g_connected ? -NormalizeThumb(g_state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) : 0.0f;
}

float Input_GetRotateAxis()
{
    if (!g_connected)
    {
        return 0.0f;
    }
    return NormalizeTrigger(g_state.Gamepad.bRightTrigger) - NormalizeTrigger(g_state.Gamepad.bLeftTrigger);
}

bool Input_IsSouthButtonPressed()
{
    if (!g_connected)
    {
        return false;
    }

    const WORD current = g_state.Gamepad.wButtons;
    const WORD previous = g_prevState.Gamepad.wButtons;
    return (current & XINPUT_GAMEPAD_A) != 0 && (previous & XINPUT_GAMEPAD_A) == 0;
}
