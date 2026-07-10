#pragma once
#include "asset_manifest.h"
#include "event_bus.h"
#include "scene.h"
#include <string>

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
    void DrawFreeImages(float offsetX = 0.0f) const;
    EventBus* GetEventBus() override;
    void DrawCapturedPhotosGrid(float offsetX = 0.0f) const;

private:
    void DrawBackdrop(float offsetX) const;
    void DrawMenu(float offsetX) const;
    void DrawResultFilmFrame() const;
    void UpdateMenuInput();
    void ConfirmSelection();
    float GetIntroOffsetX() const;

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
    float m_introTimer;
    bool m_showPrompt;
    // Primary menu option resolved from the last played stage and end reason.
    std::string m_primaryOptionLabel;
    std::string m_primaryOptionMapCsv;
    int m_selectedOption = 0; // 0: ruins�֐i��, 1: �^�C�g���֖߂�
};
