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
inline bool HasTag(const Entity& entity, const char* value)
{
    const auto* tag = entity.GetComponent<TagComponent>();
    return tag && tag->tag == value;
}

inline bool IntersectsBounds(const TransformComponent& a, const TransformComponent& b)
{
    const float aRight = a.x + a.width * a.scale;
    const float aBottom = a.y + a.height * a.scale;
    const float bRight = b.x + b.width * b.scale;
    const float bBottom = b.y + b.height * b.scale;
    return a.x < bRight && aRight > b.x && a.y < bBottom && aBottom > b.y;
}

template <typename SnapToGroundFn, typename PlayEnemyGunFn, typename CheckPhotoBoxCollisionFn, typename IsSolidTileFn>
inline void UpdateEnemies(
    std::vector<std::unique_ptr<Entity>>& entities,
    int tileTexture,
    GameSceneFlowState& flow,
    const PhotoState& photo,
    const TransformComponent* playerTransform,
    SnapToGroundFn&& snapToGround,
    PlayEnemyGunFn&& playEnemyGun,
    CheckPhotoBoxCollisionFn&& checkPhotoBoxCollision,
    IsSolidTileFn&& isSolidTile)
{
    flow.enemyCount = 0;
    std::vector<std::unique_ptr<Entity>> newBullets;
    std::vector<std::unique_ptr<Entity>> newShields;

    for (const auto& entity : entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled())
        {
            continue;
        }

        ++flow.enemyCount;

        if (!playerTransform)
        {
            continue;
        }

        auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform)
        {
            continue;
        }

        if (enemy->GetArchetype() == EnemyArchetype::Walker)
        {
            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::fabs(dx);
            constexpr float kWalkerSpeed = 120.0f;
            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            constexpr float kWalkerAttackActiveSeconds = 0.18f;

            const bool inDetectRange = dist < enemy->detectRange && std::fabs(dy) < enemy->detectHeight;

            // 重力処理
            enemy->velocityY = std::min(kMaxFallSpeed, enemy->velocityY + kGravity * flow.lastDeltaTime);
            transform->y += enemy->velocityY * flow.lastDeltaTime;
            const bool onGround = snapToGround(*transform);
            if (onGround)
            {
                enemy->velocityY = 0.0f;
            }

            // 向き更新
            if (enemy->GetAIState() != EnemyComponent::AIState::Attack)
            {
                enemy->facing = dx > 0.0f
                    ? EnemyComponent::FacingDirection::Right
                    : EnemyComponent::FacingDirection::Left;
            }

            // 攻撃四角の残り時間を更新
            if (enemy->attackRectActive)
            {
                enemy->attackRectRemaining -= flow.lastDeltaTime;
                if (enemy->attackRectRemaining <= 0.0f)
                {
                    enemy->attackRectActive = false;
                }
            }

            switch (enemy->GetAIState())
            {
            case EnemyComponent::AIState::Idle:
                if (inDetectRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                break;
            case EnemyComponent::AIState::Chase:
                if (dist < enemy->attackRange)
                {
                    // 攻撃開始時に向きを固定して攻撃四角を生成
                    enemy->facing = dx > 0.0f
                        ? EnemyComponent::FacingDirection::Right
                        : EnemyComponent::FacingDirection::Left;

                    const float attackWidth = 48.0f;
                    const float attackHeight = 60.0f;
                    const float attackOffsetY = transform->height * transform->scale * -0.1f;

                    enemy->attackRectX = enemy->facing == EnemyComponent::FacingDirection::Right
                        ? transform->x + transform->width * transform->scale
                        : transform->x - attackWidth;
                    enemy->attackRectY = transform->y + attackOffsetY;
                    enemy->attackRectWidth = attackWidth;
                    enemy->attackRectHeight = attackHeight;
                    enemy->attackRectRemaining = kWalkerAttackActiveSeconds;
                    enemy->attackRectActive = true;

                    enemy->attackTimer = 0.0f;
                    enemy->SetAIState(EnemyComponent::AIState::Attack);
                }
                else if (!inDetectRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Idle);
                }
                else
                {
                    transform->x += (dx > 0.0f ? 1.0f : -1.0f) * kWalkerSpeed * flow.lastDeltaTime;
                    snapToGround(*transform);
                }
                break;
            case EnemyComponent::AIState::Attack:
                enemy->attackTimer += flow.lastDeltaTime;
                if (dist >= enemy->attackRange)
                {
                    enemy->attackRectActive = false;
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                else if (enemy->attackTimer >= enemy->attackCooldown)
                {
                    enemy->attackTimer = 0.0f;
                    enemy->attackRectActive = false;
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                break;
            }
        }

        else if (enemy->GetArchetype() == EnemyArchetype::Ranged)
        {
            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            enemy->velocityY = std::min(kMaxFallSpeed, enemy->velocityY + kGravity * flow.lastDeltaTime);
            transform->y += enemy->velocityY * flow.lastDeltaTime;
            const bool onGround = snapToGround(*transform);
            if (onGround)
            {
                enemy->velocityY = 0.0f;
            }

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            
            const bool inDetectRange = dist < enemy->detectRange && std::fabs(dy) < enemy->detectHeight;

      

            enemy->attackTimer += flow.lastDeltaTime;

            if (inDetectRange && enemy->attackTimer >= enemy->attackCooldown)
            {
                enemy->attackTimer = 0.0f;

                constexpr float kBulletSpeed = 300.0f;
                
                const float velX = (dx > 0.0f ? 1.0f : -1.0f) * kBulletSpeed;
                const float velY = 0.0f;

                auto bullet = std::make_unique<Entity>();
                bullet->AddComponent<TagComponent>(kTagBullet);
                bullet->AddComponent<TransformComponent>(
                    transform->x + 24.0f,
                    transform->y + 24.0f,
                    48.0f, 24.0f); 
                bullet->AddComponent<TintComponent>(1.0f, 0.9f, 0.2f, 1.0f);
                bullet->AddComponent<SpriteRenderComponent>(tileTexture);
                bullet->AddComponent<ProjectileComponent>(velX, velY, 1);
                playEnemyGun(*entity);
                newBullets.push_back(std::move(bullet));
            }
        }

        else if (enemy->GetArchetype() == EnemyArchetype::Ghost)
        {
            auto* ghost = entity->GetComponent<GhostComponent>();
            if (!ghost) continue;

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            // 検知範囲内なら追従（地形無視で直進）
            if (dist < ghost->detectRange)
            {
                const float length = std::max(1.0f, dist);
                transform->x += (dx / length) * ghost->moveSpeed * flow.lastDeltaTime;
                transform->y += (dy / length) * ghost->moveSpeed * flow.lastDeltaTime;
            }
        }

        else if (enemy->GetArchetype() == EnemyArchetype::BlasterRobot)
        {
            auto* blaster = entity->GetComponent<BlasterRobotComponent>();
            if (!blaster) continue;

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            blaster->facingRight = dx > 0.0f;

            blaster->cooldownTimer += flow.lastDeltaTime;
            blaster->burstTimer += flow.lastDeltaTime;

            // 検知範囲内でクールダウン終了したら連射開始
            if (dist < blaster->detectRange && blaster->shotsRemaining == 0
                && blaster->cooldownTimer >= blaster->cooldown)
            {
                blaster->shotsRemaining = blaster->burstCount;
                blaster->burstTimer = 0.0f;
                blaster->cooldownTimer = 0.0f;
            }

            // 連射処理
            if (blaster->shotsRemaining > 0 && blaster->burstTimer >= blaster->burstInterval)
            {
                blaster->burstTimer = 0.0f;
                blaster->shotsRemaining--;

                constexpr float kBulletSpeed = 350.0f;
                const float length = std::max(1.0f, dist);
                const float velX = (dx / length) * kBulletSpeed;
                const float velY = (dy / length) * kBulletSpeed;

                auto bullet = std::make_unique<Entity>();
                bullet->AddComponent<TagComponent>("Bullet");
                bullet->AddComponent<TransformComponent>(
                    transform->x + transform->width * transform->scale * 0.5f - 12.0f,
                    transform->y + transform->height * transform->scale * 0.5f - 12.0f,
                    24.0f, 24.0f);
                bullet->AddComponent<TintComponent>(0.2f, 1.0f, 0.4f, 1.0f);
                bullet->AddComponent<SpriteRenderComponent>(tileTexture);
                auto& proj = bullet->AddComponent<ProjectileComponent>(velX, velY, 1, ProjectileComponent::Owner::BlasterRobot);
                proj.pierceRemaining = 2;
                proj.maxEnemyHits = 2;
                proj.sourceEntity = entity.get();
                newBullets.push_back(std::move(bullet));
            }
        }

        else if (enemy->GetArchetype() == EnemyArchetype::ShieldBoss)
        {
            auto* boss = entity->GetComponent<ShieldBossComponent>();
            if (!boss) continue;

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const bool inDetectRange = std::fabs(dx) < boss->detectRange
                && std::fabs(dy) < boss->detectHeight;

            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            constexpr float kTileSize = 48.0f;
            const float bossWidth = transform->width * transform->scale;
            const float bossHeight = transform->height * transform->scale;

            float& bossVelocityY = enemy->velocityY;
            float bossVelocityX = 0.0f;

            ShieldComponent* shieldComp = boss->shieldEntity
                ? boss->shieldEntity->GetComponent<ShieldComponent>()
                : nullptr;
            TransformComponent* shieldTransform = boss->shieldEntity
                ? boss->shieldEntity->GetComponent<TransformComponent>()
                : nullptr;
            TintComponent* shieldTint = boss->shieldEntity
                ? boss->shieldEntity->GetComponent<TintComponent>()
                : nullptr;

            // Shield follow while attached.
            if (shieldComp && shieldTransform && shieldComp->attached)
            {
                const float shieldW = shieldTransform->width * shieldTransform->scale;
                const float shieldH = shieldTransform->height * shieldTransform->scale;
                if (shieldTint)
                {
                    shieldTint->r = 0.72f;
                    shieldTint->g = 0.78f;
                    shieldTint->b = 0.90f;
                    shieldTint->a = 1.0f;
                }

                if (boss->state == ShieldBossState::JumpAscend)
                {
                    // During ascent the shield stays above the boss.
                    shieldTransform->x = boss->facing == ShieldBossFacing::Right
                        ? transform->x + bossWidth
                        : transform->x - shieldW;
                    shieldTransform->y = transform->y - shieldH;
                }
                else if (boss->state == ShieldBossState::AirHover)
                {
                    shieldTransform->x = boss->hoverShieldX;
                    shieldTransform->y = boss->hoverShieldY;
                }
                else if (boss->state == ShieldBossState::Rush)
                {
                    // 突進中：ボスの正面
                    shieldTransform->x = boss->facing == ShieldBossFacing::Right
                        ? transform->x + bossWidth
                        : transform->x - shieldW;
                    shieldTransform->y = transform->y;
                }
                else
                {
                    // 通常：ボスの正面
                    shieldTransform->x = boss->facing == ShieldBossFacing::Right
                        ? transform->x + bossWidth
                        : transform->x - shieldW;
                    shieldTransform->y = transform->y;
                    shieldTransform->rotation = 0.0f;
                }
            }

            // 重力（待機系）
            if (boss->state == ShieldBossState::Idle ||
                boss->state == ShieldBossState::Detect ||
                boss->state == ShieldBossState::RushCooldown ||
                boss->state == ShieldBossState::Cooldown)
            {
                bossVelocityY = std::min(kMaxFallSpeed, bossVelocityY + kGravity * flow.lastDeltaTime);
                transform->y += bossVelocityY * flow.lastDeltaTime;
                const bool onGround = snapToGround(*transform);
                if (onGround) bossVelocityY = 0.0f;
            }

            // 突進
            bool rushEndedThisFrame = false;
            if (boss->state == ShieldBossState::Rush)
            {
                const float dir = boss->facing == ShieldBossFacing::Right ? 1.0f : -1.0f;
                transform->x += dir * boss->rushSpeed * flow.lastDeltaTime;

                bool hitPlayer = false;
                if (playerTransform)
                {
                    hitPlayer = IntersectsBounds(*transform, *playerTransform)
                        || (shieldTransform && IntersectsBounds(*shieldTransform, *playerTransform));
                }

                const int rowTop = static_cast<int>((transform->y + 4.0f) / kTileSize);
                const int rowBottom = static_cast<int>((transform->y + bossHeight - 4.0f) / kTileSize);
                bool hitWall = false;
                if (boss->facing == ShieldBossFacing::Right)
                {
                    const int column = static_cast<int>((transform->x + bossWidth) / kTileSize);
                    for (int row = rowTop; row <= rowBottom; ++row)
                    {
                        if (isSolidTile(column, row))
                        {
                            transform->x = static_cast<float>(column) * kTileSize - bossWidth;
                            hitWall = true;
                            break;
                        }
                    }
                }
                else
                {
                    const int column = static_cast<int>(transform->x / kTileSize);
                    for (int row = rowTop; row <= rowBottom; ++row)
                    {
                        if (isSolidTile(column, row))
                        {
                            transform->x = static_cast<float>(column + 1) * kTileSize;
                            hitWall = true;
                            break;
                        }
                    }
                }

                if (hitPlayer)
                {
                    boss->rushCount++;
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                    transform->x -= dir * (kTileSize * 3.0f);
                    rushEndedThisFrame = true;
                }
                else if (hitWall || checkPhotoBoxCollision(*transform, *entity))
                {
                    boss->rushCount++;
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                    rushEndedThisFrame = true;
                }

                if (rushEndedThisFrame) continue;
            }

            // ジャンプ上昇
            if (boss->state == ShieldBossState::JumpAscend)
            {
                const float jumpHeightPx = boss->jumpHeight * kTileSize;
                const float ascendProgress = std::min(1.0f, boss->stateTimer / boss->jumpAscendDuration);
                const float easedProgress = std::sin(ascendProgress * 3.1415926f * 0.5f);
                transform->y = boss->targetY - jumpHeightPx * easedProgress;

                const float targetDx = boss->targetX - transform->x;
                if (std::fabs(targetDx) > 2.0f)
                {
                    transform->x += targetDx * std::min(1.0f, flow.lastDeltaTime * 6.0f);
                }

                boss->stateTimer += flow.lastDeltaTime;
                if (boss->stateTimer >= boss->jumpAscendDuration)
                {
                    if (shieldComp && shieldTransform)
                    {
                        boss->hoverShieldX = shieldTransform->x;
                        boss->hoverShieldY = boss->targetY - jumpHeightPx;
                        shieldComp->attached = false;
                        shieldComp->gravityEnabled = false;
                        shieldComp->velocityX = 0.0f;
                        shieldComp->velocityY = 0.0f;
                        shieldComp->rotationSpeed = 0.0f;
                        shieldTransform->x = boss->hoverShieldX;
                        shieldTransform->y = boss->hoverShieldY;
                    }
                    boss->state = ShieldBossState::AirHover;
                    boss->stateTimer = 0.0f;
                }
                continue;
            }

            // Air hover.
            if (boss->state == ShieldBossState::AirHover)
            {
                boss->stateTimer += flow.lastDeltaTime;
                if (boss->stateTimer >= boss->airHoverDuration)
                {
                    boss->state = ShieldBossState::JumpDescend;
                    boss->stateTimer = 0.0f;
                }
                continue;
            }
            if (boss->state == ShieldBossState::JumpDescend)
            {
                if (!(shieldComp && shieldTransform))
                {
                    boss->state = ShieldBossState::Cooldown;
                    boss->stateTimer = 0.0f;
                    continue;
                }

                shieldTransform->y += boss->descendSpeed * flow.lastDeltaTime;

                const float shieldW = shieldTransform->width * shieldTransform->scale;
                const float shieldH = shieldTransform->height * shieldTransform->scale;
                const float feetY = shieldTransform->y + shieldH;
                const int bottomRow = static_cast<int>(feetY / kTileSize);
                const int leftCol = static_cast<int>((shieldTransform->x + 4.0f) / kTileSize);
                const int rightCol = static_cast<int>((shieldTransform->x + shieldW - 4.0f) / kTileSize);

                bool onGround = false;
                for (int col = leftCol; col <= rightCol; ++col)
                {
                    if (isSolidTile(col, bottomRow))
                    {
                        shieldTransform->y = static_cast<float>(bottomRow) * kTileSize - shieldH;
                        onGround = true;
                        break;
                    }
                }

                if (onGround)
                {
                    bossVelocityY = 0.0f;
                    transform->rotation = 0.0f;
                    transform->y = boss->targetY;

                    // Switch the shield to its slam hitbox once it reaches the ground.
                    const float slamW = kTileSize * 5.0f;
                    const float slamH = kTileSize * 1.0f;
                    shieldComp->attached = false;
                    shieldComp->attackType = ShieldAttackType::Slam;
                    shieldComp->contactDamage = 2;
                    shieldComp->gravityEnabled = false;
                    shieldComp->velocityX = 0.0f;
                    shieldComp->velocityY = 0.0f;
                    shieldComp->rotationSpeed = 0.0f;
                    shieldTransform->width = slamW;
                    shieldTransform->height = slamH;
                    shieldTransform->x -= (slamW - shieldW) * 0.5f;
                    if (shieldTint)
                    {
                        shieldTint->r = 1.0f;
                        shieldTint->g = 0.45f;
                        shieldTint->b = 0.12f;
                        shieldTint->a = 1.0f;
                    }

                    boss->state = ShieldBossState::SlamPhase1;
                    boss->stateTimer = 0.0f;
                }
                continue;
            }

            boss->stateTimer += flow.lastDeltaTime;

            switch (boss->state)
            {
            case ShieldBossState::Idle:
                if (inDetectRange)
                {
                    boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                    boss->state = ShieldBossState::Detect;
                    boss->stateTimer = 0.0f;
                    boss->rushCount = 0;
                }
                break;

            case ShieldBossState::Detect:
                boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                if (boss->stateTimer >= 0.5f)
                {
                    boss->state = ShieldBossState::Rush;
                    boss->stateTimer = 0.0f;
                }
                break;

            case ShieldBossState::Rush:
                if (boss->stateTimer >= boss->rushDuration)
                {
                    boss->rushCount++;
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                }
                break;

            case ShieldBossState::RushCooldown:
                if (boss->stateTimer >= boss->rushCooldown)
                {
                    if (boss->rushCount < boss->rushCountMax)
                    {
                        boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                        boss->state = ShieldBossState::Rush;
                        boss->stateTimer = 0.0f;
                        if (shieldComp)
                        {
                            shieldComp->attached = true;
                            shieldComp->attackType = ShieldAttackType::None;
                            shieldComp->velocityX = 0.0f;
                            shieldComp->velocityY = 0.0f;
                            shieldComp->rotationSpeed = 0.0f;
                            shieldComp->gravityEnabled = false;
                            shieldComp->baseAttackElapsed = 0.0f;
                            shieldComp->contactDamage = 1;
                            if (shieldTransform)
                            {
                                shieldTransform->width = kTileSize * 1.0f;
                                shieldTransform->height = kTileSize * 3.0f;
                                shieldTransform->rotation = 0.0f;
                            }
                        }
                    }
                    else
                    {
                        boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                        boss->rushCount = 0;
                        const float playerCenterX = playerTransform->x
                            + playerTransform->width * playerTransform->scale * 0.5f;
                        const float jumpShieldWidth = kTileSize * 3.0f;
                        boss->targetX = boss->facing == ShieldBossFacing::Right
                            ? playerCenterX - bossWidth - jumpShieldWidth * 0.5f
                            : playerCenterX + jumpShieldWidth * 0.5f;
                        boss->targetY = transform->y;
                        bossVelocityY = 0.0f;
                        bossVelocityX = 0.0f;
                        boss->state = ShieldBossState::JumpAscend;
                        boss->stateTimer = 0.0f;

                        if (shieldComp && shieldTransform)
                        {
                            shieldComp->attached = true;
                            shieldComp->attackType = ShieldAttackType::None;
                            shieldComp->velocityX = 0.0f;
                            shieldComp->velocityY = 0.0f;
                            shieldComp->rotationSpeed = 0.0f;
                            shieldComp->gravityEnabled = false;
                            shieldComp->baseAttackElapsed = 0.0f;
                            shieldComp->contactDamage = 1;
                            shieldTransform->width = kTileSize * 3.0f;
                            shieldTransform->height = kTileSize * 1.0f;
                            shieldTransform->rotation = 0.0f;
                        }
                    }
                }
                break;
            case ShieldBossState::SlamPhase1:
                if (boss->stateTimer >= boss->slamPhase1Duration)
                {
                    // 衝撃波エンティティを生成（ShieldShockwaveComponent）
                    if (shieldComp && shieldTransform)
                    {
                        const float shockW = kTileSize * 8.0f;
                        const float shockH = kTileSize * 3.0f;
                        auto shockwave = std::make_unique<Entity>();
                        shockwave->AddComponent<TagComponent>("BossShockwave");
                        shockwave->AddComponent<TransformComponent>(
                            shieldTransform->x + shieldTransform->width * shieldTransform->scale * 0.5f - shockW * 0.5f,
                            shieldTransform->y + shieldTransform->height * shieldTransform->scale - shockH,
                            shockW,
                            shockH);
                        shockwave->AddComponent<TintComponent>(0.18f, 0.95f, 1.0f, 0.75f);
                        auto& shockComp = shockwave->AddComponent<ShieldShockwaveComponent>();
                        shockComp.ownerBoss = entity.get();
                        shockComp.damage = 1;
                        shockComp.lifetime = 0.25f;
                        newShields.push_back(std::move(shockwave));

                        // 盾本体の判定を無効化（衝撃波に引き継ぐ）
                        shieldComp->contactDamage = 0;
                    }
                    boss->state = ShieldBossState::SlamPhase2;
                    boss->stateTimer = 0.0f;
                }
                break;

            case ShieldBossState::SlamPhase2:
                if (boss->stateTimer >= boss->slamPhase2Duration + 0.3f)
                {
                    boss->state = ShieldBossState::Cooldown;
                    boss->stateTimer = 0.0f;
                    // 盾を通常サイズに戻して再装着
                    if (shieldComp)
                    {
                        shieldComp->attached = true;
                        shieldComp->attackType = ShieldAttackType::None;
                        shieldComp->contactDamage = 1;
                        shieldComp->gravityEnabled = false;
                        shieldComp->velocityX = 0.0f;
                        shieldComp->velocityY = 0.0f;
                        shieldComp->rotationSpeed = 0.0f;
                        if (shieldTransform)
                        {
                            shieldTransform->width = kTileSize * 1.0f;
                            shieldTransform->height = kTileSize * 3.0f;
                            shieldTransform->rotation = 0.0f;
                        }
                        if (shieldTint)
                        {
                            shieldTint->r = 0.72f;
                            shieldTint->g = 0.78f;
                            shieldTint->b = 0.90f;
                            shieldTint->a = 1.0f;
                        }
                    }
                }
                break;

            case ShieldBossState::Cooldown:
                if (boss->stateTimer >= boss->slamCooldown)
                {
                    boss->state = ShieldBossState::Idle;
                    boss->stateTimer = 0.0f;
                }
                break;
            }
        }
    }

    for (auto& bullet : newBullets)
    {
        entities.push_back(std::move(bullet));
    }

    for (auto& shield : newShields)  
    {
        entities.push_back(std::move(shield));
    }

    flow.goalUnlocked = photo.groups.hasSpawnedCopy || flow.goalUnlockedBySwitch;
}

template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename HandleEnemyDamageFn, typename IsSolidTileFn>
void UpdateBullets(
    std::vector<std::unique_ptr<Entity>>& entities,
    float mapWidth,
    float mapHeight,
    float deltaTime,
    Entity* player,
    IntersectsEntityFn&& intersectsEntity,
    HandlePlayerDamageFn&& handlePlayerDamage
    , HandleEnemyDamageFn&& handleEnemyDamage
    , IsSolidTileFn&& isSolidTile) 
{
    entities.erase(
        std::remove_if(
            entities.begin(),
            entities.end(),
            [&](const std::unique_ptr<Entity>& entity) -> bool
            {
                if (!entity || !HasTag(*entity, kTagBullet))
                {
                    return false;
                }

                auto* transform = entity->GetComponent<TransformComponent>();
                auto* projectile = entity->GetComponent<ProjectileComponent>();
                if (!transform || !projectile)
                {
                    return false;
                }

                transform->x += projectile->GetVelocityX() * deltaTime;
                transform->y += projectile->GetVelocityY() * deltaTime;

                if (isSolidTile(transform->x, transform->y) ||
                    isSolidTile(transform->x + transform->width, transform->y) ||
                    isSolidTile(transform->x, transform->y + transform->height) ||
                    isSolidTile(transform->x + transform->width, transform->y + transform->height))
                {
                    return true;
                }

                if (projectile->GetOwner() == ProjectileComponent::Owner::Enemy &&
                    player && intersectsEntity(*player, *entity))
                {
                    handlePlayerDamage(*player, entity.get(), "GameScene player damaged by bullet");
                    return true;
                }

                if (projectile->GetOwner() == ProjectileComponent::Owner::Photo)
                {
                    for (const auto& target : entities)
                    {
                        if (!target || target.get() == entity.get() || !HasTag(*target, kTagEnemy))
                        {
                            continue;
                        }

                        if (!intersectsEntity(*target, *entity))
                        {
                            continue;
                        }

                        handleEnemyDamage(*target, entity.get(), projectile->GetDamage(), "Photo bullet hit enemy");
                        return true;
                    }
                }

                if (projectile->GetOwner() == ProjectileComponent::Owner::BlasterRobot)
                {
                    if (player && intersectsEntity(*player, *entity))
                    {
                        handlePlayerDamage(*player, entity.get(), "GameScene player damaged by blaster");
                        return true;
                    }

                    bool hitEnemy = false;
                    for (const auto& target : entities)
                    {
                        if (!target || target.get() == entity.get() || !HasTag(*target, kTagEnemy))
                        {
                            continue;
                        }
                        if (target.get() == projectile->sourceEntity)
                        {
                            continue;
                        }
                        auto* targetEnemy = target->GetComponent<EnemyComponent>();
                        if (!targetEnemy || !targetEnemy->IsEnabled()) continue;
                        if (!intersectsEntity(*target, *entity)) continue;

                        handleEnemyDamage(*target, entity.get(), projectile->GetDamage(), "Blaster bullet hit enemy");
                        projectile->pierceRemaining--;
                        hitEnemy = true;

                        if (projectile->pierceRemaining <= 0)
                        {
                            return true;
                        }
                        break;
                    }
                }
                
                return transform->x < 0.0f
                    || transform->x > mapWidth
                    || transform->y < 0.0f
                    || transform->y > mapHeight;
            }),
        entities.end());
}
}
