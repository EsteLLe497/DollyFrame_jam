#include "pch.h"

#include "game_scene_internal.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    unsigned int GetAttackCaptureIconColor(CapturedSpawnArchetype archetype)
    {
        switch (archetype)
        {
        case CapturedSpawnArchetype::WalkerMelee:
            return GetColor(255, 94, 42);
        case CapturedSpawnArchetype::Projectile:
            return GetColor(255, 214, 72);
        case CapturedSpawnArchetype::ShieldRushBurst:
        case CapturedSpawnArchetype::ShieldJumpBurst:
        case CapturedSpawnArchetype::ShieldNormal:
            return GetColor(86, 156, 255);
        case CapturedSpawnArchetype::MidBoss3FistAttack:
            return GetColor(255, 136, 54);
        case CapturedSpawnArchetype::MidBoss3DrillAttack:
            return GetColor(255, 196, 76);
        default:
            return GetColor(210, 86, 255);
        }
    }

    CapturedSpawnArchetype GetPrimaryAttackCaptureArchetype(const PhotoCaptureState& capture)
    {
        for (const auto& item : capture.items)
        {
            if (item.enemyAttackPaste)
            {
                return item.spawnArchetype;
            }
        }

        return CapturedSpawnArchetype::None;
    }
}

void GameScene::DrawPlayerHpBar() const
{
    const Entity* player = FindEntityByTag(kTagPlayer);
    if (!player) return;

    const auto* health = player->GetComponent<HealthComponent>();
    if (!health) return;

    const int maxHp = (std::max)(1, health->GetMaxHealth());
    const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);

    constexpr float kBarWidth = 240.0f;
    constexpr float kBarHeight = 24.0f;
    constexpr float kPanelPadding = 12.0f;
    constexpr float kMarginLeft = 32.0f;
    constexpr float kMarginTop = 32.0f;

    const float panelWidth = kBarWidth + kPanelPadding * 2.0f;
    const float panelHeight = kBarHeight + 38.0f;
    const float panelX = kMarginLeft;
    const float panelY = kMarginTop;
    const float barX = panelX + kPanelPadding;
    const float barY = panelY + kPanelPadding;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 204);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(14, 20, 28),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(200, 214, 230),
        FALSE);

    const float targetRatio = static_cast<float>(currentHp) / static_cast<float>(maxHp);
    const float displayRatio = m_ui.hpUiInitialized ? m_ui.hpDisplayRatio : targetRatio;
    const float lagRatio = m_ui.hpUiInitialized ? m_ui.hpDamageLagRatio : targetRatio;
    const float flash = m_ui.hpDamageFlash;

    // バー背景
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(38, 46, 58),
        TRUE);

    // 被弾遅延バー（減った量が一瞬残る）
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * std::clamp(lagRatio, 0.0f, 1.0f))),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(232, 94, 84),
        TRUE);

    // 現在HPバー（割合で色を変化）
    const float clampedRatio = std::clamp(displayRatio, 0.0f, 1.0f);
    const int hpR = static_cast<int>(std::round(230.0f - 160.0f * clampedRatio));
    const int hpG = static_cast<int>(std::round(76.0f + 144.0f * clampedRatio));
    const int hpB = static_cast<int>(std::round(72.0f + 46.0f * clampedRatio));
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * clampedRatio)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(hpR, hpG, hpB),
        TRUE);

    // ハイライト
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 84);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * clampedRatio)),
        static_cast<int>(std::round(barY + kBarHeight * 0.45f)),
        GetColor(255, 255, 255),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // 被弾フラッシュ
    if (flash > 0.0f)
    {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(150.0f * flash)));
        DrawBox(
            static_cast<int>(std::round(barX)),
            static_cast<int>(std::round(barY)),
            static_cast<int>(std::round(barX + kBarWidth)),
            static_cast<int>(std::round(barY + kBarHeight)),
            GetColor(255, 246, 238),
            TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // 目盛り
    for (int i = 1; i < maxHp; ++i)
    {
        const float x = barX + kBarWidth * (static_cast<float>(i) / static_cast<float>(maxHp));
        DrawLine(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + 2.0f)),
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + kBarHeight - 2.0f)),
            GetColor(42, 48, 58));
    }

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(232, 236, 246),
        FALSE);

    DrawString(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY - 18.0f)),
        "LIFE",
        GetColor(196, 214, 236));
    DrawFormatString(
        static_cast<int>(std::round(barX + kBarWidth * 0.5f) - 34.0f),
        static_cast<int>(std::round(barY + 4.0f)),
        GetColor(255, 255, 255),
        "HP %d / %d",
        currentHp,
        maxHp);
}

void GameScene::DrawMidBoss3HpBar() const
{
    const Entity* bossEntity = nullptr;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated())
        {
            continue;
        }
        const auto* boss = entity->GetComponent<MidBoss3Component>();
        if (!boss || !boss->introFinished)
        {
            continue;
        }
        bossEntity = entity.get();
        break;
    }

    if (!bossEntity)
    {
        return;
    }

    const auto* health = bossEntity->GetComponent<HealthComponent>();
    if (!health)
    {
        return;
    }

    const int maxHp = (std::max)(1, health->GetMaxHealth());
    const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);
    if (currentHp <= 0)
    {
        return;
    }

    constexpr float kBarWidth = 360.0f;
    constexpr float kBarHeight = 24.0f;
    constexpr float kPanelPadding = 12.0f;
    constexpr float kMarginTop = 30.0f;

    const float panelWidth = kBarWidth + kPanelPadding * 2.0f;
    const float panelHeight = kBarHeight + 38.0f;
    const float panelX = static_cast<float>(SCREEN_WIDTH) * 0.5f - panelWidth * 0.5f;
    const float panelY = kMarginTop;
    const float barX = panelX + kPanelPadding;
    const float barY = panelY + kPanelPadding;
    const float ratio = std::clamp(static_cast<float>(currentHp) / static_cast<float>(maxHp), 0.0f, 1.0f);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 204);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(28, 12, 14),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(232, 196, 196),
        FALSE);

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(48, 26, 30),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * ratio)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(218, 42, 48),
        TRUE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 84);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * ratio)),
        static_cast<int>(std::round(barY + kBarHeight * 0.45f)),
        GetColor(255, 232, 232),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    for (int i = 1; i < maxHp; ++i)
    {
        const float x = barX + kBarWidth * (static_cast<float>(i) / static_cast<float>(maxHp));
        DrawLine(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + 2.0f)),
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + kBarHeight - 2.0f)),
            GetColor(72, 30, 34));
    }

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(246, 220, 220),
        FALSE);
    DrawString(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY - 18.0f)),
        "BOSS",
        GetColor(248, 196, 196));
    DrawFormatString(
        static_cast<int>(std::round(barX + kBarWidth * 0.5f) - 34.0f),
        static_cast<int>(std::round(barY + 4.0f)),
        GetColor(255, 255, 255),
        "HP %d / %d",
        currentHp,
        maxHp);
}

void GameScene::DrawAttackCaptureSlot() const
{
    if (!m_photo.attackCapture.hasPhoto || !m_photo.attackCapture.containsEnemyAttackPaste)
    {
        return;
    }

    constexpr float kPanelX = 32.0f;
    constexpr float kPanelY = 124.0f;
    constexpr float kPanelSize = 96.0f;
    constexpr float kIconRadius = 28.0f;
    const float centerX = kPanelX + kPanelSize * 0.5f;
    const float centerY = kPanelY + kPanelSize * 0.5f + 4.0f;
    const CapturedSpawnArchetype archetype = GetPrimaryAttackCaptureArchetype(m_photo.attackCapture);
    const unsigned int iconColor = GetAttackCaptureIconColor(archetype);

    // Attack captures use a replaceable icon slot; the colored circle is a temporary asset stand-in.
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 210);
    DrawBox(
        static_cast<int>(std::round(kPanelX)),
        static_cast<int>(std::round(kPanelY)),
        static_cast<int>(std::round(kPanelX + kPanelSize)),
        static_cast<int>(std::round(kPanelY + kPanelSize)),
        GetColor(12, 18, 26),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(kPanelX)),
        static_cast<int>(std::round(kPanelY)),
        static_cast<int>(std::round(kPanelX + kPanelSize)),
        static_cast<int>(std::round(kPanelY + kPanelSize)),
        GetColor(224, 232, 242),
        FALSE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
    DrawCircleAA(centerX, centerY, kIconRadius + 14.0f, 64, iconColor, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ADD, 128);
    DrawCircleAA(centerX, centerY, kIconRadius + 6.0f, 64, iconColor, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 245);
    DrawCircleAA(centerX, centerY, kIconRadius, 64, iconColor, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawCircleAA(centerX, centerY, kIconRadius, 64, GetColor(255, 246, 226), FALSE, 2.0f);

    DrawString(
        static_cast<int>(std::round(kPanelX + 12.0f)),
        static_cast<int>(std::round(kPanelY + 8.0f)),
        "ATK",
        GetColor(240, 226, 196));
}
