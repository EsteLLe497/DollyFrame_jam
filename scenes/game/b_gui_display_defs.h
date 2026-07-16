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
    enum StageMask
    {
        StageForest = 1 << 0,
        StageRuins = 1 << 1,
    };

    struct DisplayDefinition
    {
        const char* textureKey;
        int stageMask;
        float worldX;
        float worldY;
        float width;
        float height;
        float triggerCenterX;
        float triggerCenterY;
        float triggerHalfWidth;
        float triggerHalfHeight;
    };

    inline constexpr std::array<DisplayDefinition, 7> kDefaultDisplayDefinitions = {{
        { "b_gui_move", StageForest, 536.0f, 2298.0f, 182.0f, 107.0f, 536.0f, 2298.0f, 360.0f, 240.0f },
        { "b_gui_jump", StageForest, 1988.0f, 2222.0f, 188.0f, 108.0f, 1988.0f, 2222.0f, 360.0f, 240.0f },
        { "b_gui_capture", StageForest, 2540.0f, 2180.0f, 220.0f, 128.0f, 2540.0f, 2180.0f, 360.0f, 240.0f },
        { "b_gui_paste", StageForest, 3140.0f, 2180.0f, 220.0f, 128.0f, 3140.0f, 2180.0f, 360.0f, 240.0f },
        { "b_gui_attack_paste", StageForest, 3740.0f, 2180.0f, 260.0f, 128.0f, 3740.0f, 2180.0f, 360.0f, 240.0f },
        { "b_gui_rotate", StageForest, 4340.0f, 2180.0f, 220.0f, 128.0f, 4340.0f, 2180.0f, 360.0f, 240.0f },
        { "b_gui_change_filter", StageRuins, 900.0f, 2200.0f, 260.0f, 128.0f, 900.0f, 2200.0f, 360.0f, 240.0f },
    }};

    inline constexpr size_t kDisplayCount = kDefaultDisplayDefinitions.size();
    inline std::array<DisplayDefinition, kDisplayCount> gDisplayDefinitions = kDefaultDisplayDefinitions;
    inline float gFadeInSpeed = 4.6f;
    inline float gFadeOutSpeed = 3.2f;
    inline bool gShowTriggerRects = false;
}
