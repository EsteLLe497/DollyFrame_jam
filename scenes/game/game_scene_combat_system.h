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

template <typename SnapToGroundFn, typename PlayEnemyGunFn, typename CheckPhotoBoxCollisionFn, typename IsSolidTileFn>
inline void UpdateEnemies(
    std::vector<std::unique_ptr<Entity>>& entities,
    int tileTexture,
    float mapWidth,
    float mapHeight,
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
            constexpr float kWalkerAttackActiveSeconds = 0.18f;

            const bool inDetectRange = dist < enemy->detectRange && std::fabs(dy) < enemy->detectHeight;

            // �d�͏���
            enemy->velocityY = std::min(kMaxFallSpeed, enemy->velocityY + kGravity * flow.lastDeltaTime);
            transform->y += enemy->velocityY * flow.lastDeltaTime;
            const bool onGround = snapToGround(*transform);
            if (onGround)
            {
                enemy->velocityY = 0.0f;
            }

            // �����X�V
            if (enemy->GetAIState() != EnemyComponent::AIState::Attack)
            {
                enemy->facing = dx > 0.0f
                    ? EnemyComponent::FacingDirection::Right
                    : EnemyComponent::FacingDirection::Left;
            }

            // �U���l�p�̎c�莞�Ԃ�X�V
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
                    // �U���J�n���Ɍ�����Œ肵�čU���l�p�𐶐�
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

            // ���m�͈͓�Ȃ�Ǐ]�i�n�`�����Œ��i�j
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

            // ���m�͈͓�ŃN�[���_�E���I��������A�ˊJ�n
            if (dist < blaster->detectRange && blaster->shotsRemaining == 0
                && blaster->cooldownTimer >= blaster->cooldown)
            {
                blaster->shotsRemaining = blaster->burstCount;
                blaster->burstTimer = 0.0f;
                blaster->cooldownTimer = 0.0f;
            }

            // �A�ˏ���
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
            const bool inDetectRange = std::fabs(dx) < boss->detectRange && std::fabs(dy) < boss->detectHeight;

            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            constexpr float kTileSize = 48.0f;

            // �d�͏����i�W�����v���̂݁j
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

                // �ǃ`�F�b�N
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

                // �U������X�V
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
                // �ҋ@�n�X�e�[�g�̂ݒn�ʃX�i�b�v
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
                // ���m�シ���ːi��
                boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                if (boss->stateTimer >= 0.5f)
                {
                    boss->state = ShieldBossState::Rush;
                    boss->stateTimer = 0.0f;
                    boss->hitEntities.clear();
                }
                break;

            case ShieldBossState::Rush:
                // �ːi�I������F���Ԑ؂�
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
                        // ���̓ːi��
                        boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                        boss->state = ShieldBossState::Rush;
                        boss->stateTimer = 0.0f;
                        boss->hitEntities.clear();
                    }
                    else
                    {
                        // �W�����v�U����
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
                // �㏸���͒��n���肵�Ȃ�
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
                    // ����@����������A����
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

        else if (enemy->GetArchetype() == EnemyArchetype::MidBoss2)
        {
            auto* boss = entity->GetComponent<MidBoss2Component>();
            if (!boss) continue;

            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            constexpr float kTileSize = 48.0f;
            constexpr float kHoverMoveSpeed = 360.0f;
            constexpr float kLandingMoveSpeed = 320.0f;
            constexpr float kSpearSpeed = 640.0f;
            constexpr float kBeamFireDuration = 2.2f;
            constexpr float kBeamFireShakeSeconds = 0.16f;
            constexpr float kBeamFireShakeAmplitude = 24.0f;

            if (!boss->initializedHome)
            {
                boss->homeX = 0.0f;
                boss->homeY = transform->y;
                boss->initializedHome = true;
            }

            const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
            const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
            const float bossCenterX = transform->x + transform->width * transform->scale * 0.5f;
            const float bossCenterY = transform->y + transform->height * transform->scale * 0.5f;
            const float dx = playerCenterX - bossCenterX;
            const float dy = playerCenterY - bossCenterY;
            boss->facingRight = dx >= 0.0f;
            const bool visualFacingRight = boss->facingRight;
            const bool beamFacingRight = boss->facingRight;
            if (auto* bossSprite = entity->GetComponent<SpriteRenderComponent>())
            {
                bossSprite->SetFlipX(visualFacingRight);
            }
            boss->attackFlowStep = boss->state == MidBoss2State::BeamCharge || boss->state == MidBoss2State::BeamFire || boss->state == MidBoss2State::BeamCooldown ? 2 : 1;
            boss->captureWindowActive = boss->state == MidBoss2State::BeamFire;
            boss->stateTimer += flow.lastDeltaTime;
            boss->cooldownRemaining = 0.0f;

            auto ensureBeamEntities = [&]()
            {
                if (boss->beamEntitiesSpawned)
                {
                    return;
                }

                auto turretEntity = std::make_unique<Entity>();
                boss->beamTurretEntity = turretEntity.get();
                boss->beamTurretEntity->AddComponent<TagComponent>("LaserTurret");
                boss->beamTurretEntity->AddComponent<TransformComponent>(-10000.0f, -10000.0f, kTileSize, boss->params.beamHeightGrid * kTileSize);
                boss->beamTurretEntity->AddComponent<TintComponent>(1.0f, 0.55f, 0.20f, 0.0f);
                boss->beamTurretEntity->AddComponent<SpriteRenderComponent>(tileTexture);
                boss->beamTurretEntity->AddComponent<BossBeamCaptureComponent>();
                auto& turret = boss->beamTurretEntity->AddComponent<LaserTurretComponent>(
                    boss->params.beamHeightGrid * kTileSize,
                    boss->params.beamDamagePerSecond);
                turret.fireToLeft = false;
                turret.active = false;
                newBullets.push_back(std::move(turretEntity));

                auto beamEntity = std::make_unique<Entity>();
                boss->beamEntity = beamEntity.get();
                boss->beamEntity->AddComponent<TagComponent>("LaserBeam");
                boss->beamEntity->AddComponent<TransformComponent>(-10000.0f, -10000.0f, 0.0f, boss->params.beamHeightGrid * kTileSize);
                boss->beamEntity->AddComponent<TintComponent>(0.48f, 0.78f, 1.0f, 0.0f);
                boss->beamEntity->AddComponent<SpriteRenderComponent>(tileTexture);
                boss->beamEntity->AddComponent<LaserBeamComponent>();
                if (auto* turretEntity = boss->beamTurretEntity)
                {
                    if (auto* turret = turretEntity->GetComponent<LaserTurretComponent>())
                    {
                        turret->beamEntity = boss->beamEntity;
                    }
                }
                newBullets.push_back(std::move(beamEntity));

                boss->beamEntitiesSpawned = true;
            };

            auto hideBeamEntities = [&]()
            {
                if (auto* turretEntity = boss->beamTurretEntity)
                {
                    if (auto* turretTransform = turretEntity->GetComponent<TransformComponent>())
                    {
                        turretTransform->x = -10000.0f;
                        turretTransform->y = -10000.0f;
                    }
                    if (auto* turret = turretEntity->GetComponent<LaserTurretComponent>())
                    {
                        turret->active = false;
                    }
                    if (auto* captureComponent = turretEntity->GetComponent<BossBeamCaptureComponent>())
                    {
                        captureComponent->captureEnabled = false;
                    }
                    if (auto* turretTint = turretEntity->GetComponent<TintComponent>())
                    {
                        turretTint->a = 0.0f;
                    }
                }
                if (auto* beamEntity = boss->beamEntity)
                {
                    if (auto* beamTransform = beamEntity->GetComponent<TransformComponent>())
                    {
                        beamTransform->x = -10000.0f;
                        beamTransform->y = -10000.0f;
                        beamTransform->width = 0.0f;
                    }
                    if (auto* beamTint = beamEntity->GetComponent<TintComponent>())
                    {
                        beamTint->a = 0.0f;
                    }
                }
            };

            auto showBeamEntities = [&]()
            {
                ensureBeamEntities();
                const float bossWidth = transform->width * transform->scale;
                const float bossHeight = transform->height * transform->scale;
                const float beamHeight = boss->params.beamHeightGrid * kTileSize;
                const float turretWidth = kTileSize;
                const float beamOriginX = beamFacingRight
                    ? transform->x + bossWidth + kTileSize
                    : transform->x - kTileSize;
                const float beamOriginY = transform->y + bossHeight * 0.5f;
                const float turretX = beamOriginX - turretWidth * 0.5f;
                const float turretY = beamOriginY - beamHeight * 0.5f;

                if (auto* turretEntity = boss->beamTurretEntity)
                {
                    if (auto* turretTransform = turretEntity->GetComponent<TransformComponent>())
                    {
                        turretTransform->x = turretX;
                        turretTransform->y = turretY;
                        turretTransform->width = turretWidth;
                        turretTransform->height = beamHeight;
                    }
                    if (auto* turret = turretEntity->GetComponent<LaserTurretComponent>())
                    {
                        turret->beamThickness = beamHeight;
                        turret->damagePerSecond = boss->params.beamDamagePerSecond;
                        turret->fireToLeft = !beamFacingRight;
                        turret->active = true;
                        turret->beamOriginOffsetX = turretWidth * 0.5f;
                        turret->beamOriginOffsetY = beamHeight * 0.5f;
                    }
                    if (auto* turretSprite = turretEntity->GetComponent<SpriteRenderComponent>())
                    {
                        turretSprite->SetFlipX(beamFacingRight);
                    }
                    if (auto* captureComponent = turretEntity->GetComponent<BossBeamCaptureComponent>())
                    {
                        captureComponent->captureEnabled = true;
                    }
                    if (auto* turretTint = turretEntity->GetComponent<TintComponent>())
                    {
                        turretTint->a = 1.0f;
                    }
                }
                if (auto* beamEntity = boss->beamEntity)
                {
                    if (auto* beamTransform = beamEntity->GetComponent<TransformComponent>())
                    {
                        beamTransform->x = beamOriginX;
                        beamTransform->y = beamOriginY - beamHeight * 0.5f;
                        beamTransform->height = beamHeight;
                    }
                    if (auto* beamTint = beamEntity->GetComponent<TintComponent>())
                    {
                        beamTint->a = 0.86f;
                    }
                }
            };

            if (enemy->IsDefeated())
            {
                boss->state = MidBoss2State::Dead;
                boss->captureWindowActive = false;
                hideBeamEntities();
                continue;
            }

            switch (boss->state)
            {
            case MidBoss2State::Idle:
                hideBeamEntities();
                if (std::fabs(dx) <= enemy->detectRange && std::fabs(dy) <= enemy->detectHeight)
                {
                    boss->spearCycleCount = 0;
                    boss->spearShotsFired = 0;
                    boss->hoverStartX = transform->x;
                    boss->hoverStartY = transform->y;
                    const float bossWidth = transform->width * transform->scale;
                    const float bossHeight = transform->height * transform->scale;
                    const float maxBossX = std::max(0.0f, mapWidth - bossWidth);
                    const float maxBossY = std::max(0.0f, mapHeight - bossHeight);
                    boss->hoverTargetX = std::clamp(
                        mapWidth * 0.5f - bossWidth * 0.5f,
                        0.0f,
                        maxBossX);
                    boss->hoverTargetY = std::clamp(
                        boss->homeY - boss->params.spearJumpHeightGrid * kTileSize,
                        0.0f,
                        maxBossY);
                    boss->state = MidBoss2State::SpearJump;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::SpearJump:
            {
                const float moveT = std::min(1.0f, flow.lastDeltaTime * 3.2f);
                transform->x += (boss->hoverTargetX - transform->x) * moveT;
                transform->y += (boss->hoverTargetY - transform->y) * moveT;
                if (std::fabs(transform->x - boss->hoverTargetX) <= 4.0f &&
                    std::fabs(transform->y - boss->hoverTargetY) <= 4.0f)
                {
                    transform->x = boss->hoverTargetX;
                    transform->y = boss->hoverTargetY;
                    boss->spearShotsFired = 0;
                    boss->state = MidBoss2State::SpearThrow;
                    boss->stateTimer = 0.0f;
                }
                break;
            }

            case MidBoss2State::SpearThrow:
                if (boss->spearShotsFired < 3 && boss->stateTimer >= boss->params.spearInterval)
                {
                    boss->stateTimer = 0.0f;
                    ++boss->spearShotsFired;

                    const float spawnWidth = kTileSize * 3.0f;
                    const float spawnHeight = kTileSize;
                    const float rawSpawnX = boss->facingRight
                        ? transform->x - spawnWidth
                        : transform->x + transform->width * transform->scale;
                    const float spawnX = std::clamp(rawSpawnX, 0.0f, std::max(0.0f, mapWidth - spawnWidth));
                    const float spawnY = std::clamp(
                        transform->y + transform->height * transform->scale * 0.25f,
                        0.0f,
                        std::max(0.0f, mapHeight - spawnHeight));
                    const float aimDx = playerCenterX - (spawnX + spawnWidth * 0.5f);
                    const float aimDy = playerCenterY - (spawnY + spawnHeight * 0.5f);
                    const float length = std::max(1.0f, std::sqrt(aimDx * aimDx + aimDy * aimDy));
                    const float dirX = aimDx / length;
                    const float dirY = aimDy / length;

                    auto spear = std::make_unique<Entity>();
                    spear->AddComponent<TagComponent>(kTagBullet);
                    spear->AddComponent<TransformComponent>(spawnX, spawnY, spawnWidth, spawnHeight);
                    spear->AddComponent<TintComponent>(0.68f, 0.92f, 1.0f, 1.0f);
                    spear->AddComponent<SpriteRenderComponent>(tileTexture);
                    auto& projectile = spear->AddComponent<ProjectileComponent>(
                        0.0f,
                        0.0f,
                        boss->params.spearDamage,
                        ProjectileComponent::Owner::Enemy);
                    projectile.sourceEntity = entity.get();
                    auto& spearComponent = spear->AddComponent<MidBoss2SpearComponent>();
                    spearComponent.launched = false;
                    spearComponent.fadeRemaining = boss->params.spearFadeTime;
                    spearComponent.fadeDuration = boss->params.spearFadeTime;
                    spearComponent.directionX = 0.0f;
                    spearComponent.directionY = -1.0f;
                    spearComponent.targetDirectionX = dirX;
                    spearComponent.targetDirectionY = dirY;
                    spearComponent.launchDelay = boss->params.spearInterval;
                    spearComponent.launchTimer = 0.0f;
                    boss->lastSpearDirX = dirX;
                    boss->lastSpearDirY = dirY;
                    if (auto* spearTransform = spear->GetComponent<TransformComponent>())
                    {
                        spearTransform->rotation = -1.5707963268f;
                    }
                    newBullets.push_back(std::move(spear));
                    playEnemyGun(*entity);
                }

                if (boss->spearShotsFired >= 3)
                {
                    ++boss->spearCycleCount;
                    const float bossWidth = transform->width * transform->scale;
                    boss->landingTargetX = std::clamp(
                        boss->homeX + (playerCenterX >= boss->homeX ? -1.0f : 1.0f) * boss->params.spearJumpHorizontalGrid * kTileSize,
                        0.0f,
                        std::max(0.0f, mapWidth - bossWidth));
                    boss->landingTargetY = boss->homeY;
                    boss->state = MidBoss2State::SpearLanding;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::SpearLanding:
            {
                const float moveX = boss->landingTargetX - transform->x;
                const float moveY = boss->landingTargetY - transform->y;
                const float distance = std::sqrt(moveX * moveX + moveY * moveY);
                if (distance <= 6.0f)
                {
                    transform->x = boss->landingTargetX;
                    transform->y = boss->landingTargetY;
                    boss->state = MidBoss2State::SpearCooldown;
                    boss->stateTimer = 0.0f;
                }
                else
                {
                    const float step = std::min(distance, kLandingMoveSpeed * flow.lastDeltaTime);
                    transform->x += moveX / distance * step;
                    transform->y += moveY / distance * step;
                }
                break;
            }

            case MidBoss2State::SpearCooldown:
                boss->cooldownRemaining = std::max(0.0f, boss->params.spearCooldownAfterLanding - boss->stateTimer);
                if (boss->stateTimer >= boss->params.spearLandingPauseTime)
                {
                    transform->x += (boss->homeX - transform->x) * std::min(1.0f, flow.lastDeltaTime * 4.0f);
                    transform->y += (boss->homeY - transform->y) * std::min(1.0f, flow.lastDeltaTime * 4.0f);
                }
                if (boss->stateTimer >= boss->params.spearCooldownAfterLanding)
                {
                    if (boss->spearCycleCount >= 3)
                    {
                        boss->state = MidBoss2State::BeamCharge;
                    }
                    else
                    {
                        const float bossWidth = transform->width * transform->scale;
                        const float bossHeight = transform->height * transform->scale;
                        const float maxBossX = std::max(0.0f, mapWidth - bossWidth);
                        const float maxBossY = std::max(0.0f, mapHeight - bossHeight);
                        boss->hoverTargetX = std::clamp(
                            mapWidth * 0.5f - bossWidth * 0.5f,
                            0.0f,
                            maxBossX);
                        boss->hoverTargetY = std::clamp(
                            boss->homeY - boss->params.spearJumpHeightGrid * kTileSize,
                            0.0f,
                            maxBossY);
                        boss->state = MidBoss2State::SpearJump;
                    }
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::BeamCharge:
                hideBeamEntities();
                boss->cooldownRemaining = std::max(0.0f, boss->params.beamChargeTime - boss->stateTimer);
                transform->x += (boss->homeX - transform->x) * std::min(1.0f, flow.lastDeltaTime * 4.0f);
                transform->y += (boss->homeY - transform->y) * std::min(1.0f, flow.lastDeltaTime * 4.0f);
                if (boss->stateTimer >= boss->params.beamChargeTime)
                {
                    flow.screenShakeRemaining = kBeamFireShakeSeconds;
                    flow.screenShakeDuration = kBeamFireShakeSeconds;
                    flow.screenShakeAmplitude = kBeamFireShakeAmplitude;
                    boss->state = MidBoss2State::BeamFire;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::BeamFire:
                showBeamEntities();
                transform->x += (boss->facingRight ? -1.0f : 1.0f) * 18.0f * flow.lastDeltaTime;
                if (boss->stateTimer >= kBeamFireDuration)
                {
                    boss->state = MidBoss2State::BeamCooldown;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::BeamCooldown:
                hideBeamEntities();
                boss->cooldownRemaining = std::max(0.0f, boss->params.spearCooldownAfterLanding - boss->stateTimer);
                transform->x += (boss->homeX - transform->x) * std::min(1.0f, flow.lastDeltaTime * 3.6f);
                transform->y += (boss->homeY - transform->y) * std::min(1.0f, flow.lastDeltaTime * 3.6f);
                if (boss->stateTimer >= boss->params.spearCooldownAfterLanding)
                {
                    boss->spearCycleCount = 0;
                    boss->spearShotsFired = 0;
                    boss->state = MidBoss2State::Idle;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::Damaged:
                if (boss->stateTimer >= 0.2f)
                {
                    boss->state = MidBoss2State::Idle;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::Dead:
                hideBeamEntities();
                break;
            }

            enemy->attackRectActive = false;
            if (boss->state != MidBoss2State::SpearJump &&
                boss->state != MidBoss2State::SpearThrow &&
                boss->state != MidBoss2State::SpearLanding)
            {
                enemy->velocityY = std::min(kMaxFallSpeed, enemy->velocityY + kGravity * flow.lastDeltaTime);
                transform->y += enemy->velocityY * flow.lastDeltaTime;
                if (snapToGround(*transform))
                {
                    enemy->velocityY = 0.0f;
                }
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
    constexpr float kMidBoss2SpearSpeed = 640.0f;
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
                auto* spear = entity->GetComponent<MidBoss2SpearComponent>();

                if (spear && spear->stuck)
                {
                    spear->fadeRemaining -= deltaTime;
                    if (auto* tint = entity->GetComponent<TintComponent>())
                    {
                        const float alpha = spear->fadeDuration > 0.0f
                            ? std::clamp(spear->fadeRemaining / spear->fadeDuration, 0.0f, 1.0f)
                            : 0.0f;
                        tint->a = alpha;
                    }
                    if (player && intersectsEntity(*player, *entity))
                    {
                        handlePlayerDamage(*player, entity.get(), "GameScene player damaged by spear");
                    }
                    return spear->fadeRemaining <= 0.0f;
                }

                if (spear)
                {
                    if (!spear->launched)
                    {
                        spear->launchTimer += deltaTime;
                        const float launchDuration = std::max(0.0001f, spear->launchDelay);
                        const float startAngle = -1.5707963268f;
                        float targetAngle = std::atan2(spear->targetDirectionY, spear->targetDirectionX);
                        while (targetAngle < startAngle)
                        {
                            targetAngle += 6.2831853072f;
                        }

                        const float progress = std::clamp(spear->launchTimer / launchDuration, 0.0f, 1.0f);
                        const float currentAngle = startAngle + (targetAngle - startAngle) * progress;
                        transform->rotation = currentAngle;
                        spear->directionX = std::cos(currentAngle);
                        spear->directionY = std::sin(currentAngle);
                        if (progress >= 1.0f)
                        {
                            spear->directionX = spear->targetDirectionX;
                            spear->directionY = spear->targetDirectionY;
                            transform->rotation = NormalizeAngleRadians(targetAngle);
                            spear->launched = true;
                        }
                        return false;
                    }

                    transform->rotation = std::atan2(spear->directionY, spear->directionX);
                    transform->x += spear->directionX * kMidBoss2SpearSpeed * deltaTime;
                    transform->y += spear->directionY * kMidBoss2SpearSpeed * deltaTime;
                }
                else
                {
                    transform->x += projectile->GetVelocityX() * deltaTime;
                    transform->y += projectile->GetVelocityY() * deltaTime;
                }

                if (isSolidTile(transform->x, transform->y) ||
                    isSolidTile(transform->x + transform->width, transform->y) ||
                    isSolidTile(transform->x, transform->y + transform->height) ||
                    isSolidTile(transform->x + transform->width, transform->y + transform->height))
                {
                    if (spear)
                    {
                        spear->stuck = true;
                        return false;
                    }
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
