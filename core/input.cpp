#include "input.h"

#include "DxLib.h"

namespace
{
    DxLib::XINPUT_STATE g_state{};
    DxLib::XINPUT_STATE g_prevState{};
    bool g_keyState[256]{};
    bool g_prevKeyState[256]{};
    int g_mouseButtons = 0;
    int g_prevMouseButtons = 0;
    bool g_connected = false;

    int ToDxKey(int virtualKey)
    {
        switch (virtualKey)
        {
        case '0': return KEY_INPUT_0;
        case '1': return KEY_INPUT_1;
        case '2': return KEY_INPUT_2;
        case '3': return KEY_INPUT_3;
        case '4': return KEY_INPUT_4;
        case '5': return KEY_INPUT_5;
        case '6': return KEY_INPUT_6;
        case '7': return KEY_INPUT_7;
        case '8': return KEY_INPUT_8;
        case '9': return KEY_INPUT_9;
        case 'A': return KEY_INPUT_A;
        case 'B': return KEY_INPUT_B;
        case 'C': return KEY_INPUT_C;
        case 'D': return KEY_INPUT_D;
        case 'E': return KEY_INPUT_E;
        case 'F': return KEY_INPUT_F;
        case 'Q': return KEY_INPUT_Q;
        case 'R': return KEY_INPUT_R;
        case 'S': return KEY_INPUT_S;
        case 'T': return KEY_INPUT_T;
        case 'W': return KEY_INPUT_W;
        case 'X': return KEY_INPUT_X;
        case 'Z': return KEY_INPUT_Z;
        case VK_LEFT: return KEY_INPUT_LEFT;
        case VK_RIGHT: return KEY_INPUT_RIGHT;
        case VK_UP: return KEY_INPUT_UP;
        case VK_DOWN: return KEY_INPUT_DOWN;
        case VK_SPACE: return KEY_INPUT_SPACE;
        case VK_SHIFT: return KEY_INPUT_LSHIFT;
        case VK_LSHIFT: return KEY_INPUT_LSHIFT;
        case VK_RSHIFT: return KEY_INPUT_RSHIFT;
        case VK_RETURN: return KEY_INPUT_RETURN;
        case VK_ESCAPE: return KEY_INPUT_ESCAPE;
        case VK_F1: return KEY_INPUT_F1;
        case VK_F2: return KEY_INPUT_F2;
        case VK_F3: return KEY_INPUT_F3;
        case VK_F4: return KEY_INPUT_F4;
        case VK_F5: return KEY_INPUT_F5;
        case VK_F6: return KEY_INPUT_F6;
        case VK_F7: return KEY_INPUT_F7;
        case VK_F8: return KEY_INPUT_F8;
        case VK_F9: return KEY_INPUT_F9;
        case VK_F10: return KEY_INPUT_F10;
        case VK_F11: return KEY_INPUT_F11;
        case VK_F12: return KEY_INPUT_F12;
        default: return -1;
        }
    }

    constexpr short kThumbDeadZone = 7849;
    constexpr unsigned char kTriggerThreshold = 30;

    float NormalizeThumb(short value, short deadZone)
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

    float NormalizeTrigger(unsigned char value)
    {
        if (value <= kTriggerThreshold)
        {
            return 0.0f;
        }
        return static_cast<float>(value - kTriggerThreshold) /
            static_cast<float>(255 - kTriggerThreshold);
    }

    bool IsGamepadSouthPressed()
    {
        if (!g_connected)
        {
            return false;
        }

        return g_state.Buttons[XINPUT_BUTTON_A] != 0 && g_prevState.Buttons[XINPUT_BUTTON_A] == 0;
    }

    bool IsGamepadEastPressed()
    {
        if (!g_connected)
        {
            return false;
        }

        return g_state.Buttons[XINPUT_BUTTON_B] != 0 && g_prevState.Buttons[XINPUT_BUTTON_B] == 0;
    }

    bool IsGamepadNorthPressed()
    {
        if (!g_connected)
        {
            return false;
        }
        return g_state.Buttons[XINPUT_BUTTON_Y] != 0 && g_prevState.Buttons[XINPUT_BUTTON_Y] == 0;
	}

    //LT がホールドされているか
    bool IsGamepadLeftTriggerDown()
    {
        if (!g_connected)
        {
            return false;
        }
        return g_state.LeftTrigger > kTriggerThreshold;
    }

    //RTを押したとき
    bool IsGamepadRightTriggerPressed()
    {
        if (!g_connected)
        {
            return false;
        }
        return g_state.RightTrigger > kTriggerThreshold && g_prevState.RightTrigger <= kTriggerThreshold;
    }

    //右スティック取得
    float GetGamepadRightX()
    {
        return g_connected ? NormalizeThumb(g_state.ThumbRX, kThumbDeadZone) : 0.0f;
    }
    float GetGamepadRightY()
    {
        // 上方向が負になるように左スティックの取扱に合わせる
        return g_connected ? -NormalizeThumb(g_state.ThumbRY, kThumbDeadZone) : 0.0f;
    }
}

bool Input_Initialize()
{
    ZeroMemory(&g_state, sizeof(g_state));
    ZeroMemory(&g_prevState, sizeof(g_prevState));
    ZeroMemory(g_keyState, sizeof(g_keyState));
    ZeroMemory(g_prevKeyState, sizeof(g_prevKeyState));
    g_mouseButtons = 0;
    g_prevMouseButtons = 0;
    g_connected = false;
    return true;
}

void Input_Update()
{
    g_prevState = g_state;
    for (int key = 0; key < 256; ++key)
    {
        g_prevKeyState[key] = g_keyState[key];
        const int dxKey = ToDxKey(key);
        g_keyState[key] = dxKey >= 0 && CheckHitKey(dxKey) != 0;
    }

    g_prevMouseButtons = g_mouseButtons;
    g_mouseButtons = GetMouseInput();

    ZeroMemory(&g_state, sizeof(g_state));
    g_connected = GetJoypadXInputState(DX_INPUT_PAD1, &g_state) == 0;
}

bool Input_IsActionDown(InputAction action)
{
    switch (action)
    {
    case InputAction::HoldCamera:
        return Input_IsKeyDown(VK_RBUTTON)||IsGamepadLeftTriggerDown();
    case InputAction::HoldPlacement:
        return Input_IsKeyDown('E')||IsGamepadNorthPressed();
    case InputAction::RotatePlacementLeft:
        return Input_IsKeyDown('Z');
    case InputAction::RotatePlacementRight:
        return Input_IsKeyDown('X');
    case InputAction::MoveLeft:
        return Input_IsKeyDown('A') || Input_IsKeyDown(VK_LEFT);
    case InputAction::MoveRight:
        return Input_IsKeyDown('D') || Input_IsKeyDown(VK_RIGHT);
    case InputAction::MoveUp:
        return Input_IsKeyDown('W') || Input_IsKeyDown(VK_UP);
    case InputAction::MoveDown:
        return Input_IsKeyDown('S') || Input_IsKeyDown(VK_DOWN);
    default:
        return Input_IsActionPressed(action);
    }
}

bool Input_IsActionPressed(InputAction action)
{
    switch (action)
    {
    case InputAction::Confirm:
    case InputAction::StartGame:
        return Input_IsKeyPressed(VK_RETURN) || Input_IsKeyPressed(VK_SPACE) || IsGamepadSouthPressed();
    case InputAction::Cancel:
        return Input_IsKeyPressed(VK_ESCAPE);
    case InputAction::OpenDemoScene:
        return Input_IsKeyPressed('D');
    case InputAction::OpenShaderShowcase:
        return Input_IsKeyPressed('S');
    case InputAction::RestartScene:
        return Input_IsKeyPressed('R');
    case InputAction::ReturnToTitle:
        return Input_IsKeyPressed('T');
    case InputAction::ToggleTuningPanel:
        return Input_IsKeyPressed(VK_F1);
    case InputAction::ToggleCollisionDebug:
        return Input_IsKeyPressed(VK_F3);
    case InputAction::CycleFilter:
        return Input_IsKeyPressed('C');
    case InputAction::SelectFilterNone:
        return Input_IsKeyPressed('1');
    case InputAction::SelectFilterHot:
        return Input_IsKeyPressed('2');
    case InputAction::SelectFilterCold:
        return Input_IsKeyPressed('3');
    case InputAction::SelectFilterInvert:
        return Input_IsKeyPressed('4');
    case InputAction::SelectFilterSepia:
        return Input_IsKeyPressed('5');
    case InputAction::HoldCamera:
        return Input_IsKeyPressed(VK_RBUTTON);
    case InputAction::CapturePhoto:
    case InputAction::ConfirmPlacement:
        return Input_IsMouseLeftPressed()||IsGamepadRightTriggerPressed();
    case InputAction::HoldPlacement:
        return Input_IsKeyPressed('E')||IsGamepadNorthPressed();
    case InputAction::CyclePlacementLayer:
        return Input_IsKeyPressed('Q');
    case InputAction::FlipPlacement:
        return Input_IsKeyPressed('F');
    case InputAction::ToggleBridgePlacement:
        return Input_IsKeyPressed('B');
    case InputAction::RotatePlacementLeft:
        return Input_IsKeyPressed('Z');
    case InputAction::RotatePlacementRight:
        return Input_IsKeyPressed('X');
    case InputAction::MoveLeft:
        return Input_IsKeyPressed('A') || Input_IsKeyPressed(VK_LEFT);
    case InputAction::MoveRight:
        return Input_IsKeyPressed('D') || Input_IsKeyPressed(VK_RIGHT);
    case InputAction::MoveUp:
        return Input_IsKeyPressed('W') || Input_IsKeyPressed(VK_UP);
    case InputAction::MoveDown:
        return Input_IsKeyPressed('S') || Input_IsKeyPressed(VK_DOWN);
    case InputAction::Jump:
        return Input_IsKeyPressed(VK_SPACE) || Input_IsKeyPressed('W') || Input_IsKeyPressed(VK_UP) || IsGamepadSouthPressed();
    case InputAction::Dodge:
        return Input_IsKeyPressed(VK_LSHIFT) || Input_IsKeyPressed(VK_RSHIFT) || Input_IsKeyPressed(VK_SHIFT)||IsGamepadEastPressed();
    case InputAction::ExitPromptYes:
        return Input_IsKeyPressed(VK_RETURN) || Input_IsKeyPressed('Y');
    case InputAction::ExitPromptNo:
        return Input_IsKeyPressed('N') || Input_IsKeyPressed(VK_ESCAPE);
    default:
        return false;
    }
}

float Input_GetAxis(InputAxis axis)
{
    switch (axis)
    {
    case InputAxis::MoveX:
        return Input_GetMoveX();
    case InputAxis::MoveY:
        return Input_GetMoveY();
    case InputAxis::Rotate:
        return Input_GetRotateAxis();
    default:
        return 0.0f;
    }
}

bool Input_IsKeyDown(int virtualKey)
{
    if (virtualKey == VK_LBUTTON)
    {
        return (g_mouseButtons & MOUSE_INPUT_LEFT) != 0;
    }
    if (virtualKey == VK_RBUTTON)
    {
        return (g_mouseButtons & MOUSE_INPUT_RIGHT) != 0;
    }

    return virtualKey >= 0 &&
        virtualKey < 256 &&
        g_keyState[virtualKey];
}

bool Input_IsKeyPressed(int virtualKey)
{
    if (virtualKey == VK_LBUTTON)
    {
        return (g_mouseButtons & MOUSE_INPUT_LEFT) != 0 &&
            (g_prevMouseButtons & MOUSE_INPUT_LEFT) == 0;
    }
    if (virtualKey == VK_RBUTTON)
    {
        return (g_mouseButtons & MOUSE_INPUT_RIGHT) != 0 &&
            (g_prevMouseButtons & MOUSE_INPUT_RIGHT) == 0;
    }

    return virtualKey >= 0 &&
        virtualKey < 256 &&
        g_keyState[virtualKey] &&
        !g_prevKeyState[virtualKey];
}

bool Input_IsMouseLeftPressed()
{
    return (g_mouseButtons & MOUSE_INPUT_LEFT) != 0 &&
        (g_prevMouseButtons & MOUSE_INPUT_LEFT) == 0;
}

bool Input_IsGamepadConnected()
{
    return g_connected;
}

float Input_GetMoveX()
{
    return g_connected ? NormalizeThumb(g_state.ThumbLX, kThumbDeadZone) : 0.0f;
}

float Input_GetMoveY()
{
    return g_connected ? -NormalizeThumb(g_state.ThumbLY, kThumbDeadZone) : 0.0f;
}

float Input_GetRotateAxis()
{
    if (!g_connected)
    {
        return 0.0f;
    }
    return NormalizeTrigger(g_state.RightTrigger) - NormalizeTrigger(g_state.LeftTrigger);
}

bool Input_IsSouthButtonPressed()
{
    return IsGamepadSouthPressed();
}

bool Input_IsEastButtonPressed()
{
    return IsGamepadEastPressed();
}

bool Input_IsNorthButtonPressed()
{
    return IsGamepadNorthPressed();
}

bool Input_IsLeftTriggerDown()
{
    return IsGamepadLeftTriggerDown();
}

bool Input_IsRightTriggerPressed()
{
    return IsGamepadRightTriggerPressed();
}

float Input_GetRightStickX()
{
    return GetGamepadRightX();
}

float Input_GetRightStickY()
{
    return GetGamepadRightY();
}

int Input_GetMouseX()
{
    int x = 0;
    int y = 0;
    GetMousePoint(&x, &y);
    return x;
}

int Input_GetMouseY()
{
    int x = 0;
    int y = 0;
    GetMousePoint(&x, &y);
    return y;
}
