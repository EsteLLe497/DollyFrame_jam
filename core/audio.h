#pragma once

bool Audio_Initialize();
void Audio_Shutdown();
void Audio_Update();
void Audio_PlayTestTone();
void Audio_PlayCue(const char* cueName);
void Audio_SetMasterVolume(float volume);
float Audio_GetMasterVolume();
