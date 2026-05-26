#pragma once

#include <memory>
#include <string>
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
    bool OnCancelAction() override;
    EventBus* GetEventBus() override;

private:
    // Camera marker data
    struct CameraTransitionMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int column = 0;
        int row = 0;
        bool wasInside = false;
    };

    struct CameraFixedRange
    {
        float startX = 0.0f;
        float endX = 0.0f;
        float cameraX = 0.0f;
        float cameraY = 0.0f;

        float backCameraX = 0.0f;
        bool followY = false;
        bool followPlayer = false;
        float viewWidth = 0.0f;
        float viewHeight = 0.0f;
        int cameraNum = 0;
    };

    struct CameraZoomMarker
    {
        float x;
        float y;
        float width;
        float height;

        float viewWidth;
        float viewHeight;

        bool wasInside = false;
        bool isReset = false;
    };

    // Core lifecycle / facade
    void ResetSceneState();
    void BeginFrameUpdate(float deltaTime);
    bool TryHandleModalUpdates(float deltaTime);
    float PrepareGameplayDeltaTime(float deltaTime);
    void TickEntities(float effectiveGameplayDeltaTime);
    void FinalizeGameplayFrame(float effectiveGameplayDeltaTime);
    void PrepareFrameRendering();
    void DrawWorldAndUiLayers();
    void ResetFrameRendering();

    // Lifecycle / setup
    void LoadTuningState();
    void RefreshStageRenderProfile();
    void InitializeStageResources(ResourceManager& resources);
    void InitializeStageEntities();
    void BuildCameraMarkers();
    void UpdateCameraByMarkers(const TransformComponent& playerTransform, float deltaTime);
    bool TryGetFixedCameraByPlayerPosition(float playerCenterX, float playerCenterY, float& outCameraX, float& outCameraY) const;
    void StartFloorCameraTransition(int directionX, int directionY);

    // Entity query / spawn
    Entity& SpawnStagePrefab(PrefabFactory& prefabs, const char* prefabId, float x, float y);
    Entity* FindEntityByTag(const char* tag) const;

    // Gameplay pipeline
    void UpdatePlayer(float deltaTime);
    void UpdateBarrels(float deltaTime);
    void UpdateBatteries(float deltaTime);
    void UpdateLaserTurrets(float deltaTime);
    void UpdateSingleBattery(
        Entity& batteryEntity,
        Entity* player,
        const std::vector<Entity*>& enemies,
        const std::vector<TransformComponent>& groundPlatforms,
        float deltaTime,
        float tileSize);
    bool IsBatteryCollidingWithWorld(const TransformComponent& bounds, const Entity* self, float tileSize) const;
    bool IsBatteryOnTopOfSwitchOrElevator(const TransformComponent& bounds, const Entity* self, float tileSize) const;
    bool SnapBatteryToSwitchOrElevatorTop(TransformComponent& bounds, const Entity* self, float tileSize) const;
    float GetBatteryPushDirectionFromPlayer(const TransformComponent& playerTransform, const TransformComponent& batteryTransform) const;
    void BuildPlayerSolidObjectBounds(std::vector<TransformComponent>& bounds) const;
    void UpdateLinkedGimmicks(float deltaTime);
    void UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame);
    void UpdatePlayerAfterimages(float deltaTime);
    void TrySpawnPlayerAfterimage(const TransformComponent& transform);
    bool SnapEnemyToGround(TransformComponent& transform) const;
    void ConfigureWalkerSpriteAnimation(Entity& enemy);
    void ConfigureRangedSpriteAnimation(Entity& enemy);
    void UpdateEnemies();
    int HandleFinderDefeatGhosts(float frameX, float frameY, float frameWidth, float frameHeight);
    void UpdateBullets();
    void SpawnDropItems(float x, float y, int count); 
    void UpdateDropItems();                            
    int GetEnemyDropCount(EnemyArchetype archetype) const;
    void UpdateCameraMode();
    float UpdatePhotoModes(float deltaTime);
    void UpdateCaptureFinderZoomInput();
    void ProcessFilterInput();
    void UpdateTuningHotReload(float deltaTime);
    void HandleGlobalSceneShortcuts();
    void UpdateMapEditorInput(float deltaTime);
    void UpdateMapEditorStatusMessage(float deltaTime);
    bool HandleMapEditorModeShortcuts();
    void UpdateMapEditorCameraPan(float deltaTime);
    void UpdateMapEditorBrushSelection();
    void HandleMapEditorFileShortcuts(float tileSize);
    void ApplyMapEditorMousePaint(float tileSize);
    void RefreshEnemiesFromMarkers();
    void RefreshBatteriesFromMarkers();
    void RefreshLogsFromMarkers();
    void RefreshMarkerLightsFromMarkers();
    void RefreshStageLightsFromMarkers();
    void RefreshLaserTurretsFromMarkers();
    void RefreshLinkedGimmicksFromMarkers();
    void RefreshProtectiveWallsFromMarkers();
    void RefreshDamageFootholdsFromMarkers();
	void RefleshSepiaRubblesFromMarkers();
    void RefreshMarkerDrivenSystems();
    void RefreshMarkerDrivenSystemsByMarkerChange(char before, char after);
    void UpdateEscapeMenuInput();
    void ToggleEscapeMenuBgm();
    bool UpdatePitRestartFlow(float deltaTime);
    bool UpdateStageTransitionFlow(float deltaTime);
    void UpdateFrameTimers(float deltaTime, float gameplayDeltaTime, float effectiveGameplayDeltaTime);
    void StartCameraFlashPulse(float durationSeconds);
    void RunGameplayFrame(float gameplayDeltaTime);
    void UpdateGameplayActors(float gameplayDeltaTime);
    void ResolveGameplayOutcomes(float gameplayDeltaTime);
    void FlushPendingEntities();

    // Photo control / photo runtime
    void HandleEnemyPlayerCollisions(Entity& player);
    void HandleWalkerMeleeAttackCollisions(Entity& player);
    void UpdateShields(float deltaTime);
    void HandleAttackHits();
    void HandlePhotoCapture();
    void HandlePhotoSpawn();
    void TryUseAttackCaptureSlot();
    void StoreCapturedPhoto();
    void CommitPendingCapturedPhoto();
    void SetSelectedPhotoSlot(int slotIndex);
    void ConsumeSelectedPhotoSlot();
    void UpdatePhotoTraySelection();

    // World flow / result / damage
    void UpdateGoalVisual(float deltaTime);
    void HandleWorldInteractions();
    bool TryQueueStageTransition(Entity& player);
    bool ExecuteStageTransition(const std::string& destinationMapCsv, char spawnMarker, char marker);
    void HandleWorldTileInteractions(Entity& player);
    void HandleWorldEntityInteractions(Entity& player, std::vector<Entity*>& consumedGimmicks);
    void HandlePhotoBoxInteractions(Entity& player, std::vector<Entity*>& consumedPickups, std::vector<Entity*>& defeatedEnemies);
    void RemoveEntitiesByPointerList(const std::vector<Entity*>& entitiesToRemove);
    void RemoveDefeatedEnemies();
    void RefreshPhotoGroupState();
    void ApplyHazardDamageToPlayer(Entity& player, Entity* sourceEntity, const char* logMessage, int amount = 1);
    void HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage, int amount = 1);
    void HandleEnemyDamage(Entity& enemy, Entity* sourceEntity, int amount, const char* logMessage);
    void ActivateCheckpoint(Entity& player, Entity& checkpoint);
    void RespawnPlayer(Entity& player);
    void StartPitRestart(Entity* player, const char* logMessage);
    void SpawnBarrelBreakEffect(float x, float y, float width, float height);
    void QueueResult(GameEndReason reason);

    // Effects / UI overlays
    void UpdateEffects(float deltaTime);
    void UpdateTuningPanel();
    void DrawTuningPanel();
    void DrawPitRestartOverlay() const;
    void DrawStageDarknessOverlay() const;
    void DrawSepiaFilmFilterOverlay() const;
    void DrawMarkerLightOutlines() const;
    void DrawEffects() const;
    void DrawEnemyAttackRects() const;
    void DrawCaptureOverlay() const;
    void DrawDevelopedPhotoPreview() const;
    void DrawPhotoStorageTray() const;
    void DrawPhotoPlacementPreview() const;
    void DrawPhotoBoxesByLayer(PhotoCopyLayer layer) const;
    void DrawBossShockwavesUnderlay() const;
    void DrawPastedEntitiesFront() const;
    void DrawPlayerHpBar() const;
    void DrawAttackCaptureSlot() const;
    void DrawBatterySwitchCounters() const;
    void DrawEscapeMenuOverlay() const;
    void DrawMapEditorOverlay() const;
    void DrawEntity(const Entity& entity) const;
    void DrawBackdrop() const;
    void DrawBackdropBaseInView(float viewOriginX, float viewOriginY, float viewWidth, float viewHeight, float viewScale) const;
    void DrawBackdropGridInView(float viewOriginX, float viewOriginY, float viewWidth, float viewHeight, float viewScale) const;
    void DrawBackdropFrameInView(float viewOriginX, float viewOriginY, float viewWidth, float viewHeight) const;
    void DrawCameraWorldInView(float viewOriginX, float viewOriginY, float viewScale) const;
    void DrawStageTransitionMarkersInView(float viewOriginX, float viewOriginY, float viewScale) const;
    void DrawMapEditorMarkersInView(float viewOriginX, float viewOriginY, float viewScale) const;
    void DrawStageGuideInView() const;
    void DrawPhotoFilterPanelInView() const;

    // Collision / map query helpers
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
    bool TrySnapToGroundUsingPlatforms(
        TransformComponent& transform,
        float maxSnapDistance,
        const std::vector<TransformComponent>& groundPlatforms) const;
    bool TrySnapToGround(TransformComponent& transform, float maxSnapDistance) const;
    bool IntersectsSolidPhotoBox(const TransformComponent& transform) const;
    bool IntersectsSolidPhotoBoxForMovement(const TransformComponent& transform) const;
    bool GetSlopeSurfaceY(int column, int row, float worldX, float& outSurfaceY) const;
    bool IntersectsHazardTile(const TransformComponent& transform) const;
    bool IntersectsPitTile(const TransformComponent& transform) const;
    bool IntersectsGoalTile(const TransformComponent& transform) const;
    bool IntersectsEntity(const Entity& a, const Entity& b) const;
    bool IntersectsHazardEntity(const Entity& player, const Entity& hazard) const;
    bool GetEntityBoundsByTag(const char* tag, float& x, float& y, float& width, float& height) const;
    void GetEntityBoundsByTag(const char* tag, std::vector<TransformComponent>& bounds) const;
    bool IsGroundPlatformEntity(const Entity& entity) const;
    void GetGroundPlatformBounds(std::vector<TransformComponent>& bounds) const;
    void GetPhotoBoxBounds(std::vector<TransformComponent>& bounds) const;
    const Entity* FindNearestMarkerLightEntity(
        const TransformComponent& referenceTransform,
        int linkId = -1,
        bool requireActivated = true) const;
    bool FindSpawnPosition(float desiredX, float objectWidth, float objectHeight, float& outX, float& outY) const;
    bool IsPhotoPlacementValid(float x, float y, float width, float height) const;
    float GetMapPixelWidth() const;
    float GetMapPixelHeight() const;

    friend class PhotoSystem;
    friend class PhotoCaptureSystem;
    friend class PhotoPasteSystem;

    // Runtime state
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
    GameSceneMapEditorState m_mapEditor;
    std::vector<CameraTransitionMarker> m_cameraTransitionMarkers;
    std::vector<CameraFixedRange> m_cameraFixedRanges;
    bool m_hasPreviousPlayerCameraProbe = false;
    float m_previousPlayerCameraProbeX = 0.0f;
    float m_previousPlayerCameraProbeY = 0.0f;
    bool m_floorCameraTransitionActive = false;
    float m_floorCameraTransitionElapsed = 0.0f;
    float m_floorCameraTransitionDuration = 0.45f;
    float m_floorCameraTransitionStartX = 0.0f;
    float m_floorCameraTransitionStartY = 0.0f;
    float m_floorCameraTransitionTargetX = 0.0f;
    float m_floorCameraTransitionTargetY = 0.0f;
    bool m_cameraFixedLockActive = false;
    float m_cameraFixedLockStartX = 0.0f;
    float m_cameraFixedLockEndX = 0.0f;
    float m_cameraFixedLockX = 0.0f;
    float m_cameraFixedLockY = 0.0f;
    bool m_hasPendingStageTransition = false;
    std::string m_pendingStageTransitionMapCsv;
    char m_pendingStageTransitionSpawnMarker = '\0';
    char m_pendingStageTransitionMarker = '\0';
    bool m_darknessStageEnabled = false;

    int m_cameraFixedLockNum = -1;
    void ActivateCameraRange(int cameraNum);
    std::vector<CameraZoomMarker> m_zoomMarkers;
    void RecalculateViewScale();
    bool m_isZoomed = false;
    float m_zoomedViewWidth = 2560.0f;
    float m_zoomedViewHeight = 1440.0f;
};
