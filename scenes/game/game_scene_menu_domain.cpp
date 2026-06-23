// =========================================================
// ファイルの情報[game_scene_menu_domain.cpp]
//
// 制作者:Masatora Tanaka        日付:2026/06/23
// =========================================================
#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>
#include <cmath>

#include "DxLib.h"
#include "audio.h"

using namespace game_scene_detail;

namespace
{
    constexpr int kEscapeMenuItemCount = 15;
    constexpr int kEscapeMenuPanelWidth = 560;
    constexpr int kEscapeMenuPanelHeight = 660;
    constexpr int kEscapeMenuRowStartOffset = 86;
    constexpr int kEscapeMenuRowHeight = 38;
    constexpr int kEscapeMenuRowPaddingX = 18;
    constexpr int kEscapeMenuRowBottomInset = 4;

    // =========================================================
    // メニュー項目の名前取得
    // =========================================================
    const char* GetEscapeMenuItemLabel(int index)
    {
        switch (index)
        {
        case 0: return "ゲームに戻る";
        case 1: return "配置プレビューパルス";
        case 2: return "貼り付け吸着アニメ";
        case 3: return "貼り付けリング演出";
        case 4: return "BGM";
        case 5: return "マスター音量";
        case 6: return "SE音量";
        case 7: return "画面揺れ";
        case 8: return "暗闇エフェクト";
        case 9: return "プレイヤーHPダメージ";
        case 10: return "写真以外のUI";
        case 11: return "テスト写真UI";
        case 12: return "シーンを再読み込み";
        case 13: return "タイトルへ戻る";
        case 14: return "ゲームを終了";
        default: return "";
        }
    }

    // =========================================================
    // オンオフ表示の名前取得
    // =========================================================
    const char* GetOnOffLabel(bool enabled)
    {
        return enabled ? "オン" : "オフ";
    }
}

// =========================================================
// ESCメニュー入力更新
// =========================================================
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
        case 8:
            m_lifecycle.darknessStageEnabled = !m_lifecycle.darknessStageEnabled;
            break;
        case 9:
            m_debug.playerHealthDamageEnabled = !m_debug.playerHealthDamageEnabled;
            break;
        case 10:
            m_debug.hideNonPhotoUi = !m_debug.hideNonPhotoUi;
            break;
        case 11:
            m_testPhotos.enabled = !m_testPhotos.enabled;
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
        m_lifecycle.darknessStageEnabled = !m_lifecycle.darknessStageEnabled;
        break;
    case 9:
        m_debug.playerHealthDamageEnabled = !m_debug.playerHealthDamageEnabled;
        break;
    case 10:
        m_debug.hideNonPhotoUi = !m_debug.hideNonPhotoUi;
        break;
    case 11:
        m_testPhotos.enabled = !m_testPhotos.enabled;
        break;
    case 12:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
        break;
    case 13:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
        break;
    case 14:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::ExitApplicationRequested, nullptr, nullptr, "", 0.0f, 0.0f });
        break;
    default:
        break;
    }
}

// =========================================================
// BGMオンオフ切り替え
// =========================================================
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

    const float restoreVolume = m_debug.bgmRestoreVolume > 0.001f ? m_debug.bgmRestoreVolume : 1.0f;
    Audio_SetMasterVolume(restoreVolume);
    m_debug.bgmEnabled = true;
}

// =========================================================
// ESCメニュー描画
// =========================================================
void GameScene::DrawEscapeMenuOverlay() const
{
    if (!m_debug.showEscapeMenu)
    {
        return;
    }

    const int left = (SCREEN_WIDTH - kEscapeMenuPanelWidth) / 2;
    const int top = (SCREEN_HEIGHT - kEscapeMenuPanelHeight) / 2;
    const int right = left + kEscapeMenuPanelWidth;
    const int bottom = top + kEscapeMenuPanelHeight;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 156);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawBox(left, top, right, bottom, GetColor(18, 24, 30), TRUE);
    DrawBox(left, top, right, bottom, GetColor(210, 220, 236), FALSE);
    DrawString(left + 22, top + 18, "一時停止メニュー", GetColor(245, 248, 255));
    DrawString(left + 22, top + 44, "W/S・上下キー: 選択  A/D・左右キー: 調整  Enter/A/クリック: 決定  Esc: 閉じる", GetColor(170, 194, 220));

    const int rowStartY = top + kEscapeMenuRowStartOffset;
    for (int index = 0; index < kEscapeMenuItemCount; ++index)
    {
        const int rowTop = rowStartY + index * kEscapeMenuRowHeight;
        const int rowBottom = rowTop + kEscapeMenuRowHeight - kEscapeMenuRowBottomInset;
        const bool selected = (m_debug.escapeMenuSelection == index);

        DrawBox(
            left + kEscapeMenuRowPaddingX,
            rowTop,
            right - kEscapeMenuRowPaddingX,
            rowBottom,
            selected ? GetColor(72, 102, 136) : GetColor(28, 36, 46),
            TRUE);
        DrawBox(
            left + kEscapeMenuRowPaddingX,
            rowTop,
            right - kEscapeMenuRowPaddingX,
            rowBottom,
            selected ? GetColor(236, 244, 255) : GetColor(92, 116, 140),
            FALSE);

        const int textColor = selected ? GetColor(245, 252, 255) : GetColor(204, 218, 232);
        switch (index)
        {
        case 1:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_debug.effectPlacementPulseEnabled));
            break;
        case 2:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_debug.effectPasteStickEnabled));
            break;
        case 3:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_debug.effectPasteRingEnabled));
            break;
        case 4:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_debug.bgmEnabled));
            break;
        case 5:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %d%%", GetEscapeMenuItemLabel(index), static_cast<int>(std::round(Audio_GetMasterVolume() * 100.0f)));
            break;
        case 6:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %d%%", GetEscapeMenuItemLabel(index), static_cast<int>(std::round(Audio_GetSeVolume() * 100.0f)));
            break;
        case 7:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_debug.screenShakeEnabled));
            break;
        case 8:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_lifecycle.darknessStageEnabled));
            break;
        case 9:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_debug.playerHealthDamageEnabled));
            break;
        case 10:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(!m_debug.hideNonPhotoUi));
            break;
        case 11:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(index), GetOnOffLabel(m_testPhotos.enabled));
            break;
        default:
            DrawString(left + 34, rowTop + 10, GetEscapeMenuItemLabel(index), textColor);
            break;
        }
    }
}
