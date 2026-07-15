#pragma once

#include "event_bus.h"
#include "scene.h"

#include <array>
#include <cstddef>

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
        StageSelect,
    };

    struct MenuOptionRect
    {
        int left;
        int top;
        int right;
        int bottom;
    };

    struct TitleParticle
    {
        float x;
        float y;
        float speed;
        float driftAmplitude;
        float driftSpeed;
        float size;
        float phase;
        float alpha;
    };

    static constexpr size_t kTitleParticleCount = 72;

    void DrawBackdrop() const;
    void DrawMenu() const;
    void DrawMainMenu() const;
    void DrawOptionsMenu() const;
    void DrawStageSelectMenu() const;
    void DrawStartTransition() const;
    void DrawTitleParticles() const;
    void GetTitleShakeOffset(float& x, float& y) const;
    void GetTitleImageRect(float& x, float& y, float& width, float& height) const;
    MenuOptionRect GetMainMenuOptionRect(int index) const;
    MenuOptionRect GetOptionsMenuOptionRect(int index) const;
    MenuOptionRect GetStageSelectOptionRect(int index) const;
    bool IsPointInsideMenuOption(const MenuOptionRect& rect, int x, int y) const;
    void InitializeTitleParticles();
    void ResetTitleParticle(size_t index, float y);
    void UpdateTitleParticles(float deltaTime);
    void UpdateMenuInput();
    void BeginStartTransition(const char* sceneId);
    void ConfirmMainMenu();
    void ConfirmOptionsMenu();
    void ConfirmStageSelectMenu();
    void PublishSceneChange(const char* sceneId);
    void ToggleBgm();

    EventBus m_eventBus;
    int m_whiteTexture;
    std::array<int, 4> m_titleLayerTextures;
    std::array<int, 4> m_titleLayerTextureWidths;
    std::array<int, 4> m_titleLayerTextureHeights;
    std::array<TitleParticle, kTitleParticleCount> m_titleParticles;
    float m_blinkTimer;
    float m_sceneTime;
    bool m_showPrompt;
    MenuMode m_menuMode;
    int m_menuSelection;
    int m_stageSelection;
    int m_optionsSelection;
    bool m_bgmEnabled;
    float m_bgmRestoreVolume;
    bool m_startTransitionActive;
    bool m_startTransitionSceneRequested;
    bool m_loadingPreviewRequested;
    float m_startTransitionTimer;
    const char* m_startTransitionSceneId;
};
