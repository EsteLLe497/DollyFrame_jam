#pragma once

#include <vector>

#include "game_scene_photo_state.h"

class GameScene;

namespace photo_system_bridge
{
void HandleCaptureBridge(GameScene& scene);
void HandleSpawnBridge(GameScene& scene);
void DrawPlacementPreviewBridge(const GameScene& scene);

std::vector<CapturedPhotoItem> BuildPlacementItemsBridge(
    const PhotoCaptureState& capture,
    const PhotoPlacementState& placement,
    int whiteTexture,
    float& outWidth,
    float& outHeight);

void ApplyPreviewFilterThemeBridge(CapturedPhotoItem& item);

void DrawCapturedPhotoItemBridge(
    int fallbackTextureId,
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha);
}
