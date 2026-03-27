#pragma once

#include <memory>
#include <filesystem>
#include <vector>

#include "asset_manifest.h"
#include "entity.h"
#include "event_bus.h"
#include "game_session.h"
#include "physics_world.h"
#include "scene.h"
#include "script_engine.h"
#include "game_scene_photo_state.h"
#include "game_scene_state.h"
#include "tile_map.h"

class TransformComponent;
class PhotoSystem;
class PhotoCaptureSystem;
class PhotoPasteSystem;
class PrefabFactory;

class GameScene final : public Scene
{
public:
    GameScene();
    ~GameScene() override = default;

    const char* GetSceneId() const override;
    void OnEnter(ResourceManager& resources) override;
    void OnExit() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawDebugUI() override;
    EventBus* GetEventBus() override;

private:
    void ResetSceneState();
    void LoadTuningState();
    void InitializeStageResources(ResourceManager& resources);
    void InitializeStageEntities();
    Entity& SpawnStagePrefab(PrefabFactory& prefabs, const char* prefabId, float x, float y);
    Entity* FindEntityByTag(const char* tag) const;
    void UpdatePlayer(float deltaTime);
    void UpdateBarrels(float deltaTime);
    void UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame);
    void UpdatePlayerAfterimages(float deltaTime);
    void TrySpawnPlayerAfterimage(const TransformComponent& transform);
    bool SnapEnemyToGround(TransformComponent& transform) const;
    void UpdateEnemies();
    void UpdateBullets();
    void SpawnDropItems(float x, float y, int count); // 3/23í«â¡(ìcîVè„èr)
    void UpdateDropItems();                            // 3/23í«â¡(ìcîVè„èr)
    int GetEnemyDropCount(EnemyArchetype archetype) const;
    void UpdateCameraMode();
    float UpdatePhotoModes(float deltaTime);
    void UpdateCaptureFinderZoomInput();
    void ProcessFilterInput();
    void RunGameplayFrame(float gameplayDeltaTime);
    void HandleEnemyPlayerCollisions(Entity& player);
    void HandleAttackHits();
    void HandlePhotoCapture();
    void HandlePhotoSpawn();
    void StoreCapturedPhoto();
    void CommitPendingCapturedPhoto();
    void SetSelectedPhotoSlot(int slotIndex);
    void ConsumeSelectedPhotoSlot();
    void UpdatePhotoTraySelection();
    void UpdateGoalVisual(float deltaTime);
    void HandleWorldInteractions();
    void HandleWorldTileInteractions(Entity& player);
    void HandleWorldEntityInteractions(Entity& player, std::vector<Entity*>& consumedGimmicks);
    void HandlePhotoBoxInteractions(Entity& player, std::vector<Entity*>& consumedPickups, std::vector<Entity*>& defeatedEnemies);
    void RemoveEntitiesByPointerList(const std::vector<Entity*>& entitiesToRemove);
    void RemoveDefeatedEnemies();
    void RefreshPhotoGroupState();
    void HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage, int amount = 1);
    void HandleEnemyDamage(Entity& enemy, Entity* sourceEntity, int amount, const char* logMessage);
    void ActivateCheckpoint(Entity& player, Entity& checkpoint);
    void RespawnPlayer(Entity& player);
    void StartPitRestart(Entity* player, const char* logMessage);
    void SpawnBarrelBreakEffect(float x, float y, float width, float height);
    void QueueResult(GameEndReason reason);
    void UpdateEffects(float deltaTime);
    void UpdateTuningPanel();
    void DrawTuningPanel();
    void DrawPitRestartOverlay() const;
    void DrawEffects() const;
    void DrawEnemyAttackRects() const;
    void DrawCaptureOverlay() const;
    void DrawDevelopedPhotoPreview() const;
    void DrawPhotoStorageTray() const;
    void DrawPhotoPlacementPreview() const;
    void DrawPhotoBoxesByLayer(PhotoCopyLayer layer) const;
    void DrawPlayerHpBar() const;
    void DrawEntity(const Entity& entity) const;
    void DrawBackdrop() const;
    bool IsPhotoTrayHit(float screenX, float screenY) const;
    void GetCaptureFrameRect(const TransformComponent& playerTransform, float& x, float& y, float& width, float& height) const;
    Entity* FindCaptureTarget(const TransformComponent& playerTransform) const;
    bool IsSolidTile(int column, int row) const;
    bool IsSlopeTile(int column, int row) const;
    bool IsTileBlockingFromLeft(int column, int row) const;
    bool IsTileBlockingFromRight(int column, int row) const;
    bool IsPlatformTile(int column, int row) const;
    bool IsHazardTile(int column, int row) const;
    bool IsPitTile(int column, int row) const;
    bool IsGoalTile(int column, int row) const;
    bool IsStandingOnGround(const TransformComponent& transform) const;
    bool TrySnapToGround(TransformComponent& transform, float maxSnapDistance) const;
    bool IntersectsSolidPhotoBox(const TransformComponent& transform) const;
    bool GetSlopeSurfaceY(int column, int row, float worldX, float& outSurfaceY) const;
    bool IntersectsHazardTile(const TransformComponent& transform) const;
    bool IntersectsPitTile(const TransformComponent& transform) const;
    bool IntersectsGoalTile(const TransformComponent& transform) const;
    bool IntersectsEntity(const Entity& a, const Entity& b) const;
    bool GetEntityBoundsByTag(const char* tag, float& x, float& y, float& width, float& height) const;
    void GetEntityBoundsByTag(const char* tag, std::vector<TransformComponent>& bounds) const;
    void GetPhotoBoxBounds(std::vector<TransformComponent>& bounds) const;
    bool FindSpawnPosition(float desiredX, float objectWidth, float objectHeight, float& outX, float& outY) const;
    bool IsPhotoPlacementValid(float x, float y, float width, float height) const;
    float GetMapPixelWidth() const;
    float GetMapPixelHeight() const;

    friend class PhotoSystem;
    friend class PhotoCaptureSystem;
    friend class PhotoPasteSystem;

    AssetManifest m_assets;
    int m_whiteTexture;
    int m_tileTexture;
    EventBus m_eventBus;
    PhysicsWorld m_physicsWorld;
    ScriptEngine m_scriptEngine;
    TileMap m_tileMap;
    std::vector<std::unique_ptr<Entity>> m_entities;
    std::vector<std::unique_ptr<Entity>> m_pendingEntities;
    PhotoState m_photo;
    GameSceneFlowState m_flow;
    GameScenePlayerState m_player;
    GameSceneDebugState m_debug;
    GameSceneEffectsState m_effects;
};
