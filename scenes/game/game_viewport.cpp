#include "pch.h"

#include "game_viewport.h"

#include <algorithm>
#include <cmath>

namespace game_viewport
{
    float ComputeViewScale(
        int screenWidth,
        int screenHeight,
        float cameraViewWidth,
        float cameraViewHeight,
        float viewScaleMultiplier)
    {
        const float marginX = std::clamp(static_cast<float>(screenWidth) * 0.04f, 48.0f, 96.0f);
        const float marginY = std::clamp(static_cast<float>(screenHeight) * 0.04f, 36.0f, 72.0f);
        const float maxWidth = static_cast<float>(screenWidth) - marginX * 2.0f;
        const float maxHeight = static_cast<float>(screenHeight) - marginY * 2.0f;
        return std::min(maxWidth / cameraViewWidth, maxHeight / cameraViewHeight) * viewScaleMultiplier;
    }

    float ComputeViewWidth(float cameraViewWidth, float viewScale)
    {
        return cameraViewWidth * viewScale;
    }

    float ComputeViewHeight(float cameraViewHeight, float viewScale)
    {
        return cameraViewHeight * viewScale;
    }

    float ComputeViewOriginX(
        int screenWidth,
        float viewWidth,
        bool zoomAnchorScreenCenter,
        float zoomAnchorX,
        float shakeOffsetX)
    {
        if (viewWidth >= static_cast<float>(screenWidth))
        {
            if (zoomAnchorScreenCenter)
            {
                return std::round(zoomAnchorX - viewWidth * 0.5f) + shakeOffsetX;
            }
            return shakeOffsetX;
        }

        return std::round((static_cast<float>(screenWidth) - viewWidth) * 0.5f) + shakeOffsetX;
    }

    float ComputeViewOriginY(
        int screenHeight,
        float viewHeight,
        bool zoomAnchorScreenCenter,
        float zoomAnchorY,
        float shakeOffsetY)
    {
        if (viewHeight >= static_cast<float>(screenHeight))
        {
            if (zoomAnchorScreenCenter)
            {
                return std::round(zoomAnchorY - viewHeight * 0.5f) + shakeOffsetY;
            }
            return shakeOffsetY;
        }

        return std::round((static_cast<float>(screenHeight) - viewHeight) * 0.5f) + shakeOffsetY;
    }
}
