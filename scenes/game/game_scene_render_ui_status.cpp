#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_hp_ui_layout.h"
#include "game_scene_combat_common.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr const char* kHpTextureKey = "ui_hp";

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
        case CapturedSpawnArchetype::LaserTurret:
            return GetColor(82, 168, 255);
        default:
            return GetColor(210, 86, 255);
        }
    }

    int GetAttackCaptureCount(const PhotoCaptureState& capture)
    {
        if (capture.attackCaptureCount > 0)
        {
            return capture.attackCaptureCount;
        }

        const int countedItems = static_cast<int>(std::count_if(
            capture.items.begin(),
            capture.items.end(),
            [](const CapturedPhotoItem& item)
            {
                return item.enemyAttackPaste;
            }));
        if (countedItems > 0)
        {
            return countedItems;
        }

        return capture.hasPhoto && capture.containsEnemyAttackPaste ? 1 : 0;
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
    const float targetRatio = static_cast<float>(currentHp) / static_cast<float>(maxHp);
    const float displayRatio = m_ui.hpUiInitialized ? m_ui.hpDisplayRatio : targetRatio;
    const float lagRatio = m_ui.hpUiInitialized ? m_ui.hpDamageLagRatio : targetRatio;
    const float flash = m_ui.hpDamageFlash;
    const int hpTexture = m_assets.GetTexture(kHpTextureKey);
    if (hpTexture < 0)
    {
        return;
    }

    const int activeSlotCount = (std::min)(maxHp, static_cast<int>(game_scene_hp_ui_layout::kHpSlotRects.size()));
    const float displayHp = std::clamp(displayRatio * static_cast<float>(maxHp), 0.0f, static_cast<float>(maxHp));
    const float lagHp = std::clamp(lagRatio * static_cast<float>(maxHp), 0.0f, static_cast<float>(maxHp));
    constexpr float kHeartSize = 72.0f;
    constexpr float kHeartShadowOffsetX = 4.0f;
    constexpr float kHeartShadowOffsetY = 5.0f;

    Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
    for (int slotIndex = 0; slotIndex < activeSlotCount; ++slotIndex)
    {
        const auto& slot = game_scene_hp_ui_layout::GetHpSlotRect(slotIndex);
        const float slotFill = std::clamp(displayHp - static_cast<float>(slotIndex), 0.0f, 1.0f);
        const float slotLag = std::clamp(lagHp - static_cast<float>(slotIndex), 0.0f, 1.0f);
        const bool filled = currentHp > slotIndex;
        const float heartX = slot.x + (slot.width - kHeartSize) * 0.5f;
        const float heartY = slot.y + (slot.height - kHeartSize) * 0.5f + 2.0f;

        Shader_SetTint(0.0f, 0.0f, 0.0f, filled ? 0.24f : 0.18f);
        SpriteDraw(
            hpTexture,
            heartX + kHeartShadowOffsetX,
            heartY + kHeartShadowOffsetY,
            kHeartSize,
            kHeartSize,
            0.0f,
            0.0f,
            1.0f,
            1.0f);

        Shader_SetTint(
            filled ? (1.0f - 0.03f * (1.0f - slotFill)) : 0.40f,
            filled ? (0.42f + 0.14f * slotFill) : 0.14f,
            filled ? (0.45f + 0.12f * slotFill) : 0.18f,
            filled ? (0.96f + 0.04f * slotFill) : 0.32f);
        SpriteDraw(
            hpTexture,
            heartX,
            heartY,
            kHeartSize,
            kHeartSize,
            0.0f,
            0.0f,
            1.0f,
            1.0f);

        if (filled)
        {
            Shader_SetBlendMode(ShaderBlendMode2D::Additive);
            Shader_SetTint(1.0f, 0.82f, 0.88f, 0.10f + 0.10f * slotFill);
            SpriteDraw(
                hpTexture,
                heartX - 1.0f,
                heartY - 1.0f,
                kHeartSize + 2.0f,
                kHeartSize + 2.0f,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
            Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
        }

        if (slotLag > slotFill)
        {
            Shader_SetBlendMode(ShaderBlendMode2D::Additive);
            Shader_SetTint(1.0f, 0.42f, 0.48f, 0.12f + 0.30f * (slotLag - slotFill));
            SpriteDraw(
                hpTexture,
                heartX - 2.0f,
                heartY - 2.0f,
                kHeartSize + 4.0f,
                kHeartSize + 4.0f,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
            Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
        }

        if (slotIndex == 0)
        {
            DrawString(
                static_cast<int>(std::round(slot.x - 214.0f)),
                static_cast<int>(std::round(slot.y + 8.0f)),
                "LIFE",
                GetColor(196, 214, 236));
        }
    }

    DrawFormatString(
        static_cast<int>(std::round(game_scene_hp_ui_layout::GetHpSlotRect(0).x - 214.0f)),
        static_cast<int>(std::round(game_scene_hp_ui_layout::GetHpSlotRect(0).y + 34.0f)),
        GetColor(255, 255, 255),
        "HP %d / %d",
        currentHp,
        maxHp);

    if (flash > 0.0f)
    {
        Shader_SetBlendMode(ShaderBlendMode2D::Additive);
        Shader_SetTint(1.0f, 0.82f, 0.84f, 0.10f + 0.18f * flash);
        for (int slotIndex = 0; slotIndex < activeSlotCount; ++slotIndex)
        {
            const auto& slot = game_scene_hp_ui_layout::GetHpSlotRect(slotIndex);
            if (currentHp > slotIndex)
            {
                SpriteDraw(
                    hpTexture,
                    slot.x,
                    slot.y,
                    slot.width,
                    slot.height,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f);
            }
        }
        Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
    }

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void GameScene::DrawPartsHud() const
{
    const GameSessionState& session = GameSession_Get();

    constexpr float kPanelWidth = 176.0f;
    constexpr float kPanelHeight = 58.0f;
    constexpr float kMarginRight = 30.0f;
    constexpr float kMarginBottom = 28.0f;
    const float panelX = static_cast<float>(SCREEN_WIDTH) - kPanelWidth - kMarginRight;
    const float panelY = static_cast<float>(SCREEN_HEIGHT) - kPanelHeight - kMarginBottom;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 204);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + kPanelWidth)),
        static_cast<int>(std::round(panelY + kPanelHeight)),
        GetColor(14, 20, 28),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + kPanelWidth)),
        static_cast<int>(std::round(panelY + kPanelHeight)),
        GetColor(232, 214, 126),
        FALSE);

    const float iconX = panelX + 18.0f;
    const float iconY = panelY + 17.0f;
    DrawBox(
        static_cast<int>(std::round(iconX)),
        static_cast<int>(std::round(iconY)),
        static_cast<int>(std::round(iconX + 22.0f)),
        static_cast<int>(std::round(iconY + 22.0f)),
        GetColor(255, 214, 62),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(iconX + 5.0f)),
        static_cast<int>(std::round(iconY + 5.0f)),
        static_cast<int>(std::round(iconX + 13.0f)),
        static_cast<int>(std::round(iconY + 13.0f)),
        GetColor(255, 242, 148),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(iconX)),
        static_cast<int>(std::round(iconY)),
        static_cast<int>(std::round(iconX + 22.0f)),
        static_cast<int>(std::round(iconY + 22.0f)),
        GetColor(136, 92, 20),
        FALSE);

    DrawString(
        static_cast<int>(std::round(panelX + 52.0f)),
        static_cast<int>(std::round(panelY + 10.0f)),
        "部品",
        GetColor(220, 230, 236));
    DrawFormatString(
        static_cast<int>(std::round(panelX + 52.0f)),
        static_cast<int>(std::round(panelY + 30.0f)),
        GetColor(255, 246, 184),
        "x %d",
        session.parts);
}

void GameScene::DrawMidBoss2HpBar() const
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
        const auto* boss = entity->GetComponent<MidBoss2Component>();
        if (!boss || boss->state == MidBoss2State::Dead)
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
    const auto* boss = bossEntity->GetComponent<MidBoss2Component>();
    const bool beamPressureState = boss &&
        (boss->state == MidBoss2State::BeamCharge || boss->state == MidBoss2State::BeamFire);
    const bool spearPressureState = boss &&
        (boss->state == MidBoss2State::SpearJump || boss->state == MidBoss2State::SpearThrow);
    const char* phaseLabel = boss ? game_scene_combat_system::ToMidBoss2StateLabel(boss->state) : "Unknown";
    if (boss)
    {
        switch (boss->state)
        {
        case MidBoss2State::Idle:
            phaseLabel = "IDLE";
            break;
        case MidBoss2State::SpearJump:
            phaseLabel = "TELEPORT";
            break;
        case MidBoss2State::SpearThrow:
            phaseLabel = "ATTACK";
            break;
        case MidBoss2State::SpearLanding:
            phaseLabel = "LANDING";
            break;
        case MidBoss2State::SpearCooldown:
            phaseLabel = "RESET";
            break;
        case MidBoss2State::BeamCharge:
            phaseLabel = "CHARGE";
            break;
        case MidBoss2State::BeamFire:
            phaseLabel = "BEAM FIRE";
            break;
        case MidBoss2State::BeamCooldown:
            phaseLabel = "REPOSITION";
            break;
        case MidBoss2State::Damaged:
            phaseLabel = "STUN";
            break;
        default:
            break;
        }
    }
    const float phasePulse = beamPressureState
        ? 0.72f + 0.28f * std::sin(static_cast<float>(GetNowCount()) * 0.020f)
        : (spearPressureState ? 0.82f + 0.18f * std::sin(static_cast<float>(GetNowCount()) * 0.014f) : 1.0f);
    const unsigned int phaseColor = beamPressureState
        ? GetColor(255, 172, 84)
        : (spearPressureState ? GetColor(122, 224, 255) : GetColor(208, 224, 240));

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 204);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(14, 18, 28),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(190, 224, 244),
        FALSE);

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(32, 40, 54),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * ratio)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(74, 170, 248),
        TRUE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 84);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * ratio)),
        static_cast<int>(std::round(barY + kBarHeight * 0.45f)),
        GetColor(255, 255, 255),
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
            GetColor(54, 68, 82));
    }

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(214, 238, 250),
        FALSE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(beamPressureState ? 180.0f * phasePulse : 110.0f)));
    DrawLine(
        static_cast<int>(std::round(panelX + 1.0f)),
        static_cast<int>(std::round(panelY + 28.0f)),
        static_cast<int>(std::round(panelX + panelWidth - 1.0f)),
        static_cast<int>(std::round(panelY + 28.0f)),
        phaseColor);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawString(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY - 18.0f)),
        "BOSS2",
        GetColor(220, 236, 248));
    DrawFormatString(
        static_cast<int>(std::round(barX + 176.0f)),
        static_cast<int>(std::round(barY - 18.0f)),
        phaseColor,
        "STATE %s",
        phaseLabel);
    if (beamPressureState)
    {
        SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(120.0f + 100.0f * phasePulse)));
        DrawString(
            static_cast<int>(std::round(barX + kBarWidth - 86.0f)),
            static_cast<int>(std::round(barY - 18.0f)),
            "ALERT",
            GetColor(255, 124, 76));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
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

    const int attackCount = GetAttackCaptureCount(m_photo.attackCapture);
    if (attackCount > 0)
    {
        DrawFormatString(
            static_cast<int>(std::round(kPanelX + kPanelSize - 30.0f)),
            static_cast<int>(std::round(kPanelY + kPanelSize - 24.0f)),
            GetColor(30, 36, 44),
            "x %d",
            attackCount);
    }
}
