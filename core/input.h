#pragma once

#include <windows.h>

enum class InputAction
{
    Confirm,
    Cancel,
    StartGame,
    OpenDemoScene,
    OpenShaderShowcase,
    RestartScene,
    ReturnToTitle,
    ToggleTuningPanel,
    ToggleCollisionDebug,
    CycleFilter,
    SelectFilterNone,
    SelectFilterHot,
    SelectFilterCold,
    SelectFilterInvert,
    SelectFilterSepia,
    HoldCamera,
    CapturePhoto,
    HoldPlacement,
    ConfirmPlacement,
    CyclePlacementLayer,
    FlipPlacement,
    ToggleBridgePlacement,
    RotatePlacementLeft,
    RotatePlacementRight,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Jump,
    Dodge,
    ExitPromptYes,
    ExitPromptNo,
};

enum class InputAxis
{
    MoveX,
    MoveY,
    Rotate,
};

bool Input_Initialize();
void Input_Update();
bool Input_IsActionDown(InputAction action);
bool Input_IsActionPressed(InputAction action);
float Input_GetAxis(InputAxis axis);
bool Input_IsKeyDown(int virtualKey);
bool Input_IsKeyPressed(int virtualKey);
bool Input_IsMouseLeftPressed();
bool Input_IsGamepadConnected();
float Input_GetMoveX();
float Input_GetMoveY();
float Input_GetRotateAxis();
bool Input_IsSouthButtonPressed();
int Input_GetMouseX();
int Input_GetMouseY();
