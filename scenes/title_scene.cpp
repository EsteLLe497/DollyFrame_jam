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
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <tracy/Tracy.hpp>

namespace
{
    struct StageSelectItem
    {
        const char* label;
        const char* path;
    };

    constexpr int kMainMenuItemCount = 6;
    constexpr int kOptionsMenuItemCount = 4;
    constexpr int kStageSelectItemCount = 10;
    constexpr int kStageSelectColumnCount = 2;
    constexpr int kStageSelectRowCount = (kStageSelectItemCount + kStageSelectColumnCount - 1) / kStageSelectColumnCount;

    constexpr const char* kMainMenuLabels[kMainMenuItemCount] = {
        "ゲーム開始",
        "ステージ選択",
        "設定",
        "デモシーン",
        "シェーダーテスト",
        "終了",
    };

    constexpr StageSelectItem kStageSelectItems[kStageSelectItemCount] = {
        { "forest.csv", "assets/maps/stages/forest.csv" },
        { "forest_boss.csv", "assets/maps/stages/forest_boss.csv" },
        { "ruins1.csv", "assets/maps/stages/ruins1.csv" },
        { "ruins_boss.csv", "assets/maps/stages/ruins_boss.csv" },
        { "under.csv", "assets/maps/stages/under.csv" },
        { "under_boss.csv", "assets/maps/stages/under_boss.csv" },
        { "stage_a.csv", "assets/maps/stages/stage_a.csv" },
        { "stage_58x25.csv", "assets/maps/stages/stage_58x25.csv" },
        { "stage_58x25_check.csv", "assets/maps/stages/stage_58x25_check.csv" },
        { "side_scroll_stage01.csv", "assets/maps/stages/side_scroll_stage01.csv" },
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
    , m_blinkTimer(0.0f)
    , m_sceneTime(0.0f)
    , m_showPrompt(true)
    , m_menuMode(MenuMode::Main)
    , m_menuSelection(0)
    , m_stageSelection(0)
    , m_optionsSelection(0)
    , m_bgmEnabled(true)
    , m_bgmRestoreVolume(1.0f)
{
}

const char* TitleScene::GetSceneId() const
{
    return "title";
}

void TitleScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    m_whiteTexture = resources.CreateSolidTexture(1, 1, 0xFFFFFFFF);
    m_eventBus.Clear();
    m_blinkTimer = 0.0f;
    m_sceneTime = 0.0f;
    m_showPrompt = true;
    m_menuMode = MenuMode::Main;
    m_menuSelection = 0;
    m_stageSelection = FindStageSelectIndex(GameSession_GetStartMapCsvPath());
    m_optionsSelection = 0;
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
    if (m_blinkTimer >= 0.5f)
    {
        m_blinkTimer = 0.0f;
        m_showPrompt = !m_showPrompt;
    }

    UpdateMenuInput();
}

void TitleScene::Draw()
{
    DrawBackdrop();
    DrawMenu();
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
    const float pulse = 0.55f + 0.45f * std::sinf(m_sceneTime * 2.2f);

    Shader_ResetStyle();
    Shader_SetTint(0.12f, 0.08f, 0.04f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_ResetStyle();
    Shader_SetTint(0.34f, 0.23f, 0.10f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, screenWidth, 208.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.22f, 0.14f, 0.07f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 208.0f, screenWidth, screenHeight - 208.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.06f, 0.04f, 0.02f, 0.5f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
    DrawCircle(720, 114, 54, GetColor(242, 214, 156), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(30.0f + pulse * 52.0f));
    DrawCircle(720, 114, 84, GetColor(214, 164, 84), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);

    for (int i = 0; i < 18; ++i)
    {
        const int x = 46 + (i * 71) % (SCREEN_WIDTH - 92);
        const int y = 34 + (i * 43) % 188;
        const int shine = static_cast<int>(150.0f + 70.0f * std::sinf(m_sceneTime * 2.0f + static_cast<float>(i)));
        DrawCircle(x, y, i % 3 == 0 ? 2 : 1, GetColor(shine, shine - 14, 100), TRUE);
    }

    DrawCameraSilhouette(616);

    DrawTitleLogo(SCREEN_WIDTH / 2, 104);

    Shader_ResetStyle();
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    Shader_ResetStyle();
}

void TitleScene::DrawMenu() const
{
    Shader_ResetStyle();
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawClassicFrame(116, 282, 844, 604);
    DrawBox(142, 306, 452, 324, GetColor(212, 165, 82), TRUE);
    DrawOutlinedString(148, 338, "ドリー・フレーム", GetColor(255, 244, 220), GetColor(28, 16, 9));
    DrawString(150, 360, "プレートを装填し、シャッターを切れ。", GetColor(242, 226, 194));

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
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2, 574, "W/S・上下キー: 選択   Enter/Space/A: 決定   Esc/B: 戻る", hintColor, GetColor(28, 16, 9));
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void TitleScene::DrawMainMenu() const
{
    constexpr int rowLeft = 146;
    constexpr int rowTop = 386;
    constexpr int rowWidth = 360;
    constexpr int rowHeight = 34;
    constexpr int rowGap = 8;

    for (int index = 0; index < kMainMenuItemCount; ++index)
    {
        const int top = rowTop + index * (rowHeight + rowGap);
        DrawMenuRow(rowLeft, top, rowWidth, rowHeight, kMainMenuLabels[index], m_menuSelection == index);
    }

    const std::string startMapName = GetStageFileName(GameSession_GetStartMapCsvPath());
    const bool loadSavedProgress = GameSession_ShouldLoadSavedProgress();
    DrawClassicFrame(548, 386, 812, 580);
    DrawOutlinedString(570, 408, "開始", GetColor(255, 244, 220), GetColor(28, 16, 9));
    DrawString(570, 436, "次の露光を始める。", GetColor(242, 226, 194));
    DrawString(570, 458, loadSavedProgress ? "セーブを続きから読み込む。" : "選択したCSVで新規開始する。", GetColor(242, 226, 194));
    DrawString(570, 480, startMapName.c_str(), GetColor(255, 226, 164));
    DrawOutlinedString(570, 506, "ステージ選択", GetColor(255, 226, 164), GetColor(28, 16, 9));
    DrawString(570, 534, "起動時に読み込むCSVを選ぶ。", GetColor(242, 226, 194));
    DrawString(570, 556, "選んだらそのままゲームへ入る。", GetColor(242, 226, 194));
}

void TitleScene::DrawOptionsMenu() const
{
    constexpr int rowLeft = 146;
    constexpr int rowTop = 386;
    constexpr int rowWidth = 520;
    constexpr int rowHeight = 34;
    constexpr int rowGap = 8;

    const int masterVolume = static_cast<int>(std::round(Audio_GetMasterVolume() * 100.0f));
    const int seVolume = static_cast<int>(std::round(Audio_GetSeVolume() * 100.0f));

    char label[96] = {};
    std::snprintf(label, sizeof(label), "BGM: %s", m_bgmEnabled ? "ON" : "OFF");
    DrawMenuRow(rowLeft, rowTop, rowWidth, rowHeight, label, m_optionsSelection == 0);
    std::snprintf(label, sizeof(label), "MASTER VOLUME: %d%%", masterVolume);
    DrawMenuRow(rowLeft, rowTop + (rowHeight + rowGap), rowWidth, rowHeight, label, m_optionsSelection == 1);
    std::snprintf(label, sizeof(label), "SE VOLUME: %d%%", seVolume);
    DrawMenuRow(rowLeft, rowTop + (rowHeight + rowGap) * 2, rowWidth, rowHeight, label, m_optionsSelection == 2);
    DrawMenuRow(rowLeft, rowTop + (rowHeight + rowGap) * 3, rowWidth, rowHeight, "BACK", m_optionsSelection == 3);

    DrawClassicFrame(682, 386, 822, 470);
    DrawString(696, 408, "左右キー", GetColor(255, 226, 164));
    DrawString(696, 432, "でダイヤルを回す。", GetColor(242, 226, 194));
}

void TitleScene::DrawStageSelectMenu() const
{
    constexpr int rowLeft = 146;
    constexpr int rowTop = 386;
    constexpr int rowWidth = 268;
    constexpr int rowHeight = 34;
    constexpr int rowGap = 8;
    constexpr int columnGap = 18;

    DrawClassicFrame(116, 282, 844, 604);
    DrawBox(142, 306, 494, 324, GetColor(212, 165, 82), TRUE);
    DrawOutlinedString(148, 338, "ステージ選択", GetColor(255, 244, 220), GetColor(28, 16, 9));
    DrawString(150, 360, "開始時に読み込むCSVを選んでください。", GetColor(242, 226, 194));

    for (int index = 0; index < kStageSelectItemCount; ++index)
    {
        const int column = index % kStageSelectColumnCount;
        const int row = index / kStageSelectColumnCount;
        const int left = rowLeft + column * (rowWidth + columnGap);
        const int top = rowTop + row * (rowHeight + rowGap);
        DrawMenuRow(left, top, rowWidth, rowHeight, kStageSelectItems[index].label, m_stageSelection == index);
    }

    const std::string currentStageName = GetStageFileName(GameSession_GetStartMapCsvPath());
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

void TitleScene::UpdateMenuInput()
{
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
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
            }
        }
        if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
        {
            const int nextRow = (currentRow + 1) % rowCount;
            const int nextIndex = nextRow * columnCount + currentColumn;
            if (nextIndex < kStageSelectItemCount)
            {
                m_stageSelection = nextIndex;
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
            }
        }
        if (Input_IsActionPressed(InputAction::MoveLeft))
        {
            const int nextIndex = currentRow * columnCount;
            if (nextIndex < kStageSelectItemCount)
            {
                m_stageSelection = nextIndex;
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
            }
        }
        if (Input_IsActionPressed(InputAction::MoveRight))
        {
            const int nextIndex = currentRow * columnCount + 1;
            if (nextIndex < kStageSelectItemCount)
            {
                m_stageSelection = nextIndex;
                m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
            }
        }

        if (Input_IsActionPressed(InputAction::Cancel) || Input_IsEastButtonPressed())
        {
            m_menuMode = MenuMode::Main;
            m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
            return;
        }

        const bool confirmPressed =
            Input_IsActionPressed(InputAction::Confirm) ||
            Input_IsActionPressed(InputAction::StartGame) ||
            Input_IsSouthButtonPressed();
        if (confirmPressed)
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
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        selection = (selection + 1) % itemCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
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
            m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
        }
    }

    if (Input_IsActionPressed(InputAction::Cancel) || Input_IsEastButtonPressed())
    {
        if (m_menuMode == MenuMode::Options)
        {
            m_menuMode = MenuMode::Main;
            m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
        }
        return;
    }

    const bool confirmPressed =
        Input_IsActionPressed(InputAction::Confirm) ||
        Input_IsActionPressed(InputAction::StartGame) ||
        Input_IsSouthButtonPressed();
    if (!confirmPressed)
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
    switch (m_menuSelection)
    {
    case 0:
        PublishSceneChange("game");
        break;
    case 1:
        m_menuMode = MenuMode::StageSelect;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "scene_change", 0.0f, 0.0f });
        break;
    case 2:
        m_menuMode = MenuMode::Options;
        m_optionsSelection = 0;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "scene_change", 0.0f, 0.0f });
        break;
    case 3:
        PublishSceneChange("demo");
        break;
    case 4:
        PublishSceneChange("shader_showcase");
        break;
    case 5:
        m_eventBus.Publish({ EventType::ExitApplicationRequested, nullptr, nullptr, "", 0.0f, 0.0f });
        break;
    default:
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
    PublishSceneChange("game");
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
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "scene_change", 0.0f, 0.0f });
}

void TitleScene::PublishSceneChange(const char* sceneId)
{
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "scene_change", 0.0f, 0.0f });
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
