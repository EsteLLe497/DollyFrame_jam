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
    void DrawFreeImages(float offsetX) const;
    EventBus* GetEventBus() override;
    void DrawCapturedPhotosGrid(float offsetX) const;
    void DrawPhotoRevealAnimation(float offsetX) const;
    void DrawResultVignette(float centerX, float centerY, int alpha) const;

private:
    struct MenuOptionRect
    {
        int left;
        int top;
        int right;
        int bottom;
    };

    void DrawBackdrop(float offsetX) const;
    void DrawMenu(float offsetX) const;
    void DrawResultFilmFrame() const;
    float GetIntroOffsetX() const;
    float GetIntroProgress() const;
    void UpdateMenuInput();
    void ConfirmSelection();
    MenuOptionRect GetOptionRect(int index) const;

    void UpdateConfirmDialogInput();
    void ConfirmDialogSelection();
    MenuOptionRect GetConfirmDialogOptionRect(int index) const;
    void DrawConfirmDialog() const;

    AssetManifest m_assets;
    EventBus m_eventBus;
    int m_whiteTexture;
    float m_blinkTimer;
    // Update() が呼ばれない間（アプリ側のシーン遷移フェード中）も進行するよう、
    // deltaTime の積算ではなく GetNowCount() を基準にした開始時刻で管理する。
    int m_introStartTimeMs;
    bool m_showPrompt;
    // Primary menu option resolved from the last played stage and end reason.
    std::string m_primaryOptionLabel;
    std::string m_primaryOptionMapCsv;
    int m_selectedOption = 0; // 0: ruins
    bool m_confirmDialogOpen = false;
    bool m_showUnderBossOption = false;
    std::string m_pendingConfirmMapCsv; // 確認ダイアログで「はい」を選んだ時に遷移する先
    int m_confirmDialogSelection = 0; // 0: はい, 1: いいえ
};