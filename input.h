#pragma once

#include <windows.h>

bool Input_Initialize();
void Input_Update();
bool Input_IsKeyDown(int virtualKey);
bool Input_IsKeyPressed(int virtualKey);
bool Input_IsMouseLeftPressed();
bool Input_IsGamepadConnected();
float Input_GetMoveX();
float Input_GetMoveY();
float Input_GetRotateAxis();
bool Input_IsSouthButtonPressed();
