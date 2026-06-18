#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "logger.h"
#include "components.h"
#include "entity.h"
#include "game_scene_photo_state.h"
#include "game_scene_state.h"

namespace game_scene_combat_system
{
inline constexpr const char* kTagBullet = "Bullet";
inline constexpr const char* kTagEnemy = "Enemy";
inline constexpr float kMidBoss2SpearSpeed = 800.0f;

inline bool HasTag(const Entity& entity, const char* value)
{
    const auto* tag = entity.GetComponent<TagComponent>();
    return tag && tag->Is(value);
}

inline bool IntersectsBounds(const TransformComponent& a, const TransformComponent& b)
{
    const float aRight = a.x + a.width * a.scale;
    const float aBottom = a.y + a.height * a.scale;
    const float bRight = b.x + b.width * b.scale;
    const float bBottom = b.y + b.height * b.scale;
    return a.x < bRight && aRight > b.x && a.y < bBottom && aBottom > b.y;
}

inline void UpdateWalkerSpriteAnimation(Entity& entity, const EnemyComponent& enemy, bool moving)
{
    auto* sprite = entity.GetComponent<SpriteRenderComponent>();
    auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>();
    if (!sprite || !animation)
    {
        return;
    }

    // Enemy1 sheets face left by default; mirror only when the AI faces right.
    sprite->SetFlipX(enemy.facing == EnemyComponent::FacingDirection::Right);
    const char* clipName = enemy.GetAIState() == EnemyComponent::AIState::Attack
        ? "attack"
        : (moving ? "move" : "idle");
    animation->Play(clipName);
}

inline void PlayRangedSpriteAnimation(Entity& entity, const char* clipName, bool restart = false)
{
    auto* sprite = entity.GetComponent<SpriteRenderComponent>();
    auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>();
    if (!sprite || !animation)
    {
        return;
    }

    // Enemy2 is authored left-facing and should never turn around.
    sprite->SetFlipX(false);
    animation->Play(clipName, restart);
}

inline const char* GetShieldBossRushClipName(ShieldBossState state)
{
    switch (state)
    {
    case ShieldBossState::Rush:
        return "attack01";
    case ShieldBossState::Detect:
        return "idle";
    case ShieldBossState::JumpAscend:
    case ShieldBossState::AirHover:
    case ShieldBossState::JumpDescend:
    case ShieldBossState::SlamPhase1:
        return "attack02";
    case ShieldBossState::SlamPhase2:
    case ShieldBossState::Cooldown:
        return "move";
    default:
        return "idle";
    }
}

inline const char* ResolveShieldBossBodyClipName(const ShieldBossComponent& boss)
{
    if (boss.deathAnimationActive)
    {
        return "death";
    }
    if (boss.knockbackActive)
    {
        return "knockback";
    }
    if (boss.appearAnimationActive)
    {
        return "appear";
    }
    if (boss.roarAnimationActive)
    {
        return "roar";
    }
    return GetShieldBossRushClipName(boss.state);
}

struct ShieldBossVisualMetrics
{
    float cellWidth = 241.8f;
    float cellHeight = 188.0f;
    float bodyLeft = 31.0f;
    float bodyTop = 51.0f;
    float bodyWidth = 169.0f;
    float bodyHeight = 123.0f;
};

inline ShieldBossVisualMetrics GetShieldBossVisualMetrics(const char* clipName)
{
    ShieldBossVisualMetrics metrics;
    if (std::strcmp(clipName, "attack02") == 0)
    {
        metrics.cellWidth = 240.0f;
        metrics.cellHeight = 195.0f;
        metrics.bodyLeft = 90.0f;
        metrics.bodyTop = 70.0f;
        metrics.bodyWidth = 114.0f;
        metrics.bodyHeight = 102.0f;
    }
    else if (std::strcmp(clipName, "knockback") == 0)
    {
        metrics.cellWidth = 240.0f;
        metrics.cellHeight = 186.7f;
        metrics.bodyLeft = 42.0f;
        metrics.bodyTop = 42.0f;
        metrics.bodyWidth = 173.0f;
        metrics.bodyHeight = 131.0f;
    }
    else if (std::strcmp(clipName, "death") == 0)
    {
        metrics.cellWidth = 240.0f;
        metrics.cellHeight = 186.6f;
        metrics.bodyLeft = 28.0f;
        metrics.bodyTop = 49.0f;
        metrics.bodyWidth = 145.0f;
        metrics.bodyHeight = 112.0f;
    }
    else if (std::strcmp(clipName, "appear") == 0 || std::strcmp(clipName, "roar") == 0)
    {
        metrics.cellWidth = 240.0f;
        metrics.cellHeight = 240.0f;
        metrics.bodyLeft = 22.0f;
        metrics.bodyTop = 72.0f;
        metrics.bodyWidth = 164.0f;
        metrics.bodyHeight = 114.5f;
    }
    else if (std::strcmp(clipName, "attack01") == 0)
    {
        metrics.bodyLeft = 31.0f;
        metrics.bodyTop = 51.0f;
        metrics.bodyWidth = 168.0f;
        metrics.bodyHeight = 123.0f;
    }
    return metrics;
}

inline void ApplyShieldBossVisualLayout(Entity& entity, ShieldBossComponent& boss, bool flipRight, const char* clipName)
{
    const auto* transform = entity.GetComponent<TransformComponent>();
    auto* sprite = entity.GetComponent<SpriteRenderComponent>();
    if (!transform || !sprite)
    {
        return;
    }

    const float hitboxWidth = transform->width * transform->scale;
    const float hitboxHeight = transform->height * transform->scale;
    const ShieldBossVisualMetrics metrics = GetShieldBossVisualMetrics(clipName);
    const float visualScaleX = metrics.cellWidth / std::max(1.0f, metrics.bodyWidth);
    const float visualScaleY = metrics.cellHeight / std::max(1.0f, metrics.bodyHeight);
    const bool introClip = std::strcmp(clipName, "appear") == 0 || std::strcmp(clipName, "roar") == 0;
    const bool deathClip = std::strcmp(clipName, "death") == 0;
    const float emphasisScale = (introClip || deathClip) ? 1.2f : 1.0f;
    const float visualWidth = hitboxWidth * visualScaleX * emphasisScale;
    const float visualHeight = hitboxHeight * visualScaleY * emphasisScale;
    const float drawScaleX = visualWidth / std::max(1.0f, hitboxWidth);
    const float drawScaleY = visualHeight / std::max(1.0f, hitboxHeight);
    const float bodyLeft = flipRight
        ? metrics.cellWidth - (metrics.bodyLeft + metrics.bodyWidth)
        : metrics.bodyLeft;
    const float drawOffsetX = (introClip || deathClip)
        ? hitboxWidth * (1.0f - emphasisScale) * 0.5f - bodyLeft / metrics.cellWidth * visualWidth
        : -bodyLeft / metrics.cellWidth * visualWidth;
    const float groundSnapOffsetY = deathClip ? hitboxHeight * 0.12f : 0.0f;
    const float drawOffsetY = (introClip || deathClip)
        ? hitboxHeight - ((metrics.bodyTop + metrics.bodyHeight) / metrics.cellHeight * visualHeight)
        : -metrics.bodyTop / metrics.cellHeight * visualHeight;
    sprite->SetRenderScale(drawScaleX, drawScaleY);
    sprite->SetRenderOffset(drawOffsetX, drawOffsetY + groundSnapOffsetY);
    sprite->SetFlipX(flipRight);

    if (!boss.shieldEntity)
    {
        return;
    }

    auto* shieldSprite = boss.shieldEntity->GetComponent<SpriteRenderComponent>();
    const auto* shieldTransform = boss.shieldEntity->GetComponent<TransformComponent>();
    if (!shieldSprite || !shieldTransform)
    {
        return;
    }

    // Shield DDS shares the body canvas, so draw it from the body hitbox origin.
    // 待機と突進は盾を構えた見た目にするため、さらに内側へ寄せる。
    constexpr float kShieldBodyVisualPullX = 24.0f;
    constexpr float kShieldBodyGuardClipPullX = 48.0f;
    const bool guardClip = std::strcmp(clipName, "idle") == 0 || std::strcmp(clipName, "attack01") == 0;
    const float shieldBodyPullX = kShieldBodyVisualPullX + (guardClip ? kShieldBodyGuardClipPullX : 0.0f);
    constexpr float kShieldBaseCellWidth = 241.8f;
    constexpr float kShieldBaseCellHeight = 188.0f;
    constexpr float kShieldBaseBodyWidth = 169.0f;
    constexpr float kShieldBaseBodyHeight = 123.0f;
    const float shieldVisualWidth = hitboxWidth * (kShieldBaseCellWidth / kShieldBaseBodyWidth);
    const float shieldVisualHeight = hitboxHeight * (kShieldBaseCellHeight / kShieldBaseBodyHeight);
    const bool slamMotionClip = std::strcmp(clipName, "attack02") == 0;
    const float shieldMotionScale = slamMotionClip ? 1.3f : 1.0f;
    const float shieldScaleX = shieldVisualWidth * shieldMotionScale / std::max(1.0f, shieldTransform->width * shieldTransform->scale);
    const float shieldScaleY = shieldVisualHeight * shieldMotionScale / std::max(1.0f, shieldTransform->height * shieldTransform->scale);
    shieldSprite->SetRenderScale(shieldScaleX, shieldScaleY);
    if (boss.slamShieldVisualLocked && boss.state == ShieldBossState::JumpDescend)
    {
        // 叩きつけ落下中は、切り出した盾フレームを盾エンティティ座標へ追従させる。
        shieldSprite->SetRenderOffset(boss.slamShieldRenderOffsetX, boss.slamShieldRenderOffsetY);
    }
    else
    {
        shieldSprite->SetRenderOffset(
            transform->x + drawOffsetX - shieldTransform->x + (flipRight ? -shieldBodyPullX : shieldBodyPullX),
            transform->y + drawOffsetY - shieldTransform->y);
    }
    shieldSprite->SetFlipX(flipRight);
}

inline void UpdateShieldBossSpriteAnimation(Entity& entity, ShieldBossComponent& boss)
{
    const bool flipRight = boss.facing == ShieldBossFacing::Right;
    const char* clipName = ResolveShieldBossBodyClipName(boss);
    ApplyShieldBossVisualLayout(entity, boss, flipRight, clipName);
    if (boss.deathAnimationActive)
    {
        if (auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>())
        {
            animation->Play("death");
        }
        if (boss.shieldEntity)
        {
            if (auto* shieldTint = boss.shieldEntity->GetComponent<TintComponent>())
            {
                shieldTint->a = 0.0f;
            }
        }
        return;
    }

    if (boss.knockbackActive)
    {
        if (auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>())
        {
            animation->Play("knockback");
        }
        if (auto* shieldAnimation = boss.shieldEntity ? boss.shieldEntity->GetComponent<SpriteSheetAnimationComponent>() : nullptr)
        {
            shieldAnimation->Play("knockback");
        }
        return;
    }

    if (boss.appearAnimationActive)
    {
        if (boss.shieldEntity)
        {
            if (auto* shieldTint = boss.shieldEntity->GetComponent<TintComponent>())
            {
                shieldTint->a = 0.0f;
            }
        }
        if (auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>())
        {
            animation->Play("appear");
            if (animation->IsCurrentClipFinished() && !boss.introDropActive)
            {
                boss.appearAnimationActive = false;
                boss.appearAnimationFinished = true;
                animation->Play("move", true);
            }
        }
        return;
    }

    if (auto* sprite = entity.GetComponent<SpriteRenderComponent>())
    {
        // Boss1 sheets face left by default; mirror only when facing right.
        sprite->SetFlipX(flipRight);
    }
    if (auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>())
    {
        if (boss.roarAnimationActive)
        {
            if (animation->GetCurrentClipName() != "roar")
            {
                Logger::Info("Boss1 roar started");
            }
            animation->Play("roar");
            if (boss.shieldEntity)
            {
                if (auto* shieldTint = boss.shieldEntity->GetComponent<TintComponent>())
                {
                    shieldTint->a = 0.0f;
                }
            }
            if (animation->IsCurrentClipFinished())
            {
                Logger::Info("Boss1 roar finished");
                boss.roarAnimationActive = false;
            }
            return;
        }
        animation->Play(clipName);
    }

    if (!boss.shieldEntity)
    {
        return;
    }

    if (auto* shieldTint = boss.shieldEntity->GetComponent<TintComponent>())
    {
        if (boss.shieldEntity->GetComponent<SpriteSheetAnimationComponent>())
        {
            shieldTint->r = 1.0f;
            shieldTint->g = 1.0f;
            shieldTint->b = 1.0f;
            shieldTint->a = 1.0f;
        }
        else
        {
            shieldTint->r = 0.72f;
            shieldTint->g = 0.78f;
            shieldTint->b = 0.90f;
            shieldTint->a = 1.0f;
        }
    }

    if (auto* shieldSprite = boss.shieldEntity->GetComponent<SpriteRenderComponent>())
    {
        shieldSprite->SetFlipX(flipRight);
    }
    if (auto* shieldAnimation = boss.shieldEntity->GetComponent<SpriteSheetAnimationComponent>())
    {
        if (boss.state != ShieldBossState::JumpDescend &&
            boss.state != ShieldBossState::SlamPhase2)
        {
            shieldAnimation->SetPlaybackSpeed(1.0f);
        }
        shieldAnimation->Play(clipName);
    }
}

inline void ApplyBossShieldGuardTint(Entity* shieldEntity, TintComponent* shieldTint)
{
    if (!shieldTint)
    {
        return;
    }

    if (shieldEntity && shieldEntity->GetComponent<SpriteSheetAnimationComponent>())
    {
        // Sprite shields keep their authored colors; only the old white shield is tinted.
        shieldTint->r = 1.0f;
        shieldTint->g = 1.0f;
        shieldTint->b = 1.0f;
        shieldTint->a = 1.0f;
        return;
    }

    shieldTint->r = 0.72f;
    shieldTint->g = 0.78f;
    shieldTint->b = 0.90f;
    shieldTint->a = 1.0f;
}

inline const char* ToMidBoss2StateLabel(MidBoss2State state)
{
    switch (state)
    {
    case MidBoss2State::Idle: return "Idle";
    case MidBoss2State::SpearJump: return "SpearJump";
    case MidBoss2State::SpearThrow: return "SpearThrow";
    case MidBoss2State::SpearLanding: return "SpearLanding";
    case MidBoss2State::SpearCooldown: return "SpearCooldown";
    case MidBoss2State::BeamCharge: return "BeamCharge";
    case MidBoss2State::BeamFire: return "BeamFire";
    case MidBoss2State::BeamCooldown: return "BeamCooldown";
    case MidBoss2State::Damaged: return "Damaged";
    case MidBoss2State::Dead: return "Dead";
    default: return "Unknown";
    }
}

inline float NormalizeAngleRadians(float radians)
{
    constexpr float kTwoPi = 6.2831853072f;
    constexpr float kPi = 3.1415926536f;
    if (std::isnan(radians) || std::isinf(radians))
    {
        return 0.0f;
    }

    radians = std::fmod(radians, kTwoPi);
    if (radians > kPi)
    {
        radians -= kTwoPi;
    }
    else if (radians < -kPi)
    {
        radians += kTwoPi;
    }

    return radians;
}
}
