#pragma once

#include <vector>

#include "game_scene_photo_state.h"

namespace photo_geometry
{
    float GetPrintedPhotoWidth(float contentWidth);
    float GetPrintedPhotoHeight(float contentHeight);
    float GetRotatedBoundsWidth(float width, float height, float rotation);
    float GetRotatedBoundsHeight(float width, float height, float rotation);

    struct DamagePlatformPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    bool ClipDamagePlatformPolygonToCrop(
        std::vector<DamagePlatformPoint>& polygon,
        float cropLeft,
        float cropTop,
        float cropRight,
        float cropBottom);

    void RotatePrintedPhotoItems(std::vector<CapturedPhotoItem>& items, float& width, float& height, float rotation);
}
