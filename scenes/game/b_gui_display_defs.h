// =========================================================
// ファイルの情報[b_gui_display_defs.h]
//
// 制作者:Masatora Tanaka        日付:2026/07/15
// =========================================================
#pragma once

#include <array>
#include <cstddef>

namespace b_gui
{
    struct DisplayDefinition
    {
        const char* textureKey;
        float worldX;
        float worldY;
        float width;
        float height;
        float triggerCenterX;
        float triggerCenterY;
        float triggerHalfWidth;
        float triggerHalfHeight;
    };

    inline constexpr std::array<DisplayDefinition, 4> kDefaultDisplayDefinitions = {{
        { "b_gui_move_pc", 360.0f, 96.0f, 420.0f, 246.0f, 256.0f, 400.0f, 360.0f, 240.0f },
        { "b_gui_move_pad", 360.0f, 360.0f, 360.0f, 249.0f, 256.0f, 400.0f, 360.0f, 240.0f },
        { "b_gui_jump_pc", 840.0f, 96.0f, 420.0f, 197.0f, 256.0f, 400.0f, 360.0f, 240.0f },
        { "b_gui_jump_pad", 840.0f, 330.0f, 360.0f, 200.0f, 256.0f, 400.0f, 360.0f, 240.0f },
    }};

    inline constexpr size_t kDisplayCount = kDefaultDisplayDefinitions.size();
    inline std::array<DisplayDefinition, kDisplayCount> gDisplayDefinitions = kDefaultDisplayDefinitions;
    inline float gFadeInSpeed = 4.6f;
    inline float gFadeOutSpeed = 3.2f;
    inline bool gShowTriggerRects = false;
}
