#pragma once

class GameScene;
class Entity;

class PhotoPasteSystem
{
public:
    static void HandleSpawn(GameScene& scene);
    static void DrawPlacementPreview(const GameScene& scene);

private:
    static bool UpdatePlacementPreview(
        GameScene& scene,
        float& spawnX,
        float& spawnY,
        float& spawnWidth,
        float& spawnHeight);

    static void SpawnPhotoGroup(
        GameScene& scene,
        Entity& player,
        float spawnX,
        float spawnY,
        float spawnWidth);
};
