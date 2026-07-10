#include "pch.h"
#include "result_scene.h"
#include "directX.h"
#include "game_session.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"
#include "DxLib.h"
#include "photo_log.h"
#include "game_scene_render_ui_helpers.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <tracy/Tracy.hpp>

namespace
{
    struct PrimaryOption
    {
        std::string label;
        std::string mapCsvPath;
    };

    // 最後に遊んだステージと終了理由から、リザルトの主選択肢を決める。
    // クリア時は次のステージ（森→廃墟→地下）、ゲームオーバー時は同じステージへの再挑戦。
    PrimaryOption BuildPrimaryOption(const GameSessionState& session)
    {
        const bool cleared = session.endReason == GameEndReason::GoalReached ||
            session.endReason == GameEndReason::BossDefeated;
        const std::string& lastMap = session.lastMapCsvPath.empty()
            ? session.startMapCsvPath
            : session.lastMapCsvPath;

        if (!cleared)
        {
            return { "同じステージに再挑戦", lastMap };
        }

        if (lastMap.find("forest") != std::string::npos)
        {
            return { "廃墟ステージへ進む", "assets/maps/stages/ruins_v2.csv" };
        }
        if (lastMap.find("ruins") != std::string::npos)
        {
            return { "地下ステージへ進む", "assets/maps/stages/under.csv" };
        }
        if (lastMap.find("under") != std::string::npos)
        {
            return { "最初のステージから遊びなおす", "assets/maps/stages/forest.csv" };
        }
        return { "もう一度遊ぶ", lastMap };
    }

    const char* ToReasonLabel(GameEndReason reason)
    {
        switch (reason)
        {
        case GameEndReason::GoalReached:
            return "クリア";
        case GameEndReason::TimeUp:
            return "時間切れ";
        case GameEndReason::HpZero:
            return "HP0";
        case GameEndReason::BossDefeated:
            return "ボス撃破";
        case GameEndReason::None:
        default:
            return "結果なし";
        }
    }
    struct FreeImagePlacement
    {
        const char* textureKey;
        float x;
        float y;
        float width;
        float height;
    };


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
            DrawCircle(left + 18, top + height / 2, 5, GetColor(241, 205, 132), TRUE);
            DrawCircle(left + 18, top + height / 2, 2, GetColor(52, 30, 14), TRUE);
        }

        const int textWidth = GetDrawStringWidth(label, static_cast<int>(std::strlen(label)));
        const int textX = left + (width - textWidth) / 2 + 12;
        const int textY = top + (height - 16) / 2;
        DrawOutlinedString(textX, textY, label, textColor, GetColor(28, 16, 9));
    }

    constexpr int kMenuOptionCount = 2;
    constexpr const char* kBackToTitleLabel = "タイトルへ戻る";
    constexpr int kMenuRowWidth = 360;
    constexpr int kMenuRowHeight = 44;
    constexpr int kMenuRowGap = 14;
    constexpr int kMenuRowTop = 420;
    constexpr float kResultIntroDuration = 0.55f;
    constexpr int kResultFilmHeight = 132;
    constexpr int kResultFilmRailHeight = 28;
    constexpr int kResultFilmHoleWidth = 20;
    constexpr int kResultFilmHoleHeight = 16;
    constexpr int kResultFilmHoleGap = 34;
    constexpr int kResultFilmFrameWidth = 270;

    float EaseOutCubic(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const float invT = 1.0f - t;
        return 1.0f - invT * invT * invT;
    }
}

ResultScene::ResultScene()
    : m_whiteTexture(-1)
    , m_blinkTimer(0.0f)
    , m_introTimer(0.0f)
    , m_showPrompt(true)
    , m_selectedOption(0)
{
}

const char* ResultScene::GetSceneId() const
{
    return "result";
}

void ResultScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_eventBus.Clear();
    m_blinkTimer = 0.0f;
    m_introTimer = 0.0f;
    m_showPrompt = true;
    m_selectedOption = 0;
    PrimaryOption primaryOption = BuildPrimaryOption(GameSession_Get());
    m_primaryOptionLabel = std::move(primaryOption.label);
    m_primaryOptionMapCsv = std::move(primaryOption.mapCsvPath);
    Logger::Info("ResultScene entered");
}

ResultScene::MenuOptionRect ResultScene::GetOptionRect(int index) const
{
    const int left = SCREEN_WIDTH / 2 - kMenuRowWidth / 2;
    const int top = kMenuRowTop + index * (kMenuRowHeight + kMenuRowGap);
    return { left, top, left + kMenuRowWidth, top + kMenuRowHeight };
}

void ResultScene::UpdateMenuInput()
{
    // キーボード / パッド操作
    if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
    {
        m_selectedOption = (m_selectedOption + kMenuOptionCount - 1) % kMenuOptionCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        m_selectedOption = (m_selectedOption + 1) % kMenuOptionCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }

    // マウス操作: ホバーで選択、クリックで決定
    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    int hoveredOption = -1;
    for (int index = 0; index < kMenuOptionCount; ++index)
    {
        const MenuOptionRect rect = GetOptionRect(index);
        if (mouseX >= rect.left && mouseX <= rect.right && mouseY >= rect.top && mouseY <= rect.bottom)
        {
            hoveredOption = index;
            break;
        }
    }

    if (hoveredOption >= 0 && hoveredOption != m_selectedOption)
    {
        m_selectedOption = hoveredOption;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }

    const bool confirmPressed =
        Input_IsActionPressed(InputAction::Confirm) ||
        Input_IsActionPressed(InputAction::StartGame) ||
        Input_IsSouthButtonPressed();
    const bool mouseClickConfirm = hoveredOption >= 0 && Input_IsMouseLeftPressed();

    if (confirmPressed || mouseClickConfirm)
    {
        ConfirmSelection();
    }
}

void ResultScene::ConfirmSelection()
{
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });

    if (m_selectedOption == 0)
    {
        GameSession_SetStartMapCsvPath(m_primaryOptionMapCsv);
        GameSession_SetLoadSavedProgress(false);
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    else
    {
        // タイトルへ戻る
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }
}

void ResultScene::Update(float deltaTime)
{
    ZoneScoped;
    m_introTimer = std::min(m_introTimer + deltaTime, kResultIntroDuration);
    m_blinkTimer += deltaTime;
    if (m_blinkTimer >= 0.45f)
    {
        m_blinkTimer = 0.0f;
        m_showPrompt = !m_showPrompt;
    }

    if (m_introTimer < kResultIntroDuration)
    {
        return;
    }

    UpdateMenuInput();
}

void ResultScene::Draw()
{
    const float offsetX = GetIntroOffsetX();
    if (offsetX > 0.5f)
    {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    }

    DrawBackdrop(offsetX);
    DrawFreeImages(offsetX);
    DrawCapturedPhotosGrid(offsetX);
    DrawMenu(offsetX);
    DrawResultFilmFrame();
}

float ResultScene::GetIntroOffsetX() const
{
    const float progress = std::clamp(m_introTimer / kResultIntroDuration, 0.0f, 1.0f);
    return (1.0f - EaseOutCubic(progress)) * static_cast<float>(SCREEN_WIDTH);
}

void ResultScene::DrawMenu(float offsetX) const
{
    const GameSessionState& session = GameSession_Get();
    const bool cleared = session.endReason == GameEndReason::GoalReached ||
        session.endReason == GameEndReason::BossDefeated;
    const int drawOffsetX = static_cast<int>(std::round(offsetX));

    // 大見出し
    if (cleared)
    {
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2 + drawOffsetX, 220, "GAME CLEAR", GetColor(255, 244, 220), GetColor(52, 30, 14));
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2 + drawOffsetX, 260, "ゲームクリア", GetColor(255, 226, 164), GetColor(28, 16, 9));
    }
    else
    {
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2 + drawOffsetX, 220, "GAME OVER", GetColor(255, 244, 220), GetColor(52, 30, 14));
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2 + drawOffsetX, 260, "ゲームオーバー", GetColor(255, 226, 164), GetColor(28, 16, 9));
    }

    char detail[64] = {};
    std::snprintf(detail, sizeof(detail), "%s", ToReasonLabel(session.endReason));
    DrawCenteredOutlinedString(SCREEN_WIDTH / 2 + drawOffsetX, 320, detail, GetColor(242, 226, 194), GetColor(28, 16, 9));

    char statsLine[96] = {};
    const int clearMinutes = static_cast<int>(session.clearTimeSeconds) / 60;
    const int clearSeconds = static_cast<int>(session.clearTimeSeconds) % 60;
    std::snprintf(
        statsLine,
        sizeof(statsLine),
        "取得アイテム: %d個   クリアタイム: %02d:%02d",
        session.partsCollectedTotal,
        clearMinutes,
        clearSeconds);
    DrawCenteredOutlinedString(SCREEN_WIDTH / 2 + drawOffsetX, 356, statsLine, GetColor(255, 236, 196), GetColor(28, 16, 9));

    // 選択肢
    const char* menuLabels[kMenuOptionCount] = { m_primaryOptionLabel.c_str(), kBackToTitleLabel };
    for (int index = 0; index < kMenuOptionCount; ++index)
    {
        const MenuOptionRect rect = GetOptionRect(index);
        DrawMenuRow(rect.left + drawOffsetX, rect.top, kMenuRowWidth, kMenuRowHeight, menuLabels[index], m_selectedOption == index);
    }

    const int hintColor = m_showPrompt ? GetColor(252, 238, 214) : GetColor(168, 140, 104);
    DrawCenteredOutlinedString(
        SCREEN_WIDTH / 2 + drawOffsetX,
        kMenuRowTop + kMenuOptionCount * (kMenuRowHeight + kMenuRowGap) + 30,
        "上下キー・マウス: 選択   Enter/Space/A/クリック: 決定",
        hintColor,
        GetColor(28, 16, 9));
}

void ResultScene::DrawResultFilmFrame() const
{
    const float progress = std::clamp(m_introTimer / kResultIntroDuration, 0.0f, 1.0f);
    const float eased = EaseOutCubic(progress);
    const int screenW = SCREEN_WIDTH;
    const int screenH = SCREEN_HEIGHT;
    const int filmLength = screenW + 520;
    const float startLeft = static_cast<float>(screenW + 160);
    const float stoppedLeft = -220.0f;
    const int filmLeft = static_cast<int>(std::round(startLeft + (stoppedLeft - startLeft) * eased));
    const int topFilmY = 36;
    const int bottomFilmY = screenH - 36 - kResultFilmHeight;
    const int filmColor = GetColor(12, 12, 12);
    const int frameColor = GetColor(248, 248, 248);
    const int dividerColor = GetColor(10, 10, 10);
    const int holeColor = GetColor(248, 248, 248);

    const auto drawFilmStrip = [&](int y, int phaseOffset)
    {
        const int left = filmLeft + phaseOffset;
        const int right = left + filmLength;
        if (right < -120 || left > screenW + 120)
        {
            return;
        }

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 248);
        DrawBox(left, y, right, y + kResultFilmHeight, filmColor, TRUE);

        const int holePhase = static_cast<int>(std::round((1.0f - eased) * 260.0f)) % kResultFilmHoleGap;
        const int firstHoleX = left + 18 + holePhase;
        for (int x = firstHoleX - kResultFilmHoleGap * 2; x < right + kResultFilmHoleGap; x += kResultFilmHoleGap)
        {
            DrawBox(x, y + 6, x + kResultFilmHoleWidth, y + 6 + kResultFilmHoleHeight, holeColor, TRUE);
            DrawBox(x, y + kResultFilmHeight - 6 - kResultFilmHoleHeight, x + kResultFilmHoleWidth, y + kResultFilmHeight - 6, holeColor, TRUE);
        }

        const int frameTop = y + kResultFilmRailHeight + 4;
        const int frameBottom = y + kResultFilmHeight - kResultFilmRailHeight - 4;
        for (int frameX = left; frameX < right; frameX += kResultFilmFrameWidth)
        {
            DrawBox(frameX + 8, frameTop + 6, frameX + kResultFilmFrameWidth - 8, frameBottom - 6, frameColor, TRUE);
            DrawBox(frameX + kResultFilmFrameWidth - 3, frameTop, frameX + kResultFilmFrameWidth + 3, frameBottom, dividerColor, TRUE);
        }

        DrawBox(left, y + kResultFilmRailHeight, right, y + kResultFilmRailHeight + 4, dividerColor, TRUE);
        DrawBox(left, y + kResultFilmHeight - kResultFilmRailHeight - 4, right, y + kResultFilmHeight - kResultFilmRailHeight, dividerColor, TRUE);
    };

    drawFilmStrip(topFilmY, 0);
    drawFilmStrip(bottomFilmY, 180);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void ResultScene::DrawDebugUI()
{
    const GameSessionState& session = GameSession_Get();
    ImGui::Begin("リザルト");
    ImGui::Text("試作版のリザルト画面です");
    ImGui::Text("karihaikei texture id: %d", m_assets.GetTexture("karihaikei"));
    ImGui::Text("Result: %s", ToReasonLabel(session.endReason));
    ImGui::Text("HP: %d / %d", session.currentHp, session.maxHp);
    ImGui::Text("残り時間: %.1f / %.1f", session.timeRemaining, session.timeLimit);
    ImGui::Text("選択中: %s", m_selectedOption == 0 ? m_primaryOptionLabel.c_str() : kBackToTitleLabel);
    ImGui::Text("主選択肢の遷移先: %s", m_primaryOptionMapCsv.c_str());
    ImGui::Text("上下キー/マウスホバーで選択、Enter/Space/A/クリックで決定");
    ImGui::Text("プロンプト表示: %s", m_showPrompt ? "あり" : "なし");
    ImGui::Text("取得アイテム(累計): %d", session.partsCollectedTotal);
    ImGui::Text("クリアタイム: %.1f 秒", session.clearTimeSeconds);
    ImGui::End();
}

EventBus* ResultScene::GetEventBus()
{
    return &m_eventBus;
}

void ResultScene::DrawBackdrop(float offsetX) const
{
    const GameSessionState& session = GameSession_Get();
    const bool cleared = session.endReason == GameEndReason::GoalReached ||
        session.endReason == GameEndReason::BossDefeated;

    // ここに追加: 一番奥の背景画像
    const int backgroundTexture = m_assets.GetTexture("karihaikei");
    if (backgroundTexture >= 0)
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        SpriteDraw(
            backgroundTexture,
            offsetX,
            0.0f,
            static_cast<float>(SCREEN_WIDTH),
            static_cast<float>(SCREEN_HEIGHT),
            0.0f,
            0.0f,
            1.0f,
            1.0f);
    }

    const float accentR = cleared ? 0.18f : 0.78f;
    const float accentG = cleared ? 0.62f : 0.24f;
    const float accentB = cleared ? 0.32f : 0.14f;
    //Shader_SetTint(0.11f, 0.07f, 0.10f, 1.0f);
    //SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
    if (m_showPrompt)
    {
        Shader_SetTint(0.88f, 0.88f, 0.88f, 1.0f);
        SpriteDraw(m_whiteTexture, 192.0f + offsetX, 430.0f, static_cast<float>(SCREEN_WIDTH) - 384.0f, 34.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void ResultScene::DrawFreeImages(float offsetX) const
{
    struct FreeImagePlacement
    {
        const char* textureKey;
        float x;
        float y;
        float width;
        float height;
    };

    // ここの数値(x, y, width, height)を書き換えて自由に位置・サイズを調整してください
    const FreeImagePlacement placements[] =
    {
        { "date", 40.0f, -250.0f, 360.0f, 2120.0f },
        { "kuria", static_cast<float>(SCREEN_WIDTH) - 300.0f,
          450.0f, 220.0f, 160.0f },
    };

    for (const auto& placement : placements)
    {
        const int textureId = m_assets.GetTexture(placement.textureKey);
        if (textureId < 0)
        {
            continue; // テクスチャが見つからない場合は描画しない
        }

        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        SpriteDraw(
            textureId,
            placement.x + offsetX,
            placement.y,
            placement.width,
            placement.height,
            0.0f,
            0.0f,
            1.0f,
            1.0f);
    }
}
void ResultScene::DrawCapturedPhotosGrid(float offsetX) const
{
    constexpr int kColumns = 3;
    constexpr int kRows = 3;
    constexpr float kCellWidth = 140.0f;
    constexpr float kCellHeight = 100.0f;
    constexpr float kCellGapX = 12.0f;
    constexpr float kCellGapY = 12.0f;
    constexpr float kMarginRight = 40.0f;
    constexpr float kMarginBottom = 40.0f;
    constexpr float kPadding = 6.0f;

    // グリッド全体のサイズから右下基準の開始座標を逆算する
    const float gridWidth = static_cast<float>(kColumns) * kCellWidth + static_cast<float>(kColumns - 1) * kCellGapX;
    const float gridHeight = static_cast<float>(kRows) * kCellHeight + static_cast<float>(kRows - 1) * kCellGapY;
    const float kGridStartX = static_cast<float>(SCREEN_WIDTH) - kMarginRight - gridWidth;
    const float kGridStartY = static_cast<float>(SCREEN_HEIGHT) - kMarginBottom - gridHeight;

    const int photoCount = PhotoLog_GetCount();

    for (int index = 0; index < kColumns * kRows; ++index)
    {
        const int column = index % kColumns;
        const int row = index / kColumns;
        const float cellX = kGridStartX + static_cast<float>(column) * (kCellWidth + kCellGapX) + offsetX;
        const float cellY = kGridStartY + static_cast<float>(row) * (kCellHeight + kCellGapY);
        DrawBox(
            static_cast<int>(cellX),
            static_cast<int>(cellY),
            static_cast<int>(cellX + kCellWidth),
            static_cast<int>(cellY + kCellHeight),
            GetColor(60, 50, 40),
            TRUE);
        DrawBox(
            static_cast<int>(cellX),
            static_cast<int>(cellY),
            static_cast<int>(cellX + kCellWidth),
            static_cast<int>(cellY + kCellHeight),
            GetColor(200, 180, 140),
            FALSE);

        if (index >= photoCount)
        {
            continue; // 未撮影スロットは枠だけ表示
        }

        const PhotoCaptureState& capture = PhotoLog_GetEntry(index);
        if (!capture.hasPhoto || capture.items.empty())
        {
            continue;
        }

        const float innerX = cellX + kPadding;
        const float innerY = cellY + kPadding;
        const float innerWidth = kCellWidth - kPadding * 2.0f;
        const float innerHeight = kCellHeight - kPadding * 2.0f;
        const float scale = std::min(
            innerWidth / std::max(1.0f, capture.width),
            innerHeight / std::max(1.0f, capture.height));
        const float contentX = innerX + (innerWidth - capture.width * scale) * 0.5f;
        const float contentY = innerY + (innerHeight - capture.height * scale) * 0.5f;

        for (const auto& item : capture.items)
        {
            game_scene_detail::DrawCapturedPreviewItem(
                m_whiteTexture,
                item,
                contentX + item.relativeX * scale,
                contentY + item.relativeY * scale,
                item.width * scale,
                item.height * scale,
                1.0f);
        }
    }
}
