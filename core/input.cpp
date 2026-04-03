#include "input.h"

#include "DxLib.h"

namespace
{
    using ActionPredicate = bool (*)();

    constexpr int kNoKey = -1;
    constexpr int kMaxBindingKeys = 4;
    constexpr int kMaxBindingPredicates = 3;

    struct ActionBinding
    {
        InputAction action;
        int downKeys[kMaxBindingKeys];
        int pressedKeys[kMaxBindingKeys];
        bool downFallsBackToPressed;
        ActionPredicate downPredicates[kMaxBindingPredicates];
        ActionPredicate pressedPredicates[kMaxBindingPredicates];
    };

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
        case 'M': return KEY_INPUT_M;
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


    bool IsGamepadBackPressed()
    {
        if (!g_connected)
        {
            return false;
        }

        return g_state.Buttons[XINPUT_BUTTON_BACK] != 0 &&
            g_prevState.Buttons[XINPUT_BUTTON_BACK] == 0;
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

    bool IsGamepadRightTriggerDown()
    {
        if (!g_connected)
        {
            return false;
        }
        return g_state.RightTrigger > kTriggerThreshold;
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

    bool IsGamepadRightShoulderDown()
    {
        if (!g_connected)
        {
            return false;
        }
        return (g_state.Buttons[XINPUT_BUTTON_RIGHT_SHOULDER] != 0);
	}

    bool IsGamepadLeftShoulderDown()
    {
        if (!g_connected)
        {
            return false;
        }
        return (g_state.Buttons[XINPUT_BUTTON_LEFT_SHOULDER] != 0);
    }

    bool IsGamepadRightShoulderPressed()
    {
        if (!g_connected)
        {
            return false;
        }
        return (g_state.Buttons[XINPUT_BUTTON_RIGHT_SHOULDER] != 0) && (g_prevState.Buttons[XINPUT_BUTTON_RIGHT_SHOULDER] == 0);
	}

    bool IsGamepadLeftShoulderPressed()
    {
        if (!g_connected)
        {
            return false;
        }
        return (g_state.Buttons[XINPUT_BUTTON_LEFT_SHOULDER] != 0) && (g_prevState.Buttons[XINPUT_BUTTON_LEFT_SHOULDER] == 0);
	}

    bool EvaluateBoundKeys(const int (&keys)[kMaxBindingKeys], bool pressed)
    {
        for (int key : keys)
        {
            if (key == kNoKey)
            {
                continue;
            }
            if (pressed ? Input_IsKeyPressed(key) : Input_IsKeyDown(key))
            {
                return true;
            }
        }
        return false;
    }

    bool EvaluatePredicates(const ActionPredicate (&predicates)[kMaxBindingPredicates])
    {
        for (ActionPredicate predicate : predicates)
        {
            if (predicate != nullptr && predicate())
            {
                return true;
            }
        }
        return false;
    }

    constexpr ActionBinding kActionBindings[] =
    {
        { InputAction::Confirm, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_RETURN, VK_SPACE, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadSouthPressed, nullptr, nullptr } },
        { InputAction::Cancel, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_ESCAPE, kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::StartGame, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_RETURN, VK_SPACE, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadSouthPressed, nullptr, nullptr } },
        { InputAction::OpenDemoScene, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'D', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::OpenShaderShowcase, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'S', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::RestartScene, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'R', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadBackPressed, nullptr, nullptr } },
        { InputAction::ReturnToTitle, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'T', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::ToggleTuningPanel, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_F1, kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::TogglePostProcess, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_F2, kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::ToggleCollisionDebug, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_F3, kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::CycleFilter, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'C', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::SelectFilterNone, { kNoKey, kNoKey, kNoKey, kNoKey }, { '1', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::SelectFilterHot, { kNoKey, kNoKey, kNoKey, kNoKey }, { '2', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::SelectFilterCold, { kNoKey, kNoKey, kNoKey, kNoKey }, { '3', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::SelectFilterInvert, { kNoKey, kNoKey, kNoKey, kNoKey }, { '4', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::SelectFilterSepia, { kNoKey, kNoKey, kNoKey, kNoKey }, { '5', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::HoldCamera, { VK_RBUTTON, kNoKey, kNoKey, kNoKey }, { VK_RBUTTON, kNoKey, kNoKey, kNoKey }, false, { IsGamepadLeftTriggerDown, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::CapturePhoto, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_LBUTTON, kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadRightTriggerPressed, nullptr, nullptr } },
        { InputAction::HoldPlacement, { 'E', kNoKey, kNoKey, kNoKey }, { 'E', kNoKey, kNoKey, kNoKey }, false, { IsGamepadNorthPressed, nullptr, nullptr }, { IsGamepadNorthPressed, nullptr, nullptr } },
        { InputAction::ConfirmPlacement, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_LBUTTON, kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadRightTriggerPressed, IsGamepadSouthPressed, nullptr } },
        { InputAction::CyclePlacementLayer, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'Q', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::FlipPlacement, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'F', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::ToggleBridgePlacement, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'B', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::RotatePlacementLeft, { 'Z', kNoKey, kNoKey, kNoKey }, { 'Z', kNoKey, kNoKey, kNoKey }, false, { IsGamepadLeftShoulderDown, nullptr, nullptr }, { IsGamepadLeftShoulderPressed, nullptr, nullptr } },
        { InputAction::RotatePlacementRight, { 'X', kNoKey, kNoKey, kNoKey }, { 'X', kNoKey, kNoKey, kNoKey }, false, { IsGamepadRightShoulderDown, nullptr, nullptr }, { IsGamepadRightShoulderPressed, nullptr, nullptr } },
        { InputAction::MoveLeft, { 'A', VK_LEFT, kNoKey, kNoKey }, { 'A', VK_LEFT, kNoKey, kNoKey }, false, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::MoveRight, { 'D', VK_RIGHT, kNoKey, kNoKey }, { 'D', VK_RIGHT, kNoKey, kNoKey }, false, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::MoveUp, { 'W', VK_UP, kNoKey, kNoKey }, { 'W', VK_UP, kNoKey, kNoKey }, false, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::MoveDown, { 'S', VK_DOWN, kNoKey, kNoKey }, { 'S', VK_DOWN, kNoKey, kNoKey }, false, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::Jump, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_SPACE, 'W', VK_UP, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadSouthPressed, nullptr, nullptr } },
        { InputAction::Dodge, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_LSHIFT, VK_RSHIFT, VK_SHIFT, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadEastPressed, nullptr, nullptr } },
        { InputAction::ExitPromptYes, { kNoKey, kNoKey, kNoKey, kNoKey }, { VK_RETURN, 'Y', kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
        { InputAction::ExitPromptNo, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'N', VK_ESCAPE, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } },
    };

    const ActionBinding* FindBinding(InputAction action)
    {
        for (const ActionBinding& binding : kActionBindings)
        {
            if (binding.action == action)
            {
                return &binding;
            }
        }
        return nullptr;
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
    const ActionBinding* binding = FindBinding(action);
    if (binding == nullptr)
    {
        return false;
    }

    if (EvaluateBoundKeys(binding->downKeys, false) || EvaluatePredicates(binding->downPredicates))
    {
        return true;
    }
    if (!binding->downFallsBackToPressed)
    {
        return false;
    }
    return EvaluateBoundKeys(binding->pressedKeys, true) || EvaluatePredicates(binding->pressedPredicates);
}

bool Input_IsActionPressed(InputAction action)
{
    const ActionBinding* binding = FindBinding(action);
    if (binding == nullptr)
    {
        return false;
    }
    return EvaluateBoundKeys(binding->pressedKeys, true) || EvaluatePredicates(binding->pressedPredicates);
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

bool Input_IsRightShoulderPressed()
{
    return IsGamepadRightShoulderPressed();
}


bool Input_IsLeftShoulderPressed()
{
    return IsGamepadLeftShoulderPressed();
}

bool Input_IsLeftTriggerDown()
{
    return IsGamepadLeftTriggerDown();
}

bool Input_IsRightTriggerPressed()
{
    return IsGamepadRightTriggerPressed();
}

bool Input_IsRightTriggerDown()
{
    return IsGamepadRightTriggerDown();
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

bool Input_IsDpadUpPressed()
{
    if (!g_connected)
    {
        return false;
    }
    return g_state.Buttons[XINPUT_BUTTON_DPAD_UP] != 0 && g_prevState.Buttons[XINPUT_BUTTON_DPAD_UP] == 0;
}

bool Input_IsDpadDownPressed()
{
    if (!g_connected)
    {
        return false;
    }
    return g_state.Buttons[XINPUT_BUTTON_DPAD_DOWN] != 0 && g_prevState.Buttons[XINPUT_BUTTON_DPAD_DOWN] == 0;
}

bool Input_IsDpadUpDown()
{
    if (!g_connected)
    {
        return false;
    }
    return g_state.Buttons[XINPUT_BUTTON_DPAD_UP] != 0;
}

bool Input_IsDpadDownDown()
{
    if (!g_connected)
    {
        return false;
    }
    return g_state.Buttons[XINPUT_BUTTON_DPAD_DOWN] != 0;
}

