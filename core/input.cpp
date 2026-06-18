#include "pch.h"

#include "input.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "DxLib.h"
#include "directX.h"
#include "logger.h"

namespace
{
    using ActionPredicate = bool (*)();
    using json = nlohmann::json;

    constexpr int kNoKey = -1;
    constexpr int kMaxBindingKeys = 4;
    constexpr int kMaxBindingPredicates = 3;
    constexpr const char* kInputBindingsPath = "assets/input_bindings.json";

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
        case 'V': return KEY_INPUT_V;
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

    bool IsGamepadButtonDown(int buttonIndex)
    {
        return g_connected && g_state.Buttons[buttonIndex] != 0;
    }

    bool IsGamepadButtonPressed(int buttonIndex)
    {
        return g_connected &&
            g_state.Buttons[buttonIndex] != 0 &&
            g_prevState.Buttons[buttonIndex] == 0;
    }

    bool IsGamepadTriggerDown(unsigned char value)
    {
        return g_connected && value > kTriggerThreshold;
    }

    bool IsGamepadTriggerPressed(unsigned char value, unsigned char prevValue)
    {
        return g_connected && value > kTriggerThreshold && prevValue <= kTriggerThreshold;
    }

    template <typename Value>
    struct NamedValue
    {
        const char* name;
        Value value;
    };

    template <typename Value, std::size_t N>
    bool TryLookupName(const std::string& key, const std::array<NamedValue<Value>, N>& entries, Value& out)
    {
        for (const auto& entry : entries)
        {
            if (key == entry.name)
            {
                out = entry.value;
                return true;
            }
        }
        return false;
    }

    constexpr std::array<NamedValue<InputAction>, 35> kActionNameMap =
    {{
        { "CONFIRM", InputAction::Confirm },
        { "CANCEL", InputAction::Cancel },
        { "STARTGAME", InputAction::StartGame },
        { "OPENDEMOSCENE", InputAction::OpenDemoScene },
        { "OPENSHOWCASE", InputAction::OpenShaderShowcase },
        { "OPENSHADERSHOWCASE", InputAction::OpenShaderShowcase },
        { "RESTARTSCENE", InputAction::RestartScene },
        { "RETURNTOTITLE", InputAction::ReturnToTitle },
        { "TOGGLETUNINGPANEL", InputAction::ToggleTuningPanel },
        { "TOGGLEPOSTPROCESS", InputAction::TogglePostProcess },
        { "TOGGLECOLLISIONDEBUG", InputAction::ToggleCollisionDebug },
        { "CYCLEFILTER", InputAction::CycleFilter },
        { "SELECTFILTERNONE", InputAction::SelectFilterNone },
        { "SELECTFILTERHOT", InputAction::SelectFilterHot },
        { "SELECTFILTERCOLD", InputAction::SelectFilterCold },
        { "SELECTFILTERINVERT", InputAction::SelectFilterInvert },
        { "SELECTFILTERSEPIA", InputAction::SelectFilterSepia },
        { "HOLDCAMERA", InputAction::HoldCamera },
        { "CAPTUREPHOTO", InputAction::CapturePhoto },
        { "HOLDPLACEMENT", InputAction::HoldPlacement },
        { "CONFIRMPLACEMENT", InputAction::ConfirmPlacement },
        { "ATTACKPASTE", InputAction::AttackPaste },
        { "CYCLEPLACEMENTLAYER", InputAction::CyclePlacementLayer },
        { "FLIPPLACEMENT", InputAction::FlipPlacement },
        { "TOGGLEBRIDGEPLACEMENT", InputAction::ToggleBridgePlacement },
        { "ROTATEPLACEMENTLEFT", InputAction::RotatePlacementLeft },
        { "ROTATEPLACEMENTRIGHT", InputAction::RotatePlacementRight },
        { "MOVELEFT", InputAction::MoveLeft },
        { "MOVERIGHT", InputAction::MoveRight },
        { "MOVEUP", InputAction::MoveUp },
        { "MOVEDOWN", InputAction::MoveDown },
        { "JUMP", InputAction::Jump },
        { "DODGE", InputAction::Dodge },
        { "EXITPROMPTYES", InputAction::ExitPromptYes },
        { "EXITPROMPTNO", InputAction::ExitPromptNo },
    }};

    constexpr std::array<NamedValue<int>, 28> kNamedVirtualKeys =
    {{
        { "ENTER", VK_RETURN },
        { "RETURN", VK_RETURN },
        { "SPACE", VK_SPACE },
        { "ESC", VK_ESCAPE },
        { "ESCAPE", VK_ESCAPE },
        { "LEFT", VK_LEFT },
        { "RIGHT", VK_RIGHT },
        { "UP", VK_UP },
        { "DOWN", VK_DOWN },
        { "SHIFT", VK_SHIFT },
        { "LSHIFT", VK_LSHIFT },
        { "RSHIFT", VK_RSHIFT },
        { "LBUTTON", VK_LBUTTON },
        { "MOUSE_LEFT", VK_LBUTTON },
        { "RBUTTON", VK_RBUTTON },
        { "MOUSE_RIGHT", VK_RBUTTON },
        { "F1", VK_F1 },
        { "F2", VK_F2 },
        { "F3", VK_F3 },
        { "F4", VK_F4 },
        { "F5", VK_F5 },
        { "F6", VK_F6 },
        { "F7", VK_F7 },
        { "F8", VK_F8 },
        { "F9", VK_F9 },
        { "F10", VK_F10 },
        { "F11", VK_F11 },
        { "F12", VK_F12 },
    }};

    bool IsGamepadSouthPressed();
    bool IsGamepadEastPressed();
    bool IsGamepadNorthPressed();
    bool IsGamepadBackPressed();
    bool IsGamepadLeftTriggerDown();
    bool IsGamepadRightTriggerDown();
    bool IsGamepadRightTriggerPressed();
    bool IsGamepadLeftShoulderDown();
    bool IsGamepadRightShoulderDown();
    bool IsGamepadLeftShoulderPressed();
    bool IsGamepadRightShoulderPressed();

    constexpr std::array<NamedValue<ActionPredicate>, 11> kPredicateNameMap =
    {{
        { "GAMEPAD_SOUTH_PRESSED", IsGamepadSouthPressed },
        { "GAMEPAD_EAST_PRESSED", IsGamepadEastPressed },
        { "GAMEPAD_NORTH_PRESSED", IsGamepadNorthPressed },
        { "GAMEPAD_BACK_PRESSED", IsGamepadBackPressed },
        { "GAMEPAD_LEFT_TRIGGER_DOWN", IsGamepadLeftTriggerDown },
        { "GAMEPAD_RIGHT_TRIGGER_DOWN", IsGamepadRightTriggerDown },
        { "GAMEPAD_RIGHT_TRIGGER_PRESSED", IsGamepadRightTriggerPressed },
        { "GAMEPAD_LEFT_SHOULDER_DOWN", IsGamepadLeftShoulderDown },
        { "GAMEPAD_RIGHT_SHOULDER_DOWN", IsGamepadRightShoulderDown },
        { "GAMEPAD_LEFT_SHOULDER_PRESSED", IsGamepadLeftShoulderPressed },
        { "GAMEPAD_RIGHT_SHOULDER_PRESSED", IsGamepadRightShoulderPressed },
    }};

    bool IsGamepadSouthPressed()
    {
        return IsGamepadButtonPressed(XINPUT_BUTTON_A);
    }

    bool IsGamepadEastPressed()
    {
        return IsGamepadButtonPressed(XINPUT_BUTTON_B);
    }

    bool IsGamepadNorthPressed()
    {
        return IsGamepadButtonPressed(XINPUT_BUTTON_Y);
	}


    bool IsGamepadBackPressed()
    {
        return IsGamepadButtonPressed(XINPUT_BUTTON_BACK);
    }

    //LT がホールドされているか
    bool IsGamepadLeftTriggerDown()
    {
        return IsGamepadTriggerDown(g_state.LeftTrigger);
    }

    bool IsGamepadRightTriggerDown()
    {
        return IsGamepadTriggerDown(g_state.RightTrigger);
	}


    //RTを押したとき
    bool IsGamepadRightTriggerPressed()
    {
        return IsGamepadTriggerPressed(g_state.RightTrigger, g_prevState.RightTrigger);
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
        return IsGamepadButtonDown(XINPUT_BUTTON_RIGHT_SHOULDER);
	}

    bool IsGamepadLeftShoulderDown()
    {
        return IsGamepadButtonDown(XINPUT_BUTTON_LEFT_SHOULDER);
    }

    bool IsGamepadRightShoulderPressed()
    {
        return IsGamepadButtonPressed(XINPUT_BUTTON_RIGHT_SHOULDER);
	}

    bool IsGamepadLeftShoulderPressed()
    {
        return IsGamepadButtonPressed(XINPUT_BUTTON_LEFT_SHOULDER);
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

    constexpr ActionBinding kDefaultActionBindings[] =
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
        { InputAction::AttackPaste, { kNoKey, kNoKey, kNoKey, kNoKey }, { 'Q', kNoKey, kNoKey, kNoKey }, true, { nullptr, nullptr, nullptr }, { IsGamepadEastPressed, nullptr, nullptr } },
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

    std::array<ActionBinding, std::size(kDefaultActionBindings)> g_actionBindings = std::to_array(kDefaultActionBindings);

    std::string ToUpperAscii(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::toupper(ch));
            });
        return value;
    }

    bool TryParseAction(const std::string& text, InputAction& outAction)
    {
        const std::string key = ToUpperAscii(text);
        return TryLookupName(key, kActionNameMap, outAction);
    }

    bool TryParseVirtualKey(const json& value, int& outKey)
    {
        if (value.is_number_integer())
        {
            const int key = value.get<int>();
            if (key >= 0 && key <= 255)
            {
                outKey = key;
                return true;
            }
            return false;
        }

        if (!value.is_string())
        {
            return false;
        }

        std::string key = ToUpperAscii(value.get<std::string>());
        if (key.rfind("VK_", 0) == 0)
        {
            key = key.substr(3);
        }

        if (key == "NONE")
        {
            outKey = kNoKey;
            return true;
        }
        if (TryLookupName(key, kNamedVirtualKeys, outKey))
        {
            return true;
        }

        if (key.size() == 1)
        {
            const unsigned char ch = static_cast<unsigned char>(key[0]);
            if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
            {
                outKey = static_cast<int>(ch);
                return true;
            }
        }

        return false;
    }

    bool TryParsePredicateName(const json& value, ActionPredicate& outPredicate)
    {
        if (!value.is_string())
        {
            return false;
        }

        std::string key = ToUpperAscii(value.get<std::string>());
        if (key == "NONE")
        {
            outPredicate = nullptr;
            return true;
        }
        return TryLookupName(key, kPredicateNameMap, outPredicate);
    }

    const char* PredicateToString(ActionPredicate predicate)
    {
        if (predicate == nullptr) { return "NONE"; }
        if (predicate == IsGamepadSouthPressed) { return "GAMEPAD_SOUTH_PRESSED"; }
        if (predicate == IsGamepadEastPressed) { return "GAMEPAD_EAST_PRESSED"; }
        if (predicate == IsGamepadNorthPressed) { return "GAMEPAD_NORTH_PRESSED"; }
        if (predicate == IsGamepadBackPressed) { return "GAMEPAD_BACK_PRESSED"; }
        if (predicate == IsGamepadLeftTriggerDown) { return "GAMEPAD_LEFT_TRIGGER_DOWN"; }
        if (predicate == IsGamepadRightTriggerDown) { return "GAMEPAD_RIGHT_TRIGGER_DOWN"; }
        if (predicate == IsGamepadRightTriggerPressed) { return "GAMEPAD_RIGHT_TRIGGER_PRESSED"; }
        if (predicate == IsGamepadLeftShoulderDown) { return "GAMEPAD_LEFT_SHOULDER_DOWN"; }
        if (predicate == IsGamepadRightShoulderDown) { return "GAMEPAD_RIGHT_SHOULDER_DOWN"; }
        if (predicate == IsGamepadLeftShoulderPressed) { return "GAMEPAD_LEFT_SHOULDER_PRESSED"; }
        if (predicate == IsGamepadRightShoulderPressed) { return "GAMEPAD_RIGHT_SHOULDER_PRESSED"; }
        return "UNKNOWN_PREDICATE";
    }

    std::string VirtualKeyToString(int key)
    {
        if (key == kNoKey) { return "NONE"; }
        if (key >= 'A' && key <= 'Z') { return std::string(1, static_cast<char>(key)); }
        if (key >= '0' && key <= '9') { return std::string(1, static_cast<char>(key)); }
        switch (key)
        {
        case VK_RETURN: return "ENTER";
        case VK_SPACE: return "SPACE";
        case VK_ESCAPE: return "ESCAPE";
        case VK_LEFT: return "LEFT";
        case VK_RIGHT: return "RIGHT";
        case VK_UP: return "UP";
        case VK_DOWN: return "DOWN";
        case VK_SHIFT: return "SHIFT";
        case VK_LSHIFT: return "LSHIFT";
        case VK_RSHIFT: return "RSHIFT";
        case VK_LBUTTON: return "LBUTTON";
        case VK_RBUTTON: return "RBUTTON";
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        default:
            break;
        }
        return "VK_" + std::to_string(key);
    }

    const char* ActionToString(InputAction action)
    {
        switch (action)
        {
        case InputAction::Confirm: return "Confirm";
        case InputAction::Cancel: return "Cancel";
        case InputAction::StartGame: return "StartGame";
        case InputAction::OpenDemoScene: return "OpenDemoScene";
        case InputAction::OpenShaderShowcase: return "OpenShaderShowcase";
        case InputAction::RestartScene: return "RestartScene";
        case InputAction::ReturnToTitle: return "ReturnToTitle";
        case InputAction::ToggleTuningPanel: return "ToggleTuningPanel";
        case InputAction::TogglePostProcess: return "TogglePostProcess";
        case InputAction::ToggleCollisionDebug: return "ToggleCollisionDebug";
        case InputAction::CycleFilter: return "CycleFilter";
        case InputAction::SelectFilterNone: return "SelectFilterNone";
        case InputAction::SelectFilterHot: return "SelectFilterHot";
        case InputAction::SelectFilterCold: return "SelectFilterCold";
        case InputAction::SelectFilterInvert: return "SelectFilterInvert";
        case InputAction::SelectFilterSepia: return "SelectFilterSepia";
        case InputAction::HoldCamera: return "HoldCamera";
        case InputAction::CapturePhoto: return "CapturePhoto";
        case InputAction::HoldPlacement: return "HoldPlacement";
        case InputAction::ConfirmPlacement: return "ConfirmPlacement";
        case InputAction::AttackPaste: return "AttackPaste";
        case InputAction::CyclePlacementLayer: return "CyclePlacementLayer";
        case InputAction::FlipPlacement: return "FlipPlacement";
        case InputAction::ToggleBridgePlacement: return "ToggleBridgePlacement";
        case InputAction::RotatePlacementLeft: return "RotatePlacementLeft";
        case InputAction::RotatePlacementRight: return "RotatePlacementRight";
        case InputAction::MoveLeft: return "MoveLeft";
        case InputAction::MoveRight: return "MoveRight";
        case InputAction::MoveUp: return "MoveUp";
        case InputAction::MoveDown: return "MoveDown";
        case InputAction::Jump: return "Jump";
        case InputAction::Dodge: return "Dodge";
        case InputAction::ExitPromptYes: return "ExitPromptYes";
        case InputAction::ExitPromptNo: return "ExitPromptNo";
        default: return "Unknown";
        }
    }

    void ClearKeyArray(int (&keys)[kMaxBindingKeys])
    {
        for (int& key : keys)
        {
            key = kNoKey;
        }
    }

    void ClearPredicateArray(ActionPredicate (&predicates)[kMaxBindingPredicates])
    {
        for (ActionPredicate& predicate : predicates)
        {
            predicate = nullptr;
        }
    }

    void ParseKeyArray(const json& arrayJson, int (&keys)[kMaxBindingKeys], const std::string& actionName, const char* fieldName)
    {
        if (!arrayJson.is_array())
        {
            return;
        }

        ClearKeyArray(keys);
        size_t writeIndex = 0;
        for (const auto& item : arrayJson)
        {
            if (writeIndex >= kMaxBindingKeys)
            {
                break;
            }

            int key = kNoKey;
            if (TryParseVirtualKey(item, key))
            {
                keys[writeIndex] = key;
                ++writeIndex;
            }
            else if (item.is_string() || item.is_number_integer())
            {
                const std::string badValue = item.is_string() ? item.get<std::string>() : std::to_string(item.get<int>());
                Logger::Warn(
                    "Input key is unknown for action '" + actionName + "' (" + fieldName + "): " + badValue);
            }
        }
    }

    void ParsePredicateArray(const json& arrayJson, ActionPredicate (&predicates)[kMaxBindingPredicates], const std::string& actionName)
    {
        if (!arrayJson.is_array())
        {
            return;
        }

        ClearPredicateArray(predicates);
        size_t writeIndex = 0;
        for (const auto& item : arrayJson)
        {
            if (writeIndex >= kMaxBindingPredicates)
            {
                break;
            }

            ActionPredicate predicate = nullptr;
            if (TryParsePredicateName(item, predicate))
            {
                predicates[writeIndex] = predicate;
                ++writeIndex;
            }
            else if (item.is_string())
            {
                Logger::Warn("Input gamepad predicate is unknown for action '" + actionName + "': " + item.get<std::string>());
            }
        }
    }

    ActionBinding* FindMutableBinding(InputAction action)
    {
        for (ActionBinding& binding : g_actionBindings)
        {
            if (binding.action == action)
            {
                return &binding;
            }
        }
        return nullptr;
    }

    const ActionBinding* FindBinding(InputAction action)
    {
        for (const ActionBinding& binding : g_actionBindings)
        {
            if (binding.action == action)
            {
                return &binding;
            }
        }
        return nullptr;
    }

    void ApplyBindingOverride(const json& bindingJson, const std::string& actionName)
    {
        InputAction action = InputAction::Confirm;
        if (!TryParseAction(actionName, action))
        {
            Logger::Warn("Input binding action is unknown: " + actionName);
            return;
        }

        ActionBinding* binding = FindMutableBinding(action);
        if (!binding)
        {
            return;
        }

        if (bindingJson.contains("down_keys"))
        {
            ParseKeyArray(bindingJson["down_keys"], binding->downKeys, actionName, "down_keys");
        }
        if (bindingJson.contains("pressed_keys"))
        {
            ParseKeyArray(bindingJson["pressed_keys"], binding->pressedKeys, actionName, "pressed_keys");
        }
        if (bindingJson.contains("down_falls_back_to_pressed") && bindingJson["down_falls_back_to_pressed"].is_boolean())
        {
            binding->downFallsBackToPressed = bindingJson["down_falls_back_to_pressed"].get<bool>();
        }
        if (bindingJson.contains("down_gamepad"))
        {
            ParsePredicateArray(bindingJson["down_gamepad"], binding->downPredicates, actionName);
        }
        if (bindingJson.contains("pressed_gamepad"))
        {
            ParsePredicateArray(bindingJson["pressed_gamepad"], binding->pressedPredicates, actionName);
        }
    }

    void LoadInputBindings()
    {
        std::ifstream stream(kInputBindingsPath, std::ios::binary);
        if (!stream.is_open())
        {
            return;
        }

        json root;
        try
        {
            stream >> root;
        }
        catch (const std::exception& e)
        {
            Logger::Warn(std::string("Failed to parse input bindings JSON: ") + e.what());
            return;
        }

        if (root.contains("bindings") && root["bindings"].is_array())
        {
            for (const auto& bindingJson : root["bindings"])
            {
                if (!bindingJson.is_object() || !bindingJson.contains("action") || !bindingJson["action"].is_string())
                {
                    continue;
                }
                ApplyBindingOverride(bindingJson, bindingJson["action"].get<std::string>());
            }
        }

        if (root.contains("actions") && root["actions"].is_object())
        {
            for (const auto& [actionName, bindingJson] : root["actions"].items())
            {
                if (!bindingJson.is_object())
                {
                    continue;
                }
                ApplyBindingOverride(bindingJson, actionName);
            }
        }

        auto joinKeys = [](const int (&keys)[kMaxBindingKeys]) -> std::string
        {
            std::ostringstream oss;
            bool first = true;
            for (int key : keys)
            {
                if (key == kNoKey)
                {
                    continue;
                }
                if (!first)
                {
                    oss << ", ";
                }
                first = false;
                oss << VirtualKeyToString(key);
            }
            return first ? "NONE" : oss.str();
        };

        auto joinPredicates = [](const ActionPredicate (&predicates)[kMaxBindingPredicates]) -> std::string
        {
            std::ostringstream oss;
            bool first = true;
            for (ActionPredicate predicate : predicates)
            {
                if (predicate == nullptr)
                {
                    continue;
                }
                if (!first)
                {
                    oss << ", ";
                }
                first = false;
                oss << PredicateToString(predicate);
            }
            return first ? "NONE" : oss.str();
        };

        Logger::Info("Input bindings loaded from assets/input_bindings.json");
        for (const ActionBinding& binding : g_actionBindings)
        {
            Logger::Info(
                std::string("Input binding: ") + ActionToString(binding.action) +
                " | down_keys=[" + joinKeys(binding.downKeys) + "]" +
                " | pressed_keys=[" + joinKeys(binding.pressedKeys) + "]" +
                " | down_gamepad=[" + joinPredicates(binding.downPredicates) + "]" +
                " | pressed_gamepad=[" + joinPredicates(binding.pressedPredicates) + "]" +
                " | fallback=" + (binding.downFallsBackToPressed ? "true" : "false"));
        }
    }
}

bool Input_Initialize()
{
    g_actionBindings = std::to_array(kDefaultActionBindings);
    LoadInputBindings();

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

bool Input_IsMouseLeftDown()
{
    return (g_mouseButtons & MOUSE_INPUT_LEFT) != 0;
}

bool Input_IsMouseLeftReleased()
{
    return (g_mouseButtons & MOUSE_INPUT_LEFT) == 0 &&
        (g_prevMouseButtons & MOUSE_INPUT_LEFT) != 0;
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
    return DirectXMapWindowToVirtualX(x);
}

int Input_GetMouseY()
{
    int x = 0;
    int y = 0;
    GetMousePoint(&x, &y);
    return DirectXMapWindowToVirtualY(y);
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

