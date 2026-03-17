#pragma once

#include <memory>
#include <vector>

#include "asset_manifest.h"
#include "entity.h"
#include "event_bus.h"
#include "physics_world.h"
#include "scene.h"
#include "script_engine.h"

class DemoScene final : public Scene
{
public:
    DemoScene();
    ~DemoScene() override = default;

    const char* GetSceneId() const override;
    void OnEnter(ResourceManager& resources) override;
    void OnExit() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawDebugUI() override;
    EventBus* GetEventBus() override;

private:
    Entity* FindEntityByTag(const char* tag) const;
    void ProcessEvents();
    void DrawBackdrop() const;

    AssetManifest m_assets;
    int m_whiteTexture;
    EventBus m_eventBus;
    PhysicsWorld m_physicsWorld;
    ScriptEngine m_scriptEngine;
    std::vector<std::unique_ptr<Entity>> m_entities;
    bool m_playerTouchingTarget;
};
