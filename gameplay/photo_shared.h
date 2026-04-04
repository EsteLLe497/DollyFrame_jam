#pragma once

#include <vector>

#include "game_scene_photo_state.h"

namespace photo_shared
{
std::vector<CapturedPhotoItem> BuildPlacementItems(
    const PhotoCaptureState& capture,
    const PhotoPlacementState& placement,
    int whiteTexture,
    float& outWidth,
    float& outHeight);

void ApplyPreviewFilterTheme(CapturedPhotoItem& item);

bool DrawDamagePlatformItemPreview(
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha);

bool DrawSpikeStripItemPreview(
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha);

void DrawCapturedPhotoItem(
    int fallbackTextureId,
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha);
}
