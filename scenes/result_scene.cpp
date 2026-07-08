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
#include <cstring>
#include <tracy/Tracy.hpp>

namespace
{
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
    constexpr const char* kMenuLabels[kMenuOptionCount] =
    {
        "廃墟ステージへ進む",
        "タイトルへ戻る",
    };
    constexpr int kMenuRowWidth = 360;
    constexpr int kMenuRowHeight = 44;
    constexpr int kMenuRowGap = 14;
    constexpr int kMenuRowTop = 420;
}

ResultScene::ResultScene()
    : m_whiteTexture(-1)
    , m_blinkTimer(0.0f)
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
    m_showPrompt = true;
    m_selectedOption = 0;
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
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        m_selectedOption = (m_selectedOption + 1) % kMenuOptionCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
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
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "test_tone", 0.0f, 0.0f });
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
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "scene_change", 0.0f, 0.0f });

    if (m_selectedOption == 0)
    {
        // 廃墟ステージ (ruins1.csv) へ進む
        GameSession_SetStartMapCsvPath("assets/maps/stages/ruins1.csv");
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
    m_blinkTimer += deltaTime;
    if (m_blinkTimer >= 0.45f)
    {
        m_blinkTimer = 0.0f;
        m_showPrompt = !m_showPrompt;
    }

    UpdateMenuInput();
}

void ResultScene::Draw()
{
    DrawBackdrop();
    DrawMenu();
}

void ResultScene::DrawMenu() const
{
    const GameSessionState& session = GameSession_Get();
    const bool cleared = session.endReason == GameEndReason::GoalReached ||
        session.endReason == GameEndReason::BossDefeated;

    // 大見出し
    if (cleared)
    {
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2, 220, "GAME CLEAR", GetColor(255, 244, 220), GetColor(52, 30, 14));
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2, 260, "ゲームクリア", GetColor(255, 226, 164), GetColor(28, 16, 9));
    }
    else
    {
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2, 220, "GAME OVER", GetColor(255, 244, 220), GetColor(52, 30, 14));
        DrawCenteredOutlinedString(SCREEN_WIDTH / 2, 260, "ゲームオーバー", GetColor(255, 226, 164), GetColor(28, 16, 9));
    }

    char detail[64] = {};
    std::snprintf(detail, sizeof(detail), "%s", ToReasonLabel(session.endReason));
    DrawCenteredOutlinedString(SCREEN_WIDTH / 2, 320, detail, GetColor(242, 226, 194), GetColor(28, 16, 9));

    // 選択肢
    for (int index = 0; index < kMenuOptionCount; ++index)
    {
        const MenuOptionRect rect = GetOptionRect(index);
        DrawMenuRow(rect.left, rect.top, kMenuRowWidth, kMenuRowHeight, kMenuLabels[index], m_selectedOption == index);
    }

    const int hintColor = m_showPrompt ? GetColor(252, 238, 214) : GetColor(168, 140, 104);
    DrawCenteredOutlinedString(
        SCREEN_WIDTH / 2,
        kMenuRowTop + kMenuOptionCount * (kMenuRowHeight + kMenuRowGap) + 30,
        "上下キー・マウス: 選択   Enter/Space/A/クリック: 決定",
        hintColor,
        GetColor(28, 16, 9));
}

void ResultScene::DrawDebugUI()
{
    const GameSessionState& session = GameSession_Get();
    ImGui::Begin("リザルト");
    ImGui::Text("試作版のリザルト画面です");
    ImGui::Text("Result: %s", ToReasonLabel(session.endReason));
    ImGui::Text("HP: %d / %d", session.currentHp, session.maxHp);
    ImGui::Text("残り時間: %.1f / %.1f", session.timeRemaining, session.timeLimit);
    ImGui::Text("選択中: %s", m_selectedOption == 0 ? "森の廃墟へ進む" : "タイトルへ戻る");
    ImGui::Text("上下キー/マウスホバーで選択、Enter/Space/A/クリックで決定");
    ImGui::Text("プロンプト表示: %s", m_showPrompt ? "あり" : "なし");
    ImGui::End();
}

EventBus* ResultScene::GetEventBus()
{
    return &m_eventBus;
}

void ResultScene::DrawBackdrop() const
{
    const GameSessionState& session = GameSession_Get();
    const bool cleared = session.endReason == GameEndReason::GoalReached ||
        session.endReason == GameEndReason::BossDefeated;
    const float accentR = cleared ? 0.18f : 0.78f;
    const float accentG = cleared ? 0.62f : 0.24f;
    const float accentB = cleared ? 0.32f : 0.14f;
    Shader_SetTint(0.11f, 0.07f, 0.10f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(accentR, accentG, accentB, 1.0f);
    SpriteDraw(m_whiteTexture, 112.0f, 112.0f, static_cast<float>(SCREEN_WIDTH) - 224.0f, 20.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.18f, 0.10f, 0.16f, 1.0f);
    SpriteDraw(m_whiteTexture, 112.0f, 164.0f, static_cast<float>(SCREEN_WIDTH) - 224.0f, 220.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.95f, 0.84f, 0.32f, 1.0f);
    SpriteDraw(m_whiteTexture, 152.0f, 206.0f, static_cast<float>(SCREEN_WIDTH) - 304.0f, 30.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    if (m_showPrompt)
    {
        Shader_SetTint(0.88f, 0.88f, 0.88f, 1.0f);
        SpriteDraw(m_whiteTexture, 192.0f, 430.0f, static_cast<float>(SCREEN_WIDTH) - 384.0f, 34.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}