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
                    const float attackOffsetY = transform->height * transform->scale * -3.3f;

                    enemy->attackRectX = enemy->facing == EnemyComponent::FacingDirection::Right
                        ? transform->x + transform->width * transform->scale
                        : transform->x - attackWidth;
                    enemy->attackRectY = transform->y + attackOffsetY;
                    enemy->attackRectWidth = attackWidth;
                    enemy->attackRectHeight = attackHeight;
                    enemy->attackRectRemaining = enemy->attackCooldown;
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
        else if (enemy->GetArchetype() == EnemyArchetype::ShieldBoss)
        {
            auto* boss = entity->GetComponent<ShieldBossComponent>();
            if (!boss) continue;

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const bool inDetectRange = std::fabs(dx) < boss->detectRange && std::fabs(dy) < boss->detectHeight;

            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            constexpr float kTileSize = 48.0f;

            // 重力処理（ジャンプ中のみ）
            if (boss->state == ShieldBossState::JumpAscend ||
                boss->state == ShieldBossState::JumpDescend)
            {
                boss->velocityY = std::min(kMaxFallSpeed, boss->velocityY + kGravity * flow.lastDeltaTime);
                transform->y += boss->velocityY * flow.lastDeltaTime;

                const float targetDx = boss->targetX - transform->x;
                if (std::fabs(targetDx) > 4.0f)
                {
                    transform->x += (targetDx > 0.0f ? 1.0f : -1.0f) * 400.0f * flow.lastDeltaTime;
                }

                if (boss->state == ShieldBossState::JumpAscend && boss->velocityY >= 0.0f)
                {
                    boss->state = ShieldBossState::JumpDescend;
                }

                const float targetRotation = boss->facing == ShieldBossFacing::Right
                    ? -0.8f  
                    : 0.8f;
                transform->rotation += (targetRotation - transform->rotation) * flow.lastDeltaTime * 5.0f;
            }
            else if (boss->state == ShieldBossState::Rush)
            {
                const float dir = boss->facing == ShieldBossFacing::Right ? 1.0f : -1.0f;
                transform->x += dir * boss->rushSpeed * flow.lastDeltaTime;

                // 壁チェック
                const float bossWidth = transform->width * transform->scale;
                const float bossHeight = transform->height * transform->scale;
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

                if (hitWall)
                {
                    boss->attackRectActive = false;
                    boss->rushCount++;
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                }

                // 攻撃判定更新
                const float rectW = kTileSize * 0.8f;
                const float rectH = kTileSize * 3.0f;
                boss->attackRectX = boss->facing == ShieldBossFacing::Right
                    ? transform->x + bossWidth
                    : transform->x - rectW;
                boss->attackRectY = transform->y;
                boss->attackRectWidth = rectW;
                boss->attackRectHeight = rectH;
                boss->attackRectActive = !hitWall;

                if (!hitWall && checkPhotoBoxCollision(*transform, *entity))
                {
                    boss->attackRectActive = false;
                    boss->rushCount++;
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                }
            }
            else if (
                boss->state == ShieldBossState::Idle ||
                boss->state == ShieldBossState::Detect ||
                boss->state == ShieldBossState::RushCooldown ||
                boss->state == ShieldBossState::Cooldown)
            {
                // 待機系ステートのみ地面スナップ
                boss->velocityY = std::min(kMaxFallSpeed, boss->velocityY + kGravity * flow.lastDeltaTime);
                transform->y += boss->velocityY * flow.lastDeltaTime;
                const bool onGround = snapToGround(*transform);
                if (onGround) boss->velocityY = 0.0f;
            }

            boss->stateTimer += flow.lastDeltaTime;

            switch (boss->state)
            {
            case ShieldBossState::Idle:
                boss->attackRectActive = false;
                if (inDetectRange)
                {
                    boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                    boss->state = ShieldBossState::Detect;
                    boss->stateTimer = 0.0f;
                    boss->rushCount = 0;
                }
                break;

            case ShieldBossState::Detect:
                // 検知後すぐ突進へ
                boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                if (boss->stateTimer >= 0.5f)
                {
                    boss->state = ShieldBossState::Rush;
                    boss->stateTimer = 0.0f;
                    boss->hitEntities.clear();
                }
                break;

            case ShieldBossState::Rush:
                // 突進終了条件：時間切れ
                if (boss->stateTimer >= boss->rushDuration)
                {
                    boss->attackRectActive = false;
                    boss->rushCount++;
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                }
                break;

            case ShieldBossState::RushCooldown:
                boss->attackRectActive = false;
                if (boss->stateTimer >= boss->rushCooldown)
                {
                    if (boss->rushCount < boss->rushCountMax)
                    {
                        // 次の突進へ
                        boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                        boss->state = ShieldBossState::Rush;
                        boss->stateTimer = 0.0f;
                        boss->hitEntities.clear();
                    }
                    else
                    {
                        // ジャンプ攻撃へ
                        boss->rushCount = 0;
                        boss->targetX = playerTransform->x
                            + playerTransform->width * playerTransform->scale * 0.5f
                            - transform->width * transform->scale * 0.5f;
                        const float jumpHeightPx = boss->jumpHeight * kTileSize;
                        boss->velocityY = -std::sqrt(2.0f * kGravity * jumpHeightPx);
                        boss->velocityX = 0.0f;
                        boss->state = ShieldBossState::JumpAscend;
                        boss->stateTimer = 0.0f;
                    }
                }
                break;

            case ShieldBossState::JumpAscend:
            case ShieldBossState::JumpDescend:
            {
                // 上昇中は着地判定しない
                if (boss->velocityY < 0.0f) break;

                const bool onGround = snapToGround(*transform);
                if (onGround && boss->stateTimer > 0.3f)
                {
                    boss->velocityY = 0.0f;

                    const float slamW = kTileSize * 4.0f;
                    const float slamH = kTileSize * 1.0f;
                    boss->attackRectX = transform->x + transform->width * transform->scale * 0.5f - slamW * 0.5f;
                    boss->attackRectY = transform->y + transform->height * transform->scale - slamH;
                    boss->attackRectWidth = slamW;
                    boss->attackRectHeight = slamH;
                    boss->attackRectDamage = boss->slamDamage1;
                    boss->attackRectActive = true;
                    boss->hitEntities.clear();

                    boss->state = ShieldBossState::SlamPhase1;
                    boss->stateTimer = 0.0f;
                    transform->rotation = 0.0f;
                }
                break;
            }

            case ShieldBossState::SlamPhase1:
                if (boss->stateTimer >= boss->slamPhase1Duration)
                {
                    // 判定①消去→判定②生成
                    const float slamW = kTileSize * 7.0f;
                    const float slamH = kTileSize * 1.0f;
                    boss->attackRectX = transform->x + transform->width * transform->scale * 0.5f - slamW * 0.5f;
                    boss->attackRectY = transform->y + transform->height * transform->scale - slamH;
                    boss->attackRectWidth = slamW;
                    boss->attackRectHeight = slamH;
                    boss->attackRectDamage = boss->slamDamage2;
                    boss->attackRectActive = true;
                    boss->hitEntities.clear();

                    boss->state = ShieldBossState::SlamPhase2;
                    boss->stateTimer = 0.0f;
                }
                break;

            case ShieldBossState::SlamPhase2:
                if (boss->stateTimer >= boss->slamPhase2Duration)
                {
                    boss->attackRectActive = false;
                    boss->state = ShieldBossState::Cooldown;
                    boss->stateTimer = 0.0f;
                }
                break;

            case ShieldBossState::Cooldown:
                boss->attackRectActive = false;
                if (boss->stateTimer >= boss->slamCooldown)
                {
                    boss->state = ShieldBossState::Detect;
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

                return transform->x < 0.0f
                    || transform->x > mapWidth
                    || transform->y < 0.0f
                    || transform->y > mapHeight;
            }),
        entities.end());
}
}
