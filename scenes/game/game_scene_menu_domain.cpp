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
    constexpr int kDebugEscapeMenuItemCount = 16;
    constexpr float kEscapeMenuSceneFadeOutDuration = 0.45f;
    constexpr std::array<int, 6> kPlayerEscapeMenuActions = { 0, 5, 6, 7, 14, 15 };

    constexpr int GetEscapeMenuItemCount()
    {
        if constexpr (build_config::kDebugFeaturesEnabled)
        {
            return kDebugEscapeMenuItemCount;
        }
        return static_cast<int>(kPlayerEscapeMenuActions.size());
    }

    int GetEscapeMenuActionIndex(int visibleIndex)
    {
        if constexpr (build_config::kDebugFeaturesEnabled)
        {
            return visibleIndex;
        }
        return kPlayerEscapeMenuActions[static_cast<size_t>(visibleIndex)];
    }

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
        case 11: return "背景グリッド";
        case 12: return "テスト写真UI";
        case 13: return "シーンを再読み込み";
        case 14: return "タイトルへ戻る";
        case 15: return "ゲームを終了";
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
void GameScene::UpdateEscapeMenuInput(float deltaTime)
{
    m_debug.escapeMenuAnimation = std::min(
        1.0f,
        m_debug.escapeMenuAnimation + std::max(0.0f, deltaTime) / 0.28f);

    const auto& menuUi = m_ui.tuning.escapeMenu;
    const int itemCount = GetEscapeMenuItemCount();
    const float panelWidth = build_config::kDebugFeaturesEnabled
        ? menuUi.panelWidth
        : std::min(760.0f, static_cast<float>(SCREEN_WIDTH) - 80.0f);
    const float panelHeight = build_config::kDebugFeaturesEnabled
        ? menuUi.panelHeight
        : 460.0f;
    const float rowHeight = build_config::kDebugFeaturesEnabled ? menuUi.rowHeight : 52.0f;
    const float rowBottomInset = build_config::kDebugFeaturesEnabled ? menuUi.rowBottomInset : 8.0f;
    const int panelLeft = static_cast<int>(std::round((static_cast<float>(SCREEN_WIDTH) - panelWidth) * 0.5f));
    const int panelTop = static_cast<int>(std::round((static_cast<float>(SCREEN_HEIGHT) - panelHeight) * 0.5f));
    const int rowStartY = static_cast<int>(std::round(panelTop + (build_config::kDebugFeaturesEnabled ? menuUi.rowStartOffset : 104.0f)));
    const int rowLeft = static_cast<int>(std::round(panelLeft + (build_config::kDebugFeaturesEnabled ? menuUi.rowPaddingX : 290.0f)));
    const int rowRight = static_cast<int>(std::round(panelLeft + panelWidth - (build_config::kDebugFeaturesEnabled ? menuUi.rowPaddingX : 28.0f)));

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    for (int index = 0; index < itemCount; ++index)
    {
        const int rowTop = static_cast<int>(std::round(rowStartY + index * rowHeight));
        const int rowBottom = static_cast<int>(std::round(rowTop + rowHeight - rowBottomInset));
        if (mouseX >= rowLeft && mouseX <= rowRight && mouseY >= rowTop && mouseY <= rowBottom)
        {
            m_debug.escapeMenuSelection = index;
            break;
        }
    }

    if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
    {
        m_debug.escapeMenuSelection = (m_debug.escapeMenuSelection + itemCount - 1) % itemCount;
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        m_debug.escapeMenuSelection = (m_debug.escapeMenuSelection + 1) % itemCount;
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
        switch (GetEscapeMenuActionIndex(m_debug.escapeMenuSelection))
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
            m_debug.showBackdropGrid = !m_debug.showBackdropGrid;
            break;
        case 12:
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

    switch (GetEscapeMenuActionIndex(m_debug.escapeMenuSelection))
    {
    case 0:
        m_debug.showEscapeMenu = false;
        m_debug.escapeMenuAnimation = 0.0f;
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
        m_debug.showBackdropGrid = !m_debug.showBackdropGrid;
        break;
    case 12:
        m_testPhotos.enabled = !m_testPhotos.enabled;
        break;
    case 13:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
        break;
    case 14:
        m_debug.showEscapeMenu = false;
        m_debug.escapeMenuAnimation = 0.0f;
        m_flow.sceneFadeOutActive = true;
        m_flow.sceneFadeOutTimer = kEscapeMenuSceneFadeOutDuration;
        m_flow.sceneFadeOutTarget = "title";
        Audio_FadeOutBgm(kEscapeMenuSceneFadeOutDuration);
        break;
    case 15:
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

    const auto& menuUi = m_ui.tuning.escapeMenu;
    const int itemCount = GetEscapeMenuItemCount();

    if constexpr (!build_config::kDebugFeaturesEnabled)
    {
        const float rawT = std::clamp(m_debug.escapeMenuAnimation, 0.0f, 1.0f);
        const float openT = 1.0f - std::pow(1.0f - rawT, 3.0f);
        const int dimAlpha = static_cast<int>(std::round(150.0f * openT));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, dimAlpha);
        DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(8, 5, 4), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        const float panelWidth = std::min(760.0f, static_cast<float>(SCREEN_WIDTH) - 80.0f);
        constexpr float panelHeight = 460.0f;
        const int left = static_cast<int>(std::round((static_cast<float>(SCREEN_WIDTH) - panelWidth) * 0.5f));
        const int targetTop = static_cast<int>(std::round((static_cast<float>(SCREEN_HEIGHT) - panelHeight) * 0.5f));
        const int top = targetTop + static_cast<int>(std::round((1.0f - openT) * 96.0f));
        const int right = static_cast<int>(std::round(left + panelWidth));
        const int bottom = top + static_cast<int>(panelHeight);
        const int paper = GetColor(232, 216, 184);
        const int paperDark = GetColor(202, 178, 137);
        const int ink = GetColor(56, 40, 30);
        const int inkMuted = GetColor(112, 86, 62);
        const int accent = GetColor(170, 91, 48);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(255.0f * openT)));
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

        const int portraitTexture = m_assets.GetTexture("dolly_pause_portrait");
        if (portraitTexture >= 0)
        {
            constexpr float portraitSize = 174.0f;
            const float portraitX = static_cast<float>(photoLeft + photoRight) * 0.5f - portraitSize * 0.5f;
            const float portraitY = static_cast<float>(photoTop + 16);
            Shader_ResetStyle();
            Shader_SetTint(1.0f, 0.96f, 0.88f, openT);
            SpriteDraw(
                portraitTexture,
                portraitX,
                portraitY,
                portraitSize,
                portraitSize,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
            Shader_ResetStyle();
        }
        DrawString(photoLeft + 22, photoBottom - 36, "FILM  /  PLAYER MENU", inkMuted);

        const int rowStartY = top + 104;
        constexpr int rowHeight = 52;
        for (int index = 0; index < itemCount; ++index)
        {
            const int actionIndex = GetEscapeMenuActionIndex(index);
            const float itemT = std::clamp((rawT - static_cast<float>(index) * 0.055f) / 0.67f, 0.0f, 1.0f);
            if (itemT <= 0.0f)
            {
                continue;
            }
            const int rowLeft = left + 290 + static_cast<int>(std::round((1.0f - itemT) * 28.0f));
            const int rowRight = right - 28;
            const int rowTop = rowStartY + index * rowHeight;
            const int rowBottom = rowTop + 44;
            const bool selected = m_debug.escapeMenuSelection == index;
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

            DrawString(rowLeft + 18, rowTop + 12, GetEscapeMenuItemLabel(actionIndex), selected ? ink : inkMuted);
            if (actionIndex == 5 || actionIndex == 6)
            {
                const float volume = actionIndex == 5 ? Audio_GetMasterVolume() : Audio_GetSeVolume();
                const int activeTicks = static_cast<int>(std::round(volume * 10.0f));
                const int tickStartX = rowRight - 128;
                for (int tick = 0; tick < 10; ++tick)
                {
                    const int tickLeft = tickStartX + tick * 11;
                    DrawBox(tickLeft, rowTop + 15, tickLeft + 7, rowTop + 29,
                        tick < activeTicks ? accent : GetColor(184, 165, 137), TRUE);
                }
            }
            else if (actionIndex == 7)
            {
                DrawString(rowRight - 58, rowTop + 12, GetOnOffLabel(m_debug.screenShakeEnabled), selected ? ink : inkMuted);
            }
        }

        DrawString(left + 32, bottom - 42, "Enter / A  決定", inkMuted);
        DrawString(right - 198, bottom - 42, "Esc / B  戻る", inkMuted);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        return;
    }

    const float panelHeight = build_config::kDebugFeaturesEnabled
        ? menuUi.panelHeight
        : menuUi.rowStartOffset + menuUi.rowHeight * static_cast<float>(itemCount) + 24.0f;
    const int left = static_cast<int>(std::round((static_cast<float>(SCREEN_WIDTH) - menuUi.panelWidth) * 0.5f));
    const int top = static_cast<int>(std::round((static_cast<float>(SCREEN_HEIGHT) - panelHeight) * 0.5f));
    const int right = static_cast<int>(std::round(left + menuUi.panelWidth));
    const int bottom = static_cast<int>(std::round(top + panelHeight));

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 156);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawBox(left, top, right, bottom, GetColor(18, 24, 30), TRUE);
    DrawBox(left, top, right, bottom, GetColor(210, 220, 236), FALSE);
    DrawString(left + 22, top + 18,
        build_config::kDebugFeaturesEnabled ? "一時停止メニュー [DEBUG]" : "一時停止メニュー",
        GetColor(245, 248, 255));
    DrawString(left + 22, top + 44, "W/S・上下キー: 選択  A/D・左右キー: 調整  Enter/A/クリック: 決定  Esc: 閉じる", GetColor(170, 194, 220));

    const int rowStartY = static_cast<int>(std::round(top + menuUi.rowStartOffset));
    for (int index = 0; index < itemCount; ++index)
    {
        const int actionIndex = GetEscapeMenuActionIndex(index);
        const int rowTop = static_cast<int>(std::round(rowStartY + index * menuUi.rowHeight));
        const int rowBottom = static_cast<int>(std::round(rowTop + menuUi.rowHeight - menuUi.rowBottomInset));
        const bool selected = (m_debug.escapeMenuSelection == index);

        DrawBox(
            left + static_cast<int>(std::round(menuUi.rowPaddingX)),
            rowTop,
            right - static_cast<int>(std::round(menuUi.rowPaddingX)),
            rowBottom,
            selected ? GetColor(72, 102, 136) : GetColor(28, 36, 46),
            TRUE);
        DrawBox(
            left + static_cast<int>(std::round(menuUi.rowPaddingX)),
            rowTop,
            right - static_cast<int>(std::round(menuUi.rowPaddingX)),
            rowBottom,
            selected ? GetColor(236, 244, 255) : GetColor(92, 116, 140),
            FALSE);

        const int textColor = selected ? GetColor(245, 252, 255) : GetColor(204, 218, 232);
        switch (actionIndex)
        {
        case 1:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_debug.effectPlacementPulseEnabled));
            break;
        case 2:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_debug.effectPasteStickEnabled));
            break;
        case 3:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_debug.effectPasteRingEnabled));
            break;
        case 4:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_debug.bgmEnabled));
            break;
        case 5:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %d%%", GetEscapeMenuItemLabel(actionIndex), static_cast<int>(std::round(Audio_GetMasterVolume() * 100.0f)));
            break;
        case 6:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %d%%", GetEscapeMenuItemLabel(actionIndex), static_cast<int>(std::round(Audio_GetSeVolume() * 100.0f)));
            break;
        case 7:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_debug.screenShakeEnabled));
            break;
        case 8:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_lifecycle.darknessStageEnabled));
            break;
        case 9:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_debug.playerHealthDamageEnabled));
            break;
        case 10:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(!m_debug.hideNonPhotoUi));
            break;
        case 11:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_debug.showBackdropGrid));
            break;
        case 12:
            DrawFormatString(left + 34, rowTop + 10, textColor, "%s: %s", GetEscapeMenuItemLabel(actionIndex), GetOnOffLabel(m_testPhotos.enabled));
            break;
        default:
            DrawString(left + 34, rowTop + 10, GetEscapeMenuItemLabel(actionIndex), textColor);
            break;
        }
    }
}
