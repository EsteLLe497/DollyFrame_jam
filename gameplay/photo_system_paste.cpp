#include "photo_system.h"
#include "photo_system_bridge.h"

namespace photo_system
{
void HandleSpawn(GameScene& scene)
{
    photo_system_bridge::HandleSpawnBridge(scene);
}

void DrawPlacementPreview(const GameScene& scene)
{
    photo_system_bridge::DrawPlacementPreviewBridge(scene);
}
}
