#pragma once

#include "game_scene_photo_state.h"
#include "input.h"

namespace game_scene_photo_tray_system
{
template <typename SelectSlotFn>
void UpdateSelection(PhotoState& photo, float trayReveal, SelectSlotFn&& selectSlot)
{
    if (trayReveal <= 0.05f)
    {
        return;
    }

    if (Input_IsGamepadConnected())
    {
        static float prevAxisX = 0.0f;
        const float axis = Input_GetMoveX();
        constexpr float kAxisThreshold = 0.5f;
        constexpr int kSlotCount = 3;

        if (axis > kAxisThreshold && prevAxisX <= kAxisThreshold)
        {
            const int start = photo.selectedCaptureSlot;
            for (int i = 1; i <= kSlotCount; ++i)
            {
                const int idx = (start + i) % kSlotCount;
                if (photo.savedCaptures[idx].hasPhoto)
                {
                    selectSlot(idx);
                    break;
                }
            }
        }
        else if (axis < -kAxisThreshold && prevAxisX >= -kAxisThreshold)
        {
            const int start = photo.selectedCaptureSlot;
            for (int i = 1; i <= kSlotCount; ++i)
            {
                const int idx = (start + kSlotCount - i) % kSlotCount;
                if (photo.savedCaptures[idx].hasPhoto)
                {
                    selectSlot(idx);
                    break;
                }
            }
        }
        prevAxisX = axis;
    }

    {
        constexpr int kSlotCount = 3;

        if (Input_IsKeyPressed('D'))
        {
            const int start = photo.selectedCaptureSlot;
            for (int i = 1; i <= kSlotCount; ++i)
            {
                const int idx = (start + i) % kSlotCount;
                if (photo.savedCaptures[idx].hasPhoto)
                {
                    selectSlot(idx);
                    break;
                }
            }
        }

        if (Input_IsKeyPressed('A'))
        {
            const int start = photo.selectedCaptureSlot;
            for (int i = 1; i <= kSlotCount; ++i)
            {
                const int idx = (start + kSlotCount - i) % kSlotCount;
                if (photo.savedCaptures[idx].hasPhoto)
                {
                    selectSlot(idx);
                    break;
                }
            }
        }
    }
}
}
