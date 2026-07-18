#pragma once
#include "asset_manifest.h"
#include "event_bus.h"
#include "scene.h"
#include <array>
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
    bool OnCancelAction() override;
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
    int GetRegularOptionCount() const;
    int GetActiveOptionCount() const;
    bool IsMerchantOptionIndex(int index) const;
    const char* GetMenuOptionLabel(int index) const;

    void UpdateMerchantPageInput();
    void ConfirmMerchantPurchase();
    void DrawMerchantPage() const;
    MenuOptionRect GetMerchantItemRect(int index) const;
    int GetMerchantItemCount() const;

    void UpdateConfirmDialogInput();
    void ConfirmDialogSelection();
    MenuOptionRect GetConfirmDialogOptionRect(int index) const;
    void DrawConfirmDialog() const;
    void UpdateAlbumLandingSounds();
    void LoadResultUiTuning();
    bool SaveResultUiTuning() const;

    struct ResultCharaTuning
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 1920.0f;
    };

    AssetManifest m_assets;
    EventBus m_eventBus;
    int m_whiteTexture;
    float m_blinkTimer;
    // Update() が呼ばれない間（アプリ側のシーン遷移フェード中）も進行するよう、
    // deltaTime の積算ではなく GetNowCount() を基準にした開始時刻で管理する。
    int m_introStartTimeMs;
    std::array<bool, 9> m_albumLandingSoundPlayed;
    bool m_showPrompt;
    // Primary menu option resolved from the last played stage and end reason.
    std::string m_primaryOptionLabel;
    std::string m_primaryOptionMapCsv;
    int m_selectedOption = 0; // 0: ruins
    bool m_confirmDialogOpen = false;
    bool m_showUnderBossOption = false;
    std::string m_pendingConfirmMapCsv; // 確認ダイアログで「はい」を選んだ時に遷移する先
    int m_confirmDialogSelection = 0; // 0: はい, 1: いいえ
    bool m_merchantAvailable = false;
    bool m_merchantOffersFolderSlot = false;
    bool m_merchantPageOpen = false;
    int m_merchantSelection = 0;
    float m_merchantMessageTimer = 0.0f;
    std::string m_merchantMessage;
    ResultCharaTuning m_resultChara;
};
