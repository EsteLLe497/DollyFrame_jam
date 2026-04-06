#pragma once

bool Audio_Initialize();
void Audio_Shutdown();
void Audio_Update();
void Audio_PlayTestTone();
void Audio_PlayCue(const char* cueName);
void Audio_SetMasterVolume(float volume);
float Audio_GetMasterVolume();
void Audio_SetSeVolume(float volume);
float Audio_GetSeVolume();

// Register external file cues and play by cue name.
bool Audio_LoadCueFromFile(const char* cueName, const char* filePath);
void Audio_UnloadCue(const char* cueName);
