#pragma once

#include <memory>
#include <vector>

#include "asset_manifest.h"
#include "entity.h"
#include "event_bus.h"
#include "game_session.h"
#include "physics_world.h"
#include "scene.h"
#include "script_engine.h"
#include "tile_map.h"

class TransformComponent;

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
    void HandlePlayerAttack();
    void UpdateGoalVisual(float deltaTime);
    void HandleWorldInteractions();
    void HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage);
    void QueueResult(GameEndReason reason);
    void DrawEntity(const Entity& entity) const;
    void DrawBackdrop() const;
    bool IsSolidTile(int column, int row) const;
    bool IsPlatformTile(int column, int row) const;
    bool IsHazardTile(int column, int row) const;
    bool IsGoalTile(int column, int row) const;
    bool IsStandingOnGround(const TransformComponent& transform) const;
    bool IntersectsHazardTile(const TransformComponent& transform) const;
    bool IntersectsGoalTile(const TransformComponent& transform) const;
    bool IntersectsEntity(const Entity& a, const Entity& b) const;
    float GetMapPixelWidth() const;
    float GetMapPixelHeight() const;

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
    float m_attackCooldownRemaining;
    float m_attackFlashRemaining;
    float m_jumpBufferRemaining;
    float m_coyoteTimeRemaining;
    bool m_playerFacingRight;
};
