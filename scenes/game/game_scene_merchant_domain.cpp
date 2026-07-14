#include "pch.h"

#include "game_scene_internal.h"

#include "DxLib.h"

#include <array>
#include <cmath>

using namespace game_scene_detail;

namespace
{
    struct MerchantShopItem
    {
        const char* name;
        const char* description;
        int cost;
    };

    constexpr std::array<MerchantShopItem, 2> kMerchantShopItems = {
        MerchantShopItem{
            "回復フィルター",
            "自身を撮影すると体力を全回復。",
            10,
        },
        MerchantShopItem{
            "フィルム",
            "写真保存枠を1つ増やす。最大3枠。",
            30,
        },
    };

    bool IsPointInRect(int x, int y, int left, int top, int right, int bottom)
    {
        return x >= left && x <= right && y >= top && y <= bottom;
    }

    int GetShopItemCount()
    {
        return static_cast<int>(kMerchantShopItems.size());
    }
}

void GameScene::UpdateMerchants(float deltaTime)
{
    Entity* player = FindEntityByTag(kTagPlayer);
    const auto* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    if (!playerTransform)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
    const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Merchant))
    {
        auto* merchant = entity ? entity->GetComponent<MerchantComponent>() : nullptr;
        const auto* transform = entity ? entity->GetComponent<TransformComponent>() : nullptr;
        if (!merchant || !transform)
        {
            continue;
        }

        merchant->promptPulse += deltaTime;
        merchant->playerInRange = false;

        const float merchantWidth = transform->width * transform->scale;
        const float merchantHeight = transform->height * transform->scale;
        const float centerX = transform->x + merchantWidth * 0.5f;
        const float centerY = transform->y + merchantHeight * 0.5f;
        const float detectWidth = tileSize * 6.0f;
        const float detectHeight = tileSize * 5.0f;
        const float detectLeft = centerX - detectWidth * 0.5f;
        const float detectTop = centerY - detectHeight * 0.5f;
        const float detectRight = centerX + detectWidth * 0.5f;
        const float detectBottom = centerY + detectHeight * 0.5f;

        merchant->playerInRange =
            playerCenterX >= detectLeft &&
            playerCenterX <= detectRight &&
            playerCenterY >= detectTop &&
            playerCenterY <= detectBottom;

        if (merchant->playerInRange && Input_IsKeyPressed('F') && !m_photo.placement.active)
        {
            m_ui.merchantShopOpen = true;
            m_debug.showEscapeMenu = false;
            m_ui.merchantSelection = 0;
            m_ui.merchantMessage.clear();
            m_ui.merchantMessageTimer = 0.0f;
        }
    }
}

void GameScene::UpdateMerchantShopInput()
{
    if (Input_IsActionPressed(InputAction::Cancel))
    {
        m_ui.merchantShopOpen = false;
        m_debug.showEscapeMenu = false;
        m_ui.merchantMessage.clear();
        m_ui.merchantMessageTimer = 0.0f;
        return;
    }

    const auto& merchantUi = m_ui.tuning.merchant;
    const int left = static_cast<int>(std::round((static_cast<float>(SCREEN_WIDTH) - merchantUi.panelWidth) * 0.5f));
    const int top = static_cast<int>(std::round((static_cast<float>(SCREEN_HEIGHT) - merchantUi.panelHeight) * 0.5f));
    const int listLeft = static_cast<int>(std::round(left + merchantUi.listLeftOffset));
    const int listTop = static_cast<int>(std::round(top + merchantUi.listTopOffset));
    const int listRight = static_cast<int>(std::round(left + merchantUi.listRightOffset));

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    for (int index = 0; index < GetShopItemCount(); ++index)
    {
        const int rowTop = static_cast<int>(std::round(listTop + index * merchantUi.rowHeight));
        const int rowBottom = static_cast<int>(std::round(rowTop + merchantUi.rowHeight - 10.0f));
        if (IsPointInRect(mouseX, mouseY, listLeft, rowTop, listRight, rowBottom))
        {
            m_ui.merchantSelection = index;
            break;
        }
    }

    if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
    {
        m_ui.merchantSelection = (m_ui.merchantSelection + GetShopItemCount() - 1) % GetShopItemCount();
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        m_ui.merchantSelection = (m_ui.merchantSelection + 1) % GetShopItemCount();
    }

    const bool confirmPressed =
        Input_IsActionPressed(InputAction::Confirm) ||
        Input_IsSouthButtonPressed() ||
        Input_IsMouseLeftPressed();
    if (!confirmPressed)
    {
        return;
    }

    const int selected = std::clamp(m_ui.merchantSelection, 0, GetShopItemCount() - 1);
    const MerchantShopItem& item = kMerchantShopItems[static_cast<size_t>(selected)];
    const GameSessionState& session = GameSession_Get();

    if (selected == 0)
    {
        if (session.recoveryFilterCount >= 3)
        {
            m_ui.merchantMessage = "回復フィルターは3個までです。";
            m_ui.merchantMessageTimer = 1.8f;
            return;
        }
        if (!GameSession_SpendParts(item.cost))
        {
            m_ui.merchantMessage = "部品が足りません。";
            m_ui.merchantMessageTimer = 1.8f;
            return;
        }
        GameSession_AddRecoveryFilter(1);
        m_ui.merchantMessage = "回復フィルターを1個購入しました。";
        m_ui.merchantMessageTimer = 1.8f;
        return;
    }

    if (selected == 1)
    {
        if (session.photoStorageSlots >= 3)
        {
            m_ui.merchantMessage = "写真の保存スペースは最大です。";
            m_ui.merchantMessageTimer = 1.8f;
            return;
        }
        if (!GameSession_SpendParts(item.cost))
        {
            m_ui.merchantMessage = "部品が足りません。";
            m_ui.merchantMessageTimer = 1.8f;
            return;
        }
        GameSession_SetPhotoStorageSlots(session.photoStorageSlots + 1);
        m_ui.merchantMessage = "フィルムを購入しました。保存スペース +1。";
        m_ui.merchantMessageTimer = 1.8f;
    }
}

void GameScene::DrawMerchantPrompts() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Merchant))
    {
        const auto* merchant = entity ? entity->GetComponent<MerchantComponent>() : nullptr;
        const auto* transform = entity ? entity->GetComponent<TransformComponent>() : nullptr;
        if (!merchant || !transform || !merchant->playerInRange)
        {
            continue;
        }

        const auto& merchantUi = m_ui.tuning.merchant;
        const float pulse = 0.5f + 0.5f * std::sin(merchant->promptPulse * merchantUi.promptPulseSpeed);
        const float width = transform->width * transform->scale;
        const float height = transform->height * transform->scale;
        const float screenX = viewOriginX + (transform->x + width * 0.5f - m_flow.cameraX) * viewScale;
        const float screenY =
            viewOriginY + (transform->y + height * 0.22f - m_flow.cameraY) * viewScale - pulse * merchantUi.promptRiseOffsetY;
        const int boxLeft = static_cast<int>(std::round(screenX - merchantUi.promptHalfWidth));
        const int boxTop = static_cast<int>(std::round(screenY));
        const int boxRight = static_cast<int>(std::round(screenX + merchantUi.promptHalfWidth));
        const int boxBottom = static_cast<int>(std::round(screenY + merchantUi.promptHeight));

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 210);
        DrawBox(boxLeft, boxTop, boxRight, boxBottom, GetColor(16, 20, 24), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawBox(boxLeft, boxTop, boxRight, boxBottom, GetColor(238, 216, 132), FALSE);
        DrawString(
            boxLeft + static_cast<int>(std::round(merchantUi.promptTextX)),
            boxTop + static_cast<int>(std::round(merchantUi.promptTextY)),
            "F: アイテムを購入する",
            GetColor(255, 246, 196));
    }
}

void GameScene::DrawMerchantShopOverlay() const
{
    if (!m_ui.merchantShopOpen)
    {
        return;
    }

    const GameSessionState& session = GameSession_Get();
    const auto& merchantUi = m_ui.tuning.merchant;
    const int left = static_cast<int>(std::round((static_cast<float>(SCREEN_WIDTH) - merchantUi.panelWidth) * 0.5f));
    const int top = static_cast<int>(std::round((static_cast<float>(SCREEN_HEIGHT) - merchantUi.panelHeight) * 0.5f));
    const int right = static_cast<int>(std::round(left + merchantUi.panelWidth));
    const int bottom = static_cast<int>(std::round(top + merchantUi.panelHeight));
    const int listLeft = static_cast<int>(std::round(left + merchantUi.listLeftOffset));
    const int listTop = static_cast<int>(std::round(top + merchantUi.listTopOffset));
    const int listRight = static_cast<int>(std::round(left + merchantUi.listRightOffset));

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawBox(left, top, right, bottom, GetColor(18, 22, 26), TRUE);
    DrawBox(left, top, right, bottom, GetColor(224, 206, 132), FALSE);
    DrawString(left + 36, top + 28, "行商人", GetColor(255, 246, 196));
    DrawString(left + 36, top + 58, "Esc: 退店   上下: 選択   Enter/クリック: 購入", GetColor(178, 196, 208));
    DrawFormatString(right - 172, top + 34, GetColor(255, 224, 92), "部品 x %d", session.parts);

    for (int index = 0; index < GetShopItemCount(); ++index)
    {
        const MerchantShopItem& item = kMerchantShopItems[static_cast<size_t>(index)];
        const int rowTop = static_cast<int>(std::round(listTop + index * merchantUi.rowHeight));
        const int rowBottom = static_cast<int>(std::round(rowTop + merchantUi.rowHeight - 10.0f));
        const bool selected = index == std::clamp(m_ui.merchantSelection, 0, GetShopItemCount() - 1);
        const bool affordable = session.parts >= item.cost;

        DrawBox(
            listLeft,
            rowTop,
            listRight,
            rowBottom,
            selected ? GetColor(78, 88, 74) : GetColor(30, 36, 42),
            TRUE);
        DrawBox(
            listLeft,
            rowTop,
            listRight,
            rowBottom,
            selected ? GetColor(255, 238, 166) : GetColor(92, 108, 116),
            FALSE);
        DrawString(listLeft + 18, rowTop + 14, item.name, selected ? GetColor(255, 252, 224) : GetColor(218, 228, 234));
        DrawFormatString(
            listRight - 116,
            rowTop + 14,
            affordable ? GetColor(255, 222, 90) : GetColor(150, 154, 154),
            "部品 %d",
            item.cost);
        DrawString(listLeft + 18, rowTop + 40, item.description, GetColor(162, 180, 190));
    }

    const int selected = std::clamp(m_ui.merchantSelection, 0, GetShopItemCount() - 1);
    const MerchantShopItem& selectedItem = kMerchantShopItems[static_cast<size_t>(selected)];
    const int detailLeft = static_cast<int>(std::round(left + merchantUi.detailLeftOffset));
    const int detailTop = static_cast<int>(std::round(top + merchantUi.detailTopOffset));
    const int detailRight = right - 44;
    const int detailBottom = static_cast<int>(std::round(bottom - merchantUi.detailBottomOffset));
    DrawBox(detailLeft, detailTop, detailRight, detailBottom, GetColor(26, 31, 34), TRUE);
    DrawBox(detailLeft, detailTop, detailRight, detailBottom, GetColor(96, 112, 116), FALSE);
    DrawString(detailLeft + 22, detailTop + 22, selectedItem.name, GetColor(255, 246, 196));
    DrawString(detailLeft + 22, detailTop + 56, selectedItem.description, GetColor(204, 218, 224));

    if (selected == 0)
    {
        DrawFormatString(detailLeft + 22, detailTop + 104, GetColor(220, 230, 236), "所持数: %d / 3", session.recoveryFilterCount);
    }
    else
    {
        DrawFormatString(detailLeft + 22, detailTop + 104, GetColor(220, 230, 236), "写真保存枠: %d / 3", session.photoStorageSlots);
    }

    const int buyLeft = detailLeft + 22;
    const int buyTop = detailBottom - 66;
    const int buyRight = detailRight - 22;
    const int buyBottom = detailBottom - 20;
    const bool canBuy =
        session.parts >= selectedItem.cost &&
        ((selected == 0 && session.recoveryFilterCount < 3) || (selected == 1 && session.photoStorageSlots < 3));
    DrawBox(buyLeft, buyTop, buyRight, buyBottom, canBuy ? GetColor(188, 148, 40) : GetColor(70, 76, 78), TRUE);
    DrawBox(buyLeft, buyTop, buyRight, buyBottom, canBuy ? GetColor(255, 238, 148) : GetColor(116, 124, 128), FALSE);
    DrawString(buyLeft + 108, buyTop + 14, "購入する", canBuy ? GetColor(32, 28, 18) : GetColor(172, 180, 182));

    if (m_ui.merchantMessageTimer > 0.0f && !m_ui.merchantMessage.empty())
    {
        DrawString(left + 48, bottom - 54, m_ui.merchantMessage.c_str(), GetColor(255, 218, 116));
    }
}
