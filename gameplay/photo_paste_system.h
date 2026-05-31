#pragma once

class GameScene;
class Entity;

class PhotoPasteSystem
{
public:
    static void HandleSpawn(GameScene& scene);
    static void DrawPlacementPreview(const GameScene& scene);

private:
    static int GetPhotoTraySlotAt(const GameScene& scene, float screenX, float screenY);
    static void BeginPhotoPlacement(GameScene& scene, bool draggingFromTray);
    static void CancelPhotoPlacement(GameScene& scene);

    static bool UpdatePlacementPreview(
        GameScene& scene,
        bool confirmDrop,
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
