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
    enum class MenuMode
    {
        Main,
        Options,
    };

    void DrawBackdrop() const;
    void DrawMenu() const;
    void DrawMainMenu() const;
    void DrawOptionsMenu() const;
    void UpdateMenuInput();
    void ConfirmMainMenu();
    void ConfirmOptionsMenu();
    void PublishSceneChange(const char* sceneId);
    void ToggleBgm();

    AssetManifest m_assets;
    EventBus m_eventBus;
    int m_whiteTexture;
    int m_titleTexture;
    float m_blinkTimer;
    float m_sceneTime;
    bool m_showPrompt;
    MenuMode m_menuMode;
    int m_menuSelection;
    int m_optionsSelection;
    bool m_bgmEnabled;
    float m_bgmRestoreVolume;
};
