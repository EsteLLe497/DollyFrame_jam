#pragma once

#include <array>
#include <cstddef>

namespace game_scene_hp_ui_layout
{
    struct SlotRect
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    inline constexpr std::array<SlotRect, 5> kHpSlotRects =
    {{
        { 244.0f, 19.0f, 100.0f, 96.0f },
        { 360.0f, 19.0f, 100.0f, 96.0f },
        { 476.5f, 19.0f, 100.0f, 96.0f },
        { 592.5f, 19.0f, 100.0f, 96.0f },
        { 708.0f, 19.0f, 100.0f, 96.0f },
    }};

    inline const SlotRect& GetHpSlotRect(int slotIndex)
    {
        return kHpSlotRects[static_cast<size_t>(slotIndex)];
    }
}
