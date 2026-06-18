#pragma once

namespace game_viewport
{
    float ComputeViewScale(
        int screenWidth,
        int screenHeight,
        float cameraViewWidth,
        float cameraViewHeight,
        float viewScaleMultiplier);

    float ComputeViewWidth(float cameraViewWidth, float viewScale);
    float ComputeViewHeight(float cameraViewHeight, float viewScale);

    float ComputeViewOriginX(
        int screenWidth,
        float viewWidth,
        bool zoomAnchorScreenCenter,
        float zoomAnchorX,
        float shakeOffsetX);

    float ComputeViewOriginY(
        int screenHeight,
        float viewHeight,
        bool zoomAnchorScreenCenter,
        float zoomAnchorY,
        float shakeOffsetY);
}
