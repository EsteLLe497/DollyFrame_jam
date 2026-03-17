#pragma once

#include <memory>
#include <filesystem>
#include <vector>

#include "asset_manifest.h"
#include "components.h"
#include "entity.h"
#include "event_bus.h"
#include "game_session.h"
#include "physics_world.h"
#include "scene.h"
#include "script_engine.h"
#include "tile_map.h"

class TransformComponent;
class PhotoSystem;

struct CapturedPhotoItem
{
    int textureId = -1;
    PhotoCopyRole role = PhotoCopyRole::Solid;
    PhotoCopyLayer layer = PhotoCopyLayer::Foreground;
    PhotoCopyOrigin origin = PhotoCopyOrigin::Generic;
    PhotoFilterTheme appliedTheme = PhotoFilterTheme::None;
    float relativeX = 0.0f;
    float relativeY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float sourceX = 0.0f;
    float sourceY = 0.0f;
    float sourceWidth = 1.0f;
    float sourceHeight = 1.0f;
    float tintR = 1.0f;
    float tintG = 1.0f;
    float tintB = 1.0f;
    float tintA = 1.0f;
    bool flipX = false;
};

struct PlayerAfterimage
{
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float scale = 1.0f;
    bool flipX = false;
    float life = 0.0f;
};

struct PhotoCaptureState
{
    bool hasPhoto = false;
    PhotoFilterTheme selectedTheme = PhotoFilterTheme::None;
    PhotoFilterTheme capturedTheme = PhotoFilterTheme::None;
    std::vector<CapturedPhotoItem> items;
    int textureId = -1;
    float width = 64.0f;
    float height = 64.0f;
    float sourceX = 0.0f;
    float sourceY = 0.0f;
    float sourceWidth = 1.0f;
    float sourceHeight = 1.0f;
    float tintR = 0.86f;
    float tintG = 0.92f;
    float tintB = 1.0f;
    float tintA = 1.0f;
};

struct PhotoPlacementState
{
    bool active = false;
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    PhotoCopyLayer layer = PhotoCopyLayer::Foreground;
    bool flipX = false;
    bool bridgeEnabled = false;
};

struct PhotoGroupState
{
    bool hasSpawnedCopy = false;
    int nextGroupId = 1;
    int activeGroupCount = 0;
};

struct PhotoState
{
    PhotoCaptureState capture;
    PhotoPlacementState placement;
    PhotoGroupState groups;
};

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
    Entity* FindEntityByTag(const char* tag) const;
    void UpdatePlayer(float deltaTime);
    void UpdatePlayerAfterimages(float deltaTime);
    void TrySpawnPlayerAfterimage(const TransformComponent& transform);
    void UpdateEnemies();
    void UpdateCameraMode();
    void HandleAttackHits();
    void HandlePhotoCapture();
    void HandlePhotoSpawn();
    void UpdateGoalVisual(float deltaTime);
    void HandleWorldInteractions();
    void RemoveDefeatedEnemies();
    void HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage);
    void QueueResult(GameEndReason reason);
    void UpdateTuningPanel();
    void DrawTuningPanel() const;
    void DrawCaptureOverlay() const;
    void DrawPhotoPlacementPreview() const;
    void DrawPhotoBoxesByLayer(PhotoCopyLayer layer) const;
    void DrawEntity(const Entity& entity) const;
    void DrawBackdrop() const;
    void GetCaptureFrameRect(const TransformComponent& playerTransform, float& x, float& y, float& width, float& height) const;
    Entity* FindCaptureTarget(const TransformComponent& playerTransform) const;
    bool IsSolidTile(int column, int row) const;
    bool IsPlatformTile(int column, int row) const;
    bool IsHazardTile(int column, int row) const;
    bool IsGoalTile(int column, int row) const;
    bool IsStandingOnGround(const TransformComponent& transform) const;
    bool TrySnapToGround(TransformComponent& transform, float maxSnapDistance) const;
    bool IntersectsHazardTile(const TransformComponent& transform) const;
    bool IntersectsGoalTile(const TransformComponent& transform) const;
    bool IntersectsEntity(const Entity& a, const Entity& b) const;
    bool GetEntityBoundsByTag(const char* tag, float& x, float& y, float& width, float& height) const;
    void GetPhotoBoxBounds(std::vector<TransformComponent>& bounds) const;
    bool FindSpawnPosition(float desiredX, float objectWidth, float objectHeight, float& outX, float& outY) const;
    bool IsPhotoPlacementValid(float x, float y, float width, float height) const;
    float GetMapPixelWidth() const;
    float GetMapPixelHeight() const;

    friend class PhotoSystem;

    AssetManifest m_assets;
    int m_whiteTexture;
    int m_tileTexture;
    EventBus m_eventBus;
    PhysicsWorld m_physicsWorld;
    ScriptEngine m_scriptEngine;
    TileMap m_tileMap;
    std::vector<std::unique_ptr<Entity>> m_entities;
    bool m_playerTouchingTarget;
    bool m_playerTouchingHazard;
    bool m_resultQueued;
    bool m_playerGrounded;
    float m_timeLimit;
    float m_timeRemaining;
    float m_cameraX;
    float m_playerVelocityX;
    float m_playerVelocityY;
    float m_goalPulse;
    float m_pickupPulse;
    float m_playerDodgeRemaining;
    float m_playerDodgeCooldownRemaining;
    float m_playerDodgeDirection;
    float m_coyoteTimeRemaining;
    float m_captureSlowRemaining;
    float m_placementSlowRemaining;
    bool m_goalUnlocked;
    bool m_cameraMode;
    int m_enemyCount;
    bool m_playerFacingRight;
    PhotoState m_photo;
    float m_shutterFlashRemaining;
    bool m_showCollisionDebug;
    bool m_showTuningPanel;
    int m_tuningSelection;
    float m_tuningReloadTimer;
    std::filesystem::file_time_type m_tuningFileWriteTime;
    bool m_hasTuningFileWriteTime;
    std::vector<PlayerAfterimage> m_playerAfterimages;
};
