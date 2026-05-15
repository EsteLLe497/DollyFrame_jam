#include "photo_system.h"
#include "photo_capture_system.h"
#include "photo_paste_system.h"

void photo_system::HandleCapture(GameScene& scene)
{
    PhotoCaptureSystem::HandleCapture(scene);
}

void photo_system::HandleSpawn(GameScene& scene)
{
    PhotoPasteSystem::HandleSpawn(scene);
}

void photo_system::DrawPlacementPreview(const GameScene& scene)
{
    PhotoPasteSystem::DrawPlacementPreview(scene);
}
