#include "pch.h"

#include "title_scene.h"

#include "DxLib.h"
#include "audio.h"
#include "directX.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "game_session.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

namespace
{
    struct StageSelectItem
    {
        const char* label;
        const char* path;
    };

    enum class MainMenuAction
    {
        StartGame,
        StageSelect,
        Options,
        ExitGame,
    };

    struct MainMenuItem
    {
        const char* label;
        MainMenuAction action;
    };

    constexpr int kOptionsMenuItemCount = 4;
    constexpr int kStageSelectItemCount = 10;
    constexpr int kStageSelectColumnCount = 2;
    constexpr int kStageSelectRowCount = (kStageSelectItemCount + kStageSelectColumnCount - 1) / kStageSelectColumnCount;
    constexpr int kMainMenuRowWidth = 340;
    constexpr int kMainMenuRowHeight = 48;
    constexpr int kMainMenuRowGap = 16;
    constexpr int kMainMenuButtonNormalState = 0;
    constexpr int kMainMenuButtonSelectedState = 1;
    constexpr const char* kTitleUiTuningPath = "assets/title_ui_tuning.json";
    constexpr const char* kMainMenuButtonTuningKeys[] = {
        "start",
        "settings",
        "exit",
    };
    constexpr const char* kMainMenuButtonDebugNames[] = {
        "Start",
        "Settings",
        "Exit",
    };
    constexpr const wchar_t* kMainMenuButtonTexturePaths[3][2] = {
        {
            L"assets\\texture\\BG\\title\\UI_Title_GameStartButton_002.png",
            L"assets\\texture\\BG\\title\\UI_Title_GameStartButton_001.png",
        },
        {
            L"assets\\texture\\BG\\title\\UI_Title_SettingButton_002.png",
            L"assets\\texture\\BG\\title\\UI_Title_SettingButton_001.png",
        },
        {
            L"assets\\texture\\BG\\title\\UI_Title_GameEndButton_002.png",
            L"assets\\texture\\BG\\title\\UI_Title_GameEndButton_001.png",
        },
    };
    constexpr float kOptionsMenuPanelWidth = 760.0f;
    constexpr float kOptionsMenuPanelHeight = 460.0f;
    constexpr int kOptionsMenuRowHeight = 52;
    constexpr int kOptionsMenuRowBottomInset = 8;
    constexpr int kOptionsMenuRowStartOffset = 104;
    constexpr int kOptionsMenuRowLeftOffset = 290;
    constexpr int kOptionsMenuRowRightInset = 28;
    constexpr int kStageSelectRowLeft = 146;
    constexpr int kStageSelectRowTop = 386;
    constexpr int kStageSelectRowWidth = 268;
    constexpr int kStageSelectRowHeight = 34;
    constexpr int kStageSelectRowGap = 8;
    constexpr int kStageSelectColumnGap = 18;
    constexpr float kStartTransitionDuration = 0.65f;
    constexpr int kStartTransitionBladeCount = 6;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTitleLogoLayerScale = 0.58f;
    constexpr float kTitleLogoLayerOffsetX = -0.028f;
    constexpr float kTitleLogoLayerOffsetY = 0.048f;
    constexpr float kTitleShakeAmplitudeX = 2.4f;
    constexpr float kTitleShakeAmplitudeY = 1.6f;
    constexpr float kTitleShakeGuard = 8.0f;
    constexpr float kTitleParticleRespawnMargin = 48.0f;
    constexpr const wchar_t* kTitleLayerPaths[] = {
        L"assets\\texture\\BG\\title\\title.png",
        L"assets\\texture\\BG\\title\\logo1.png",
        L"assets\\texture\\BG\\title\\logo2.png",
        L"assets\\texture\\BG\\title\\logo3.png",
    };

    constexpr MainMenuItem kMainMenuItems[] = {
        { "ゲーム開始", MainMenuAction::StartGame },
#if defined(_DEBUG)
        // シーン選択は開発用メニューとして、デバッグビルドだけに表示する。
        { "シーン選択", MainMenuAction::StageSelect },
#endif
        { "設定", MainMenuAction::Options },
        { "ゲームを終了", MainMenuAction::ExitGame },
    };
    constexpr int kMainMenuItemCount = static_cast<int>(sizeof(kMainMenuItems) / sizeof(kMainMenuItems[0]));

    constexpr StageSelectItem kStageSelectItems[kStageSelectItemCount] = {
        { "森", "assets/maps/stages/forest_v2.csv" },
        { "森（ボス）", "assets/maps/stages/forest_boss.csv" },
        { "廃墟", "assets/maps/stages/ruins_v2.csv" },
        { "廃墟（ボス）", "assets/maps/stages/ruins_boss.csv" },
        { "地下", "assets/maps/stages/under.csv" },
        { "地下（ボス）", "assets/maps/stages/under_boss.csv" },
        { "テスト: stage_a", "assets/maps/stages/stage_a.csv" },
        { "テスト: 58x25", "assets/maps/stages/stage_58x25.csv" },
        { "テスト: 58x25_check", "assets/maps/stages/stage_58x25_check.csv" },
        { "テスト: 横スクロール", "assets/maps/stages/side_scroll_stage01.csv" },
    };

    int FindStageSelectIndex(const std::string& path)
    {
        for (int index = 0; index < kStageSelectItemCount; ++index)
        {
            if (path == kStageSelectItems[index].path)
            {
                return index;
            }
        }
        return 0;
    }

    std::string GetStageFileName(const std::string& path)
    {
        try
        {
            return std::filesystem::path(path).filename().string();
        }
        catch (...)
        {
            return path;
        }
    }

    std::string GetStageDisplayName(const std::string& path)
    {
        for (const StageSelectItem& item : kStageSelectItems)
        {
            if (path == item.path)
            {
                return item.label;
            }
        }
        return GetStageFileName(path);
    }

    void DrawOutlinedString(int x, int y, const char* text, int textColor, int outlineColor)
    {
        DrawString(x - 2, y, text, outlineColor);
        DrawString(x + 2, y, text, outlineColor);
        DrawString(x, y - 2, text, outlineColor);
        DrawString(x, y + 2, text, outlineColor);
        DrawString(x - 1, y - 1, text, outlineColor);
        DrawString(x + 1, y - 1, text, outlineColor);
        DrawString(x - 1, y + 1, text, outlineColor);
        DrawString(x + 1, y + 1, text, outlineColor);
        DrawString(x, y, text, textColor);
    }

    void DrawCenteredOutlinedString(int centerX, int y, const char* text, int textColor, int outlineColor)
    {
        const int width = GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
        DrawOutlinedString(centerX - width / 2, y, text, textColor, outlineColor);
    }

    void DrawClassicFrame(int left, int top, int right, int bottom)
    {
        DrawBox(left, top, right, bottom, GetColor(55, 34, 16), TRUE);
        DrawBox(left + 4, top + 4, right - 4, bottom - 4, GetColor(111, 78, 38), TRUE);
        DrawBox(left, top, right, bottom, GetColor(255, 255, 255), FALSE);
        DrawBox(left + 2, top + 2, right - 2, bottom - 2, GetColor(214, 189, 139), FALSE);
        DrawBox(left + 6, top + 6, right - 6, bottom - 6, GetColor(32, 20, 11), FALSE);
        DrawBox(left + 10, top + 10, left + 24, top + 24, GetColor(212, 165, 82), TRUE);
        DrawBox(right - 24, top + 10, right - 10, top + 24, GetColor(212, 165, 82), TRUE);
        DrawBox(left + 10, bottom - 24, left + 24, bottom - 10, GetColor(212, 165, 82), TRUE);
        DrawBox(right - 24, bottom - 24, right - 10, bottom - 10, GetColor(212, 165, 82), TRUE);
    }

    void DrawTitleLogo(int centerX, int top)
    {
        const int shadowBrown = GetColor(36, 20, 8);
        const int deepBrown = GetColor(72, 42, 17);
        const int brass = GetColor(212, 165, 82);
        const int paleIvory = GetColor(246, 232, 206);
        const int copper = GetColor(188, 98, 48);

        DrawBox(centerX - 270, top + 12, centerX + 270, top + 92, GetColor(52, 30, 14), TRUE);
        DrawBox(centerX - 262, top + 20, centerX + 262, top + 84, GetColor(96, 62, 30), TRUE);
        DrawBox(centerX - 258, top + 24, centerX + 258, top + 80, GetColor(25, 16, 11), FALSE);
        DrawBox(centerX - 238, top + 32, centerX - 180, top + 72, brass, TRUE);
        DrawCircle(centerX - 209, top + 52, 20, GetColor(24, 24, 28), TRUE);
        DrawCircle(centerX - 209, top + 52, 12, GetColor(214, 189, 139), FALSE);

        DrawCircle(centerX - 296, top + 48, 24, GetColor(74, 48, 22), TRUE);
        DrawCircle(centerX - 296, top + 48, 16, GetColor(204, 164, 96), FALSE);
        DrawCircle(centerX + 296, top + 48, 24, GetColor(74, 48, 22), TRUE);
        DrawCircle(centerX + 296, top + 48, 16, GetColor(204, 164, 96), FALSE);

        DrawBox(centerX - 108, top + 40, centerX + 108, top + 56, GetColor(194, 150, 80), TRUE);
        DrawBox(centerX - 96, top + 30, centerX + 96, top + 66, GetColor(36, 20, 11), FALSE);
        DrawCircle(centerX - 116, top + 48, 11, copper, TRUE);
        DrawCircle(centerX + 116, top + 48, 11, copper, TRUE);

        DrawCenteredOutlinedString(centerX + 2, top + 3, "DOLLY FRAME", shadowBrown, shadowBrown);
        DrawCenteredOutlinedString(centerX, top, "DOLLY FRAME", brass, deepBrown);
        DrawCenteredOutlinedString(centerX, top + 22, "DOLLY FRAME", paleIvory, deepBrown);
        DrawCenteredOutlinedString(centerX, top + 86, "写真 / カメラ / スチーム", GetColor(255, 244, 220), shadowBrown);
    }

    void DrawCameraSilhouette(int baseY)
    {
        const int dark = GetColor(38, 22, 13);
        const int brass = GetColor(198, 154, 84);
        const int copper = GetColor(176, 92, 44);
        const int ivory = GetColor(240, 226, 201);

        DrawBox(76, baseY - 110, 214, baseY, dark, TRUE);
        DrawBox(92, baseY - 126, 198, baseY - 96, brass, TRUE);
        DrawBox(104, baseY - 148, 186, baseY - 126, copper, TRUE);
        DrawCircle(144, baseY - 62, 44, GetColor(24, 24, 24), TRUE);
        DrawCircle(144, baseY - 62, 28, ivory, TRUE);
        DrawCircle(144, baseY - 62, 18, GetColor(60, 38, 18), TRUE);
        DrawCircle(144, baseY - 62, 8, brass, TRUE);
        DrawBox(180, baseY - 152, 236, baseY - 112, dark, TRUE);
        DrawBox(196, baseY - 168, 220, baseY - 152, brass, TRUE);
        DrawTriangle(196, baseY - 168, 208, baseY - 194, 220, baseY - 168, copper, TRUE);

        DrawBox(670, baseY - 108, 812, baseY, dark, TRUE);
        DrawBox(690, baseY - 124, 792, baseY - 96, brass, TRUE);
        DrawBox(716, baseY - 148, 766, baseY - 124, copper, TRUE);
        DrawCircle(732, baseY - 58, 38, GetColor(24, 24, 24), TRUE);
        DrawCircle(732, baseY - 58, 24, ivory, TRUE);
        DrawCircle(732, baseY - 58, 15, GetColor(60, 38, 18), TRUE);
        DrawCircle(732, baseY - 58, 7, brass, TRUE);
        DrawBox(614, baseY - 164, 678, baseY - 124, dark, TRUE);
        DrawBox(632, baseY - 180, 660, baseY - 164, brass, TRUE);
        DrawTriangle(632, baseY - 180, 646, baseY - 208, 660, baseY - 180, copper, TRUE);

        DrawBox(360, baseY - 90, 488, baseY, dark, TRUE);
        DrawBox(396, baseY - 118, 452, baseY - 90, brass, TRUE);
        DrawCircle(424, baseY - 48, 24, GetColor(24, 24, 24), TRUE);
        DrawCircle(424, baseY - 48, 14, ivory, TRUE);
        DrawCircle(424, baseY - 48, 7, copper, TRUE);
    }

    void DrawMenuRow(int left, int top, int width, int height, const char* label, bool selected)
    {
        const int right = left + width;
        const int bottom = top + height;
        const int fillColor = selected ? GetColor(131, 92, 51) : GetColor(66, 44, 22);
        const int lineColor = selected ? GetColor(241, 205, 132) : GetColor(188, 148, 82);
        const int textColor = selected ? GetColor(255, 246, 230) : GetColor(236, 216, 182);

        DrawBox(left, top, right, bottom, fillColor, TRUE);
        DrawBox(left, top, right, bottom, lineColor, FALSE);
        if (selected)
        {
            DrawCircle(left + 18, top + 16, 5, GetColor(241, 205, 132), TRUE);
            DrawCircle(left + 18, top + 16, 2, GetColor(52, 30, 14), TRUE);
        }
        DrawOutlinedString(left + 52, top + 11, label, textColor, GetColor(28, 16, 9));
    }

}

TitleScene::TitleScene()
    : m_whiteTexture(-1)
    , m_titleLayerTextures{ -1, -1, -1, -1 }
    , m_titleLayerTextureWidths{ 0, 0, 0, 0 }
    , m_titleLayerTextureHeights{ 0, 0, 0, 0 }
    , m_mainMenuButtonTextures{}
    , m_mainMenuButtonTextureWidths{}
    , m_mainMenuButtonTextureHeights{}
    , m_mainMenuButtonTunings{}
    , m_blinkTimer(0.0f)
    , m_sceneTime(0.0f)
    , m_showPrompt(true)
    , m_menuMode(MenuMode::Main)
    , m_menuSelection(0)
    , m_stageSelection(0)
    , m_optionsSelection(0)
    , m_bgmEnabled(true)
    , m_bgmRestoreVolume(1.0f)
    , m_startTransitionActive(false)
    , m_startTransitionSceneRequested(false)
    , m_loadingPreviewRequested(false)
    , m_startTransitionTimer(0.0f)
    , m_startTransitionSceneId(nullptr)
{
    for (auto& buttonTextures : m_mainMenuButtonTextures)
    {
        buttonTextures.fill(-1);
    }
    ResetTitleUiTuning();
}

const char* TitleScene::GetSceneId() const
{
    return "title";
}

void TitleScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    m_whiteTexture = resources.CreateSolidTexture(1, 1, 0xFFFFFFFF);
    for (size_t index = 0; index < m_titleLayerTextures.size(); ++index)
    {
        m_titleLayerTextures[index] = resources.LoadTexture(kTitleLayerPaths[index]);
        m_titleLayerTextureWidths[index] = 0;
        m_titleLayerTextureHeights[index] = 0;
        if (m_titleLayerTextures[index] >= 0)
        {
            m_titleLayerTextureWidths[index] = TextureGetWidth(m_titleLayerTextures[index]);
            m_titleLayerTextureHeights[index] = TextureGetHeight(m_titleLayerTextures[index]);
        }
        if (m_titleLayerTextures[index] < 0 ||
            m_titleLayerTextureWidths[index] <= 0 ||
            m_titleLayerTextureHeights[index] <= 0)
        {
            char message[160] = {};
            std::snprintf(message, sizeof(message), "TitleScene title layer texture could not be loaded: layer %zu", index);
            Logger::Warn(message);
        }
    }
    for (size_t buttonIndex = 0; buttonIndex < m_mainMenuButtonTextures.size(); ++buttonIndex)
    {
        for (size_t stateIndex = 0; stateIndex < m_mainMenuButtonTextures[buttonIndex].size(); ++stateIndex)
        {
            m_mainMenuButtonTextures[buttonIndex][stateIndex] =
                resources.LoadTexture(kMainMenuButtonTexturePaths[buttonIndex][stateIndex]);
            m_mainMenuButtonTextureWidths[buttonIndex][stateIndex] = 0;
            m_mainMenuButtonTextureHeights[buttonIndex][stateIndex] = 0;
            if (m_mainMenuButtonTextures[buttonIndex][stateIndex] >= 0)
            {
                m_mainMenuButtonTextureWidths[buttonIndex][stateIndex] =
                    TextureGetWidth(m_mainMenuButtonTextures[buttonIndex][stateIndex]);
                m_mainMenuButtonTextureHeights[buttonIndex][stateIndex] =
                    TextureGetHeight(m_mainMenuButtonTextures[buttonIndex][stateIndex]);
            }
            if (m_mainMenuButtonTextures[buttonIndex][stateIndex] < 0)
            {
                char message[192] = {};
                std::snprintf(
                    message,
                    sizeof(message),
                    "TitleScene menu button texture could not be loaded: button %zu state %zu",
                    buttonIndex,
                    stateIndex);
                Logger::Warn(message);
            }
        }
    }
    LoadTitleUiTuning();
    m_eventBus.Clear();
    m_blinkTimer = 0.0f;
    m_sceneTime = 0.0f;
    m_showPrompt = true;
    m_menuMode = MenuMode::Main;
    m_menuSelection = 0;
    m_stageSelection = FindStageSelectIndex(GameSession_GetStartMapCsvPath());
    m_optionsSelection = 0;
    m_startTransitionActive = false;
    m_startTransitionSceneRequested = false;
    m_loadingPreviewRequested = false;
    m_startTransitionTimer = 0.0f;
    m_startTransitionSceneId = nullptr;
    InitializeTitleParticles();
    GameSession_SetLoadSavedProgress(true);
    m_bgmEnabled = Audio_GetMasterVolume() > 0.001f;
    m_bgmRestoreVolume = m_bgmEnabled ? Audio_GetMasterVolume() : 1.0f;
    Logger::Info("TitleScene entered");
}

void TitleScene::Update(float deltaTime)
{
    ZoneScoped;
    m_blinkTimer += deltaTime;
    m_sceneTime += deltaTime;
    UpdateTitleParticles(deltaTime);
    if (m_blinkTimer >= 0.5f)
    {
        m_blinkTimer = 0.0f;
        m_showPrompt = !m_showPrompt;
    }

    if (m_startTransitionActive)
    {
        m_startTransitionTimer += deltaTime;
        if (!m_startTransitionSceneRequested && m_startTransitionTimer >= kStartTransitionDuration)
        {
            const char* sceneId = m_startTransitionSceneId != nullptr ? m_startTransitionSceneId : "game";
            m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, sceneId, 0.0f, 0.0f });
            m_startTransitionSceneRequested = true;
        }
        return;
    }

    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        if (m_loadingPreviewRequested)
        {
            m_loadingPreviewRequested = false;
            PublishSceneChange("loading_preview");
            return;
        }
    }

    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        if (Input_IsKeyPressed(VK_F6))
        {
            PublishSceneChange("loading_preview");
            return;
        }
    }

    UpdateMenuInput();
}

void TitleScene::Draw()
{
    DrawBackdrop();
    DrawMenu();
    DrawStartTransition();
}

void TitleScene::DrawDebugUI()
{
    ImGui::Begin("Title Scene");
    ImGui::Text("ドリー・フレーム");
    ImGui::Text("モード: %s", m_menuMode == MenuMode::Main ? "メイン" : m_menuMode == MenuMode::Options ? "設定" : "ステージ選択");
    ImGui::Text("メイン選択: %d", m_menuSelection);
    ImGui::Text("ステージ選択: %d", m_stageSelection);
    ImGui::Text("設定選択: %d", m_optionsSelection);
    ImGui::Text("開始CSV: %s", GameSession_GetStartMapCsvPath().c_str());
    ImGui::Text("操作: W/S・上下キーで選択、Enter/Space/Aで決定、Esc/Bで戻る");
    ImGui::Text("プロンプト表示: %s", m_showPrompt ? "あり" : "なし");
    ImGui::Separator();
    ImGui::TextUnformatted("F6: ロード画面プレビュー");
    if (ImGui::Button("ロード画面プレビューを開く"))
    {
        m_loadingPreviewRequested = true;
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Title UI"))
    {
        for (size_t index = 0; index < m_mainMenuButtonTunings.size(); ++index)
        {
            ImGui::PushID(static_cast<int>(index));
            TitleMenuButtonTuning& tuning = m_mainMenuButtonTunings[index];
            ImGui::TextUnformatted(kMainMenuButtonDebugNames[index]);
            ImGui::DragFloat2("Position", &tuning.x, 1.0f, -200.0f, 1400.0f, "%.1f");
            ImGui::DragFloat2("Size", &tuning.width, 1.0f, 16.0f, 1200.0f, "%.1f");
            ImGui::PopID();
        }
        if (ImGui::Button("Title UI 保存"))
        {
            SaveTitleUiTuning();
        }
        ImGui::SameLine();
        if (ImGui::Button("Title UI 初期化"))
        {
            ResetTitleUiTuning();
        }
    }
    ImGui::End();
}

EventBus* TitleScene::GetEventBus()
{
    return &m_eventBus;
}

void TitleScene::DrawBackdrop() const
{
    const float screenWidth = static_cast<float>(SCREEN_WIDTH);
    const float screenHeight = static_cast<float>(SCREEN_HEIGHT);

    Shader_ResetStyle();
    Shader_SetTint(0.02f, 0.02f, 0.025f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    float imageX = 0.0f;
    float imageY = 0.0f;
    float imageWidth = 0.0f;
    float imageHeight = 0.0f;
    GetTitleImageRect(imageX, imageY, imageWidth, imageHeight);
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    GetTitleShakeOffset(shakeX, shakeY);

    // タイトル画面は背景だけ全画面、ロゴ3枚は理想画像に合わせて左上へ寄せる。
    for (size_t index = 0; index < m_titleLayerTextures.size(); ++index)
    {
        if (m_titleLayerTextures[index] < 0 ||
            m_titleLayerTextureWidths[index] <= 0 ||
            m_titleLayerTextureHeights[index] <= 0)
        {
            continue;
        }

        Shader_ResetStyle();
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        const bool isLogoLayer = index > 0;
        const float drawX = isLogoLayer ? imageX + imageWidth * kTitleLogoLayerOffsetX : imageX;
        const float drawY = isLogoLayer ? imageY + imageHeight * kTitleLogoLayerOffsetY : imageY;
        const float drawWidth = isLogoLayer ? imageWidth * kTitleLogoLayerScale : imageWidth;
        const float drawHeight = isLogoLayer ? imageHeight * kTitleLogoLayerScale : imageHeight;
        const float guard = isLogoLayer ? 0.0f : kTitleShakeGuard;
        SpriteDraw(
            m_titleLayerTextures[index],
            drawX + shakeX - guard,
            drawY + shakeY - guard,
            drawWidth + guard * 2.0f,
            drawHeight + guard * 2.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f);
    }

    DrawTitleParticles();
    Shader_ResetStyle();
}

void TitleScene::DrawTitleParticles() const
{
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    GetTitleShakeOffset(shakeX, shakeY);

    SetDrawBlendMode(DX_BLENDMODE_ADD, 160);
    for (const TitleParticle& particle : m_titleParticles)
    {
        const float wave = std::sinf(m_sceneTime * particle.driftSpeed + particle.phase);
        const float x = particle.x + wave * particle.driftAmplitude + shakeX;
        const float y = particle.y + shakeY;
        const int alpha = std::clamp(static_cast<int>(particle.alpha * 255.0f), 0, 255);
        SetDrawBlendMode(DX_BLENDMODE_ADD, alpha);
        DrawCircleAA(
            x,
            y,
            particle.size,
            12,
            GetColor(210, 255, 255),
            TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void TitleScene::GetTitleShakeOffset(float& x, float& y) const
{
    // 複数の低周波を混ぜて、規則的すぎない手持ちカメラ風の揺れにする。
    x =
        std::sinf(m_sceneTime * 2.7f) * kTitleShakeAmplitudeX +
        std::sinf(m_sceneTime * 6.1f + 1.2f) * (kTitleShakeAmplitudeX * 0.35f);
    y =
        std::cosf(m_sceneTime * 2.2f + 0.6f) * kTitleShakeAmplitudeY +
        std::sinf(m_sceneTime * 5.4f) * (kTitleShakeAmplitudeY * 0.30f);
}

void TitleScene::DrawMenu() const
{
    Shader_ResetStyle();
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    if (m_menuMode == MenuMode::Main)
    {
        DrawMainMenu();
    }
    else if (m_menuMode == MenuMode::Options)
    {
        DrawOptionsMenu();
    }
    else
    {
        DrawStageSelectMenu();
    }

    if (m_menuMode != MenuMode::StageSelect)
    {
        const int hintColor = m_showPrompt ? GetColor(252, 238, 214) : GetColor(168, 140, 104);
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 44, "W/S・上下キー: 選択   Enter/Space/A: 決定   Esc/B: 戻る", hintColor, GetColor(28, 16, 9));
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void TitleScene::DrawMainMenu() const
{
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    for (int index = 0; index < kMainMenuItemCount; ++index)
    {
        const MenuOptionRect rect = GetMainMenuOptionRect(index);
        const int buttonIndex = GetMainMenuButtonIndexForItem(index);
        if (buttonIndex >= 0)
        {
            DrawMainMenuButton(buttonIndex, m_menuSelection == index);
            continue;
        }
        DrawMenuRow(
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            kMainMenuItems[index].label,
            m_menuSelection == index);
    }
}

void TitleScene::DrawMainMenuButton(int buttonIndex, bool selected) const
{
    if (buttonIndex < 0 || buttonIndex >= static_cast<int>(m_mainMenuButtonTunings.size()))
    {
        return;
    }

    const int stateIndex = selected ? kMainMenuButtonSelectedState : kMainMenuButtonNormalState;
    const int texture = m_mainMenuButtonTextures[buttonIndex][stateIndex];
    if (texture < 0)
    {
        const MenuOptionRect rect = {
            static_cast<int>(std::round(m_mainMenuButtonTunings[buttonIndex].x)),
            static_cast<int>(std::round(m_mainMenuButtonTunings[buttonIndex].y)),
            static_cast<int>(std::round(m_mainMenuButtonTunings[buttonIndex].x + m_mainMenuButtonTunings[buttonIndex].width)),
            static_cast<int>(std::round(m_mainMenuButtonTunings[buttonIndex].y + m_mainMenuButtonTunings[buttonIndex].height)),
        };
        DrawMenuRow(
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            "TITLE BUTTON MISSING",
            selected);
        return;
    }

    const TitleMenuButtonTuning& tuning = m_mainMenuButtonTunings[buttonIndex];
    SpriteDraw(
        texture,
        tuning.x,
        tuning.y,
        tuning.width,
        tuning.height,
        0.0f,
        0.0f,
        1.0f,
        1.0f);
}

void TitleScene::DrawOptionsMenu() const
{
    const int left = static_cast<int>(std::round((static_cast<float>(SCREEN_WIDTH) - kOptionsMenuPanelWidth) * 0.5f));
    const int top = static_cast<int>(std::round((static_cast<float>(SCREEN_HEIGHT) - kOptionsMenuPanelHeight) * 0.5f));
    const int right = static_cast<int>(std::round(left + kOptionsMenuPanelWidth));
    const int bottom = static_cast<int>(std::round(top + kOptionsMenuPanelHeight));
    const int paper = GetColor(232, 216, 184);
    const int paperDark = GetColor(202, 178, 137);
    const int ink = GetColor(56, 40, 30);
    const int inkMuted = GetColor(112, 86, 62);
    const int accent = GetColor(170, 91, 48);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(8, 5, 4), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawBox(left + 14, top + 16, right + 14, bottom + 16, GetColor(3, 2, 2), TRUE);
    DrawBox(left, top, right, bottom, paper, TRUE);
    DrawBox(left, top, right, bottom, paperDark, FALSE);
    DrawBox(left + 12, top + 12, right - 12, bottom - 12, GetColor(136, 103, 70), FALSE);
    DrawString(left + 28, top + 22, "PAUSE", ink);
    DrawString(right - 152, top + 24, "DOLLY FRAME", inkMuted);
    DrawBox(left + 28, top + 60, right - 28, top + 63, accent, TRUE);

    const int photoLeft = left + 32;
    const int photoTop = top + 104;
    const int photoRight = left + 258;
    const int photoBottom = top + 344;
    DrawBox(photoLeft + 8, photoTop + 10, photoRight + 8, photoBottom + 10, GetColor(94, 68, 50), TRUE);
    DrawBox(photoLeft, photoTop, photoRight, photoBottom, GetColor(244, 234, 211), TRUE);
    DrawBox(photoLeft + 16, photoTop + 16, photoRight - 16, photoBottom - 50, GetColor(48, 42, 36), TRUE);
    DrawString(photoLeft + 22, photoBottom - 36, "FILM  /  OPTION MENU", inkMuted);

    constexpr const char* labels[kOptionsMenuItemCount] = {
        "BGM",
        "マスター音量",
        "SE音量",
        "戻る",
    };
    const int rowStartY = top + kOptionsMenuRowStartOffset;
    const int rowLeftBase = left + kOptionsMenuRowLeftOffset;
    const int rowRight = right - kOptionsMenuRowRightInset;
    for (int index = 0; index < kOptionsMenuItemCount; ++index)
    {
        const int rowTop = rowStartY + index * kOptionsMenuRowHeight;
        const int rowBottom = rowTop + kOptionsMenuRowHeight - kOptionsMenuRowBottomInset;
        const bool selected = m_optionsSelection == index;
        const int rowLeft = rowLeftBase;
        DrawBox(rowLeft, rowTop, rowRight, rowBottom,
            selected ? GetColor(218, 193, 151) : GetColor(226, 211, 184), TRUE);
        DrawBox(rowLeft, rowTop, rowRight, rowBottom,
            selected ? accent : GetColor(174, 151, 116), FALSE);

        if (selected)
        {
            constexpr int corner = 13;
            DrawLine(rowLeft - 6, rowTop - 5, rowLeft + corner, rowTop - 5, accent);
            DrawLine(rowLeft - 6, rowTop - 5, rowLeft - 6, rowTop + corner, accent);
            DrawLine(rowRight + 6, rowTop - 5, rowRight - corner, rowTop - 5, accent);
            DrawLine(rowRight + 6, rowTop - 5, rowRight + 6, rowTop + corner, accent);
            DrawLine(rowLeft - 6, rowBottom + 5, rowLeft + corner, rowBottom + 5, accent);
            DrawLine(rowLeft - 6, rowBottom + 5, rowLeft - 6, rowBottom - corner, accent);
            DrawLine(rowRight + 6, rowBottom + 5, rowRight - corner, rowBottom + 5, accent);
            DrawLine(rowRight + 6, rowBottom + 5, rowRight + 6, rowBottom - corner, accent);
        }

        const int textColor = selected ? ink : inkMuted;
        if (index == 0)
        {
            DrawFormatString(rowLeft + 18, rowTop + 12, textColor, "%s: %s", labels[index], m_bgmEnabled ? "オン" : "オフ");
        }
        else if (index == 1 || index == 2)
        {
            const float volume = index == 1 ? Audio_GetMasterVolume() : Audio_GetSeVolume();
            const int activeTicks = static_cast<int>(std::round(volume * 10.0f));
            DrawString(rowLeft + 18, rowTop + 12, labels[index], textColor);
            const int tickStartX = rowRight - 128;
            for (int tick = 0; tick < 10; ++tick)
            {
                const int tickLeft = tickStartX + tick * 11;
                DrawBox(tickLeft, rowTop + 15, tickLeft + 7, rowTop + 29,
                    tick < activeTicks ? accent : GetColor(184, 165, 137), TRUE);
            }
        }
        else
        {
            DrawString(rowLeft + 18, rowTop + 12, labels[index], textColor);
        }
    }

    DrawString(left + 32, bottom - 42, "Enter / A  決定", inkMuted);
    DrawString(right - 198, bottom - 42, "Esc / B  戻る", inkMuted);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void TitleScene::DrawStageSelectMenu() const
{
    DrawClassicFrame(116, 282, 844, 604);
    DrawBox(142, 306, 494, 324, GetColor(212, 165, 82), TRUE);
    DrawOutlinedString(148, 338, "ステージ選択", GetColor(255, 244, 220), GetColor(28, 16, 9));
    DrawString(150, 360, "開始時に読み込むCSVを選んでください。", GetColor(242, 226, 194));

    for (int index = 0; index < kStageSelectItemCount; ++index)
    {
        const MenuOptionRect rect = GetStageSelectOptionRect(index);
        DrawMenuRow(
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            kStageSelectItems[index].label,
            m_stageSelection == index);
    }

    const std::string currentStageName = GetStageDisplayName(GameSession_GetStartMapCsvPath());
    const std::string currentStageText = std::string("現在: ") + currentStageName;
    DrawCenteredOutlinedString(
        SCREEN_WIDTH / 2,
        552,
        currentStageText.c_str(),
        GetColor(255, 226, 164),
        GetColor(28, 16, 9));
    DrawCenteredOutlinedString(
        SCREEN_WIDTH / 2,
        574,
        "上下: 行移動   左右: 列切替   Enter/Space/A: 決定すると即ゲーム開始   Esc/B: 戻る",
        GetColor(252, 238, 214),
        GetColor(28, 16, 9));
}

void TitleScene::DrawStartTransition() const
{
    if (!m_startTransitionActive)
    {
        return;
    }

    const float rawT = std::clamp(m_startTransitionTimer / kStartTransitionDuration, 0.0f, 1.0f);
    const float t = rawT * rawT * (3.0f - 2.0f * rawT);
    const float centerX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    const float centerY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
    const float outerRadius = std::sqrt(
        static_cast<float>(SCREEN_WIDTH * SCREEN_WIDTH + SCREEN_HEIGHT * SCREEN_HEIGHT)) * 0.75f;
    const float apertureRadius = std::max(0.0f, outerRadius * (1.0f - t) - 18.0f * t);
    const float rotation = -0.34f + t * 0.82f;
    const int bladeAlpha = std::clamp(static_cast<int>(std::round(255.0f * (0.25f + 0.75f * t))), 0, 255);
    const int bladeColor = GetColor(6, 6, 8);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, bladeAlpha);
    for (int index = 0; index < kStartTransitionBladeCount; ++index)
    {
        const float angle0 = rotation + (static_cast<float>(index) / kStartTransitionBladeCount) * kPi * 2.0f;
        const float angle1 = rotation + (static_cast<float>(index + 1) / kStartTransitionBladeCount) * kPi * 2.0f;
        const float inner0X = centerX + std::cos(angle0) * apertureRadius;
        const float inner0Y = centerY + std::sin(angle0) * apertureRadius;
        const float inner1X = centerX + std::cos(angle1) * apertureRadius;
        const float inner1Y = centerY + std::sin(angle1) * apertureRadius;
        const float outer0X = centerX + std::cos(angle0 - 0.18f) * outerRadius;
        const float outer0Y = centerY + std::sin(angle0 - 0.18f) * outerRadius;
        const float outer1X = centerX + std::cos(angle1 + 0.18f) * outerRadius;
        const float outer1Y = centerY + std::sin(angle1 + 0.18f) * outerRadius;

        DrawTriangleAA(inner0X, inner0Y, outer0X, outer0Y, outer1X, outer1Y, bladeColor, TRUE);
        DrawTriangleAA(inner0X, inner0Y, outer1X, outer1Y, inner1X, inner1Y, bladeColor, TRUE);
    }

    if (rawT > 0.76f)
    {
        const int fadeAlpha = std::clamp(static_cast<int>(std::round((rawT - 0.76f) / 0.24f * 255.0f)), 0, 255);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, fadeAlpha);
        DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    }

    if (apertureRadius > 8.0f)
    {
        const int glintAlpha = std::clamp(static_cast<int>(std::round((1.0f - rawT) * 120.0f)), 0, 120);
        SetDrawBlendMode(DX_BLENDMODE_ADD, glintAlpha);
        DrawCircleAA(centerX, centerY, apertureRadius * 0.045f, 32, GetColor(255, 235, 190), TRUE);
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void TitleScene::GetTitleImageRect(float& x, float& y, float& width, float& height) const
{
    const float screenWidth = static_cast<float>(SCREEN_WIDTH);
    const float screenHeight = static_cast<float>(SCREEN_HEIGHT);
    if (m_titleLayerTextureWidths[0] <= 0 || m_titleLayerTextureHeights[0] <= 0)
    {
        x = 0.0f;
        y = 0.0f;
        width = screenWidth;
        height = screenHeight;
        return;
    }

    const float textureWidth = static_cast<float>(m_titleLayerTextureWidths[0]);
    const float textureHeight = static_cast<float>(m_titleLayerTextureHeights[0]);
    const float scale = (std::min)(screenWidth / textureWidth, screenHeight / textureHeight);
    width = textureWidth * scale;
    height = textureHeight * scale;
    x = (screenWidth - width) * 0.5f;
    y = (screenHeight - height) * 0.5f;
}

void TitleScene::ResetTitleUiTuning()
{
    constexpr float buttonWidth = 560.0f;
    constexpr float buttonHeight = 100.0f;
    constexpr float buttonGap = 16.0f;
    constexpr float buttonCount = 3.0f;
    const float baseX = (static_cast<float>(SCREEN_WIDTH) - buttonWidth) * 0.5f;
    const float baseY =
        (static_cast<float>(SCREEN_HEIGHT) - buttonHeight * buttonCount - buttonGap * (buttonCount - 1.0f)) * 0.5f;

    m_mainMenuButtonTunings[0] = { baseX, baseY, buttonWidth, buttonHeight };
    m_mainMenuButtonTunings[1] = { baseX, baseY + buttonHeight + buttonGap, buttonWidth, buttonHeight };
    m_mainMenuButtonTunings[2] = { baseX, baseY + (buttonHeight + buttonGap) * 2.0f, buttonWidth, buttonHeight };
}

void TitleScene::LoadTitleUiTuning()
{
    ResetTitleUiTuning();

    std::ifstream input(kTitleUiTuningPath, std::ios::binary);
    if (!input)
    {
        return;
    }

    try
    {
        nlohmann::json root = nlohmann::json::parse(input);
        if (!root.contains("mainButtons") || !root["mainButtons"].is_object())
        {
            return;
        }

        const nlohmann::json& mainButtons = root["mainButtons"];
        for (size_t index = 0; index < m_mainMenuButtonTunings.size(); ++index)
        {
            const char* key = kMainMenuButtonTuningKeys[index];
            if (!mainButtons.contains(key) || !mainButtons[key].is_object())
            {
                continue;
            }

            const nlohmann::json& button = mainButtons[key];
            TitleMenuButtonTuning& tuning = m_mainMenuButtonTunings[index];
            tuning.x = button.value("x", tuning.x);
            tuning.y = button.value("y", tuning.y);
            tuning.width = std::max(16.0f, button.value("width", tuning.width));
            tuning.height = std::max(16.0f, button.value("height", tuning.height));
        }
    }
    catch (const std::exception& error)
    {
        Logger::Warn(std::string("Failed to load title UI tuning: ") + error.what());
        ResetTitleUiTuning();
    }
}

bool TitleScene::SaveTitleUiTuning() const
{
    try
    {
        const std::filesystem::path path(kTitleUiTuningPath);
        if (!path.parent_path().empty())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        nlohmann::json root = nlohmann::json::object();
        root["mainButtons"] = nlohmann::json::object();
        for (size_t index = 0; index < m_mainMenuButtonTunings.size(); ++index)
        {
            const TitleMenuButtonTuning& tuning = m_mainMenuButtonTunings[index];
            root["mainButtons"][kMainMenuButtonTuningKeys[index]] = {
                { "x", tuning.x },
                { "y", tuning.y },
                { "width", tuning.width },
                { "height", tuning.height },
            };
        }

        std::ofstream output(kTitleUiTuningPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            Logger::Warn("Failed to open title UI tuning file for write");
            return false;
        }

        output << root.dump(4);
        Logger::Info("Saved title UI tuning");
        return true;
    }
    catch (const std::exception& error)
    {
        Logger::Warn(std::string("Failed to save title UI tuning: ") + error.what());
        return false;
    }
}

void TitleScene::InitializeTitleParticles()
{
    const float screenHeight = static_cast<float>(SCREEN_HEIGHT);
    for (size_t index = 0; index < m_titleParticles.size(); ++index)
    {
        const float y = std::fmod(static_cast<float>(index * 137), screenHeight + kTitleParticleRespawnMargin * 2.0f)
            - kTitleParticleRespawnMargin;
        ResetTitleParticle(index, y);
    }
}

void TitleScene::ResetTitleParticle(size_t index, float y)
{
    const float screenWidth = static_cast<float>(SCREEN_WIDTH);
    TitleParticle& particle = m_titleParticles[index];

    // 疑似乱数テーブル代わりにindexから値を作り、タイトル演出の再現性を保つ。
    const float seed = static_cast<float>(index);
    particle.x = std::fmod(seed * 269.0f + 31.0f, screenWidth);
    particle.y = y;
    particle.speed = 34.0f + std::fmod(seed * 19.0f, 58.0f);
    particle.driftAmplitude = 8.0f + std::fmod(seed * 11.0f, 22.0f);
    particle.driftSpeed = 0.65f + std::fmod(seed * 0.17f, 1.15f);
    particle.size = 1.6f + std::fmod(seed * 0.37f, 3.2f);
    particle.phase = std::fmod(seed * 1.618f, kPi * 2.0f);
    particle.alpha = 0.28f + std::fmod(seed * 0.07f, 0.42f);
}

void TitleScene::UpdateTitleParticles(float deltaTime)
{
    const float bottomY = static_cast<float>(SCREEN_HEIGHT) + kTitleParticleRespawnMargin;
    for (size_t index = 0; index < m_titleParticles.size(); ++index)
    {
        TitleParticle& particle = m_titleParticles[index];
        particle.y -= particle.speed * deltaTime;
        if (particle.y < -kTitleParticleRespawnMargin)
        {
            ResetTitleParticle(index, bottomY);
        }
    }
}

TitleScene::MenuOptionRect TitleScene::GetMainMenuOptionRect(int index) const
{
    const int buttonIndex = GetMainMenuButtonIndexForItem(index);
    if (buttonIndex >= 0)
    {
        const TitleMenuButtonTuning& tuning = m_mainMenuButtonTunings[buttonIndex];
        const int left = static_cast<int>(std::round(tuning.x));
        const int top = static_cast<int>(std::round(tuning.y));
        return {
            left,
            top,
            static_cast<int>(std::round(tuning.x + tuning.width)),
            static_cast<int>(std::round(tuning.y + tuning.height)),
        };
    }

    const int fallbackLeft = static_cast<int>(std::round(m_mainMenuButtonTunings[0].x));
    const int fallbackTop = static_cast<int>(std::round(
        m_mainMenuButtonTunings[0].y - kMainMenuRowHeight - kMainMenuRowGap));
    return { fallbackLeft, fallbackTop, fallbackLeft + kMainMenuRowWidth, fallbackTop + kMainMenuRowHeight };
}

int TitleScene::GetMainMenuButtonIndexForItem(int itemIndex) const
{
    if (itemIndex < 0 || itemIndex >= kMainMenuItemCount)
    {
        return -1;
    }

    switch (kMainMenuItems[itemIndex].action)
    {
    case MainMenuAction::StartGame:
        return 0;
    case MainMenuAction::Options:
        return 1;
    case MainMenuAction::ExitGame:
        return 2;
    default:
        return -1;
    }
}

TitleScene::MenuOptionRect TitleScene::GetOptionsMenuOptionRect(int index) const
{
    const int panelLeft = static_cast<int>(std::round((static_cast<float>(SCREEN_WIDTH) - kOptionsMenuPanelWidth) * 0.5f));
    const int panelTop = static_cast<int>(std::round((static_cast<float>(SCREEN_HEIGHT) - kOptionsMenuPanelHeight) * 0.5f));
    const int panelRight = static_cast<int>(std::round(panelLeft + kOptionsMenuPanelWidth));
    const int rowTop = panelTop + kOptionsMenuRowStartOffset + index * kOptionsMenuRowHeight;
    return {
        panelLeft + kOptionsMenuRowLeftOffset,
        rowTop,
        panelRight - kOptionsMenuRowRightInset,
        rowTop + kOptionsMenuRowHeight - kOptionsMenuRowBottomInset,
    };
}

TitleScene::MenuOptionRect TitleScene::GetStageSelectOptionRect(int index) const
{
    const int column = index % kStageSelectColumnCount;
    const int row = index / kStageSelectColumnCount;
    const int left = kStageSelectRowLeft + column * (kStageSelectRowWidth + kStageSelectColumnGap);
    const int top = kStageSelectRowTop + row * (kStageSelectRowHeight + kStageSelectRowGap);
    return { left, top, left + kStageSelectRowWidth, top + kStageSelectRowHeight };
}

bool TitleScene::IsPointInsideMenuOption(const MenuOptionRect& rect, int x, int y) const
{
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void TitleScene::UpdateMenuInput()
{
    if (m_startTransitionActive)
    {
        return;
    }

    if (m_menuMode == MenuMode::StageSelect)
    {
        constexpr int columnCount = kStageSelectColumnCount;
        constexpr int rowCount = kStageSelectRowCount;

        const int currentRow = m_stageSelection / columnCount;
        const int currentColumn = m_stageSelection % columnCount;

        if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
        {
            const int nextRow = (currentRow + rowCount - 1) % rowCount;
            const int nextIndex = nextRow * columnCount + currentColumn;
            if (nextIndex < kStageSelectItemCount)
            {
                m_stageSelection = nextIndex;
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
            }
        }
        if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
        {
            const int nextRow = (currentRow + 1) % rowCount;
            const int nextIndex = nextRow * columnCount + currentColumn;
            if (nextIndex < kStageSelectItemCount)
            {
                m_stageSelection = nextIndex;
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
            }
        }
        if (Input_IsActionPressed(InputAction::MoveLeft))
        {
            const int nextIndex = currentRow * columnCount;
            if (nextIndex < kStageSelectItemCount)
            {
                m_stageSelection = nextIndex;
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
            }
        }
        if (Input_IsActionPressed(InputAction::MoveRight))
        {
            const int nextIndex = currentRow * columnCount + 1;
            if (nextIndex < kStageSelectItemCount)
            {
                m_stageSelection = nextIndex;
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
            }
        }

        const int mouseX = Input_GetMouseX();
        const int mouseY = Input_GetMouseY();
        int hoveredStage = -1;
        for (int index = 0; index < kStageSelectItemCount; ++index)
        {
            if (IsPointInsideMenuOption(GetStageSelectOptionRect(index), mouseX, mouseY))
            {
                hoveredStage = index;
                break;
            }
        }
        if (hoveredStage >= 0 && hoveredStage != m_stageSelection)
        {
            m_stageSelection = hoveredStage;
            m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
        }

        if (Input_IsActionPressed(InputAction::Cancel) || Input_IsEastButtonPressed())
        {
            m_menuMode = MenuMode::Main;
            m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
            return;
        }

        const bool confirmPressed =
            Input_IsActionPressed(InputAction::Confirm) ||
            Input_IsActionPressed(InputAction::StartGame) ||
            Input_IsSouthButtonPressed();
        const bool mouseClickConfirm = hoveredStage >= 0 && Input_IsMouseLeftPressed();
        if (confirmPressed || mouseClickConfirm)
        {
            ConfirmStageSelectMenu();
        }
        return;
    }

    int& selection = (m_menuMode == MenuMode::Main) ? m_menuSelection : m_optionsSelection;
    const int itemCount = (m_menuMode == MenuMode::Main) ? kMainMenuItemCount : kOptionsMenuItemCount;

    if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
    {
        selection = (selection + itemCount - 1) % itemCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        selection = (selection + 1) % itemCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    int hoveredSelection = -1;
    for (int index = 0; index < itemCount; ++index)
    {
        const MenuOptionRect rect = (m_menuMode == MenuMode::Main)
            ? GetMainMenuOptionRect(index)
            : GetOptionsMenuOptionRect(index);
        if (IsPointInsideMenuOption(rect, mouseX, mouseY))
        {
            hoveredSelection = index;
            break;
        }
    }
    if (hoveredSelection >= 0 && hoveredSelection != selection)
    {
        selection = hoveredSelection;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }

    if (m_menuMode == MenuMode::Options)
    {
        const bool adjustLeft = Input_IsActionPressed(InputAction::MoveLeft);
        const bool adjustRight = Input_IsActionPressed(InputAction::MoveRight);
        if (adjustLeft || adjustRight)
        {
            const float delta = adjustRight ? 0.05f : -0.05f;
            switch (m_optionsSelection)
            {
            case 0:
                ToggleBgm();
                break;
            case 1:
                Audio_SetMasterVolume(std::clamp(Audio_GetMasterVolume() + delta, 0.0f, 1.0f));
                m_bgmEnabled = Audio_GetMasterVolume() > 0.001f;
                if (m_bgmEnabled)
                {
                    m_bgmRestoreVolume = Audio_GetMasterVolume();
                }
                break;
            case 2:
                Audio_SetSeVolume(std::clamp(Audio_GetSeVolume() + delta, 0.0f, 1.0f));
                break;
            default:
                break;
            }
            m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
        }
    }

    if (Input_IsActionPressed(InputAction::Cancel) || Input_IsEastButtonPressed())
    {
        if (m_menuMode == MenuMode::Options)
        {
            m_menuMode = MenuMode::Main;
            m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
        }
        return;
    }

    const bool confirmPressed =
        Input_IsActionPressed(InputAction::Confirm) ||
        Input_IsActionPressed(InputAction::StartGame) ||
        Input_IsSouthButtonPressed();
    const bool mouseClickConfirm = hoveredSelection >= 0 && Input_IsMouseLeftPressed();
    if (!confirmPressed && !mouseClickConfirm)
    {
        return;
    }

    if (m_menuMode == MenuMode::Main)
    {
        ConfirmMainMenu();
    }
    else
    {
        ConfirmOptionsMenu();
    }
}

void TitleScene::ConfirmMainMenu()
{
    if (m_menuSelection < 0 || m_menuSelection >= kMainMenuItemCount)
    {
        m_menuSelection = 0;
    }

    switch (kMainMenuItems[m_menuSelection].action)
    {
    case MainMenuAction::StartGame:
        BeginStartTransition("game");
        break;
    case MainMenuAction::StageSelect:
        m_menuMode = MenuMode::StageSelect;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });
        break;
    case MainMenuAction::Options:
        m_menuMode = MenuMode::Options;
        m_optionsSelection = 0;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });
        break;
    case MainMenuAction::ExitGame:
        m_eventBus.Publish({ EventType::ExitApplicationRequested, nullptr, nullptr, "", 0.0f, 0.0f });
        break;
    }
}

void TitleScene::ConfirmStageSelectMenu()
{
    if (m_stageSelection < 0 || m_stageSelection >= kStageSelectItemCount)
    {
        m_stageSelection = 0;
    }

    GameSession_SetStartMapCsvPath(kStageSelectItems[m_stageSelection].path);
    GameSession_SetLoadSavedProgress(false);
    BeginStartTransition("game");
}

void TitleScene::BeginStartTransition(const char* sceneId)
{
    if (m_startTransitionActive)
    {
        return;
    }

    m_startTransitionActive = true;
    m_startTransitionSceneRequested = false;
    m_startTransitionTimer = 0.0f;
    m_startTransitionSceneId = sceneId;
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });
    Audio_FadeOutBgm(kStartTransitionDuration);
}

void TitleScene::ConfirmOptionsMenu()
{
    switch (m_optionsSelection)
    {
    case 0:
        ToggleBgm();
        break;
    case 1:
        Audio_SetMasterVolume(std::clamp(Audio_GetMasterVolume() + 0.05f, 0.0f, 1.0f));
        m_bgmEnabled = Audio_GetMasterVolume() > 0.001f;
        if (m_bgmEnabled)
        {
            m_bgmRestoreVolume = Audio_GetMasterVolume();
        }
        break;
    case 2:
        Audio_SetSeVolume(std::clamp(Audio_GetSeVolume() + 0.05f, 0.0f, 1.0f));
        break;
    case 3:
        m_menuMode = MenuMode::Main;
        break;
    default:
        break;
    }
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });
}

void TitleScene::PublishSceneChange(const char* sceneId)
{
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });
    Audio_FadeOutBgm(0.65f);
    m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, sceneId, 0.0f, 0.0f });
}

void TitleScene::ToggleBgm()
{
    if (m_bgmEnabled)
    {
        const float currentVolume = Audio_GetMasterVolume();
        if (currentVolume > 0.001f)
        {
            m_bgmRestoreVolume = currentVolume;
        }
        Audio_SetMasterVolume(0.0f);
        m_bgmEnabled = false;
        return;
    }

    Audio_SetMasterVolume(m_bgmRestoreVolume > 0.001f ? m_bgmRestoreVolume : 1.0f);
    m_bgmEnabled = true;
}
