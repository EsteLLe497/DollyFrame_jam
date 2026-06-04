#pragma once

#include "game_object_fwd.h"

class GameScene;

class PhotoCaptureSystem
{
public:
    static void HandleCapture(GameScene& scene);

private:
    static void CaptureEntitiesInFrame(
        GameScene& scene,
        float frameX,
        float frameY,
        float frameWidth,
        float frameHeight,
        float& capturedMaxRight,
        float& capturedMaxBottom,
        bool& restoredSepiaBackground);

    static void CaptureTilesInFrame(
        GameScene& scene,
        float frameX,
        float frameY,
        float frameWidth,
        float frameHeight,
        float& capturedMaxRight,
        float& capturedMaxBottom);

    static void FinalizeCapturedPhoto(
        GameScene& scene,
        Entity& player,
        float frameWidth,
        float frameHeight);
};
