#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_combat_common.h"
#include "game_scene_combat_enemy_system.h"
#include "game_scene_combat_bullet_system.h"
#include "audio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <utility>

using namespace game_scene_detail;

namespace
{
    inline constexpr int kEnemy1SheetColumns = 5;
    inline constexpr int kEnemy1SheetRows = 6;
    inline constexpr int kEnemy1FrameCount = 30;
    inline constexpr float kEnemy1IdleFps = 12.0f;
    inline constexpr float kEnemy1MoveFps = 14.0f;
    inline constexpr int kEnemy1AttackSheetColumns = 8;
    inline constexpr int kEnemy1AttackSheetRows = 7;
    inline constexpr int kEnemy1AttackStartFrame = 24;
    inline constexpr int kEnemy1AttackFrameCount = 16;
    inline constexpr float kEnemy1AttackFps = 18.0f;
    inline constexpr int kEnemy2IdleSheetColumns = 10;
    inline constexpr int kEnemy2IdleSheetRows = 11;
    inline constexpr int kEnemy2IdleFrameCount = 110;
    inline constexpr float kEnemy2IdleFps = 48.0f;
    inline constexpr int kEnemy2AttackSheetColumns = 10;
    inline constexpr int kEnemy2AttackSheetRows = 11;
    inline constexpr int kEnemy2AttackFrameCount = 80;
    inline constexpr float kEnemy2AttackFps = 30.0f;
    inline constexpr int kBoss1MoveSheetColumns = 5;
    inline constexpr int kBoss1MoveSheetRows = 6;
    inline constexpr int kBoss1MoveFrameCount = 30;
    inline constexpr int kBoss1Attack01SheetColumns = 10;
    inline constexpr int kBoss1Attack01SheetRows = 18;
    inline constexpr int kBoss1Attack01FrameCount = 180;
    inline constexpr int kBoss1Attack02SheetColumns = 11;
    inline constexpr int kBoss1Attack02SheetRows = 15;
    inline constexpr int kBoss1Attack02FrameCount = 165;
    inline constexpr int kBoss1KnockbackSheetColumns = 5;
    inline constexpr int kBoss1KnockbackSheetRows = 6;
    inline constexpr int kBoss1KnockbackFrameCount = 30;
    inline constexpr int kBoss1DeathSheetColumns = 10;
    inline constexpr int kBoss1DeathSheetRows = 12;
    inline constexpr int kBoss1DeathFrameCount = 120;
    inline constexpr int kBoss1AppearSheetColumns = 10;
    inline constexpr int kBoss1AppearSheetRows = 15;
    inline constexpr int kBoss1AppearFrameCount = 150;
    inline constexpr float kBoss1IdleFps = 6.0f;
    inline constexpr float kBoss1MoveFps = 12.0f;
    inline constexpr float kBoss1AttackFps = 30.0f;
    inline constexpr float kBoss1KnockbackFps = 30.0f;
    inline constexpr float kBoss1DeathFps = 30.0f;
    inline constexpr float kBoss1AppearFps = 30.0f;

    constexpr float kEnemyDefeatHitStopSeconds = 0.08f;
    constexpr float kEnemyDefeatShakeSeconds = 0.18f;
    constexpr float kEnemyDefeatShakeAmplitude = 14.0f;
    constexpr float kEnemyKnockbackHitStopSeconds = 0.055f;
    constexpr float kEnemyKnockbackShakeSeconds = 0.16f;
    constexpr float kEnemyKnockbackShakeAmplitude = 15.0f;
    constexpr float kBossKnockbackHitStopSeconds = 0.085f;
    constexpr float kBossKnockbackShakeSeconds = 0.26f;
    constexpr float kBossKnockbackShakeAmplitude = 60.0f;
    constexpr float kNormalShieldBossHitStopSeconds = 0.10f;
    constexpr float kNormalShieldBossHitShakeSeconds = 0.34f;
    constexpr float kNormalShieldBossHitShakeAmplitude = 92.0f;
    constexpr float kBossDefeatStartHitStopSeconds = 0.14f;
    constexpr float kBossDefeatStartShakeSeconds = 0.44f;
    constexpr float kBossDefeatStartShakeAmplitude = 64.0f;
    constexpr float kBossDefeatFinishHitStopSeconds = 0.08f;
    constexpr float kBossDefeatFinishShakeSeconds = 0.34f;
    constexpr float kBossDefeatFinishShakeAmplitude = 42.0f;
    constexpr float kBossStageBgmReturnDelaySeconds = 1.25f;
    constexpr float kBossStageBgmReturnCrossFadeSeconds = 1.6f;

    float ResolveMidBoss3DamageDirection(const Entity& enemy, const Entity* sourceEntity)
    {
        if (sourceEntity)
        {
            if (const auto* projectile = sourceEntity->GetComponent<ProjectileComponent>())
            {
                const float projectileDirection = projectile->GetVelocityX();
                if (std::fabs(projectileDirection) > 0.01f)
                {
                    return projectileDirection > 0.0f ? 1.0f : -1.0f;
                }
            }
            if (const auto* capturedAttack = sourceEntity->GetComponent<CapturedMidBoss3AttackComponent>())
            {
                if (std::fabs(capturedAttack->aimX) > 0.01f)
                {
                    return capturedAttack->aimX > 0.0f ? 1.0f : -1.0f;
                }
            }
        }

        const auto* enemyTransform = enemy.GetComponent<TransformComponent>();
        const auto* sourceTransform = sourceEntity ? sourceEntity->GetComponent<TransformComponent>() : nullptr;
        if (enemyTransform && sourceTransform)
        {
            const float enemyCenterX = enemyTransform->x + enemyTransform->width * enemyTransform->scale * 0.5f;
            const float sourceCenterX = sourceTransform->x + sourceTransform->width * sourceTransform->scale * 0.5f;
            if (std::fabs(enemyCenterX - sourceCenterX) > 1.0f)
            {
                return enemyCenterX > sourceCenterX ? 1.0f : -1.0f;
            }
        }

        if (const auto* boss = enemy.GetComponent<MidBoss3Component>())
        {
            return boss->facingRight ? -1.0f : 1.0f;
        }
        return 1.0f;
    }

    struct RotatedPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    using RotatedRect = std::array<RotatedPoint, 4>;

    RotatedRect BuildRotatedRect(const TransformComponent& transform)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float cx = transform.x + width * 0.5f;
        const float cy = transform.y + height * 0.5f;
        const float c = std::cos(transform.rotation);
        const float s = std::sin(transform.rotation);
        const std::array<RotatedPoint, 4> local = {
            RotatedPoint{ -width * 0.5f, -height * 0.5f },
            RotatedPoint{  width * 0.5f, -height * 0.5f },
            RotatedPoint{  width * 0.5f,  height * 0.5f },
            RotatedPoint{ -width * 0.5f,  height * 0.5f },
        };

        RotatedRect result{};
        for (std::size_t i = 0; i < local.size(); ++i)
        {
            result[i].x = cx + local[i].x * c - local[i].y * s;
            result[i].y = cy + local[i].x * s + local[i].y * c;
        }
        return result;
    }

    bool HasSeparatingAxis(const RotatedRect& a, const RotatedRect& b)
    {
        constexpr float kEpsilon = 0.0001f;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const RotatedPoint& p0 = a[i];
            const RotatedPoint& p1 = a[(i + 1) % a.size()];
            const float edgeX = p1.x - p0.x;
            const float edgeY = p1.y - p0.y;
            const float axisX = -edgeY;
            const float axisY = edgeX;

            float minA = a[0].x * axisX + a[0].y * axisY;
            float maxA = minA;
            for (const auto& p : a)
            {
                const float projected = p.x * axisX + p.y * axisY;
                minA = (std::min)(minA, projected);
                maxA = (std::max)(maxA, projected);
            }

            float minB = b[0].x * axisX + b[0].y * axisY;
            float maxB = minB;
            for (const auto& p : b)
            {
                const float projected = p.x * axisX + p.y * axisY;
                minB = (std::min)(minB, projected);
                maxB = (std::max)(maxB, projected);
            }

            if (maxA < minB + kEpsilon || maxB < minA + kEpsilon)
            {
                return true;
            }
        }
        return false;
    }

    bool IntersectsRotatedRect(const TransformComponent& a, const TransformComponent& b)
    {
        const RotatedRect rectA = BuildRotatedRect(a);
        const RotatedRect rectB = BuildRotatedRect(b);
        return !HasSeparatingAxis(rectA, rectB) && !HasSeparatingAxis(rectB, rectA);
    }

    float GetRotatedRectBottomOffset(const TransformComponent& transform)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float c = std::fabs(std::cos(transform.rotation));
        const float s = std::fabs(std::sin(transform.rotation));
        return height * 0.5f + (width * s + height * c) * 0.5f;
    }

    void TriggerEnemyDefeatFeedback(GameSceneFlowState& flow)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kEnemyDefeatHitStopSeconds);
        flow.screenShakeRemaining = kEnemyDefeatShakeSeconds;
        flow.screenShakeDuration = kEnemyDefeatShakeSeconds;
        flow.screenShakeAmplitude = kEnemyDefeatShakeAmplitude;
    }

    void TriggerEnemyKnockbackFeedback(GameSceneFlowState& flow, bool screenShakeEnabled)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kEnemyKnockbackHitStopSeconds);
        if (!screenShakeEnabled)
        {
            return;
        }

        flow.screenShakeRemaining = (std::max)(flow.screenShakeRemaining, kEnemyKnockbackShakeSeconds);
        flow.screenShakeDuration = (std::max)(flow.screenShakeDuration, kEnemyKnockbackShakeSeconds);
        flow.screenShakeAmplitude = (std::max)(flow.screenShakeAmplitude, kEnemyKnockbackShakeAmplitude);
    }

    void TriggerBossKnockbackFeedback(GameSceneFlowState& flow, bool screenShakeEnabled)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kBossKnockbackHitStopSeconds);
        if (!screenShakeEnabled)
        {
            return;
        }

        flow.screenShakeRemaining = (std::max)(flow.screenShakeRemaining, kBossKnockbackShakeSeconds);
        flow.screenShakeDuration = (std::max)(flow.screenShakeDuration, kBossKnockbackShakeSeconds);
        flow.screenShakeAmplitude = (std::max)(flow.screenShakeAmplitude, kBossKnockbackShakeAmplitude);
    }

    void TriggerNormalShieldBossHitFeedback(GameSceneFlowState& flow, bool screenShakeEnabled)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kNormalShieldBossHitStopSeconds);
        if (!screenShakeEnabled)
        {
            return;
        }

        // 通常盾はボスを動かさないため、画面側を強めに揺らして命中感を出す。
        flow.screenShakeRemaining = kNormalShieldBossHitShakeSeconds;
        flow.screenShakeDuration = kNormalShieldBossHitShakeSeconds;
        flow.screenShakeAmplitude = kNormalShieldBossHitShakeAmplitude;
    }

    void TriggerBossDefeatStartFeedback(GameSceneFlowState& flow)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kBossDefeatStartHitStopSeconds);
        flow.screenShakeRemaining = kBossDefeatStartShakeSeconds;
        flow.screenShakeDuration = kBossDefeatStartShakeSeconds;
        flow.screenShakeAmplitude = kBossDefeatStartShakeAmplitude;
    }

    void TriggerBossDefeatFinishFeedback(GameSceneFlowState& flow)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kBossDefeatFinishHitStopSeconds);
        flow.screenShakeRemaining = kBossDefeatFinishShakeSeconds;
        flow.screenShakeDuration = kBossDefeatFinishShakeSeconds;
        flow.screenShakeAmplitude = kBossDefeatFinishShakeAmplitude;
    }

    using BossTextureResolver = std::function<int(const std::string&)>;

    void DefineLazyRowsClip(
        SpriteSheetAnimationComponent& animation,
        const char* name,
        std::vector<std::string> textureKeys,
        const BossTextureResolver& textureResolver,
        int columns,
        std::vector<int> rowsPerTexture,
        int frameCount,
        float fps,
        bool loop)
    {
        animation.DefineLazyPagedRowsClip(
            name,
            std::move(textureKeys),
            textureResolver,
            columns,
            std::move(rowsPerTexture),
            0,
            frameCount,
            fps,
            loop);
    }

    void DefineLazySingleSheetClip(
        SpriteSheetAnimationComponent& animation,
        const char* name,
        const char* textureKey,
        const BossTextureResolver& textureResolver,
        int columns,
        int rows,
        int frameCount,
        float fps,
        bool loop)
    {
        DefineLazyRowsClip(
            animation,
            name,
            std::vector<std::string>{ textureKey },
            textureResolver,
            columns,
            std::vector<int>{ rows },
            frameCount,
            fps,
            loop);
    }

    void PlayShieldBossSoundCue(const char* cueName)
    {
        if (!cueName)
        {
            return;
        }

        // 中ボス1専用SEは発生フレームが重要なので、イベント待ちにせず即時再生する。
        Audio_PlayCue(cueName);
        Logger::Info(std::string("ShieldBoss SE requested: ") + cueName);
    }

    void StopShieldBossSoundCue(const char* cueName)
    {
        if (!cueName)
        {
            return;
        }

        Audio_StopCue(cueName);
        Logger::Info(std::string("ShieldBoss SE stopped: ") + cueName);
    }

}

void GameScene::ConfigureWalkerSpriteAnimation(Entity& enemy)
{
    auto* sprite = enemy.GetComponent<SpriteRenderComponent>();
    if (!sprite)
    {
        return;
    }

    const auto* transform = enemy.GetComponent<TransformComponent>();
    constexpr float kWalkerVisualScale = 1.55f;
    constexpr float kWalkerVisualOffsetY = -22.0f;
    sprite->SetRenderScale(kWalkerVisualScale, kWalkerVisualScale);
    sprite->SetRenderOffset(
        transform ? transform->width * (1.0f - kWalkerVisualScale) * 0.5f : 0.0f,
        kWalkerVisualOffsetY);

    auto* animation = enemy.GetComponent<SpriteSheetAnimationComponent>();
    if (!animation)
    {
        animation = &enemy.AddComponent<SpriteSheetAnimationComponent>();
    }

    const int idleTexture = m_assets.GetTexture("enemy1_idle");
    const int moveTexture = m_assets.GetTexture("enemy1_move");
    const int attackTexture = m_assets.GetTexture("enemy1_attack");
    const int fallbackTexture = sprite->GetTextureId();
    const int resolvedIdleTexture = idleTexture >= 0 ? idleTexture : fallbackTexture;
    const int resolvedMoveTexture = moveTexture >= 0 ? moveTexture : resolvedIdleTexture;
    const int resolvedAttackTexture = attackTexture >= 0 ? attackTexture : resolvedIdleTexture;

    // Walker uses separate sheets; melee hit is emitted on the attack clip's 31st frame.
    animation->DefineClip("idle", resolvedIdleTexture, kEnemy1SheetColumns, kEnemy1SheetRows, 0, kEnemy1FrameCount, kEnemy1IdleFps, true);
    animation->DefineClip("move", resolvedMoveTexture, kEnemy1SheetColumns, kEnemy1SheetRows, 0, kEnemy1FrameCount, kEnemy1MoveFps, true);
    animation->DefineClip("attack", resolvedAttackTexture, kEnemy1AttackSheetColumns, kEnemy1AttackSheetRows, kEnemy1AttackStartFrame, kEnemy1AttackFrameCount, kEnemy1AttackFps, false);
    animation->Play("idle", true);
}

void GameScene::ConfigureRangedSpriteAnimation(Entity& enemy)
{
    auto* sprite = enemy.GetComponent<SpriteRenderComponent>();
    if (!sprite)
    {
        return;
    }

    auto* animation = enemy.GetComponent<SpriteSheetAnimationComponent>();
    if (!animation)
    {
        animation = &enemy.AddComponent<SpriteSheetAnimationComponent>();
    }

    const auto* transform = enemy.GetComponent<TransformComponent>();
    constexpr float kRangedVisualScale = 1.5f;
    constexpr float kRangedVisualOffsetY = -24.0f;
    sprite->SetRenderScale(kRangedVisualScale, kRangedVisualScale);
    sprite->SetRenderOffset(
        transform ? transform->width * (1.0f - kRangedVisualScale) * 0.5f : 0.0f,
        kRangedVisualOffsetY);

    const int idleTexture = m_assets.GetTexture("enemy2_idle");
    const int attackTexture = m_assets.GetTexture("enemy2_attack");
    const int resolvedIdleTexture = idleTexture >= 0 ? idleTexture : sprite->GetTextureId();
    const int resolvedAttackTexture = attackTexture >= 0 ? attackTexture : resolvedIdleTexture;
    if (auto* enemyComponent = enemy.GetComponent<EnemyComponent>())
    {
        enemyComponent->attackTimer = enemyComponent->attackCooldown;
    }

    // Enemy2 is left-facing; the shot is emitted on the attack clip's 39th frame.
    sprite->SetFlipX(false);
    animation->DefineClip("idle", resolvedIdleTexture, kEnemy2IdleSheetColumns, kEnemy2IdleSheetRows, 0, kEnemy2IdleFrameCount, kEnemy2IdleFps, true);
    animation->DefineClip("attack", resolvedAttackTexture, kEnemy2AttackSheetColumns, kEnemy2AttackSheetRows, 0, kEnemy2AttackFrameCount, kEnemy2AttackFps, false);
    animation->Play("idle", true);
}

void GameScene::ConfigureShieldBossSpriteAnimation(Entity& enemy)
{
    auto* sprite = enemy.GetComponent<SpriteRenderComponent>();
    const auto* transform = enemy.GetComponent<TransformComponent>();
    if (!sprite || !transform)
    {
        return;
    }

    auto* animation = enemy.GetComponent<SpriteSheetAnimationComponent>();
    if (!animation)
    {
        animation = &enemy.AddComponent<SpriteSheetAnimationComponent>();
    }

    const int idleTexture = m_assets.GetTexture("boss1_body_move");
    const int fallbackTexture = sprite->GetTextureId();
    const int resolvedIdleTexture = idleTexture >= 0 ? idleTexture : fallbackTexture;
    const BossTextureResolver resolveTexture = [this](const std::string& key) { return m_assets.GetTexture(key); };

    // DDS版Boss01の攻撃名を、そのまま戦闘フロー用のクリップ名に寄せる。
    animation->DefineClip("idle", resolvedIdleTexture, kBoss1MoveSheetColumns, kBoss1MoveSheetRows, 0, kBoss1MoveFrameCount, kBoss1IdleFps, true);
    animation->DefineClip("move", resolvedIdleTexture, kBoss1MoveSheetColumns, kBoss1MoveSheetRows, 0, kBoss1MoveFrameCount, kBoss1MoveFps, true);
    DefineLazySingleSheetClip(
        *animation,
        "attack01",
        "boss1_body_attack01",
        resolveTexture,
        kBoss1Attack01SheetColumns,
        kBoss1Attack01SheetRows,
        kBoss1Attack01FrameCount,
        kBoss1AttackFps,
        false);
    DefineLazySingleSheetClip(
        *animation,
        "attack02",
        "boss1_body_attack02",
        resolveTexture,
        kBoss1Attack02SheetColumns,
        kBoss1Attack02SheetRows,
        kBoss1Attack02FrameCount,
        kBoss1AttackFps,
        false);
    DefineLazySingleSheetClip(
        *animation,
        "knockback",
        "boss1_body_knockback",
        resolveTexture,
        kBoss1KnockbackSheetColumns,
        kBoss1KnockbackSheetRows,
        kBoss1KnockbackFrameCount,
        kBoss1KnockbackFps,
        true);
    DefineLazySingleSheetClip(
        *animation,
        "death",
        "boss1_body_death",
        resolveTexture,
        kBoss1DeathSheetColumns,
        kBoss1DeathSheetRows,
        kBoss1DeathFrameCount,
        kBoss1DeathFps,
        false);
    DefineLazySingleSheetClip(
        *animation,
        "appear",
        "boss1_body_start",
        resolveTexture,
        kBoss1AppearSheetColumns,
        kBoss1AppearSheetRows,
        kBoss1AppearFrameCount,
        kBoss1AppearFps,
        false);
    DefineLazySingleSheetClip(
        *animation,
        "roar",
        "boss1_body_start",
        resolveTexture,
        kBoss1AppearSheetColumns,
        kBoss1AppearSheetRows,
        kBoss1AppearFrameCount,
        kBoss1AppearFps,
        false);

    if (auto* boss = enemy.GetComponent<ShieldBossComponent>();
        boss && !boss->combatStarted && !boss->appearAnimationFinished)
    {
        if (auto* tint = enemy.GetComponent<TintComponent>())
        {
            tint->a = 0.0f;
        }
        animation->Play("idle", true);
    }
    else
    {
        animation->Play("idle", true);
    }
}

void GameScene::ConfigureBossShieldSpriteAnimation(Entity& shield)
{
    auto* sprite = shield.GetComponent<SpriteRenderComponent>();
    const auto* transform = shield.GetComponent<TransformComponent>();
    if (!sprite || !transform)
    {
        return;
    }

    auto* animation = shield.GetComponent<SpriteSheetAnimationComponent>();
    if (!animation)
    {
        animation = &shield.AddComponent<SpriteSheetAnimationComponent>();
    }

    const int idleTexture = m_assets.GetTexture("boss1_shield_move");
    const int fallbackTexture = sprite->GetTextureId();
    const int resolvedIdleTexture = idleTexture >= 0 ? idleTexture : fallbackTexture;
    const BossTextureResolver resolveTexture = [this](const std::string& key) { return m_assets.GetTexture(key); };

    // 盾用DDSは本体と同じフレーム数で、攻撃中だけ本体と同期させる。
    animation->DefineClip("idle", resolvedIdleTexture, kBoss1MoveSheetColumns, kBoss1MoveSheetRows, 0, kBoss1MoveFrameCount, kBoss1IdleFps, true);
    animation->DefineClip("move", resolvedIdleTexture, kBoss1MoveSheetColumns, kBoss1MoveSheetRows, 0, kBoss1MoveFrameCount, kBoss1MoveFps, true);
    DefineLazySingleSheetClip(
        *animation,
        "attack01",
        "boss1_shield_attack01",
        resolveTexture,
        kBoss1Attack01SheetColumns,
        kBoss1Attack01SheetRows,
        kBoss1Attack01FrameCount,
        kBoss1AttackFps,
        false);
    DefineLazySingleSheetClip(
        *animation,
        "attack02",
        "boss1_shield_attack02",
        resolveTexture,
        kBoss1Attack02SheetColumns,
        kBoss1Attack02SheetRows,
        kBoss1Attack02FrameCount,
        kBoss1AttackFps,
        false);
    DefineLazySingleSheetClip(
        *animation,
        "knockback",
        "boss1_shield_knockback",
        resolveTexture,
        kBoss1KnockbackSheetColumns,
        kBoss1KnockbackSheetRows,
        kBoss1KnockbackFrameCount,
        kBoss1KnockbackFps,
        true);
    animation->Play("idle", true);
}

void GameScene::UpdateEnemies()
{
    if (m_flow.stageBgmCrossFadePending)
    {
        m_flow.stageBgmCrossFadeDelayRemaining = std::max(
            0.0f,
            m_flow.stageBgmCrossFadeDelayRemaining - m_flow.lastDeltaTime);
        if (m_flow.stageBgmCrossFadeDelayRemaining <= 0.0f)
        {
            m_flow.stageBgmCrossFadePending = false;
            CrossFadeStageBgmForCurrentMap(kBossStageBgmReturnCrossFadeSeconds);
        }
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    const TransformComponent* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    const auto& enemyEntities = m_world.EntitiesByTag(EntityTag::Enemy);
    const auto& photoBoxEntities = m_world.EntitiesByTag(EntityTag::PhotoBox);
    const auto& batteryEntities = m_world.EntitiesByTag(EntityTag::Battery);
    const auto& barrelEntities = m_world.EntitiesByTag(EntityTag::Barrel);
    const auto& dropItemEntities = m_world.EntitiesByTag(EntityTag::DropItem);
    const auto& batterySwitchEntities = m_world.EntitiesByTag(EntityTag::BatterySwitch);
    const auto& elevatorEntities = m_world.EntitiesByTag(EntityTag::Elevator);
    const auto& laserSwitchEntities = m_world.EntitiesByTag(EntityTag::LaserSwitch);
    const auto& shutterEntities = m_world.EntitiesByTag(EntityTag::Shutter);
    const auto& protectiveWallEntities = m_world.EntitiesByTag(EntityTag::ProtectiveWall);
    const auto& laserTurretEntities = m_world.EntitiesByTag(EntityTag::LaserTurret);
    const auto& laserBeamEntities = m_world.EntitiesByTag(EntityTag::LaserBeam);
    const auto& stageLightEntities = m_world.EntitiesByTag(EntityTag::StageLight);
    const auto& markerLightEntities = m_world.EntitiesByTag(EntityTag::MarkerLight);
    const auto& sepiaRubbleEntities = m_world.EntitiesByTag(EntityTag::SepiaRubble);
    const auto& sepiaElevatorEntities = m_world.EntitiesByTag(EntityTag::SepiaElevator);
    const auto& goalEntities = m_world.EntitiesByTag(EntityTag::Goal);
    const auto& photoSourceEntities = m_world.EntitiesByTag(EntityTag::PhotoSource);
    const auto& hazardEntities = m_world.EntitiesByTag(EntityTag::Hazard);
    const auto& bulletEntities = m_world.EntitiesByTag(EntityTag::Bullet);
    const auto& shieldEntities = m_world.EntitiesByTag(EntityTag::Shield);
    const auto& bossShieldEntities = m_world.EntitiesByTag(EntityTag::BossShield);
    const auto& boss1ShieldEntities = m_world.EntitiesByTag(EntityTag::Boss1Shield);
    const auto& midBoss1ShieldEntities = m_world.EntitiesByTag(EntityTag::MidBoss1Shield);
    const auto& capturedShieldEntities = m_world.EntitiesByTag(EntityTag::CapturedShield);
    const auto& walkerMeleeAttackEntities = m_world.EntitiesByTag(EntityTag::WalkerMeleeAttack);
    const auto& bossShockwaveEntities = m_world.EntitiesByTag(EntityTag::BossShockwave);
    std::vector<Entity*> interactionEntities;
    interactionEntities.reserve(
        photoBoxEntities.size() +
        enemyEntities.size() +
        batteryEntities.size() +
        barrelEntities.size() +
        dropItemEntities.size() +
        batterySwitchEntities.size() +
        elevatorEntities.size() +
        laserSwitchEntities.size() +
        shutterEntities.size() +
        protectiveWallEntities.size() +
        laserTurretEntities.size() +
        laserBeamEntities.size() +
        stageLightEntities.size() +
        markerLightEntities.size() +
        sepiaRubbleEntities.size() +
        sepiaElevatorEntities.size() +
        goalEntities.size() +
        photoSourceEntities.size() +
        hazardEntities.size() +
        bulletEntities.size() +
        shieldEntities.size() +
        bossShieldEntities.size() +
        boss1ShieldEntities.size() +
        midBoss1ShieldEntities.size() +
        capturedShieldEntities.size() +
        walkerMeleeAttackEntities.size() +
        bossShockwaveEntities.size());
    auto appendInteractionEntities = [&](EntityTag tag)
    {
        for (Entity* candidate : m_world.EntitiesByTag(tag))
        {
            if (candidate)
            {
                interactionEntities.push_back(candidate);
            }
        }
    };
    appendInteractionEntities(EntityTag::PhotoBox);
    appendInteractionEntities(EntityTag::Enemy);
    appendInteractionEntities(EntityTag::Battery);
    appendInteractionEntities(EntityTag::Barrel);
    appendInteractionEntities(EntityTag::DropItem);
    appendInteractionEntities(EntityTag::BatterySwitch);
    appendInteractionEntities(EntityTag::Elevator);
    appendInteractionEntities(EntityTag::LaserSwitch);
    appendInteractionEntities(EntityTag::Shutter);
    appendInteractionEntities(EntityTag::ProtectiveWall);
    appendInteractionEntities(EntityTag::LaserTurret);
    appendInteractionEntities(EntityTag::LaserBeam);
    appendInteractionEntities(EntityTag::StageLight);
    appendInteractionEntities(EntityTag::MarkerLight);
    appendInteractionEntities(EntityTag::SepiaRubble);
    appendInteractionEntities(EntityTag::SepiaElevator);
    appendInteractionEntities(EntityTag::Goal);
    appendInteractionEntities(EntityTag::PhotoSource);
    appendInteractionEntities(EntityTag::Hazard);
    appendInteractionEntities(EntityTag::Bullet);
    appendInteractionEntities(EntityTag::Shield);
    appendInteractionEntities(EntityTag::BossShield);
    appendInteractionEntities(EntityTag::Boss1Shield);
    appendInteractionEntities(EntityTag::MidBoss1Shield);
    appendInteractionEntities(EntityTag::CapturedShield);
    appendInteractionEntities(EntityTag::WalkerMeleeAttack);
    appendInteractionEntities(EntityTag::BossShockwave);

    for (Entity* entity : enemyEntities)
    {
        if (!entity)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || enemy->GetArchetype() != EnemyArchetype::ShieldBoss)
        {
            continue;
        }

        // どの生成経路でも中ボス1が仮テクスチャのままにならないよう補完する。
        if (!entity->GetComponent<SpriteSheetAnimationComponent>())
        {
            ConfigureShieldBossSpriteAnimation(*entity);
        }
        if (auto* boss = entity->GetComponent<ShieldBossComponent>())
        {
            if (boss->shieldEntity && !boss->shieldEntity->GetComponent<SpriteSheetAnimationComponent>())
            {
                ConfigureBossShieldSpriteAnimation(*boss->shieldEntity);
            }
            if (boss->appearAnimationActive)
            {
                game_scene_combat_system::UpdateShieldBossSpriteAnimation(*entity, *boss);
                continue;
            }
            if (boss->deathAnimationActive)
            {
                auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>();
                if (animation)
                {
                    animation->Play("death");
                    if (animation->IsCurrentClipFinished())
                    {
                        boss->deathAnimationActive = false;
                        boss->deathAnimationFinished = true;
                        enemy->MarkDefeated();
                        enemy->respawnEnabled = false;
                        m_flow.shieldBossDefeatedThisScene = true;
                        PlayShieldBossSoundCue("boss_forest_destroy");
                        m_flow.stageBgmCrossFadePending = true;
                        m_flow.stageBgmCrossFadeDelayRemaining = kBossStageBgmReturnDelaySeconds;

                        if (const auto* transform = entity->GetComponent<TransformComponent>())
                        {
                            const float centerX = transform->x + transform->width * transform->scale * 0.5f;
                            const float centerY = transform->y + transform->height * transform->scale * 0.5f;
                            const float groundY = transform->y + transform->height * transform->scale;
                            const float width = transform->width * transform->scale;
                            SpawnDropItems(
                                centerX,
                                centerY,
                                GetEnemyDropCount(enemy->GetArchetype()));
                            SpawnBossDefeatStartEffect(centerX, groundY, width);
                        }
                        TriggerBossDefeatFinishFeedback(m_flow);

                        //リザルト画面へ遷移
                        GameSession_SetEndReason(GameEndReason::BossDefeated);
                        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "result", 0.0f, 0.0f });
                    }
                }
                if (boss->shieldEntity)
                {
                    if (auto* shieldTint = boss->shieldEntity->GetComponent<TintComponent>())
                    {
                        shieldTint->a = 0.0f;
                    }
                }
            }
        }
    }

    std::vector<Entity*> photoBoxesToRemove;

    game_scene_combat_system::UpdateEnemies(
        m_world.Entities(),
        enemyEntities,
        interactionEntities,
        m_tileTexture,
        m_assets.GetTexture("sepia_rubble"),
        GetMapPixelWidth(),
        GetMapPixelHeight(),
        m_flow,
        m_photo,
        player,
        &m_player,
        playerTransform,
        [this](TransformComponent& transform) -> bool
        {
            return SnapEnemyToGround(transform);
        },
        [this](Entity& enemyEntity)
        {
            m_eventBus.Publish({ EventType::PlaySoundRequest, &enemyEntity, nullptr, "enemy_gun", 0.0f, 0.0f });
        },
        [this](Entity& bossEntity)
        {
            static_cast<void>(bossEntity);
            PlayShieldBossSoundCue("boss_forest_roar");
        },
        [this](Entity& bossEntity, const char* cueName)
        {
            static_cast<void>(bossEntity);
            PlayShieldBossSoundCue(cueName);
        },
        [this](Entity& bossEntity, const char* cueName)
        {
            static_cast<void>(bossEntity);
            StopShieldBossSoundCue(cueName);
        },
        [this](
            float fromX,
            float fromY,
            float toX,
            float toY,
            float width,
            float height,
            const MidBoss2Component::Params& params)
        {
            SpawnTeleportTrailEffect(fromX, fromY, toX, toY, width, height, params);
        },
        [this](float centerX, float groundY, float width)
        {
            SpawnSlamImpactEffect(centerX, groundY, width);
        },
        [this](float centerX, float groundY, float direction)
        {
            SpawnRushSmokeEffect(centerX, groundY, direction);
        },
        [this](float centerX, float groundY, float width)
        {
            SpawnLightLandingEffect(centerX, groundY, width);
        },
        [this](float centerX, float groundY, float width)
        {
            SpawnBossRoarEffect(centerX, groundY, width);
        },
        [this](float x, float y, float radius, float intensity, float directionX)
        {
            BeamShockwaveParticle shockwave;
            shockwave.x = x;
            shockwave.y = y;
            shockwave.startRadius = radius * 0.22f;
            shockwave.endRadius = radius * std::max(0.1f, intensity);
            shockwave.thickness = std::max(8.0f, radius * 0.08f);
            shockwave.life = 0.36f;
            shockwave.maxLife = shockwave.life;
            shockwave.directionX = directionX;
            m_effects.beamShockwaves.push_back(shockwave);
        },
        [this](float x, float y, float width, float height)
        {
            SpawnMidBoss3FistImpactEffect(x, y, width, height);
        },
        [this, player](Entity* sourceEntity, int amount, const char* logMessage)
        {
            if (player)
            {
                HandlePlayerDamage(*player, sourceEntity, logMessage, amount);
            }
        },
        [this, &photoBoxesToRemove, &interactionEntities](const TransformComponent& bossTransform, Entity& bossEntity) -> bool
        {
            for (Entity* candidate : interactionEntities)
            {
                if (!candidate) continue;
                const bool isPhotoBox = HasTag(*candidate, EntityTag::PhotoBox);
                const bool isGroundedCapturedShield =
                    HasTag(*candidate, EntityTag::CapturedShield) &&
                    candidate->GetComponent<ShieldComponent>() &&
                    candidate->GetComponent<ShieldComponent>()->photoSpawned &&
                    candidate->GetComponent<ShieldComponent>()->grounded;
                if (!isPhotoBox && !isGroundedCapturedShield) continue;
                const auto* photoTransform = candidate->GetComponent<TransformComponent>();
                if (!photoTransform) continue;
                if (IntersectsRect(bossTransform, *photoTransform))
                {
                    if (isPhotoBox)
                    {
                        photoBoxesToRemove.push_back(candidate);
                    }
                    return true;
                }
            }
            return false;
        },
        [this](int column, int row) -> bool
        {
            return IsSolidTile(column, row);
        });

    m_world.RemoveByPointerList(photoBoxesToRemove);
}

int GameScene::HandleFinderDefeatGhosts(float frameX, float frameY, float frameWidth, float frameHeight)
{
    TransformComponent finderBounds(frameX, frameY, frameWidth, frameHeight);
    int defeatedGhostCount = 0;

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!entity) continue;

        auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled()) continue;
        if (enemy->GetArchetype() != EnemyArchetype::Ghost) continue;

        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform || !IntersectsRect(*transform, finderBounds)) continue;

        HandleEnemyDamage(*entity, nullptr, 999, "Ghost defeated in finder");
        ++defeatedGhostCount;
    }

    return defeatedGhostCount;
}


void GameScene::UpdateBullets()
{
    Entity* player = FindEntityByTag(kTagPlayer);
    const auto& bulletEntities = m_world.EntitiesByTag(EntityTag::Bullet);
    const auto& enemyEntities = m_world.EntitiesByTag(EntityTag::Enemy);
    const auto& protectiveWallEntities = m_world.EntitiesByTag(EntityTag::ProtectiveWall);
    std::vector<TransformComponent> obstacleBounds;
    {
        std::vector<TransformComponent> tempBounds;
        GetGroundPlatformBounds(tempBounds);
        obstacleBounds.insert(obstacleBounds.end(), tempBounds.begin(), tempBounds.end());
    }
    {
        std::vector<TransformComponent> tempBounds;
        GetPhotoBoxBounds(tempBounds);
        obstacleBounds.insert(obstacleBounds.end(), tempBounds.begin(), tempBounds.end());
    }
    {
        std::vector<TransformComponent> tempBounds;
        GetEntityBoundsByTag("Battery", tempBounds);
        obstacleBounds.insert(obstacleBounds.end(), tempBounds.begin(), tempBounds.end());
    }
    {
        std::vector<TransformComponent> tempBounds;
        GetEntityBoundsByTag("Log", tempBounds);
        obstacleBounds.insert(obstacleBounds.end(), tempBounds.begin(), tempBounds.end());
    }
    {
        std::vector<TransformComponent> tempBounds;
        GetEntityBoundsByTag("DamagePlatform", tempBounds);
        obstacleBounds.insert(obstacleBounds.end(), tempBounds.begin(), tempBounds.end());
    }
    {
        std::vector<TransformComponent> tempBounds;
        GetEntityBoundsByTag("DamagePlatformSpike", tempBounds);
        obstacleBounds.insert(obstacleBounds.end(), tempBounds.begin(), tempBounds.end());
    }
    std::vector<Entity*> bulletsToRemove;
    
    game_scene_combat_system::UpdateBullets(
        bulletEntities,
        enemyEntities,
        protectiveWallEntities,
        obstacleBounds,
        GetMapPixelWidth(),
        GetMapPixelHeight(),
        m_flow.lastDeltaTime,
        m_flow,
        m_debug.screenShakeEnabled,
        player,
        [this](const Entity& a, const Entity& b)
        {
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        },
        [this](Entity& enemyEntity, Entity* sourceEntity, int amount, const char* logMessage)
        {
            HandleEnemyDamage(enemyEntity, sourceEntity, amount, logMessage);
        },
        [this](float centerX, float centerY, float width, float height)
        {
            SpawnMidBoss2SpearFadeEffect(centerX, centerY, width, height);
        },
        [this](float x, float y) -> bool
        {
            const float tileSize = m_tileMap.GetTileSize();
            const int column = static_cast<int>(x / tileSize);
            const int row = static_cast<int>(y / tileSize);
            return IsSolidTile(column, row);
        },
        bulletsToRemove);

    m_world.RemoveByPointerList(bulletsToRemove);
}

void GameScene::SpawnDropItems(float x, float y, int count)
{
    for (int i = 0; i < count; ++i)
    {
        
        const float angle = (static_cast<float>(i) / static_cast<float>(count)) * 6.28318f
            + static_cast<float>(rand() % 100) * 0.063f;
        const float speed = 250.0f + static_cast<float>(rand() % 200);
        const float velX = std::cos(angle) * speed;
        const float velY = std::sin(angle) * speed - 350.0f; 

        auto item = std::make_unique<Entity>();
        item->AddComponent<TagComponent>(kTagDropItem);
        item->AddComponent<TransformComponent>(x, y, 10.0f, 10.0f);
        item->AddComponent<TintComponent>(0.42f, 0.86f, 1.0f, 1.0f);
        item->AddComponent<SpriteRenderComponent>(m_whiteTexture);
        item->AddComponent<DropItemComponent>(1, velX, velY);
        m_world.QueueSpawn(std::move(item));
    }
}

void GameScene::UpdateDropItems()
{
    Entity* player = FindEntityByTag(kTagPlayer);
    const auto* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;

    constexpr float kGravity = 1200.0f;
    constexpr float kMaxFallSpeed = 800.0f;
    constexpr float kAttractRange = 120.0f;
    constexpr float kAttractSpeed = 400.0f;
    constexpr float kCollectRange = 48.0f;
    constexpr float kFriction = 0.85f; 

    std::vector<Entity*> collected;

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::DropItem))
    {
        if (!entity) continue;

        auto* transform = entity->GetComponent<TransformComponent>();
        auto* drop = entity->GetComponent<DropItemComponent>();
        if (!transform || !drop) continue;

        if (playerTransform)
        {
            const float dx = (playerTransform->x + playerTransform->width * 0.5f)
                - (transform->x + transform->width * 0.5f);
            const float dy = (playerTransform->y + playerTransform->height * 0.5f)
                - (transform->y + transform->height * 0.5f);
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < kCollectRange)
            {
                GameSession_AddParts(drop->GetValue());
                collected.push_back(entity);
                continue;
            }

            if (dist < kAttractRange)
            {
                
                drop->SetAttracting(true);
                const float length = std::max(1.0f, dist);
                const float attractStrength = kAttractSpeed * (1.0f - dist / kAttractRange) + 200.0f;
                drop->SetVelocityX(dx / length * attractStrength);
                drop->SetVelocityY(dy / length * attractStrength);
            }
            else
            {
                drop->SetAttracting(false);
            }
        }

        if (!drop->IsAttracting())
        {
           
            drop->SetVelocityY(std::min(kMaxFallSpeed, drop->GetVelocityY() + kGravity * m_flow.lastDeltaTime));
        }

        transform->x += drop->GetVelocityX() * m_flow.lastDeltaTime;
        transform->y += drop->GetVelocityY() * m_flow.lastDeltaTime;

        
        const float prevY = transform->y;
        const bool onGround = SnapEnemyToGround(*transform);
        if (onGround)
        {
            
            drop->SetVelocityY(0.0f);
            drop->SetVelocityX(drop->GetVelocityX() * kFriction);
            
            if (std::fabs(drop->GetVelocityX()) < 5.0f)
            {
                drop->SetVelocityX(0.0f);
            }
        }

        
        const float mapHeight = GetMapPixelHeight();
        if (transform->y > mapHeight)
        {
            collected.push_back(entity);
        }
    }

    m_world.RemoveByPointerList(collected);
}

int GameScene::GetEnemyDropCount(EnemyArchetype archetype) const
{
    switch (archetype)
    {
    case EnemyArchetype::Walker:
        return 10;
    case EnemyArchetype::Ranged:
        return 10;
    case EnemyArchetype::Charger:
        return 10 + (rand() % 21);
    case EnemyArchetype::ShieldBoss:
    case EnemyArchetype::MidBoss2:
    case EnemyArchetype::MidBoss3:
        return 50;
    default:
        return 5;
    }
}

void GameScene::UpdateShields(float deltaTime)
{
    constexpr float kTileSize = 48.0f;
    constexpr float kGravity = 1900.0f;
    constexpr float kMaxFallSpeed = 980.0f;
    Entity* player = FindEntityByTag(kTagPlayer);
    const auto* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    std::vector<std::unique_ptr<Entity>> spawnedShockwaves;
    std::vector<Entity*> shieldsToRemove;
    std::vector<Entity*> objectsToRemove;
    const auto& photoBoxEntities = m_world.EntitiesByTag(EntityTag::PhotoBox);
    const auto& enemyEntities = m_world.EntitiesByTag(EntityTag::Enemy);
    const auto& batteryEntities = m_world.EntitiesByTag(EntityTag::Battery);
    const auto& barrelEntities = m_world.EntitiesByTag(EntityTag::Barrel);
    const auto& dropItemEntities = m_world.EntitiesByTag(EntityTag::DropItem);
    const auto& batterySwitchEntities = m_world.EntitiesByTag(EntityTag::BatterySwitch);
    const auto& elevatorEntities = m_world.EntitiesByTag(EntityTag::Elevator);
    const auto& laserSwitchEntities = m_world.EntitiesByTag(EntityTag::LaserSwitch);
    const auto& shutterEntities = m_world.EntitiesByTag(EntityTag::Shutter);
    const auto& protectiveWallEntities = m_world.EntitiesByTag(EntityTag::ProtectiveWall);
    const auto& laserTurretEntities = m_world.EntitiesByTag(EntityTag::LaserTurret);
    const auto& laserBeamEntities = m_world.EntitiesByTag(EntityTag::LaserBeam);
    const auto& stageLightEntities = m_world.EntitiesByTag(EntityTag::StageLight);
    const auto& markerLightEntities = m_world.EntitiesByTag(EntityTag::MarkerLight);
    const auto& sepiaRubbleEntities = m_world.EntitiesByTag(EntityTag::SepiaRubble);
    const auto& sepiaElevatorEntities = m_world.EntitiesByTag(EntityTag::SepiaElevator);
    const auto& goalEntities = m_world.EntitiesByTag(EntityTag::Goal);
    const auto& photoSourceEntities = m_world.EntitiesByTag(EntityTag::PhotoSource);
    const auto& hazardEntities = m_world.EntitiesByTag(EntityTag::Hazard);
    const auto& bulletEntities = m_world.EntitiesByTag(EntityTag::Bullet);
    const auto& shieldTagEntities = m_world.EntitiesByTag(EntityTag::Shield);
    const auto& bossShieldTagEntities = m_world.EntitiesByTag(EntityTag::BossShield);
    const auto& boss1ShieldTagEntities = m_world.EntitiesByTag(EntityTag::Boss1Shield);
    const auto& midBoss1ShieldTagEntities = m_world.EntitiesByTag(EntityTag::MidBoss1Shield);
    const auto& capturedShieldTagEntities = m_world.EntitiesByTag(EntityTag::CapturedShield);
    const auto& walkerMeleeAttackEntities = m_world.EntitiesByTag(EntityTag::WalkerMeleeAttack);
    const auto& bossShockwaveEntities = m_world.EntitiesByTag(EntityTag::BossShockwave);
    std::vector<Entity*> shieldImpactCandidates;
    shieldImpactCandidates.reserve(
        photoBoxEntities.size() +
        enemyEntities.size() +
        batteryEntities.size() +
        barrelEntities.size() +
        dropItemEntities.size() +
        batterySwitchEntities.size() +
        elevatorEntities.size() +
        laserSwitchEntities.size() +
        shutterEntities.size() +
        protectiveWallEntities.size() +
        laserTurretEntities.size() +
        laserBeamEntities.size() +
        stageLightEntities.size() +
        markerLightEntities.size() +
        sepiaRubbleEntities.size() +
        sepiaElevatorEntities.size() +
        goalEntities.size() +
        photoSourceEntities.size() +
        hazardEntities.size() +
        bulletEntities.size() +
        shieldTagEntities.size() +
        bossShieldTagEntities.size() +
        boss1ShieldTagEntities.size() +
        midBoss1ShieldTagEntities.size() +
        capturedShieldTagEntities.size() +
        walkerMeleeAttackEntities.size() +
        bossShockwaveEntities.size());
    auto appendShieldImpactCandidates = [&](EntityTag tag)
    {
        for (Entity* candidate : m_world.EntitiesByTag(tag))
        {
            if (candidate)
            {
                shieldImpactCandidates.push_back(candidate);
            }
        }
    };
    appendShieldImpactCandidates(EntityTag::PhotoBox);
    appendShieldImpactCandidates(EntityTag::Enemy);
    appendShieldImpactCandidates(EntityTag::Battery);
    appendShieldImpactCandidates(EntityTag::Barrel);
    appendShieldImpactCandidates(EntityTag::DropItem);
    appendShieldImpactCandidates(EntityTag::BatterySwitch);
    appendShieldImpactCandidates(EntityTag::Elevator);
    appendShieldImpactCandidates(EntityTag::LaserSwitch);
    appendShieldImpactCandidates(EntityTag::Shutter);
    appendShieldImpactCandidates(EntityTag::ProtectiveWall);
    appendShieldImpactCandidates(EntityTag::LaserTurret);
    appendShieldImpactCandidates(EntityTag::LaserBeam);
    appendShieldImpactCandidates(EntityTag::StageLight);
    appendShieldImpactCandidates(EntityTag::MarkerLight);
    appendShieldImpactCandidates(EntityTag::SepiaRubble);
    appendShieldImpactCandidates(EntityTag::SepiaElevator);
    appendShieldImpactCandidates(EntityTag::Goal);
    appendShieldImpactCandidates(EntityTag::PhotoSource);
    appendShieldImpactCandidates(EntityTag::Hazard);
    appendShieldImpactCandidates(EntityTag::Bullet);
    appendShieldImpactCandidates(EntityTag::Shield);
    appendShieldImpactCandidates(EntityTag::BossShield);
    appendShieldImpactCandidates(EntityTag::Boss1Shield);
    appendShieldImpactCandidates(EntityTag::MidBoss1Shield);
    appendShieldImpactCandidates(EntityTag::CapturedShield);
    appendShieldImpactCandidates(EntityTag::WalkerMeleeAttack);
    appendShieldImpactCandidates(EntityTag::BossShockwave);

    auto startEnemyKnockback = [](Entity& target, EnemyComponent& enemy, TransformComponent& transform, float direction, float distance)
    {
        if (auto* shieldBoss = target.GetComponent<ShieldBossComponent>())
        {
            if (shieldBoss->state == ShieldBossState::Jump ||
                shieldBoss->state == ShieldBossState::JumpAscend ||
                shieldBoss->state == ShieldBossState::AirHover ||
                shieldBoss->state == ShieldBossState::JumpDescend ||
                shieldBoss->state == ShieldBossState::SlamPhase1 ||
                shieldBoss->state == ShieldBossState::SlamPhase2)
            {
                return false;
            }
            shieldBoss->knockbackActive = true;
            shieldBoss->knockbackTimer = 0.0f;
            shieldBoss->knockbackStartX = transform.x;
            shieldBoss->knockbackStartY = transform.y;
            shieldBoss->knockbackTargetX = transform.x + direction * distance;
            return true;
        }
        enemy.knockbackActive = true;
        enemy.knockbackTimer = 0.0f;
        enemy.knockbackStartX = transform.x;
        enemy.knockbackStartY = transform.y;
        enemy.knockbackTargetX = transform.x + direction * distance;
        return true;
    };

    std::vector<Entity*> shieldEntities;
    shieldEntities.reserve(
        m_world.EntitiesByTag(EntityTag::BossShield).size() +
        m_world.EntitiesByTag(EntityTag::CapturedShield).size());
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::BossShield))
    {
        if (!entity)
        {
            continue;
        }
        shieldEntities.push_back(entity);
    }
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::CapturedShield))
    {
        if (!entity)
        {
            continue;
        }
        shieldEntities.push_back(entity);
    }

    for (Entity* entity : shieldEntities)
    {
        if (!entity)
        {
            continue;
        }

        auto* shield = entity->GetComponent<ShieldComponent>();
        auto* shieldTransform = entity->GetComponent<TransformComponent>();
        if (!shield || !shieldTransform)
        {
            continue;
        }

        if (shield->photoSpawned)
        {
            shield->elapsed += deltaTime;
            switch (shield->capturedMode)
            {
            case CapturedShieldMode::Normal:
                if (shield->grounded)
                {
                    const float groundY = shieldTransform->y + GetRotatedRectBottomOffset(*shieldTransform);
                    shield->contactDamage = 0;
                    shield->gravityEnabled = false;
                    shield->velocityY = 0.0f;
                    shield->velocityX *= std::pow(0.08f, deltaTime);
                    shield->rotationSpeed *= std::pow(0.05f, deltaTime);
                    constexpr float kHalfPi = 1.5707963268f;
                    if (std::fabs(shieldTransform->rotation) > 0.08f)
                    {
                        const float targetRotation = shieldTransform->rotation >= 0.0f ? kHalfPi : -kHalfPi;
                        shieldTransform->rotation += (targetRotation - shieldTransform->rotation) * std::min(1.0f, deltaTime * 7.5f);
                    }
                    else
                    {
                        shieldTransform->rotation += shield->rotationSpeed * deltaTime;
                    }
                    shieldTransform->y = groundY - GetRotatedRectBottomOffset(*shieldTransform);
                    if (!shield->fadeStarted)
                    {
                        shield->fadeStarted = true;
                        if (!entity->GetComponent<PhotoCopyLifetimeComponent>())
                        {
                            entity->AddComponent<PhotoCopyLifetimeComponent>(2.0f);
                        }
                    }
                }
                else
                {
                    shield->gravityEnabled = true;
                    shield->contactDamage = 1;
                    shield->velocityY = std::min(kMaxFallSpeed, shield->velocityY + kGravity * deltaTime);
                    if (std::fabs(shieldTransform->rotation) > 0.04f)
                    {
                        const float torqueDir = shieldTransform->rotation >= 0.0f ? 1.0f : -1.0f;
                        shield->rotationSpeed = std::clamp(shield->rotationSpeed + torqueDir * 5.5f * deltaTime, -4.8f, 4.8f);
                    }
                    shieldTransform->x += shield->velocityX * deltaTime;
                    shieldTransform->y += shield->velocityY * deltaTime;
                    shieldTransform->rotation += shield->rotationSpeed * deltaTime;
                    if (TrySnapToGround(*shieldTransform,
                        std::max(gGroundSnapDistance, std::fabs(shield->velocityY) * deltaTime + 4.0f)))
                    {
                        const float groundY = shieldTransform->y + shieldTransform->height * shieldTransform->scale;
                        shieldTransform->y = groundY - GetRotatedRectBottomOffset(*shieldTransform);
                        shield->grounded = true;
                        shield->contactDamage = 0;
                        shield->gravityEnabled = false;
                        shield->velocityY = 0.0f;
                        shield->velocityX *= 0.35f;
                        shield->rotationSpeed *= 0.35f;
                    }
                }
                break;
            case CapturedShieldMode::RushBurst:
                shieldTransform->x += shield->velocityX * deltaTime;
                break;
            case CapturedShieldMode::JumpBurst:
                if (shield->grounded)
                {
                    shield->contactDamage = 0;
                    shield->gravityEnabled = false;
                    shield->velocityX = 0.0f;
                    shield->velocityY = 0.0f;
                    shield->rotationSpeed = 0.0f;
                    break;
                }
                if (shield->followPlayer && playerTransform)
                {
                    shield->hoverElapsed += deltaTime;
                    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                    const float playerFootY = playerTransform->y + playerTransform->height * playerTransform->scale;
                    const float shieldWidth = shieldTransform->width * shieldTransform->scale;
                    const float sideOffsetX = std::fabs(shield->followOffsetX) * (m_player.facingRight ? 1.0f : -1.0f);
                    shieldTransform->x = playerCenterX + sideOffsetX - shieldWidth * 0.5f;
                    shieldTransform->y = playerFootY + shield->followOffsetY;
                    if (auto* sprite = entity->GetComponent<SpriteRenderComponent>())
                    {
                        sprite->SetFlipX(!m_player.facingRight);
                    }
                    if (shield->hoverElapsed >= shield->hoverDuration)
                    {
                        shield->followPlayer = false;
                        shield->hitEntities.clear();
                    }
                }
                else
                {
                    auto snapShieldToTileGround = [this, kTileSize](TransformComponent& transform, float maxSnapDistance) -> bool
                    {
                        const float width = transform.width * transform.scale;
                        const float height = transform.height * transform.scale;
                        const float bottom = transform.y + height;
                        const int columnStart = std::max(0, static_cast<int>((transform.x + 6.0f) / kTileSize));
                        const int columnEnd = std::min(
                            m_tileMap.GetWidth() - 1,
                            static_cast<int>((transform.x + width - 6.0f) / kTileSize));
                        const int rowStart = std::max(0, static_cast<int>((bottom - maxSnapDistance) / kTileSize));
                        const int rowEnd = std::min(
                            m_tileMap.GetHeight() - 1,
                            static_cast<int>((bottom + maxSnapDistance) / kTileSize));
                        float nearestGroundY = 0.0f;
                        bool foundGround = false;
                        const float probeXs[3] = {
                            transform.x + width * 0.25f,
                            transform.x + width * 0.5f,
                            transform.x + width * 0.75f,
                        };

                        for (int row = rowStart; row <= rowEnd; ++row)
                        {
                            for (int column = columnStart; column <= columnEnd; ++column)
                            {
                                if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                                {
                                    continue;
                                }

                                if (IsSlopeTile(column, row))
                                {
                                    for (float probeX : probeXs)
                                    {
                                        float surfaceY = 0.0f;
                                        if (!GetSlopeSurfaceY(column, row, probeX, surfaceY))
                                        {
                                            continue;
                                        }
                                        const float candidateY = surfaceY - height;
                                        if (candidateY < transform.y - maxSnapDistance)
                                        {
                                            continue;
                                        }
                                        if (!foundGround || candidateY < nearestGroundY)
                                        {
                                            nearestGroundY = candidateY;
                                            foundGround = true;
                                        }
                                    }
                                    continue;
                                }

                                const float candidateY = static_cast<float>(row) * kTileSize - height;
                                if (candidateY < transform.y - maxSnapDistance)
                                {
                                    continue;
                                }
                                if (!foundGround || candidateY < nearestGroundY)
                                {
                                    nearestGroundY = candidateY;
                                    foundGround = true;
                                }
                            }
                        }

                        if (!foundGround || std::fabs(nearestGroundY - transform.y) > maxSnapDistance)
                        {
                            return false;
                        }
                        transform.y = nearestGroundY;
                        return true;
                    };

                    const float previousY = shieldTransform->y;
                    shieldTransform->y += shield->descendSpeed * deltaTime;
                    const float travelY = (std::max)(0.0f, shieldTransform->y - previousY);
                    TransformComponent slamSweep(
                        shieldTransform->x,
                        previousY,
                        shieldTransform->width * shieldTransform->scale,
                        shieldTransform->height * shieldTransform->scale + travelY);
                    for (Entity* target : shieldImpactCandidates)
                    {
                        if (!target || target == entity)
                        {
                            continue;
                        }
                        if (HasTag(*target, "Player") ||
                            HasTag(*target, "BossShield") ||
                            HasTag(*target, "CapturedShield") ||
                            HasTag(*target, "BossShockwave"))
                        {
                            continue;
                        }
                        if (!HasTag(*target, kTagPhotoBox))
                        {
                            continue;
                        }
                        auto* targetTransform = target->GetComponent<TransformComponent>();
                        if (!targetTransform ||
                            (!IntersectsRotatedRect(*shieldTransform, *targetTransform) &&
                                !IntersectsRotatedRect(slamSweep, *targetTransform)))
                        {
                            continue;
                        }
                        objectsToRemove.push_back(target);
                    }
                    bool hitGround = false;
                    if (snapShieldToTileGround(*shieldTransform, shield->descendSpeed * deltaTime + 4.0f))
                    {
                        hitGround = true;
                    }
                    if (hitGround)
                    {
                        shield->grounded = true;
                        shield->contactDamage = 0;
                        shield->gravityEnabled = false;
                        shield->velocityX = 0.0f;
                        shield->velocityY = 0.0f;
                        shield->rotationSpeed = 0.0f;
                        shield->hitEntities.clear();

                        if (!shield->shockwaveSpawned)
                        {
                            shield->shockwaveSpawned = true;
                            const float shockW = kTileSize * 8.0f;
                            const float shockH = kTileSize * 3.0f;
                            auto shockwave = std::make_unique<Entity>();
                            shockwave->AddComponent<TagComponent>(EntityTag::BossShockwave);
                            const float shockGroundY = shieldTransform->y + shieldTransform->height * shieldTransform->scale;
                            shockwave->AddComponent<TransformComponent>(
                                shieldTransform->x + shieldTransform->width * shieldTransform->scale * 0.5f - shockW * 0.5f,
                                shockGroundY - kTileSize * 2.0f,
                                shockW,
                                shockH);
                            shockwave->AddComponent<TintComponent>(0.18f, 0.95f, 1.0f, 0.75f);
                            shockwave->AddComponent<SpriteRenderComponent>(m_whiteTexture);
                            auto& shockComp = shockwave->AddComponent<ShieldShockwaveComponent>();
                            shockComp.ownerBoss = shield->ownerBoss;
                            shockComp.damage = 1;
                            shockComp.lifetime = 0.4f;
                            shockComp.damagesPlayer = false;
                            spawnedShockwaves.push_back(std::move(shockwave));

                            m_flow.screenShakeRemaining = std::max(m_flow.screenShakeRemaining, 0.18f);
                            m_flow.screenShakeDuration = std::max(m_flow.screenShakeDuration, 0.18f);
                            m_flow.screenShakeAmplitude = std::max(m_flow.screenShakeAmplitude, 16.0f);
                            SpawnSlamImpactEffect(
                                shieldTransform->x + shieldTransform->width * shieldTransform->scale * 0.5f,
                                shockGroundY,
                                shockW);
                            if (!shield->shieldDropSoundPlayed)
                            {
                                shield->shieldDropSoundPlayed = true;
                                PlayShieldBossSoundCue("boss_forest_shield_drop");
                            }
                        }
                        shieldsToRemove.push_back(entity);
                    }
                }
                break;
            case CapturedShieldMode::None:
            default:
                break;
            }
        }
        else
        {
            const bool skipPhysics = shield->attached;
            if (!skipPhysics)
            {
                if (shield->attackType == ShieldAttackType::Base)
                {
                    shield->baseAttackElapsed += deltaTime;
                    if (shield->baseAttackElapsed >= shield->baseAttackDuration)
                    {
                        shield->attached = true;
                        shield->attackType = ShieldAttackType::None;
                        shield->velocityX = 0.0f;
                        shield->velocityY = 0.0f;
                        shield->rotationSpeed = 0.0f;
                        shield->gravityEnabled = false;
                        shield->baseAttackElapsed = 0.0f;
                        shield->contactDamage = 1;
                        continue;
                    }
                }

                if (shield->gravityEnabled)
                {
                    shield->velocityY = std::min(kMaxFallSpeed, shield->velocityY + kGravity * deltaTime);
                }

                shieldTransform->x += shield->velocityX * deltaTime;
                shieldTransform->y += shield->velocityY * deltaTime;
                shieldTransform->rotation += shield->rotationSpeed * deltaTime;

                if (TrySnapToGround(*shieldTransform,
                    std::max(gGroundSnapDistance, std::fabs(shield->velocityY) * deltaTime + 4.0f)))
                {
                    shield->velocityY = 0.0f;
                    shield->velocityX *= 0.85f;
                }
            }
        }

        const bool isBossShield = HasTag(*entity, "BossShield");
        if (isBossShield && shield->ownerBoss)
        {
            auto* ownerBoss = shield->ownerBoss->GetComponent<ShieldBossComponent>();
            auto* ownerTransform = shield->ownerBoss->GetComponent<TransformComponent>();
            if (ownerBoss && ownerTransform && ownerBoss->knockbackActive)
            {
                constexpr float kGuardShieldW = 48.0f;
                constexpr float kGuardShieldH = 192.0f;
                constexpr float kGuardOverlapX = kGuardShieldW * 1.5f;
                const float ownerW = ownerTransform->width * ownerTransform->scale;
                shield->attached = true;
                shield->attackType = ShieldAttackType::None;
                shield->velocityX = 0.0f;
                shield->velocityY = 0.0f;
                shield->rotationSpeed = 0.0f;
                shield->gravityEnabled = false;
                shieldTransform->width = kGuardShieldW;
                shieldTransform->height = kGuardShieldH;
                shieldTransform->rotation = 0.0f;
                shieldTransform->x = ownerBoss->facing == ShieldBossFacing::Right
                    ? ownerTransform->x + ownerW - kGuardOverlapX
                    : ownerTransform->x - kGuardShieldW + kGuardOverlapX;
                shieldTransform->y = ownerTransform->y;
            }
        }

        const float shieldW = shieldTransform->width * shieldTransform->scale;
        const bool normalCapturedShield =
            shield->photoSpawned &&
            shield->capturedMode == CapturedShieldMode::Normal;
        const bool canRemoveNormalCapturedShield =
            normalCapturedShield &&
            !shield->grounded &&
            shield->elapsed > 0.05f;
        if (shield->contactDamage > 0 &&
            (!shield->photoSpawned || shield->capturedMode == CapturedShieldMode::Normal) &&
            player && playerTransform && IntersectsRotatedRect(*playerTransform, *shieldTransform))
        {
            ApplyHazardDamageToPlayer(*player, entity,
                "BossShield damaged player", shield->contactDamage);
            if (canRemoveNormalCapturedShield)
            { 
                shieldsToRemove.push_back(entity);
            }
        }

        bool removeShieldAfterObjectHit = false;
        for (Entity* target : shieldImpactCandidates)
        {
            if (!target || target == entity)
            {
                continue;
            }
            if (HasTag(*target, "BossShield") || HasTag(*target, "CapturedShield") || HasTag(*target, "BossShockwave"))
            {
                continue;
            }
            if (target == shield->ownerBoss)
            {
                continue;
            }

            auto* enemyTransform = target->GetComponent<TransformComponent>();
            if (!enemyTransform || !IntersectsRotatedRect(*shieldTransform, *enemyTransform))
            {
                continue;
            }
            if (shield->contactDamage <= 0)
            {
                continue;
            }

            auto* enemy = target->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated())
            {
                if (canRemoveNormalCapturedShield)
                {
                    removeShieldAfterObjectHit = true;
                    break;
                }
                continue;
            }

            if (shield->photoSpawned)
            {
                const bool alreadyHit = std::find(
                    shield->hitEntities.begin(),
                    shield->hitEntities.end(),
                    target) != shield->hitEntities.end();
                if (alreadyHit)
                {
                    if (canRemoveNormalCapturedShield)
                    {
                        removeShieldAfterObjectHit = true;
                        break;
                    }
                    continue;
                }
                shield->hitEntities.push_back(target);
            }

            if (shield->photoSpawned &&
                shield->capturedMode == CapturedShieldMode::RushBurst &&
                !shield->knockbackSoundPlayed)
            {
                shield->knockbackSoundPlayed = true;
                PlayShieldBossSoundCue("boss_forest_knockback");
            }
            if (shield->photoSpawned &&
                shield->capturedMode == CapturedShieldMode::Normal &&
                !shield->shieldDropSoundPlayed)
            {
                shield->shieldDropSoundPlayed = true;
                PlayShieldBossSoundCue("boss_forest_shield_drop");
            }

            HandleEnemyDamage(*target, entity, shield->contactDamage, "BossShield damaged enemy");

            const auto* shieldBoss = target->GetComponent<ShieldBossComponent>();
            float dir = 0.0f;
            if (shieldBoss && enemy->GetArchetype() == EnemyArchetype::ShieldBoss)
            {
                dir = shieldBoss->facing == ShieldBossFacing::Right ? -1.0f : 1.0f;
            }
            else
            {
                const float shieldCenterX = shieldTransform->x + shieldW * 0.5f;
                const float enemyCenterX = enemyTransform->x + enemyTransform->width * enemyTransform->scale * 0.5f;
                dir = enemyCenterX >= shieldCenterX ? 1.0f : -1.0f;
            }
            const bool skipNormalShieldBossKnockback =
                normalCapturedShield &&
                shieldBoss &&
                enemy->GetArchetype() == EnemyArchetype::ShieldBoss;
            if (skipNormalShieldBossKnockback)
            {
                // 通常盾はボスを動かさず、命中感だけヒットストップとシェイクで出す。
                TriggerNormalShieldBossHitFeedback(m_flow, m_debug.screenShakeEnabled);
            }
            else if (startEnemyKnockback(*target, *enemy, *enemyTransform, dir, shield->knockbackGrids * kTileSize))
            {
                if (shieldBoss)
                {
                    TriggerBossKnockbackFeedback(m_flow, m_debug.screenShakeEnabled);
                }
                else
                {
                    TriggerEnemyKnockbackFeedback(m_flow, m_debug.screenShakeEnabled);
                }
            }

            if (canRemoveNormalCapturedShield)
            {
                removeShieldAfterObjectHit = true;
                break;
            }
        }

        if (removeShieldAfterObjectHit)
        {
            shieldsToRemove.push_back(entity);
        }
    }

    for (auto& shockwave : spawnedShockwaves)
    {
        m_world.Spawn(std::move(shockwave));
    }

    m_world.RemoveByPointerList(objectsToRemove);

    std::vector<Entity*> shockwavesToRemove;
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::BossShockwave))
    {
        if (!entity) continue;

        auto* shockwave = entity->GetComponent<ShieldShockwaveComponent>();
        auto* shockTransform = entity->GetComponent<TransformComponent>();
        if (!shockwave || !shockTransform) continue;

        shockwave->elapsed += deltaTime;
        if (shockwave->elapsed >= shockwave->lifetime)
        {
            shockwavesToRemove.push_back(entity);
            continue;
        }

        if (shockwave->damagesPlayer &&
            !shockwave->hitPlayer && player && playerTransform
            && IntersectsRect(*playerTransform, *shockTransform))
        {
            ApplyHazardDamageToPlayer(*player, entity,
                "BossShockwave damaged player", shockwave->damage);
            shockwave->hitPlayer = true;
        }

        for (Entity* target : shieldImpactCandidates)
        {
            if (!target || target == entity) continue;
            if (HasTag(*target, "BossShield") || HasTag(*target, "CapturedShield") || HasTag(*target, "BossShockwave")) continue;
            if (target == shockwave->ownerBoss) continue;

            auto* enemyTransform = target->GetComponent<TransformComponent>();
            if (!enemyTransform || !IntersectsRect(*shockTransform, *enemyTransform)) continue;

            if (HasTag(*target, kTagPhotoBox))
            {
                objectsToRemove.push_back(target);
                continue;
            }

            auto* enemy = target->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated()) continue;

            const bool alreadyHit = std::find(
                shockwave->hitEntities.begin(),
                shockwave->hitEntities.end(),
                target) != shockwave->hitEntities.end();
            if (alreadyHit) continue;

            HandleEnemyDamage(*target, entity, shockwave->damage, "BossShockwave damaged enemy");
            shockwave->hitEntities.push_back(target);

            const auto* shieldBoss = target->GetComponent<ShieldBossComponent>();
            float dir = 0.0f;
            if (shieldBoss && enemy->GetArchetype() == EnemyArchetype::ShieldBoss)
            {
                const float shockCenterX = shockTransform->x + shockTransform->width * shockTransform->scale * 0.5f;
                const float enemyCenterX = enemyTransform->x + enemyTransform->width * enemyTransform->scale * 0.5f;
                const bool pastedJumpAttack = shockwave->ownerBoss == nullptr && !shockwave->damagesPlayer;
                const bool hitFromBehind = shieldBoss->facing == ShieldBossFacing::Right
                    ? shockCenterX < enemyCenterX
                    : shockCenterX > enemyCenterX;
                if (pastedJumpAttack && hitFromBehind)
                {
                    dir = shieldBoss->facing == ShieldBossFacing::Right ? 1.0f : -1.0f;
                }
                else
                {
                    dir = shieldBoss->facing == ShieldBossFacing::Right ? -1.0f : 1.0f;
                }
            }
            else
            {
                const float shockCenterX = shockTransform->x + shockTransform->width * shockTransform->scale * 0.5f;
                const float enemyCenterX = enemyTransform->x + enemyTransform->width * enemyTransform->scale * 0.5f;
                dir = enemyCenterX >= shockCenterX ? 1.0f : -1.0f;
            }
            if (startEnemyKnockback(*target, *enemy, *enemyTransform, dir, shockwave->knockbackGrids * kTileSize))
            {
                if (shieldBoss)
                {
                    TriggerBossKnockbackFeedback(m_flow, m_debug.screenShakeEnabled);
                }
                else
                {
                    TriggerEnemyKnockbackFeedback(m_flow, m_debug.screenShakeEnabled);
                }
            }
        }
    }

    m_world.RemoveByPointerList(objectsToRemove);
    m_world.RemoveByPointerList(shieldsToRemove);
    m_world.RemoveByPointerList(shockwavesToRemove);
}
void GameScene::HandleEnemyPlayerCollisions(Entity& player)
{
    const auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!entity || entity == &player)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        auto* enemyTransform = entity->GetComponent<TransformComponent>();
        if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated() || !enemyTransform)
        {
            continue;
        }

        TransformComponent expandedBounds(
            enemyTransform->x - 2.0f,
            enemyTransform->y - 2.0f,
            enemyTransform->width + 4.0f,
            enemyTransform->height + 4.0f);
        expandedBounds.scale = enemyTransform->scale;
        if (!IntersectsRect(*playerTransform, expandedBounds))
        {
            continue;
        }

        ApplyHazardDamageToPlayer(
            player,
            entity,
            "GameScene player damaged by enemy",
            enemy->GetContactDamage());
    }
}

void GameScene::HandleWalkerMeleeAttackCollisions()
{
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::WalkerMeleeAttack))
    {
        if (!entity) continue;

        const auto* meleeTransform = entity->GetComponent<TransformComponent>();
        if (!meleeTransform) continue;

        // 敵へのダメージ
        for (Entity* target : m_world.EntitiesByTag(EntityTag::Enemy))
        {
            if (!target) continue;

            auto* enemy = target->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated()) continue;

            const auto* enemyTransform = target->GetComponent<TransformComponent>();
            if (!enemyTransform) continue;

            if (!IntersectsRect(*meleeTransform, *enemyTransform)) continue;

            HandleEnemyDamage(*target, entity, 1, "WalkerMeleeAttack hit enemy");
        }
    }
}

void GameScene::RemoveDefeatedEnemies()
{
    const float cameraLeft = m_flow.cameraX - 48.0f;
    const float cameraRight = m_flow.cameraX + gCameraViewWidth + 48.0f;

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!entity) continue;

        auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsDefeated()) continue;
        if (enemy->GetArchetype() == EnemyArchetype::ShieldBoss)
        {
            enemy->respawnEnabled = false;
            m_flow.shieldBossDefeatedThisScene = true;
        }
        if (enemy->GetArchetype() == EnemyArchetype::MidBoss3)
        {
            GameSession_SetCameraFlashOwned(true);
            m_ui.cameraFlash.unlocked = true;
        }
        if (!enemy->respawnEnabled) continue;

        auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform) continue;

        
        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            tint->a = 0.0f;
        }
        enemy->SetEnabled(false);
       
        transform->x = -9999.0f;
        transform->y = -9999.0f;

        const float enemyX = transform->x;
        if (enemy->spawnX < cameraLeft || enemy->spawnX > cameraRight)
        {
            transform->x = enemy->spawnX;
            transform->y = enemy->spawnY - 96.0f;
            SnapEnemyToGround(*transform);
            enemy->velocityY = 0.0f;
            enemy->attackTimer = 0.0f;
            enemy->SetAIState(EnemyComponent::AIState::Idle);
            enemy->Restore();

            if (auto* tint = entity->GetComponent<TintComponent>())
            {
                tint->a = 1.0f;
            }
        }
    }

    m_world.EraseIf(
        [&](const std::unique_ptr<Entity>& entity)
        {
            const auto* enemy = entity ? entity->GetComponent<EnemyComponent>() : nullptr;
            if (enemy && enemy->IsDefeated() && !enemy->respawnEnabled)
            {
                return true;
            }
            const auto* shield = entity ? entity->GetComponent<ShieldComponent>() : nullptr;
            if (shield && HasTag(*entity, "BossShield"))
            {
                bool ownerFound = false;
                bool ownerDefeated = true;
                for (Entity* candidate : m_world.EntitiesByTag(EntityTag::Enemy))
                {
                    if (!candidate || candidate != shield->ownerBoss)
                    {
                        continue;
                    }
                    ownerFound = true;
                    const auto* ownerEnemy = candidate->GetComponent<EnemyComponent>();
                    ownerDefeated = !ownerEnemy || ownerEnemy->IsDefeated();
                    break;
                }
                if (!ownerFound || ownerDefeated)
                {
                    return true;
                }
            }
            const auto* midBoss3Fist = entity ? entity->GetComponent<MidBoss3FistComponent>() : nullptr;
            if (midBoss3Fist && midBoss3Fist->ownerBoss)
            {
                const auto* ownerEnemy = midBoss3Fist->ownerBoss->GetComponent<EnemyComponent>();
                if (!ownerEnemy || ownerEnemy->IsDefeated())
                {
                    return true;
                }
            }
            const auto* capturedMidBoss3Attack = entity ? entity->GetComponent<CapturedMidBoss3AttackComponent>() : nullptr;
            if (capturedMidBoss3Attack && capturedMidBoss3Attack->carriedBoss)
            {
                const auto* carriedEnemy = capturedMidBoss3Attack->carriedBoss->GetComponent<EnemyComponent>();
                if (!carriedEnemy || carriedEnemy->IsDefeated())
                {
                    return true;
                }
            }
            const auto* lifetime = entity ? entity->GetComponent<PhotoCopyLifetimeComponent>() : nullptr;
            if (!lifetime)
            {
                return false;
            }
            return lifetime->IsExpired();
        });

    RefreshPhotoGroupState();
}

void GameScene::HandleEnemyDamage(Entity& enemy, Entity* sourceEntity, int amount, const char* logMessage)
{
    auto* enemyComponent = enemy.GetComponent<EnemyComponent>();
    if (!enemyComponent || !enemyComponent->IsEnabled() || enemyComponent->IsDefeated())
    {
        return;
    }
    if (auto* shieldBoss = enemy.GetComponent<ShieldBossComponent>())
    {
        if (shieldBoss->deathAnimationActive || shieldBoss->deathAnimationFinished)
        {
            return;
        }
    }
    const auto cleanupMidBoss3Defeat = [](Entity& defeatedBoss)
    {
        auto* midBoss3 = defeatedBoss.GetComponent<MidBoss3Component>();
        if (!midBoss3)
        {
            return;
        }
        midBoss3->drillActive = false;
        midBoss3->drillFormed = false;
        midBoss3->drillGroundRush = false;
        midBoss3->reloadActive = false;
        for (Entity* fistEntity : midBoss3->fistEntities)
        {
            auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
            if (fist)
            {
                fist->state = MidBoss3FistState::Broken;
                fist->broken = true;
                fist->captureJammerActive = false;
                fist->impactAttackActive = false;
                fist->impactDamageApplied = false;
                fist->impactAttackRemaining = 0.0f;
            }
            if (auto* tint = fistEntity ? fistEntity->GetComponent<TintComponent>() : nullptr)
            {
                tint->a = 0.0f;
            }
            if (auto* transform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr)
            {
                transform->x = -10000.0f;
                transform->y = -10000.0f;
            }
        }
    };

    auto* damageFlash = enemy.GetComponent<DamageCooldownComponent>();
    if (!damageFlash)
    {
        damageFlash = &enemy.AddComponent<DamageCooldownComponent>(0.18f);
    }
    damageFlash->Trigger();

    if (auto* midBoss3 = enemy.GetComponent<MidBoss3Component>())
    {
        bool requestDamageMotion = true;
        if (sourceEntity)
        {
            if (const auto* capturedAttack = sourceEntity->GetComponent<CapturedMidBoss3AttackComponent>())
            {
                requestDamageMotion = capturedAttack->kind != CapturedMidBoss3AttackKind::Drill ||
                    !capturedAttack->attachedToBoss;
            }
        }
        if (requestDamageMotion)
        {
            midBoss3->damageMotionRequested = true;
            midBoss3->damageMotionDirection = ResolveMidBoss3DamageDirection(enemy, sourceEntity);
        }
    }

    if (auto* boss = enemy.GetComponent<ShieldBossComponent>())
    {
        if (boss->shieldEntity)
        {
            auto* shieldFlash = boss->shieldEntity->GetComponent<DamageCooldownComponent>();
            if (!shieldFlash)
            {
                shieldFlash = &boss->shieldEntity->AddComponent<DamageCooldownComponent>(0.18f);
            }
            shieldFlash->Trigger();
        }
    }

    bool defeatedThisHit = false;
    if (auto* health = enemy.GetComponent<HealthComponent>())
    {
        health->ApplyDamage(amount);
        if (health->IsDead())
        {
            if (auto* shieldBoss = enemy.GetComponent<ShieldBossComponent>())
            {
                if (auto* animation = enemy.GetComponent<SpriteSheetAnimationComponent>())
                {
                    shieldBoss->deathAnimationActive = true;
                    shieldBoss->stateTimer = 0.0f;
                    animation->Play("death", true);
                    if (!shieldBoss->deadSoundPlayed)
                    {
                        shieldBoss->deadSoundPlayed = true;
                        PlayShieldBossSoundCue("boss_forest_dead");
                    }
                    if (shieldBoss->shieldEntity)
                    {
                        if (auto* shieldTint = shieldBoss->shieldEntity->GetComponent<TintComponent>())
                        {
                            shieldTint->a = 0.0f;
                        }
                    }
                    TriggerBossDefeatStartFeedback(m_flow);
                    m_eventBus.Publish({ EventType::PlaySoundRequest, &enemy, sourceEntity, "contact_tone", 0.0f, 0.0f });
                    m_eventBus.Publish({ EventType::LogMessage, &enemy, sourceEntity, logMessage, 0.0f, 0.0f });
                    return;
                }
                shieldBoss->deathAnimationActive = false;
                shieldBoss->deathAnimationFinished = true;
                if (shieldBoss->shieldEntity)
                {
                    if (auto* shieldTint = shieldBoss->shieldEntity->GetComponent<TintComponent>())
                    {
                        shieldTint->a = 0.0f;
                    }
                }
                TriggerBossDefeatStartFeedback(m_flow);
            }

            enemyComponent->MarkDefeated();
            defeatedThisHit = true;
            if (enemyComponent->GetArchetype() == EnemyArchetype::ShieldBoss)
            {
                m_flow.shieldBossDefeatedThisScene = true;
            }
            if (auto* midBoss2 = enemy.GetComponent<MidBoss2Component>())
            {
                if (auto* turretEntity = midBoss2->beamTurretEntity)
                {
                    if (auto* turret = turretEntity->GetComponent<LaserTurretComponent>())
                    {
                        turret->active = false;
                    }
                    if (auto* capture = turretEntity->GetComponent<BossBeamCaptureComponent>())
                    {
                        capture->captureEnabled = false;
                    }
                    if (auto* transform = turretEntity->GetComponent<TransformComponent>())
                    {
                        transform->x = -10000.0f;
                        transform->y = -10000.0f;
                    }
                }
                if (auto* beamEntity = midBoss2->beamEntity)
                {
                    if (auto* beamTransform = beamEntity->GetComponent<TransformComponent>())
                    {
                        beamTransform->x = -10000.0f;
                        beamTransform->y = -10000.0f;
                        beamTransform->width = 0.0f;
                    }
                }
            }
            
            if (const auto* transform = enemy.GetComponent<TransformComponent>())
            {
                const int dropCount = GetEnemyDropCount(enemyComponent->GetArchetype());
                SpawnDropItems(
                    transform->x + transform->width * transform->scale * 0.5f,
                    transform->y + transform->height * transform->scale * 0.5f,
                    dropCount);
            }
        }
    }
    else
    {
        enemyComponent->MarkDefeated();
        defeatedThisHit = true;
        if (enemyComponent->GetArchetype() == EnemyArchetype::ShieldBoss)
        {
            m_flow.shieldBossDefeatedThisScene = true;
        }
        if (auto* midBoss2 = enemy.GetComponent<MidBoss2Component>())
        {
            if (auto* turretEntity = midBoss2->beamTurretEntity)
            {
                if (auto* turret = turretEntity->GetComponent<LaserTurretComponent>())
                {
                    turret->active = false;
                }
                if (auto* capture = turretEntity->GetComponent<BossBeamCaptureComponent>())
                {
                    capture->captureEnabled = false;
                }
                if (auto* transform = turretEntity->GetComponent<TransformComponent>())
                {
                    transform->x = -10000.0f;
                    transform->y = -10000.0f;
                }
            }
            if (auto* beamEntity = midBoss2->beamEntity)
            {
                if (auto* beamTransform = beamEntity->GetComponent<TransformComponent>())
                {
                    beamTransform->x = -10000.0f;
                    beamTransform->y = -10000.0f;
                    beamTransform->width = 0.0f;
                }
            }
        }
        
        if (const auto* transform = enemy.GetComponent<TransformComponent>())
        {
            const int dropCount = GetEnemyDropCount(enemyComponent->GetArchetype());
            SpawnDropItems(
                transform->x + transform->width * transform->scale * 0.5f,
                transform->y + transform->height * transform->scale * 0.5f,
                dropCount);
        }
    }
    if (defeatedThisHit)
    {
        cleanupMidBoss3Defeat(enemy);
        if (enemyComponent->GetArchetype() == EnemyArchetype::MidBoss3)
        {
            // 森林ボスと同じ余韻を残してから、廃墟ステージBGMへ戻す。
            m_flow.stageBgmCrossFadePending = true;
            m_flow.stageBgmCrossFadeDelayRemaining = kBossStageBgmReturnDelaySeconds;
        }
        if (enemyComponent->GetArchetype() == EnemyArchetype::ShieldBoss)
        {
            TriggerBossDefeatStartFeedback(m_flow);
        }
        else
        {
            TriggerEnemyDefeatFeedback(m_flow);
        }
    }
    m_eventBus.Publish({ EventType::PlaySoundRequest, &enemy, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &enemy, sourceEntity, logMessage, 0.0f, 0.0f });
}


