#pragma once

bool Audio_Initialize();
void Audio_Shutdown();
void Audio_Update();
void Audio_PlayTestTone();
void Audio_PlayCue(const char* cueName);
void Audio_SetMasterVolume(float volume);
float Audio_GetMasterVolume();

// 外部ファイルを登録して名前で再生できるようにする
bool Audio_LoadCueFromFile(const char* cueName, const char* filePath);
void Audio_UnloadCue(const char* cueName);
