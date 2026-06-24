#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace game_scene_photo_storage_layout
{
    struct SlotRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    inline constexpr std::array<SlotRect, 3> kSlotRects =
    {{
        { 68.0f, 906.0f, 149.0f, 96.0f },
        { 253.0f, 906.0f, 149.0f, 96.0f },
        { 441.5f, 906.0f, 149.0f, 96.0f },
    }};

    inline constexpr float kRevealThreshold = 0.05f;

    inline const SlotRect& GetSlotRect(int slotIndex)
    {
        return kSlotRects[static_cast<size_t>(slotIndex)];
    }

    inline bool IsVisibleHit(float screenX, float screenY, float trayReveal)
    {
        if (trayReveal <= kRevealThreshold)
        {
            return false;
        }

        for (const auto& slot : kSlotRects)
        {
            if (screenX >= slot.x &&
                screenX <= slot.x + slot.width &&
                screenY >= slot.y &&
                screenY <= slot.y + slot.height)
            {
                return true;
            }
        }

        return false;
    }

    inline int FindUnlockedSlotIndexAt(float screenX, float screenY, float trayReveal, int unlockedSlotCount)
    {
        if (trayReveal <= kRevealThreshold)
        {
            return -1;
        }

        const int clampedUnlockedSlotCount = std::clamp(unlockedSlotCount, 0, static_cast<int>(kSlotRects.size()));
        for (int slotIndex = 0; slotIndex < clampedUnlockedSlotCount; ++slotIndex)
        {
            const auto& slot = kSlotRects[static_cast<size_t>(slotIndex)];
            if (screenX >= slot.x &&
                screenX <= slot.x + slot.width &&
                screenY >= slot.y &&
                screenY <= slot.y + slot.height)
            {
                return slotIndex;
            }
        }

        return -1;
    }
}
