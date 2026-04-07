#include "game_scene_internal.h"
#include "audio.h"
#include "DxLib.h"

#include <algorithm>
#include <cmath>

using namespace game_scene_detail;

namespace
{
    constexpr int kEscapeMenuItemCount = 11;
    constexpr int kEscapeMenuPanelWidth = 560;
    constexpr int kEscapeMenuPanelHeight = 540;
    constexpr int kEscapeMenuRowStartOffset = 86;
    constexpr int kEscapeMenuRowHeight = 38;
    constexpr int kEscapeMenuRowPaddingX = 18;
    constexpr int kEscapeMenuRowBottomInset = 4;
}

void GameScene::UpdateEscapeMenuInput()
{
    const int panelLeft = (SCREEN_WIDTH - kEscapeMenuPanelWidth) / 2;
    const int panelTop = (SCREEN_HEIGHT - kEscapeMenuPanelHeight) / 2;
    const int rowStartY = panelTop + kEscapeMenuRowStartOffset;
    const int rowLeft = panelLeft + kEscapeMenuRowPaddingX;
    const int rowRight = panelLeft + kEscapeMenuPanelWidth - kEscapeMenuRowPaddingX;

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    for (int index = 0; index < kEscapeMenuItemCount; ++index)
    {
        const int rowTop = rowStartY + index * kEscapeMenuRowHeight;
        const int rowBottom = rowTop + kEscapeMenuRowHeight - kEscapeMenuRowBottomInset;
        if (mouseX >= rowLeft && mouseX <= rowRight && mouseY >= rowTop && mouseY <= rowBottom)
        {
            m_debug.escapeMenuSelection = index;
            break;
        }
    }

    if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
    {
        m_debug.escapeMenuSelection = (m_debug.escapeMenuSelection + kEscapeMenuItemCount - 1) % kEscapeMenuItemCount;
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        m_debug.escapeMenuSelection = (m_debug.escapeMenuSelection + 1) % kEscapeMenuItemCount;
    }

    const auto adjustMasterVolume = [&](float delta)
    {
        const float currentVolume = Audio_GetMasterVolume();
        const float nextVolume = std::clamp(currentVolume + delta, 0.0f, 1.0f);
        Audio_SetMasterVolume(nextVolume);
        m_debug.bgmEnabled = nextVolume > 0.001f;
        if (nextVolume > 0.001f)
        {
            m_debug.bgmRestoreVolume = nextVolume;
        }
    };
    const auto adjustSeVolume = [&](float delta)
    {
        const float currentVolume = Audio_GetSeVolume();
        const float nextVolume = std::clamp(currentVolume + delta, 0.0f, 1.0f);
        Audio_SetSeVolume(nextVolume);
    };

    const bool toggleLeft = Input_IsActionPressed(InputAction::MoveLeft);
    const bool toggleRight = Input_IsActionPressed(InputAction::MoveRight);
    if (toggleLeft || toggleRight)
    {
        switch (m_debug.escapeMenuSelection)
        {
        case 1:
            m_debug.effectPlacementPulseEnabled = !m_debug.effectPlacementPulseEnabled;
            break;
        case 2:
            m_debug.effectPasteStickEnabled = !m_debug.effectPasteStickEnabled;
            break;
        case 3:
            m_debug.effectPasteRingEnabled = !m_debug.effectPasteRingEnabled;
            break;
        case 4:
            ToggleEscapeMenuBgm();
            break;
        case 5:
            adjustMasterVolume(toggleRight ? 0.05f : -0.05f);
            break;
        case 6:
            adjustSeVolume(toggleRight ? 0.05f : -0.05f);
            break;
        case 7:
            m_debug.screenShakeEnabled = !m_debug.screenShakeEnabled;
            break;
        default:
            break;
        }
    }

    const bool confirmPressed =
        Input_IsActionPressed(InputAction::Confirm) ||
        Input_IsSouthButtonPressed() ||
        Input_IsMouseLeftPressed();
    if (!confirmPressed)
    {
        return;
    }

    switch (m_debug.escapeMenuSelection)
    {
    case 0:
        m_debug.showEscapeMenu = false;
        break;
    case 1:
        m_debug.effectPlacementPulseEnabled = !m_debug.effectPlacementPulseEnabled;
        break;
    case 2:
        m_debug.effectPasteStickEnabled = !m_debug.effectPasteStickEnabled;
        break;
    case 3:
        m_debug.effectPasteRingEnabled = !m_debug.effectPasteRingEnabled;
        break;
    case 4:
        ToggleEscapeMenuBgm();
        break;
    case 5:
        adjustMasterVolume(0.05f);
        break;
    case 6:
        adjustSeVolume(0.05f);
        break;
    case 7:
        m_debug.screenShakeEnabled = !m_debug.screenShakeEnabled;
        break;
    case 8:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
        break;
    case 9:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
        break;
    case 10:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::ExitApplicationRequested, nullptr, nullptr, "", 0.0f, 0.0f });
        break;
    default:
        break;
    }
}

void GameScene::ToggleEscapeMenuBgm()
{
    if (m_debug.bgmEnabled)
    {
        const float currentVolume = Audio_GetMasterVolume();
        if (currentVolume > 0.001f)
        {
            m_debug.bgmRestoreVolume = currentVolume;
        }
        Audio_SetMasterVolume(0.0f);
        m_debug.bgmEnabled = false;
        return;
    }

    const float restoreVolume = m_debug.bgmRestoreVolume > 0.001f ? m_debug.bgmRestoreVolume : 0.6f;
    Audio_SetMasterVolume(restoreVolume);
    m_debug.bgmEnabled = true;
}

void GameScene::DrawEscapeMenuOverlay() const
{
    if (!m_debug.showEscapeMenu)
    {
        return;
    }

    const int panelWidth = 560;
    const int panelHeight = 540;
    const int left = (SCREEN_WIDTH - panelWidth) / 2;
    const int top = (SCREEN_HEIGHT - panelHeight) / 2;
    const int right = left + panelWidth;
    const int bottom = top + panelHeight;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 156);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawBox(left, top, right, bottom, GetColor(18, 24, 30), TRUE);
    DrawBox(left, top, right, bottom, GetColor(210, 220, 236), FALSE);
    DrawString(left + 22, top + 18, "一時停止メニュー", GetColor(245, 248, 255));
    DrawString(left + 22, top + 44, "W/S・十字キー・マウス: 選択  A/D・左右キー: 調整  Enter/A/左クリック: 決定  Esc: 閉じる", GetColor(170, 194, 220));

    const int rowStartY = top + 86;
    const int rowHeight = 38;
    for (int index = 0; index < 11; ++index)
    {
        const int rowTop = rowStartY + index * rowHeight;
        const int rowBottom = rowTop + rowHeight - 4;
        const bool selected = (m_debug.escapeMenuSelection == index);

        DrawBox(
            left + 18,
            rowTop,
            right - 18,
            rowBottom,
            selected ? GetColor(72, 102, 136) : GetColor(28, 36, 46),
            TRUE);
        DrawBox(
            left + 18,
            rowTop,
            right - 18,
            rowBottom,
            selected ? GetColor(236, 244, 255) : GetColor(92, 116, 140),
            FALSE);

        const int textColor = selected ? GetColor(245, 252, 255) : GetColor(204, 218, 232);
        switch (index)
        {
        case 0:
            DrawString(left + 34, rowTop + 10, "ゲームに戻る", textColor);
            break;
        case 1:
            DrawFormatString(left + 34, rowTop + 10, textColor, "配置プレビュー脈動: %s", m_debug.effectPlacementPulseEnabled ? "ON" : "OFF");
            break;
        case 2:
            DrawFormatString(left + 34, rowTop + 10, textColor, "貼り付きアニメ: %s", m_debug.effectPasteStickEnabled ? "ON" : "OFF");
            break;
        case 3:
            DrawFormatString(left + 34, rowTop + 10, textColor, "貼り付けリング演出: %s", m_debug.effectPasteRingEnabled ? "ON" : "OFF");
            break;
        case 4:
            DrawFormatString(left + 34, rowTop + 10, textColor, "BGM: %s", m_debug.bgmEnabled ? "ON" : "OFF");
            break;
        case 5:
            DrawFormatString(left + 34, rowTop + 10, textColor, "マスター音量: %d%%", static_cast<int>(std::round(Audio_GetMasterVolume() * 100.0f)));
            break;
        case 6:
            DrawFormatString(left + 34, rowTop + 10, textColor, "SE音量: %d%%", static_cast<int>(std::round(Audio_GetSeVolume() * 100.0f)));
            break;
        case 7:
            DrawFormatString(left + 34, rowTop + 10, textColor, "画面揺れ: %s", m_debug.screenShakeEnabled ? "ON" : "OFF");
            break;
        case 8:
            DrawString(left + 34, rowTop + 10, "シーンをリスタート", textColor);
            break;
        case 9:
            DrawString(left + 34, rowTop + 10, "タイトルに戻る", textColor);
            break;
        case 10:
            DrawString(left + 34, rowTop + 10, "ゲームを終える", textColor);
            break;
        default:
            break;
        }
    }
}
