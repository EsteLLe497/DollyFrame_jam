#include "photo_system.h"
#include "photo_system_bridge.h"
#include "photo_capture_system.h"
#include "photo_paste_system.h"
#include "photo_shared.h"

namespace photo_system_bridge
{
std::vector<CapturedPhotoItem> BuildPlacementItemsBridge(
    const PhotoCaptureState& capture,
    const PhotoPlacementState& placement,
    int whiteTexture,
    float& outWidth,
    float& outHeight)
{
    return photo_shared::BuildPlacementItems(capture, placement, whiteTexture, outWidth, outHeight);
}

void ApplyPreviewFilterThemeBridge(CapturedPhotoItem& item)
{
    photo_shared::ApplyPreviewFilterTheme(item);
}

void DrawCapturedPhotoItemBridge(
    int fallbackTextureId,
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha)
{
    photo_shared::DrawCapturedPhotoItem(
        fallbackTextureId,
        item,
        drawX,
        drawY,
        drawWidth,
        drawHeight,
        alpha);
}

void HandleCaptureBridge(GameScene& scene)
{
    PhotoCaptureSystem::HandleCapture(scene);
}

void HandleSpawnBridge(GameScene& scene)
{
    PhotoPasteSystem::HandleSpawn(scene);
}

void DrawPlacementPreviewBridge(const GameScene& scene)
{
    PhotoPasteSystem::DrawPlacementPreview(scene);
}
}
