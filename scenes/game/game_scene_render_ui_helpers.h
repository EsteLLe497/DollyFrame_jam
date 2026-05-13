#pragma once

#include "game_scene_internal.h"
#include "photo_shared.h"

namespace game_scene_detail
{
    struct RgbColor
    {
        int r;
        int g;
        int b;
    };

    RgbColor GetEditorMarkerColor(char marker);

    void UpdatePadCursor(
        float mouseWorldX,
        float mouseWorldY,
        bool mouseMoved,
        float rightX,
        float rightY,
        float dt,
        float& cursorWorldX,
        float& cursorWorldY,
        float& velocityX,
        float& velocityY,
        float& lastPadInputSeconds,
        float nowSeconds);

    const char* GetStageGuideText(float playerX);

    void DrawCapturedPreviewItem(
        int fallbackTextureId,
        const CapturedPhotoItem& item,
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        float alpha);
}
