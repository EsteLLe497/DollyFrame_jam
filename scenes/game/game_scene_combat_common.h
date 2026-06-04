#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

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
    case ShieldBossState::Detect:
        return "rush_start";
    case ShieldBossState::Rush:
        return "rush_attack";
    case ShieldBossState::RushCooldown:
        return "rush_end";
    default:
        return "idle";
    }
}

inline void UpdateShieldBossSpriteAnimation(Entity& entity, ShieldBossComponent& boss)
{
    constexpr float kBoss1NormalVisualScale = 1.35f;
    constexpr float kBoss1RoarVisualScale = 1.35f;
    constexpr float kBoss1DeathVisualScale = 1.68f;
    constexpr float kBoss1DeathGroundOffsetY = 72.0f;

    if (boss.deathAnimationActive)
    {
        if (auto* sprite = entity.GetComponent<SpriteRenderComponent>())
        {
            if (const auto* transform = entity.GetComponent<TransformComponent>())
            {
                // Death frames have more visual padding, so push the image down and enlarge it.
                sprite->SetRenderScale(kBoss1DeathVisualScale, kBoss1DeathVisualScale);
                sprite->SetRenderOffset(
                    transform->width * (1.0f - kBoss1DeathVisualScale) * 0.5f,
                    transform->height * (1.0f - kBoss1DeathVisualScale) + kBoss1DeathGroundOffsetY);
            }
        }
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

    const bool flipRight = boss.facing == ShieldBossFacing::Right;
    if (boss.appearAnimationActive)
    {
        if (auto* sprite = entity.GetComponent<SpriteRenderComponent>())
        {
            if (const auto* transform = entity.GetComponent<TransformComponent>())
            {
                sprite->SetRenderScale(kBoss1NormalVisualScale, kBoss1NormalVisualScale);
                sprite->SetRenderOffset(
                    transform->width * (1.0f - kBoss1NormalVisualScale) * 0.5f,
                    transform->height * (1.0f - kBoss1NormalVisualScale));
            }
            sprite->SetFlipX(flipRight);
        }
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
            if (animation->IsCurrentClipFinished())
            {
                boss.appearAnimationActive = false;
                boss.appearAnimationFinished = true;
                animation->Play("idle", true);
                if (boss.shieldEntity)
                {
                    if (auto* shieldTint = boss.shieldEntity->GetComponent<TintComponent>())
                    {
                        shieldTint->a = 1.0f;
                    }
                }
            }
        }
        return;
    }

    const char* clipName = GetShieldBossRushClipName(boss.state);

    if (auto* sprite = entity.GetComponent<SpriteRenderComponent>())
    {
        if (const auto* transform = entity.GetComponent<TransformComponent>())
        {
            const float visualScale = boss.roarAnimationActive ? kBoss1RoarVisualScale : kBoss1NormalVisualScale;
            sprite->SetRenderScale(visualScale, visualScale);
            sprite->SetRenderOffset(
                transform->width * (1.0f - visualScale) * 0.5f,
                transform->height * (1.0f - visualScale));
        }
        // Boss1 sheets face left by default; mirror only when facing right.
        sprite->SetFlipX(flipRight);
    }
    if (auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>())
    {
        if (boss.roarAnimationActive)
        {
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
