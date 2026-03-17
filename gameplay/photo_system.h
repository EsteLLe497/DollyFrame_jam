#pragma once

class GameScene;

namespace photo_system
{
void HandleCapture(GameScene& scene);
void HandleSpawn(GameScene& scene);
void DrawPlacementPreview(const GameScene& scene);
}
