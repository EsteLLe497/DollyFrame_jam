#pragma once

#include "asset_manifest.h"
#include "event_bus.h"
#include "scene.h"

class TitleScene final : public Scene
{
public:
    TitleScene();
    ~TitleScene() override = default;

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
    int m_titleTexture;
    int m_ringTexture;
    int m_burstTexture;
    int m_windTexture;
    int m_cloudTexture;
    int m_laserTexture;
    float m_blinkTimer;
    float m_sceneTime;
    bool m_showPrompt;
};
