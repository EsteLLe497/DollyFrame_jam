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
    void DrawMenu() const;
    void UpdateMenuInput();
    void ConfirmSelection();

    struct MenuOptionRect
    {
        int left;
        int top;
        int right;
        int bottom;
    };
    MenuOptionRect GetOptionRect(int index) const;

    AssetManifest m_assets;
    EventBus m_eventBus;
    int m_whiteTexture;
    float m_blinkTimer;
    bool m_showPrompt;
    int m_selectedOption = 0; // 0: ruinsへ進む, 1: タイトルへ戻る
};