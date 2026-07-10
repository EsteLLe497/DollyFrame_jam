#pragma once

#include "game_scene_photo_state.h"

inline constexpr int kPhotoLogCapacity = 9;

void PhotoLog_Reset();
void PhotoLog_Add(const PhotoCaptureState& capture);
int PhotoLog_GetCount();
const PhotoCaptureState& PhotoLog_GetEntry(int index); // index: 0 = ˆê”ÔŒÃ‚¢ ~ count-1 = ˆê”ÔV‚µ‚¢