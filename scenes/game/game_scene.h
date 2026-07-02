#pragma once

#include <memory>
#include <string>
#include <vector>

#include "asset_manifest.h"
#include "entity.h"
#include "event_bus.h"
#include "game_object_world.h"
#include "game_session.h"
#include "physics_world.h"
#include "scene.h"
#include "script_engine.h"
#include "game_scene_photo_state.h"
#include "game_scene_state.h"
#include "game_scene_test_photos.h"
#include "tile_map.h"
#include "game_scene_camerawork.h"

class TransformComponent;
class PhotoSystem;
class PhotoCaptureSystem;
class PhotoPasteSystem;
class PrefabFactory;
enum class SwitchPressMode;

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
    GameSceneTuningState& Tuning();
    const GameSceneTuningState& Tuning() const;
    float GetViewScale() const;
    float GetViewWidth() const;
    float GetViewHeight() const;
    float GetViewOriginX() const;
    float GetViewOriginY() const;
    const std::vector<Entity*>& EntitiesByTag(EntityTag tag) const;

    void DrawBackgroundPartsInView(float viewOriginX, float viewOriginY, float viewScale) const;

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

        bool followPlayer = false;
        int cameraNum = 0;
    };

   

    // Core lifecycle / facade
    void ResetSceneState();
    void BeginFrameUpdate(float deltaTime);
    bool TryHandleModalUpdates(float deltaTime);
    bool UpdateTutorialModal(float deltaTime);
    float PrepareGameplayDeltaTime(float deltaTime);
    void TickEntities(float effectiveGameplayDeltaTime);
    void FinalizeGameplayFrame(float effectiveGameplayDeltaTime);
    void TryStartCameraTutorial();
    void CompleteCameraTutorial();
    bool beginTutorialConversation(int tutorialNumber);
    bool loadTutorialData(int tutorialNumber);
    void EnsureTutorialPortraitTexture();
    void PrepareFrameRendering();
    void UpdatePostProcessPlayerLight() const;
    void DrawWorldAndUiLayers();
    void DrawGameWorldLayers();
    void DrawGameUiLayers(bool hideUiForIntroCinematic);
    void ResetFrameRendering();
    bool IsMidBoss3IntroCinematicActive() const;
    bool IsShieldBossIntroCinematicActive() const;

    // Lifecycle / setup
    void UpdateLoading(float deltaTime);
    void AdvanceLoadingStep();
    void FinishLoading();
    void PlayStageBgmForCurrentMap();
    void CrossFadeStageBgmForCurrentMap(float durationSeconds);
    void UpdateShieldBossBgmCue();
    void DrawLoadingScreen() const;
    void LoadTuningState();
    bool SaveUiTuningState();
    bool LoadUiTuningState();
    void RefreshStageRenderProfile();
    void InitializeStageResources(ResourceManager& resources);
    void InitializeStageEntities();
    void InitializeTestPhotoResources(ResourceManager& resources);
    void RefreshTileTextureForCurrentMap();
    void ApplyTileTextureKey(const std::string& tileTextureKey);
    std::string ResolveDefaultTileTextureKeyForCurrentMap() const;
    void BuildCameraMarkers();
    void UpdateCameraByMarkers(const TransformComponent& playerTransform, float deltaTime, bool followY = true);
    void ApplyShieldBossSlamCameraWork(float deltaTime);
    void ApplyShieldBossFramingCameraWork(float deltaTime);
    bool TryGetFixedCameraByPlayerPosition(float playerCenterX, float playerCenterY, float& outCameraX, float& outCameraY) const;
    void StartFloorCameraTransition(int directionX, int directionY);

    // Entity query / spawn
    Entity& SpawnStagePrefab(PrefabFactory& prefabs, const char* prefabId, float x, float y);
    void ApplyMidBoss2TuningToActiveBosses();
    Entity* FindEntityByTag(const char* tag) const;
    Entity* FindEntityByTag(EntityTag tag) const;

    // Gameplay pipeline
    void UpdatePlayer(float deltaTime);
    void UpdateBarrels(float deltaTime);
    void UpdateFallingRocks(float deltaTime);
    void UpdateHangingGravityObjects(float deltaTime);
    void UpdateJumpPads(float deltaTime);
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
    bool IsBatteryOnTopOfSwitchOrDynamicEntity(const TransformComponent& bounds, const Entity* self, float tileSize) const;
    bool SnapBatteryToSwitchOrDynamicEntity(TransformComponent& bounds, const Entity* self, float tileSize) const;
    float GetBatteryPushDirectionFromPlayer(const TransformComponent& playerTransform, const TransformComponent& batteryTransform) const;
    bool IsConveyorUnderBattery(const TransformComponent& batteryTransform, float tileSize, int& outDirectionX,float& velocityX) const;
    void BuildPlayerSolidObjectBounds(std::vector<TransformComponent>& bounds) const;
    void UpdateLinkedGimmicks(float deltaTime);
    void UpdateMerchants(float deltaTime);
    void UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame);
    void UpdatePlayerAfterimages(float deltaTime);
    void TrySpawnPlayerAfterimage(const TransformComponent& transform);
    bool SnapEnemyToGround(TransformComponent& transform) const;
    void ConfigureWalkerSpriteAnimation(Entity& enemy);
    void ConfigureRangedSpriteAnimation(Entity& enemy);
    void ConfigureShieldBossSpriteAnimation(Entity& enemy);
    void ConfigureBossShieldSpriteAnimation(Entity& shield);
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
    void HandleGlobalSceneShortcuts(float deltaTime);
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
    void RefreshJumpPadsFromMarkers();
    void RefreshMarkerLightsFromMarkers();
    void RefreshStageLightsFromMarkers();
    void RefreshLaserTurretsFromMarkers();
    void RefreshLinkedGimmicksFromMarkers();
    void RefreshProtectiveWallsFromMarkers();
    void RefreshDamageFootholdsFromMarkers();
    void RefreshVanishObjectsFromMarkers();
    void RefreshConveyorBeltsFromMarkers();
	void RefleshSepiaRubblesFromMarkers();
    void ReflashFallingRockfromMarkers();
    void RefreshHangingGravityObjectsFromMarkers();
    void ResetHangingGravityObjectsForRespawn();
    void RefreshMarkerDrivenSystems();
    void RefreshMarkerDrivenSystemsByMarkerChange(char before, char after);
    void UpdateEscapeMenuInput();
    void UpdateMerchantShopInput();
    void ToggleEscapeMenuBgm();
    bool UpdatePitRestartFlow(float deltaTime);
    bool UpdateStageTransitionFlow(float deltaTime);
    void UpdateFrameTimers(float deltaTime, float gameplayDeltaTime, float effectiveGameplayDeltaTime);
    void StartCameraFlashPulse(float durationSeconds);
    void RunGameplayFrame(float gameplayDeltaTime);
    void UpdateGameplayActors(float gameplayDeltaTime);
    void ResolveGameplayOutcomes(float gameplayDeltaTime);
    void FlushPendingEntities();
    void SpawnBatterySwitchMarker(float x, float y, int requiredBatteryCount, bool controlsLaserPower, int linkId, float tileSize, SwitchPressMode pressMode);
    void SpawnBatteryGeneratorMarker(float x, float y, int linkId, int spawnDirectionX, float tileSize);
    void SpawnGearMarker(float x, float y, int gearNo, float tileSize);
    void SpawnGearSocketMarker(float x, float y, int gearNo, int requiredGearCount, int linkId, float tileSize);
    void SpawnConveyorBeltMarker(float x, float y, int widthTiles, int directionX, float tileSize);
    void SpawnElevatorMarker(float x, float y, int moveRangeTiles, float widthTiles, int linkId, float tileSize);
    void SpawnLaserSwitchMarker(float x, float y, int linkId, float tileSize);
    void SpawnShutterMarker(float x, float y, float widthTiles, float heightTiles, float moveRangeXTiles, float moveRangeYTiles, int linkId, bool useBossDefeatSignal, bool opensWhenUnpowered, float tileSize);
    void SpawnProtectiveWallMarker(float x, float y, int durability, int linkId, int widthTiles, int markerHeightTiles, int heightTiles, float tileSize);

    // Photo control / photo runtime
    void HandleEnemyPlayerCollisions(Entity& player);
    void HandleWalkerMeleeAttackCollisions();
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
    void UpdateSepiaRestoredLifetimes(float deltaTime);
    void ApplyHazardDamageToPlayer(Entity& player, Entity* sourceEntity, const char* logMessage, int amount = 1);
    void HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage, int amount = 1);
    void HandleEnemyDamage(Entity& enemy, Entity* sourceEntity, int amount, const char* logMessage);
    void ActivateCheckpoint(Entity& player, Entity& checkpoint);
    void RespawnPlayer(Entity& player);
    void StartPitRestart(Entity* player, const char* logMessage);
    void SpawnBarrelBreakEffect(float x, float y, float width, float height);
    void SpawnSlamImpactEffect(float centerX, float groundY, float width);
    void SpawnBossDefeatStartEffect(float centerX, float groundY, float width);
    void SpawnRushSmokeEffect(float centerX, float groundY, float direction);
    void SpawnLightLandingEffect(float centerX, float groundY, float width);
    void SpawnBossRoarEffect(float centerX, float groundY, float width);
    void SpawnMidBoss2SpearFadeEffect(float centerX, float centerY, float width, float height);
    void SpawnTeleportTrailEffect(
        float fromX,
        float fromY,
        float toX,
        float toY,
        float width,
        float height,
        const MidBoss2Component::Params& params);
    void SpawnMidBoss3FistImpactEffect(float x, float y, float width, float height);
    void QueueResult(GameEndReason reason);

    // Effects / UI overlays
    void UpdateEffects(float deltaTime);
    void UpdateTuningPanel();
    void DrawTuningPanel();
    void DrawUiAdjustmentWindow();
    void DrawTutorialAdjustmentPanel();
    void DrawTutorialOverlay();
    void DrawMidBoss2DebugWindow();
    void DrawProgressSavePanel();
    void DrawPitRestartOverlay() const;
    void DrawStageDarknessOverlay() const;
    void DrawSepiaFilmFilterOverlay() const;
    void DrawShieldBossSlamVignetteOverlay() const;
    void DrawShieldBossIntroCurtainOverlay() const;
    void DrawMarkerLightOutlines() const;
    void DrawEffects() const;
    void DrawEnemyAttackRects() const;
    void DrawTestPhotos() const;
    void DrawTestPhotoPanel();
    void DrawCaptureOverlay() const;
    void DrawDevelopedPhotoPreview() const;
    void DrawPhotoStorageTray() const;
    void DrawPhotoPlacementPreview() const;
    void DrawPhotoBoxesByLayer(PhotoCopyLayer layer) const;
    void DrawBossShockwavesUnderlay() const;
    void DrawPastedEntitiesFront() const;
    void DrawPlayerHpBar() const;
    void DrawPartsHud() const;
    void DrawMidBoss2HpBar() const;
    void DrawMidBoss3HpBar() const;
    void DrawAttackCaptureSlot() const;
    void DrawMerchantPrompts() const;
    void DrawMerchantShopOverlay() const;
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
    void DrawMidBoss2TeleportSlotsInView(float viewOriginX, float viewOriginY, float viewScale) const;
    void DrawStageGuideInView() const;
    void DrawPhotoFilterPanelInView() const;
    bool SaveProgressState();
    bool LoadProgressStateFromDisk();
    void ApplyLoadedProgressState();

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
    int m_tileTexture2;
    int m_tileTexture3;
    int m_tileTexture4;
    EventBus m_eventBus;
    PhysicsWorld m_physicsWorld;
    ScriptEngine m_scriptEngine;
    TileMap m_tileMap;
    GameObjectWorld m_world;
    PhotoState m_photo;
    GameSceneFlowState m_flow;
    GameScenePlayerState m_player;
    GameSceneDebugState m_debug;
    GameSceneEffectsState m_effects;
    GameSceneMapEditorState m_mapEditor;
    GameSceneUiState m_ui;
    GameSceneRenderState m_render;
    GameSceneTuningState m_tuning;
    GameSceneSaveState m_save;
    GameSceneTestPhotoState m_testPhotos;
    GameSceneTutorialState m_tutorial;
    struct CameraRuntimeState
    {
        std::vector<fixedCameraRange> fixedRanges;
        int backdropTextureId = -1;
        int backdropTexture1Id = -1;
        std::vector<CameraTransitionMarker> transitionMarkers;
        bool hasPreviousPlayerCameraProbe = false;
        float previousPlayerCameraProbeX = 0.0f;
        float previousPlayerCameraProbeY = 0.0f;
        bool hasCameraSmoothedPlayerY = false;
        float cameraSmoothedPlayerCenterY = 0.0f;
        bool floorCameraTransitionActive = false;
        float floorCameraTransitionElapsed = 0.0f;
        float floorCameraTransitionDuration = 0.45f;
        float floorCameraTransitionStartX = 0.0f;
        float floorCameraTransitionStartY = 0.0f;
        float floorCameraTransitionTargetX = 0.0f;
        float floorCameraTransitionTargetY = 0.0f;
        bool cameraFixedLockActive = false;
        float cameraFixedLockStartX = 0.0f;
        float cameraFixedLockEndX = 0.0f;
        float cameraFixedLockX = 0.0f;
        float cameraFixedLockY = 0.0f;
        bool midBoss3CameraYLockInitialized = false;
        float midBoss3CameraYLock = 0.0f;
        int prevCameraIndex = -1;
        bool easingActive = false;
        float easingElapsedTime = 0.0f;
        float easingStartX = 0.0f;
        float easingStartY = 0.0f;
        float easingTargetX = 0.0f;
        float easingTargetY = 0.0f;
        float shieldBossCameraOffsetX = 0.0f;
        float shieldBossCameraOffsetY = 0.0f;
        float shieldBossCameraBaseY = 0.0f;
        float shieldBossDistanceZoomScale = 1.0f;
        float shieldBossSideChangeTimer = 0.0f;
        int shieldBossCameraSide = 1;
        int shieldBossPendingCameraSide = 1;
        int shieldBossZoomTier = 0;
        bool shieldBossCameraBaseYInitialized = false;
    };

    CameraRuntimeState m_camera;
    GameSceneLifecycleState m_lifecycle;
    static constexpr float easingTime = 0.35f;
};
