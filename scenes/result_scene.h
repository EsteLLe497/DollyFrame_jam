#pragma once

#include "asset_manifest.h"
#include "event_bus.h"
#include "scene.h"

class ResultScene final : public Scene
{
public:
    ResultScene();
    ~ResultScene() override = default;

    const char* GetSceneId() const override;
    void OnEnter(ResourceManager& resources) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawDebugUI() override;
    EventBus* GetEventBus() override;

private:
    void DrawBackdrop() const;

    AssetManifest m_assets;
    EventBus m_eventBus;
    int m_whiteTexture;
    float m_blinkTimer;
    bool m_showPrompt;
};
