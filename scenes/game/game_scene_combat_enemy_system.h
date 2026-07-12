#pragma once

#include <array>
#include <limits>

#include "game_scene_combat_common.h"

namespace game_scene_combat_system
{
template <typename SnapToGroundFn, typename PlayEnemyGunFn, typename PlayShieldBossRoarFn, typename PlayShieldBossCueFn, typename StopShieldBossCueFn, typename SpawnTeleportTrailFn, typename SpawnSlamImpactEffectFn, typename SpawnRushSmokeEffectFn, typename SpawnLightLandingEffectFn, typename SpawnBossRoarEffectFn, typename SpawnBeamShockwaveFn, typename SpawnMidBoss3FistImpactEffectFn, typename SpawnDropItemsFn, typename HandlePlayerDamageFn, typename CheckPhotoBoxCollisionFn, typename IsSolidTileFn>
inline void UpdateEnemies(
    std::vector<std::unique_ptr<Entity>>& entities,
    const std::vector<Entity*>& enemyEntities,
    const std::vector<Entity*>& interactionEntities,
    int tileTexture,
    int rubbleTexture,
    float mapWidth,
    float mapHeight,
    GameSceneFlowState& flow,
    const PhotoState& photo,
    Entity* player,
    GameScenePlayerState* playerState,
    const TransformComponent* playerTransform,
    SnapToGroundFn&& snapToGround,
    PlayEnemyGunFn&& playEnemyGun,
    PlayShieldBossRoarFn&& playShieldBossRoar,
    PlayShieldBossCueFn&& playShieldBossCue,
    StopShieldBossCueFn&& stopShieldBossCue,
    SpawnTeleportTrailFn&& spawnTeleportTrail,
    SpawnSlamImpactEffectFn&& spawnSlamImpactEffect,
    SpawnRushSmokeEffectFn&& spawnRushSmokeEffect,
    SpawnLightLandingEffectFn&& spawnLightLandingEffect,
    SpawnBossRoarEffectFn&& spawnBossRoarEffect,
    SpawnBeamShockwaveFn&& spawnBeamShockwave,
    SpawnMidBoss3FistImpactEffectFn&& spawnMidBoss3FistImpactEffect,
    SpawnDropItemsFn&& spawnDropItems,
    HandlePlayerDamageFn&& handlePlayerDamage,
    CheckPhotoBoxCollisionFn&& checkPhotoBoxCollision,
    IsSolidTileFn&& isSolidTile)
{
    flow.enemyCount = 0;
    std::vector<std::unique_ptr<Entity>> newBullets;
    std::vector<std::unique_ptr<Entity>> newShields;
    std::vector<std::unique_ptr<Entity>> newRubbles;
    std::vector<Entity*> entitiesToRemove;
    constexpr float kMidBoss2JumpCenterGridX = 36.0f;
    constexpr float kMidBoss2ArenaHalfWidthGrid = 18.0f;
    constexpr float kMidBoss2ArenaCenterMinGridX = kMidBoss2JumpCenterGridX - kMidBoss2ArenaHalfWidthGrid;
    constexpr float kMidBoss2ArenaCenterMaxGridX = kMidBoss2JumpCenterGridX + kMidBoss2ArenaHalfWidthGrid;
    constexpr float kMidBoss2LandingInsetGrid = 6.0f;
    constexpr float kMidBoss2SideLandingInsetGrid = 3.0f;
    constexpr float kMidBoss2GroundOffsetGridY = 4.0f;
    constexpr float kEnemyGravityStepSeconds = 1.0f / 120.0f;

    const auto applyEnemyGravity = [&](EnemyComponent& enemy, TransformComponent& transform, float gravity, float maxFallSpeed) -> bool
    {
        const float totalDeltaTime = std::max(0.0f, flow.lastDeltaTime);
        if (totalDeltaTime <= 0.0f)
        {
            return false;
        }

        const int stepCount = std::max(1, static_cast<int>(std::ceil(totalDeltaTime / kEnemyGravityStepSeconds)));
        const float stepDeltaTime = totalDeltaTime / static_cast<float>(stepCount);
        bool grounded = false;
        for (int step = 0; step < stepCount; ++step)
        {
            enemy.velocityY = std::min(maxFallSpeed, enemy.velocityY + gravity * stepDeltaTime);
            transform.y += enemy.velocityY * stepDeltaTime;
            if (snapToGround(transform))
            {
                enemy.velocityY = 0.0f;
                grounded = true;
                break;
            }
        }
        return grounded;
    };

    for (Entity* entity : enemyEntities)
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

        if (enemy->GetArchetype() == EnemyArchetype::MidBoss3)
        {
            auto* boss = entity->GetComponent<MidBoss3Component>();
            if (!boss)
            {
                continue;
            }

            constexpr float kTileSize = 48.0f;
            const float deltaTime = flow.lastDeltaTime;
            const auto smoothStep = [](float value) -> float
            {
                value = std::clamp(value, 0.0f, 1.0f);
                return value * value * (3.0f - 2.0f * value);
            };
            const auto moveToward = [](float current, float target, float maxDelta) -> float
            {
                const float diff = target - current;
                if (std::fabs(diff) <= maxDelta)
                {
                    return target;
                }
                return current + (diff > 0.0f ? maxDelta : -maxDelta);
            };
            const auto bossWidth = transform->width * transform->scale;
            const auto bossHeight = transform->height * transform->scale;
            const auto bossCenterX = [&]() -> float
            {
                return transform->x + bossWidth * 0.5f;
            };
            const auto bossCenterY = [&]() -> float
            {
                return transform->y + bossHeight * 0.5f;
            };
            const float stageCenterX = mapWidth * 0.5f;
            const float playerCenterX = playerTransform
                ? playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f
                : bossCenterX();
            const float playerCenterY = playerTransform
                ? playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f
                : bossCenterY();
            const auto normalizeSide = [](int side) -> int
            {
                return side < 0 ? -1 : 1;
            };
            const auto resolvePlayerSideFromBoss = [&](int fallbackSide) -> int
            {
                const int fallback = (fallbackSide == -1 || fallbackSide == 1)
                    ? fallbackSide
                    : normalizeSide(boss->lastFlowMoveSide);
                const float referenceX = boss->homeX + bossWidth * 0.5f;
                const float deadZone = kTileSize;
                if (playerCenterX < referenceX - deadZone)
                {
                    return -1;
                }
                if (playerCenterX > referenceX + deadZone)
                {
                    return 1;
                }
                return fallback;
            };

            if (!boss->initializedHome)
            {
                boss->homeX = transform->x;
                boss->homeY = transform->y;
                boss->initializedHome = true;
            }
            if (!boss->initializedArena)
            {
                boss->arenaCenterX = boss->homeX;
                boss->arenaCenterY = boss->homeY;
                boss->moveStartX = boss->homeX;
                boss->moveStartY = boss->homeY;
                boss->moveTargetX = boss->homeX;
                boss->moveTargetY = boss->homeY;
                boss->initializedArena = true;
            }
            boss->idleTimer += deltaTime;
            boss->stateTimer += deltaTime;
            boss->facingRight = playerCenterX >= (boss->homeX + bossWidth * 0.5f);
            if (auto* sprite = entity->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetFlipX(false);
            }

            const auto isBossGrounded = [&]() -> bool
            {
                const int row = static_cast<int>(std::floor((boss->homeY + bossHeight + 2.0f) / kTileSize));
                const int leftColumn = static_cast<int>(std::floor((boss->homeX + kTileSize * 0.25f) / kTileSize));
                const int centerColumn = static_cast<int>(std::floor((boss->homeX + bossWidth * 0.5f) / kTileSize));
                const int rightColumn = static_cast<int>(std::floor((boss->homeX + bossWidth - kTileSize * 0.25f) / kTileSize));
                return isSolidTile(leftColumn, row) ||
                    isSolidTile(centerColumn, row) ||
                    isSolidTile(rightColumn, row);
            };
            if (boss->damageMotionRequested)
            {
                boss->damageMotionRequested = false;
                boss->damageMotionAirborne = !isBossGrounded();
                boss->damageMotionDuration = boss->damageMotionAirborne ? 0.46f : 0.34f;
                boss->damageMotionRemaining = boss->damageMotionDuration;
                boss->damageMotionDirection = boss->damageMotionDirection >= 0.0f ? 1.0f : -1.0f;
            }
            boss->damageMotionOffsetX = 0.0f;
            boss->damageMotionOffsetY = 0.0f;
            if (boss->damageMotionRemaining > 0.0f && boss->damageMotionDuration > 0.0f)
            {
                const float progress = std::clamp(
                    1.0f - boss->damageMotionRemaining / boss->damageMotionDuration,
                    0.0f,
                    1.0f);
                const float pushInProgress = std::clamp(progress / 0.24f, 0.0f, 1.0f);
                const float pushOutProgress = std::clamp((progress - 0.24f) / 0.76f, 0.0f, 1.0f);
                const float pushIn = smoothStep(pushInProgress);
                const float pushOut = smoothStep(pushOutProgress);
                const float push = pushIn * (1.0f - pushOut);
                const float arc = std::sin(progress * 3.1415926f);
                if (boss->damageMotionAirborne)
                {
                    boss->damageMotionOffsetX = boss->damageMotionDirection * kTileSize * 0.85f * push;
                    boss->damageMotionOffsetY = kTileSize * 0.32f * arc;
                }
                else
                {
                    boss->damageMotionOffsetX = boss->damageMotionDirection * kTileSize * 0.58f * push;
                }
                boss->damageMotionRemaining = std::max(0.0f, boss->damageMotionRemaining - deltaTime);
            }

            const auto getFistDockX = [&](const MidBoss3FistComponent& fist, const TransformComponent& fistTransform, float bossX) -> float
            {
                if (!boss->facingRight)
                {
                    return bossX + fist.baseOffsetX;
                }
                return bossX + bossWidth - fist.baseOffsetX - fistTransform.width * fistTransform.scale;
            };
            const auto getFistDockY = [&](const MidBoss3FistComponent& fist, const TransformComponent&) -> float
            {
                return boss->homeY + fist.baseOffsetY +
                    std::sin(boss->idleTimer * boss->params.idleFloatSpeed + fist.idlePhase) *
                    boss->params.idleFloatAmplitude;
            };
            const auto rectIntersectsBlockingEntity = [&](float x, float y, float width, float height, const Entity* ignoreEntity) -> bool
            {
                TransformComponent attackBounds(x, y, width, height);
                for (Entity* candidate : interactionEntities)
                {
                    if (!candidate || candidate == ignoreEntity || candidate == entity)
                    {
                        continue;
                    }

                    const bool isShutter = HasTag(*candidate, "Shutter");
                    const bool isCapturedMidBoss3Attack = candidate->GetComponent<CapturedMidBoss3AttackComponent>() != nullptr;
                    const auto* otherFist = candidate->GetComponent<MidBoss3FistComponent>();
                    if (otherFist && otherFist->ownerBoss == entity)
                    {
                        continue;
                    }
                    if (!isShutter && !isCapturedMidBoss3Attack && !otherFist)
                    {
                        continue;
                    }
                    if (otherFist)
                    {
                        const auto* tint = candidate->GetComponent<TintComponent>();
                        if (otherFist->state == MidBoss3FistState::Docked ||
                            otherFist->state == MidBoss3FistState::Broken ||
                            (tint && tint->a <= 0.05f))
                        {
                            continue;
                        }
                    }

                    const auto* candidateTransform = candidate->GetComponent<TransformComponent>();
                    if (!candidateTransform || !IntersectsBounds(attackBounds, *candidateTransform))
                    {
                        continue;
                    }
                    return true;
                }
                return false;
            };
            const auto rectIntersectsSolidIgnoring = [&](float x, float y, float width, float height, const Entity* ignoreEntity) -> bool
            {
                const int columnCount = std::max(1, static_cast<int>(mapWidth / kTileSize));
                const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                const int leftColumn = static_cast<int>(std::floor(x / kTileSize));
                const int rightColumn = static_cast<int>(std::floor((x + width - 1.0f) / kTileSize));
                const int topRow = static_cast<int>(std::floor(y / kTileSize));
                const int bottomRow = static_cast<int>(std::floor((y + height - 1.0f) / kTileSize));

                for (int row = topRow; row <= bottomRow; ++row)
                {
                    for (int column = leftColumn; column <= rightColumn; ++column)
                    {
                        if (column < 0 || column >= columnCount || row >= rowCount)
                        {
                            return true;
                        }
                        if (row < 0)
                        {
                            continue;
                        }
                        if (isSolidTile(column, row))
                        {
                            return true;
                        }
                    }
                }
                return rectIntersectsBlockingEntity(x, y, width, height, ignoreEntity);
            };
            const auto rectBottomEnteredSolidFloor = [&](float previousX, float previousY, float x, float y, float width, float height) -> bool
            {
                if (y <= previousY)
                {
                    return false;
                }

                const int columnCount = std::max(1, static_cast<int>(mapWidth / kTileSize));
                const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                const float travelX = x - previousX;
                const float travelY = y - previousY;
                const float travelLength = std::max(std::fabs(travelX), std::fabs(travelY));
                const int steps = std::max(1, static_cast<int>(std::ceil(travelLength / std::max(1.0f, kTileSize * 0.35f))));
                const auto pointInSolidTile = [&](float px, float py) -> bool
                {
                    const int column = static_cast<int>(std::floor(px / kTileSize));
                    const int row = static_cast<int>(std::floor(py / kTileSize));
                    if (column < 0 || column >= columnCount || row < 0)
                    {
                        return false;
                    }
                    if (row >= rowCount)
                    {
                        return true;
                    }
                    return isSolidTile(column, row);
                };

                for (int step = 1; step <= steps; ++step)
                {
                    const float t = static_cast<float>(step) / static_cast<float>(steps);
                    const float previousT = static_cast<float>(step - 1) / static_cast<float>(steps);
                    const float sampleX = previousX + travelX * t;
                    const float sampleY = previousY + travelY * t;
                    const float prevSampleX = previousX + travelX * previousT;
                    const float prevSampleY = previousY + travelY * previousT;
                    for (float sampleFactor : { 0.25f, 0.5f, 0.75f })
                    {
                        const float currentBottomX = sampleX + width * sampleFactor;
                        const float currentBottomY = sampleY + height - 1.0f;
                        const float previousBottomX = prevSampleX + width * sampleFactor;
                        const float previousBottomY = prevSampleY + height - 1.0f;
                        if (!pointInSolidTile(previousBottomX, previousBottomY) &&
                            pointInSolidTile(currentBottomX, currentBottomY))
                        {
                            return true;
                        }
                    }
                }
                return false;
            };
            const auto rectIntersectsSolid = [&](float x, float y, float width, float height) -> bool
            {
                return rectIntersectsSolidIgnoring(x, y, width, height, nullptr);
            };
            const auto rectLeadingSideIntersectsSolid = [&](float x, float y, float width, float height, float velocityX, const Entity* ignoreEntity = nullptr) -> bool
            {
                const int columnCount = std::max(1, static_cast<int>(mapWidth / kTileSize));
                const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                const float sampleX = velocityX >= 0.0f ? x + width - 1.0f : x;
                const int column = static_cast<int>(std::floor(sampleX / kTileSize));
                if (column < 0 || column >= columnCount)
                {
                    return true;
                }

                const int topRow = static_cast<int>(std::floor((y + height * 0.18f) / kTileSize));
                const int bottomRow = static_cast<int>(std::floor((y + height * 0.72f) / kTileSize));
                for (int row = topRow; row <= bottomRow; ++row)
                {
                    if (row >= rowCount)
                    {
                        return true;
                    }
                    if (row < 0)
                    {
                        continue;
                    }
                    if (isSolidTile(column, row))
                    {
                        return true;
                    }
                }
                return rectIntersectsBlockingEntity(x, y, width, height, ignoreEntity);
            };
            const auto findIntroGroundY = [&]() -> float
            {
                const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                const int leftColumn = static_cast<int>(std::floor((boss->introFloatHomeX + kTileSize * 0.25f) / kTileSize));
                const int centerColumn = static_cast<int>(std::floor((boss->introFloatHomeX + bossWidth * 0.5f) / kTileSize));
                const int rightColumn = static_cast<int>(std::floor((boss->introFloatHomeX + bossWidth - kTileSize * 0.25f) / kTileSize));
                const int startRow = std::max(0, static_cast<int>(std::floor((boss->introFloatHomeY + bossHeight) / kTileSize)));
                for (int row = startRow; row < rowCount; ++row)
                {
                    if (isSolidTile(leftColumn, row) ||
                        isSolidTile(centerColumn, row) ||
                        isSolidTile(rightColumn, row))
                    {
                        return std::clamp(
                            static_cast<float>(row) * kTileSize - bossHeight,
                            0.0f,
                            std::max(0.0f, mapHeight - bossHeight));
                    }
                }
                return std::clamp(boss->introFloatHomeY, 0.0f, std::max(0.0f, mapHeight - bossHeight));
            };
            const auto setIntroFists = [&](float alpha)
            {
                const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
                for (Entity* fistEntity : boss->fistEntities)
                {
                    auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                    auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                    if (!fist)
                    {
                        continue;
                    }

                    fist->state = clampedAlpha > 0.01f ? MidBoss3FistState::Docked : MidBoss3FistState::Broken;
                    fist->velocityX = 0.0f;
                    fist->velocityY = 0.0f;
                    fist->launchTimer = 0.0f;
                    fist->damageApplied = false;
                    fist->broken = clampedAlpha <= 0.01f;
                    fist->captureJammerActive = false;
                    fist->impactAttackActive = false;
                    fist->impactDamageApplied = false;
                    fist->impactAttackRemaining = 0.0f;
                    if (fistTransform)
                    {
                        const float dockX = getFistDockX(*fist, *fistTransform, boss->homeX);
                        const float dockY = getFistDockY(*fist, *fistTransform);
                        fistTransform->x = dockX;
                        fistTransform->y = dockY;
                        fistTransform->rotation = 0.0f;
                    }
                    if (auto* tint = fistEntity->GetComponent<TintComponent>())
                    {
                        tint->a = clampedAlpha;
                    }
                }
            };
            const auto playMidBoss3Clip = [&](const char* clipName, bool restart = false)
            {
                auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>();
                if (!animation || !clipName)
                {
                    return;
                }
                if (restart || animation->GetCurrentClipName() != clipName)
                {
                    animation->Play(clipName, restart);
                }
            };
            const auto isPinnedByCapturedDrill = [&]() -> bool
            {
                for (const auto& candidate : entities)
                {
                    const auto* capturedAttack = candidate ? candidate->GetComponent<CapturedMidBoss3AttackComponent>() : nullptr;
                    if (capturedAttack &&
                        capturedAttack->kind == CapturedMidBoss3AttackKind::Drill &&
                        capturedAttack->attachedToBoss &&
                        capturedAttack->carriedBoss == entity)
                    {
                        return true;
                    }
                }
                return false;
            };
            const auto updateIntroSequence = [&]() -> bool
            {
                if (boss->introFinished)
                {
                    return false;
                }

                if (!boss->introGroundInitialized)
                {
                    if (boss->introFloatHomeX == 0.0f && boss->introFloatHomeY == 0.0f)
                    {
                        boss->introFloatHomeX = boss->homeX;
                        boss->introFloatHomeY = boss->homeY;
                    }
                    boss->introGroundY = findIntroGroundY();
                    boss->introGroundInitialized = true;
                }

                if (boss->debugRequestedAttack > 0)
                {
                    boss->introWaitingForTrigger = false;
                    boss->introStarted = true;
                    boss->introFinished = true;
                    boss->homeX = boss->introFloatHomeX;
                    boss->homeY = boss->introFloatHomeY;
                    transform->x = boss->homeX;
                    transform->y = boss->homeY;
                    boss->state = MidBoss3State::Move;
                    boss->stateTimer = 0.0f;
                    boss->flowStarted = false;
                    setIntroFists(1.0f);
                    return false;
                }

                if (!boss->introStarted)
                {
                    playMidBoss3Clip("idle");
                    boss->homeX = boss->introFloatHomeX;
                    boss->homeY = boss->introGroundY;
                    transform->x = boss->homeX;
                    transform->y = boss->homeY;
                    boss->stateTimer = 0.0f;
                    boss->idleTimer = 0.0f;
                    boss->drillActive = false;
                    setIntroFists(0.0f);

                    const float playerWidth = playerTransform->width * playerTransform->scale;
                    const float playerRight = playerTransform->x + playerWidth;
                    if (playerRight >= boss->introTriggerX)
                    {
                        boss->introStarted = true;
                        boss->introWaitingForTrigger = false;
                        boss->introTimer = 0.0f;
                        playMidBoss3Clip("appear", true);
                    }
                    return true;
                }

                playMidBoss3Clip("appear");
                boss->introTimer += deltaTime;
                const float progress = smoothStep(boss->introTimer / std::max(0.01f, boss->params.introRiseTime));
                boss->homeX = boss->introFloatHomeX;
                boss->homeY = boss->introGroundY + (boss->introFloatHomeY - boss->introGroundY) * progress;
                transform->x = boss->homeX;
                transform->y = boss->homeY;
                boss->drillActive = false;
                boss->state = MidBoss3State::Move;
                boss->stateTimer = 0.0f;
                boss->flowStarted = false;
                setIntroFists(std::clamp((progress - 0.45f) / 0.45f, 0.0f, 1.0f));

                const auto* introAnimation = entity->GetComponent<SpriteSheetAnimationComponent>();
                const bool introAnimationFinished =
                    !introAnimation ||
                    !introAnimation->HasClip("appear") ||
                    (introAnimation->GetCurrentClipName() == "appear" && introAnimation->IsCurrentClipFinished());
                if (progress >= 1.0f)
                {
                    boss->homeX = boss->introFloatHomeX;
                    boss->homeY = boss->introFloatHomeY;
                    transform->x = boss->homeX;
                    transform->y = boss->homeY;
                    setIntroFists(1.0f);
                }

                if (progress >= 1.0f && introAnimationFinished)
                {
                    boss->introFinished = true;
                    boss->homeX = boss->introFloatHomeX;
                    boss->homeY = boss->introFloatHomeY;
                    transform->x = boss->homeX;
                    transform->y = boss->homeY;
                    boss->stateTimer = 0.0f;
                    boss->idleTimer = 0.0f;
                    setIntroFists(1.0f);
                    playMidBoss3Clip("idle", true);
                }
                return true;
            };
            if (boss->deathAnimationActive)
            {
                playMidBoss3Clip("death");
                if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>())
                {
                    animation->SetPlaybackSpeed(1.0f);
                }
                boss->drillActive = false;
                boss->reloadActive = false;
                for (Entity* fistEntity : boss->fistEntities)
                {
                    if (auto* tint = fistEntity ? fistEntity->GetComponent<TintComponent>() : nullptr)
                    {
                        tint->a = 0.0f;
                    }
                }
                if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>();
                    animation && animation->IsCurrentClipFinished())
                {
                    animation->SetPlaybackSpeed(1.0f);
                    boss->deathAnimationActive = false;
                    boss->deathAnimationFinished = true;
                    enemy->MarkDefeated();
                    enemy->respawnEnabled = false;
                    // ボス撁E��でゴールを解放する�E�シャチE��ーは useBossDefeatSignal 側で自動的に開く�E�、E
                    flow.midBoss3DefeatedThisScene = true;
                    flow.goalUnlockedBySwitch = true;
                    flow.stageBgmCrossFadePending = true;
                    flow.stageBgmCrossFadeDelayRemaining = 1.25f;
                    spawnDropItems(*entity);
                }
                continue;
            }
            if (updateIntroSequence())
            {
                continue;
            }
            if (isPinnedByCapturedDrill())
            {
                boss->state = MidBoss3State::AttackCooldown;
                boss->stateTimer = 0.0f;
                boss->launcherShotTimer = 0.0f;
                boss->launcherShotsFired = 0;
                boss->meteorShotsFired = 0;
                boss->drillActive = false;
                boss->drillFormed = false;
                boss->drillGroundRush = false;
                boss->drillDamageApplied = false;
                boss->drillFloorObjectHits = 0;
                boss->drillVelocityX = 0.0f;
                boss->drillVelocityY = 0.0f;
                boss->reloadActive = false;
                setIntroFists(1.0f);
                transform->x = boss->homeX;
                transform->y = boss->homeY;
                playMidBoss3Clip("idle");
                continue;
            }
            const auto isEntityPendingRemove = [&](const Entity* target) -> bool
            {
                return target &&
                    std::find(entitiesToRemove.begin(), entitiesToRemove.end(), target) != entitiesToRemove.end();
            };
            const auto spawnSepiaCollisionRubble = [&](float x, float y, float width, float height, SepiaRubbleSource source)
            {
                const bool attackRubble =
                    source == SepiaRubbleSource::MidBoss3Fist ||
                    source == SepiaRubbleSource::MidBoss3Drill;
                const float rubbleWidth = attackRubble
                    ? std::max(kTileSize * 0.5f, width)
                    : std::clamp(std::min(width, height), kTileSize * 0.75f, kTileSize * 2.0f);
                const float rubbleHeight = attackRubble
                    ? std::max(kTileSize * 0.5f, height)
                    : rubbleWidth;
                auto rubble = std::make_unique<Entity>();
                rubble->AddComponent<TagComponent>("SepiaRubble");
                rubble->AddComponent<TransformComponent>(
                    x + width * 0.5f - rubbleWidth * 0.5f,
                    y + height * 0.5f - rubbleHeight * 0.5f,
                    rubbleWidth,
                    rubbleHeight);
                rubble->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
                rubble->AddComponent<SpriteRenderComponent>(rubbleTexture >= 0 ? rubbleTexture : tileTexture);
                rubble->AddComponent<SepiaRubbleComponent>(source);
                rubble->AddComponent<PhotoCopyLifetimeComponent>(1.4f);
                newRubbles.push_back(std::move(rubble));
            };
            const auto queueDestroyEntity = [&](Entity* target, SepiaRubbleSource source)
            {
                if (!target || target == entity || target == player || isEntityPendingRemove(target))
                {
                    return;
                }

                const auto* targetTransform = target->GetComponent<TransformComponent>();
                if (targetTransform)
                {
                    spawnSepiaCollisionRubble(
                        targetTransform->x,
                        targetTransform->y,
                        targetTransform->width * targetTransform->scale,
                        targetTransform->height * targetTransform->scale,
                        source);
                }
                entitiesToRemove.push_back(target);
            };
            const auto breakFistAtCollision = [&](MidBoss3FistComponent& targetFist, TransformComponent& targetTransform)
            {
                constexpr float kFistImpactHitStopSeconds = 0.075f;
                constexpr float kFistImpactShakeSeconds = 0.32f;
                constexpr float kFistImpactShakeAmplitude = 38.0f;
                flow.hitStopRemaining = std::max(flow.hitStopRemaining, kFistImpactHitStopSeconds);
                flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kFistImpactShakeSeconds);
                flow.screenShakeDuration = std::max(flow.screenShakeDuration, kFistImpactShakeSeconds);
                flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kFistImpactShakeAmplitude);
                spawnMidBoss3FistImpactEffect(
                    targetTransform.x,
                    targetTransform.y,
                    targetTransform.width * targetTransform.scale,
                    targetTransform.height * targetTransform.scale);
                targetFist.state = MidBoss3FistState::Broken;
                targetFist.broken = true;
                targetFist.damageApplied = true;
                targetFist.velocityX = 0.0f;
                targetFist.velocityY = 0.0f;
                targetFist.captureJammerActive = false;
                targetFist.impactAttackActive = false;
                spawnSepiaCollisionRubble(
                    targetTransform.x,
                    targetTransform.y,
                    targetTransform.width * targetTransform.scale,
                    targetTransform.height * targetTransform.scale,
                    SepiaRubbleSource::MidBoss3Fist);
                if (auto* tint = targetFist.GetGameObject().GetComponent<TintComponent>())
                {
                    tint->a = 0.0f;
                }
            };
            const auto isSepiaObject = [&](const Entity& target) -> bool
            {
                if (target.GetComponent<SepiaRubbleComponent>() ||
                    target.GetComponent<SepiaRubbleGroupComponent>() ||
                    HasTag(target, "SepiaRubble"))
                {
                    return true;
                }
                if (const auto* effect = target.GetComponent<PhotoCopyEffectComponent>())
                {
                    return effect->GetTheme() == PhotoFilterTheme::Sepia;
                }
                return false;
            };
            const auto isSepiaDriveObject = [&](const Entity& target) -> bool
            {
                return HasTag(target, "PhotoSource") || target.GetComponent<PhotoFilterComponent>() != nullptr;
            };
            const auto isFloorObject = [&](const Entity& target) -> bool
            {
                if (!HasTag(target, "PhotoBox"))
                {
                    return false;
                }
                const auto* role = target.GetComponent<PhotoCopyRoleComponent>();
                const auto* layer = target.GetComponent<PhotoCopyLayerComponent>();
                const auto* origin = target.GetComponent<PhotoCopyOriginComponent>();
                return (!layer || layer->layer == PhotoCopyLayer::Foreground) &&
                    role && role->role == PhotoCopyRole::Solid &&
                    origin && origin->origin == PhotoCopyOrigin::Tile;
            };
            const auto isBreakableObject = [&](const Entity& target) -> bool
            {
                if (isSepiaObject(target) || isSepiaDriveObject(target) || isFloorObject(target))
                {
                    return false;
                }
                return target.GetComponent<BarrelComponent>() ||
                    target.GetComponent<BatteryComponent>() ||
                    target.GetComponent<VanishOnCaptureComponent>() ||
                    HasTag(target, "Log") ||
                    HasTag(target, "Battery") ||
                    HasTag(target, "DamagePlatform") ||
                    HasTag(target, "DamagePlatformSpike") ||
                    HasTag(target, "PhotoBox");
            };
            const auto getProjectileObjectCollision = [&](const TransformComponent& projectileTransform, Entity* sourceFist) -> Entity*
            {
                for (const auto& target : entities)
                {
                    if (!target ||
                        target.get() == entity ||
                        target.get() == sourceFist ||
                        target.get() == player ||
                        isEntityPendingRemove(target.get()))
                    {
                        continue;
                    }

                    const auto* targetTransform = target->GetComponent<TransformComponent>();
                    if (!targetTransform || !IntersectsBounds(projectileTransform, *targetTransform))
                    {
                        continue;
                    }

                    if (HasTag(*target, kTagEnemy))
                    {
                        const auto* targetEnemy = target->GetComponent<EnemyComponent>();
                        if (targetEnemy && targetEnemy->GetArchetype() == EnemyArchetype::MidBoss3)
                        {
                            continue;
                        }
                    }
                    return target.get();
                }
                return nullptr;
            };
            const auto resetFistForAttack = [](MidBoss3FistComponent& fist)
            {
                fist.velocityX = 0.0f;
                fist.velocityY = 0.0f;
                fist.launchTimer = 0.0f;
                fist.attackReadyTimer = 0.0f;
                fist.meteorAimX = 0.0f;
                fist.meteorAimY = 1.0f;
                fist.meteorTargetGroundY = 0.0f;
                fist.meteorHoldInitialized = false;
                fist.damageApplied = false;
                fist.atAttackStart = false;
                fist.broken = false;
                fist.impactAttackActive = false;
                fist.impactDamageApplied = false;
                fist.impactAttackRemaining = 0.0f;
            };
            const auto chooseUnjammedFist = [&]() -> int
            {
                return static_cast<int>(std::fmod(
                    std::fabs(std::sin(boss->idleTimer * 19.137f + static_cast<float>(boss->movePattern) * 71.91f)) * 43758.5453f,
                    4.0f));
            };
            const auto getMovePoint = [&](int side, int pattern, int step, float& outX, float& outY)
            {
                const float sideSign = side < 0 ? -1.0f : 1.0f;
                const int safePattern = ((pattern % 3) + 3) % 3;
                const int safeStep = std::max(0, step);
                float offsetXGrid = 0.0f;
                float offsetYGrid = 0.0f;

                if (safePattern == 0)
                {
                    const float points[3][2] = {
                        {  6.0f, -5.0f },
                        { 12.0f,  0.0f },
                        {  4.0f,  0.0f },
                    };
                    const int pointIndex = std::min(safeStep, 2);
                    offsetXGrid = points[pointIndex][0];
                    offsetYGrid = points[pointIndex][1];
                }
                else if (safePattern == 1)
                {
                    const float points[2][2] = {
                        { 12.0f, 0.0f },
                        {  0.0f, 0.0f },
                    };
                    const int pointIndex = std::min(safeStep, 1);
                    offsetXGrid = points[pointIndex][0];
                    offsetYGrid = points[pointIndex][1];
                }
                else
                {
                    const float points[3][2] = {
                        {  6.0f, -5.0f },
                        { -6.0f, -5.0f },
                        {-12.0f,  0.0f },
                    };
                    const int pointIndex = std::min(safeStep, 2);
                    offsetXGrid = points[pointIndex][0];
                    offsetYGrid = points[pointIndex][1];
                }

                outX = boss->arenaCenterX + offsetXGrid * sideSign * kTileSize;
                outY = boss->arenaCenterY + offsetYGrid * kTileSize;
            };
            const auto getMovePointCount = [](int pattern) -> int
            {
                return pattern == 1 ? 2 : 3;
            };
            const auto setAllFistsState = [&](MidBoss3FistState state, bool useJammer)
            {
                const int unjammedIndex = chooseUnjammedFist();
                for (Entity* fistEntity : boss->fistEntities)
                {
                    auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                    auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                    if (!fist)
                    {
                        continue;
                    }
                    fist->state = state;
                    resetFistForAttack(*fist);
                    fist->captureJammerActive = useJammer && fist->fistIndex != unjammedIndex;
                    if (fistTransform)
                    {
                        fistTransform->rotation = 0.0f;
                    }
                    if (auto* tint = fistEntity->GetComponent<TintComponent>())
                    {
                        tint->a = 1.0f;
                    }
                }
            };
            const auto setLauncherFistReady = [&](int fistIndex)
            {
                for (Entity* fistEntity : boss->fistEntities)
                {
                    auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                    auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                    if (!fist || fist->fistIndex != fistIndex)
                    {
                        continue;
                    }
                    if (fist->state != MidBoss3FistState::LauncherReady &&
                        fist->state != MidBoss3FistState::Launching)
                    {
                        fist->state = MidBoss3FistState::LauncherReady;
                        resetFistForAttack(*fist);
                        fist->lockedFacingRight = boss->launcherDirection > 0;
                        if (fistTransform)
                        {
                            fistTransform->rotation = 0.0f;
                        }
                        if (auto* tint = fistEntity->GetComponent<TintComponent>())
                        {
                            tint->a = 1.0f;
                        }
                    }
                    return;
                }
            };
            const auto setMeteorFistsReady = [&](int a, int b)
            {
                for (Entity* fistEntity : boss->fistEntities)
                {
                    auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                    auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                    if (!fist || (fist->fistIndex != a && fist->fistIndex != b))
                    {
                        continue;
                    }
                    if (fist->state == MidBoss3FistState::MeteorReady ||
                        fist->state == MidBoss3FistState::MeteorFalling)
                    {
                        continue;
                    }
                    fist->state = MidBoss3FistState::MeteorReady;
                    resetFistForAttack(*fist);
                    if (fistTransform)
                    {
                        fistTransform->rotation = -1.5707963f;
                    }
                    if (auto* tint = fistEntity->GetComponent<TintComponent>())
                    {
                        tint->a = 1.0f;
                    }
                }
            };
            const auto beginReload = [&]()
            {
                if (boss->reloadActive)
                {
                    return;
                }
                boss->reloadActive = true;
                boss->reloadTimer = 0.0f;
                boss->drillActive = false;
                for (Entity* fistEntity : boss->fistEntities)
                {
                    auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                    auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                    if (!fist || !fistTransform)
                    {
                        continue;
                    }
                    const float fistHeight = fistTransform->height * fistTransform->scale;
                    const float fistWidth = fistTransform->width * fistTransform->scale;
                    const bool reloadFromTop = fist->fistIndex == 0 || fist->fistIndex == 2;
                    const std::array<float, 4> reloadStartOffsets = {
                        -10.0f,
                        -5.0f,
                        5.0f,
                        10.0f,
                    };
                    const int reloadIndex = std::clamp(fist->fistIndex, 0, 3);
                    const float scatteredStartX = boss->arenaCenterX + reloadStartOffsets[reloadIndex] * kTileSize;
                    fist->state = MidBoss3FistState::Reloading;
                    fist->reloadStartX = std::clamp(scatteredStartX, -fistWidth, mapWidth);
                    fist->reloadStartY = reloadFromTop ? -fistHeight - kTileSize * 1.5f : mapHeight + kTileSize * 1.5f;
                    fistTransform->x = fist->reloadStartX;
                    fistTransform->y = fist->reloadStartY;
                    fistTransform->rotation = 0.0f;
                    fist->broken = false;
                    fist->damageApplied = false;
                    fist->launchTimer = 0.0f;
                    fist->attackReadyTimer = 0.0f;
                    fist->meteorAimX = 0.0f;
                    fist->meteorAimY = 1.0f;
                    fist->meteorHoldInitialized = false;
                    fist->captureJammerActive = false;
                    fist->impactAttackActive = false;
                    fist->impactDamageApplied = false;
                    fist->impactAttackRemaining = 0.0f;
                    if (auto* tint = fistEntity->GetComponent<TintComponent>())
                    {
                        tint->a = 1.0f;
                    }
                }
            };
            const auto findLauncherGroundY = [&]() -> float
            {
                const int columnCount = std::max(1, static_cast<int>(mapWidth / kTileSize));
                const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                const int centerColumn = std::clamp(
                    static_cast<int>(stageCenterX / kTileSize),
                    0,
                    columnCount - 1);
                const int maxSearchOffset = std::min(columnCount - 1, 12);
                for (int row = rowCount / 2; row < rowCount; ++row)
                {
                    for (int offset = 0; offset <= maxSearchOffset; ++offset)
                    {
                        const int leftColumn = centerColumn - offset;
                        const int rightColumn = centerColumn + offset;
                        if (leftColumn >= 0 && isSolidTile(leftColumn, row))
                        {
                            return static_cast<float>(row) * kTileSize;
                        }
                        if (rightColumn < columnCount && rightColumn != leftColumn && isSolidTile(rightColumn, row))
                        {
                            return static_cast<float>(row) * kTileSize;
                        }
                    }
                }
                return mapHeight;
            };
            const auto setFixedLauncherLanes = [&](float fistHeight)
            {
                const float groundY = findLauncherGroundY();
                const float lowerLaneY = groundY - fistHeight - kTileSize * 0.25f;
                boss->launcherLowerLaneY = std::clamp(lowerLaneY, 0.0f, std::max(0.0f, mapHeight - fistHeight));
                boss->launcherUpperLaneY = std::clamp(
                    boss->launcherLowerLaneY - kTileSize * 2.0f,
                    0.0f,
                    std::max(0.0f, mapHeight - fistHeight));
            };
            const auto prepareLauncherAttack = [&]()
            {
                boss->state = MidBoss3State::LauncherFist;
                boss->stateTimer = 0.0f;
                boss->launcherShotTimer = 0.0f;
                boss->launcherShotsFired = 0;
                boss->cooldownAttack = 1;
                boss->launcherDirection = resolvePlayerSideFromBoss(boss->lastFlowMoveSide);
                boss->lastFlowMoveSide = boss->launcherDirection;
                float fistHeight = kTileSize * 2.0f;
                for (Entity* fistEntity : boss->fistEntities)
                {
                    if (const auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr)
                    {
                        fistHeight = fistTransform->height * fistTransform->scale;
                        break;
                    }
                }
                setFixedLauncherLanes(fistHeight);
                setAllFistsState(MidBoss3FistState::Docked, true);
            };
            const auto prepareMeteorAttack = [&]()
            {
                boss->state = MidBoss3State::MeteorFist;
                boss->stateTimer = 0.0f;
                boss->launcherShotTimer = 0.0f;
                boss->meteorShotsFired = 0;
                boss->cooldownAttack = 2;
                boss->meteorDirection = resolvePlayerSideFromBoss(boss->lastFlowMoveSide);
                boss->lastFlowMoveSide = boss->meteorDirection;
                setAllFistsState(MidBoss3FistState::MeteorReady, true);
            };
            const auto prepareComboAttack = [&]()
            {
                boss->state = MidBoss3State::LauncherMeteorFist;
                boss->stateTimer = 0.0f;
                boss->launcherShotTimer = 0.0f;
                boss->launcherShotsFired = 0;
                boss->meteorShotsFired = 0;
                boss->cooldownAttack = 3;
                boss->launcherDirection = resolvePlayerSideFromBoss(boss->lastFlowMoveSide);
                boss->meteorDirection = boss->launcherDirection;
                boss->lastFlowMoveSide = boss->launcherDirection;
                float fistHeight = kTileSize * 2.0f;
                for (Entity* fistEntity : boss->fistEntities)
                {
                    if (const auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr)
                    {
                        fistHeight = fistTransform->height * fistTransform->scale;
                        break;
                    }
                }
                setFixedLauncherLanes(fistHeight);
                setAllFistsState(MidBoss3FistState::Docked, true);
                setMeteorFistsReady(0, 1);
            };
            const auto prepareDrillAttack = [&]()
            {
                boss->state = MidBoss3State::DrillFist;
                boss->stateTimer = 0.0f;
                boss->cooldownAttack = 4;
                boss->drillActive = false;
                boss->drillFormed = false;
                boss->drillGroundRush = false;
                boss->drillDamageApplied = false;
                boss->drillFloorObjectHits = 0;
                boss->drillVelocityX = 0.0f;
                boss->drillVelocityY = 0.0f;
                boss->drillWidth = kTileSize * 4.0f;
                boss->drillHeight = kTileSize * 2.0f;
                boss->drillDirection = resolvePlayerSideFromBoss(boss->lastFlowMoveSide);
                boss->lastFlowMoveSide = boss->drillDirection;
                const float drillX = boss->drillDirection > 0
                    ? transform->x + bossWidth + kTileSize * 0.5f
                    : transform->x - boss->drillWidth - kTileSize * 0.5f;
                boss->drillX = std::clamp(drillX, 0.0f, std::max(0.0f, mapWidth - boss->drillWidth));
                boss->drillY = std::clamp(transform->y + bossHeight * 0.5f - boss->drillHeight * 0.5f, 0.0f, std::max(0.0f, mapHeight - boss->drillHeight));
                boss->drillChargeBaseX = boss->drillX;
                boss->drillChargeBaseY = boss->drillY;
                const float aimDx = playerCenterX - (boss->drillX + boss->drillWidth * 0.5f);
                const float aimDy = playerCenterY - (boss->drillY + boss->drillHeight * 0.5f);
                const float aimLength = std::max(0.001f, std::hypot(aimDx, aimDy));
                boss->drillAimX = aimDx / aimLength;
                boss->drillAimY = aimDy / aimLength;
                setAllFistsState(MidBoss3FistState::DrillForming, false);
                for (Entity* fistEntity : boss->fistEntities)
                {
                    if (auto* tint = fistEntity ? fistEntity->GetComponent<TintComponent>() : nullptr)
                    {
                        tint->a = 1.0f;
                    }
                }
            };
            const auto prepareNextFlowAttack = [&]()
            {
                boss->flowStarted = true;
                switch (boss->nextFlowAttack)
                {
                case 1:
                    prepareLauncherAttack();
                    break;
                case 2:
                    prepareMeteorAttack();
                    break;
                case 3:
                    prepareComboAttack();
                    break;
                case 4:
                    prepareDrillAttack();
                    break;
                default:
                    boss->nextFlowAttack = 2;
                    prepareMeteorAttack();
                    break;
                }
            };

            if (boss->debugRequestedAttack > 0)
            {
                const int requested = boss->debugRequestedAttack;
                boss->debugRequestedAttack = 0;
                boss->flowStarted = true;
                if (requested == 1) prepareLauncherAttack();
                else if (requested == 2) prepareMeteorAttack();
                else if (requested == 3) prepareComboAttack();
                else if (requested == 4) prepareDrillAttack();
            }

            if (boss->reloadActive)
            {
                boss->reloadTimer += deltaTime;
                if (boss->reloadTimer >= boss->params.fistReloadTime)
                {
                    boss->reloadActive = false;
                    boss->reloadTimer = boss->params.fistReloadTime;
                    for (Entity* fistEntity : boss->fistEntities)
                    {
                        auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                        if (!fist)
                        {
                            continue;
                        }
                        fist->state = MidBoss3FistState::Docked;
                        fist->broken = false;
                        fist->captureJammerActive = false;
                    }
                }
            }

            if (boss->state == MidBoss3State::Move)
            {
                if (!boss->flowStarted)
                {
                    if (boss->stateTimer < boss->params.initialFlowDelayTime)
                    {
                        const float bossWave = std::sin(boss->idleTimer * boss->params.idleFloatSpeed);
                        transform->x = boss->homeX + boss->damageMotionOffsetX;
                        transform->y = boss->homeY + bossWave * boss->params.idleFloatAmplitude + boss->damageMotionOffsetY;
                    }
                    else
                    {
                        prepareNextFlowAttack();
                    }
                }
                else
                {
                    boss->moveTimer += deltaTime;
                    if (boss->moving)
                    {
                        const float progress = std::clamp(
                            boss->moveTimer / std::max(0.01f, boss->params.moveDuration),
                            0.0f,
                            1.0f);
                        const float eased = smoothStep(progress);
                        const float arc = std::sin(progress * 3.1415926f) * boss->params.moveArcHeightGrid * kTileSize;
                        boss->homeX = boss->moveStartX + (boss->moveTargetX - boss->moveStartX) * eased;
                        boss->homeY = boss->moveStartY + (boss->moveTargetY - boss->moveStartY) * eased - arc;
                        if (progress >= 1.0f)
                        {
                            boss->homeX = boss->moveTargetX;
                            boss->homeY = boss->moveTargetY;
                            boss->moving = false;
                            boss->moveTimer = 0.0f;
                            ++boss->moveStep;
                        }
                    }
                    else
                    {
                        const int movePointCount = getMovePointCount(boss->movePattern);
                        if (boss->moveStep >= movePointCount)
                        {
                            if (!boss->reloadActive)
                            {
                                prepareNextFlowAttack();
                            }
                        }
                        else if (boss->moveTimer >= boss->params.movePauseTime)
                        {
                            if (boss->moveStep == 0)
                            {
                                if (boss->chooseMoveSideFromStageCenter)
                                {
                                    boss->moveSide = resolvePlayerSideFromBoss(boss->lastFlowMoveSide);
                                    boss->lastFlowMoveSide = boss->moveSide;
                                }
                                else if (boss->lastFlowMoveSide == -1 || boss->lastFlowMoveSide == 1)
                                {
                                    boss->moveSide = boss->lastFlowMoveSide;
                                }
                                boss->chooseMoveSideFromStageCenter = false;
                            }

                            boss->moveStartX = boss->homeX;
                            boss->moveStartY = boss->homeY;
                            if (!boss->reloadStartedForMove &&
                                boss->reloadStartMoveStep >= 0 &&
                                boss->moveStep == boss->reloadStartMoveStep)
                            {
                                boss->reloadStartedForMove = true;
                                beginReload();
                            }
                            getMovePoint(boss->moveSide, boss->movePattern, boss->moveStep, boss->moveTargetX, boss->moveTargetY);
                            boss->moveTargetX = std::clamp(
                                boss->moveTargetX,
                                0.0f,
                                std::max(0.0f, mapWidth - bossWidth));
                            boss->moveTargetY = std::clamp(
                                boss->moveTargetY,
                                0.0f,
                                std::max(0.0f, mapHeight - bossHeight));
                            boss->moving = true;
                            boss->moveTimer = 0.0f;
                        }
                    }
                }
            }
            else if (boss->state == MidBoss3State::LauncherFist)
            {
                boss->launcherShotTimer += deltaTime;
                if (boss->launcherShotsFired < 4)
                {
                    const int launchOrder[4] = { 0, 1, 2, 3 };
                    const int fistIndex = launchOrder[boss->launcherShotsFired];
                    setLauncherFistReady(fistIndex);
                    bool launched = false;
                    for (Entity* fistEntity : boss->fistEntities)
                    {
                        auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                        auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                        if (!fist ||
                            !fistTransform ||
                            fist->fistIndex != fistIndex ||
                            !fist->atAttackStart ||
                            fist->attackReadyTimer < boss->params.fistPreLaunchShakeTime ||
                            (boss->launcherShotsFired > 0 && boss->launcherShotTimer < boss->params.launcherFistInterval))
                        {
                            continue;
                        }
                        fist->state = MidBoss3FistState::Launching;
                        fist->velocityX = boss->launcherDirection < 0 ? -boss->params.launcherFistSpeed : boss->params.launcherFistSpeed;
                        fist->velocityY = 0.0f;
                        fist->lockedFacingRight = boss->launcherDirection > 0;
                        fist->launchTimer = 0.0f;
                        fistTransform->rotation = 0.0f;
                        launched = true;
                        break;
                    }
                    if (launched)
                    {
                        boss->launcherShotTimer = 0.0f;
                        ++boss->launcherShotsFired;
                    }
                }
                if (boss->launcherShotsFired >= 4)
                {
                    boss->state = MidBoss3State::AttackCooldown;
                    boss->stateTimer = 0.0f;
                }
            }
            else if (boss->state == MidBoss3State::MeteorFist ||
                boss->state == MidBoss3State::LauncherMeteorFist)
            {
                const auto launchMeteorFist = [&](int targetIndex) -> bool
                {
                    for (Entity* fistEntity : boss->fistEntities)
                    {
                        auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                        auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                        if (!fist ||
                            !fistTransform ||
                            fist->fistIndex != targetIndex ||
                            fist->state != MidBoss3FistState::MeteorReady ||
                            !fist->atAttackStart ||
                            fist->attackReadyTimer < boss->params.fistPreLaunchShakeTime)
                        {
                            continue;
                        }
                        fist->state = MidBoss3FistState::MeteorFalling;
                        const float aimLength = std::max(0.001f, std::hypot(fist->meteorAimX, fist->meteorAimY));
                        fist->velocityX = (fist->meteorAimX / aimLength) * boss->params.meteorFistSpeed;
                        fist->velocityY = (fist->meteorAimY / aimLength) * boss->params.meteorFistSpeed;
                        fist->launchTimer = 0.0f;
                        fistTransform->rotation = std::atan2(fist->velocityY, fist->velocityX) + 3.14159265f;
                        return true;
                    }
                    return false;
                };

                const int meteorLaunchOrder[4] = { 0, 1, 2, 3 };
                const int meteorLaunchCount = boss->state == MidBoss3State::LauncherMeteorFist ? 2 : 4;
                const float meteorLaunchInterval = boss->params.meteorPairInterval;
                const float requiredWait = boss->meteorShotsFired == 0
                    ? boss->params.meteorWindupTime
                    : meteorLaunchInterval;
                if (boss->meteorShotsFired < meteorLaunchCount &&
                    boss->stateTimer >= requiredWait)
                {
                    const int fistIndex = meteorLaunchOrder[boss->meteorShotsFired];
                    if (launchMeteorFist(fistIndex))
                    {
                        ++boss->meteorShotsFired;
                        boss->stateTimer = 0.0f;
                    }
                }
                if (boss->meteorShotsFired >= meteorLaunchCount)
                {
                    if (boss->state == MidBoss3State::MeteorFist)
                    {
                        boss->state = MidBoss3State::AttackCooldown;
                        boss->stateTimer = 0.0f;
                    }
                    else
                    {
                        boss->state = MidBoss3State::LauncherFist;
                        boss->stateTimer = boss->params.launcherWindupTime;
                        boss->launcherShotsFired = 2;
                        boss->launcherShotTimer = boss->params.launcherFistInterval;
                    }
                }
            }
            else if (boss->state == MidBoss3State::DrillFist)
            {
                if (!boss->drillFormed)
                {
                    const float drillCenterX = boss->drillX + boss->drillWidth * 0.5f;
                    const float drillCenterY = boss->drillY + boss->drillHeight * 0.5f;
                    const float aimDx = playerCenterX - drillCenterX;
                    const float aimDy = playerCenterY - drillCenterY;
                    const float aimLength = std::max(0.001f, std::hypot(aimDx, aimDy));
                    boss->drillAimX = aimDx / aimLength;
                    boss->drillAimY = aimDy / aimLength;
                    boss->drillDirection = boss->drillAimX >= 0.0f ? 1 : -1;
                    boss->facingRight = boss->drillDirection > 0;
                    if (boss->stateTimer >= boss->params.drillFormTime)
                    {
                        boss->drillFormed = true;
                        boss->drillActive = true;
                        for (Entity* fistEntity : boss->fistEntities)
                        {
                            if (auto* tint = fistEntity ? fistEntity->GetComponent<TintComponent>() : nullptr)
                            {
                                tint->a = 0.0f;
                            }
                        }
                    }
                }
                if (boss->drillActive)
                {
                    if (boss->stateTimer < boss->params.drillFormTime + boss->params.drillWaitTime)
                    {
                        const float shakePhase = boss->idleTimer * 92.0f;
                        const float shakeX = std::sin(shakePhase) * boss->params.drillChargeShakeAmplitude;
                        const float shakeY = std::cos(shakePhase * 1.31f) * boss->params.drillChargeShakeAmplitude * 0.45f;
                        boss->drillX = std::clamp(
                            boss->drillChargeBaseX + shakeX,
                            0.0f,
                            std::max(0.0f, mapWidth - boss->drillWidth));
                        boss->drillY = std::clamp(
                            boss->drillChargeBaseY + shakeY,
                            0.0f,
                            std::max(0.0f, mapHeight - boss->drillHeight));
                        const float chargeCenterX = boss->drillX + boss->drillWidth * 0.5f;
                        const float chargeCenterY = boss->drillY + boss->drillHeight * 0.5f;
                        const float aimDx = playerCenterX - chargeCenterX;
                        const float aimDy = playerCenterY - chargeCenterY;
                        const float aimLength = std::max(0.001f, std::hypot(aimDx, aimDy));
                        boss->drillAimX = aimDx / aimLength;
                        boss->drillAimY = aimDy / aimLength;
                        boss->drillDirection = boss->drillAimX >= 0.0f ? 1 : -1;
                        boss->facingRight = boss->drillDirection > 0;
                    }
                    else
                    {
                        if (!boss->drillGroundRush &&
                            std::fabs(boss->drillVelocityX) < 0.001f &&
                            std::fabs(boss->drillVelocityY) < 0.001f)
                        {
                            boss->drillVelocityX = boss->drillAimX * boss->params.drillLaunchSpeed;
                            boss->drillVelocityY = boss->drillAimY * boss->params.drillLaunchSpeed;
                            if (std::fabs(boss->drillVelocityX) < boss->params.drillLaunchSpeed * 0.25f)
                            {
                                boss->drillVelocityX = static_cast<float>(boss->drillDirection) * boss->params.drillLaunchSpeed * 0.25f;
                            }
                        }

                        if (boss->drillGroundRush)
                        {
                            const float nextX = boss->drillX + static_cast<float>(boss->drillDirection) * boss->params.drillRushSpeed * deltaTime;
                            const bool hitWall =
                                nextX < 0.0f ||
                                nextX + boss->drillWidth > mapWidth ||
                                rectIntersectsSolid(nextX, boss->drillY, boss->drillWidth, boss->drillHeight);
                            if (hitWall)
                            {
                                constexpr float kDrillImpactHitStopSeconds = 0.105f;
                                constexpr float kDrillImpactShakeSeconds = 0.40f;
                                constexpr float kDrillImpactShakeAmplitude = 56.0f;
                                flow.hitStopRemaining = std::max(flow.hitStopRemaining, kDrillImpactHitStopSeconds);
                                flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kDrillImpactShakeSeconds);
                                flow.screenShakeDuration = std::max(flow.screenShakeDuration, kDrillImpactShakeSeconds);
                                flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kDrillImpactShakeAmplitude);
                                spawnMidBoss3FistImpactEffect(
                                    boss->drillX,
                                    boss->drillY,
                                    boss->drillWidth,
                                    boss->drillHeight);
                                spawnSepiaCollisionRubble(
                                    boss->drillX,
                                    boss->drillY,
                                    boss->drillWidth,
                                    boss->drillHeight,
                                    SepiaRubbleSource::MidBoss3Drill);
                                boss->drillActive = false;
                            }
                            else
                            {
                                boss->drillX = nextX;
                            }
                        }
                        else
                        {
                            float nextX = boss->drillX + boss->drillVelocityX * deltaTime;
                            float nextY = boss->drillY + boss->drillVelocityY * deltaTime;
                            const bool hitWall =
                                nextX < 0.0f ||
                                nextX + boss->drillWidth > mapWidth ||
                                rectLeadingSideIntersectsSolid(
                                    nextX,
                                    nextY,
                                    boss->drillWidth,
                                    boss->drillHeight,
                                    boss->drillVelocityX);
                            const bool hitSolid = rectIntersectsSolid(nextX, nextY, boss->drillWidth, boss->drillHeight);
                            if (hitWall)
                            {
                                constexpr float kDrillImpactHitStopSeconds = 0.105f;
                                constexpr float kDrillImpactShakeSeconds = 0.40f;
                                constexpr float kDrillImpactShakeAmplitude = 56.0f;
                                flow.hitStopRemaining = std::max(flow.hitStopRemaining, kDrillImpactHitStopSeconds);
                                flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kDrillImpactShakeSeconds);
                                flow.screenShakeDuration = std::max(flow.screenShakeDuration, kDrillImpactShakeSeconds);
                                flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kDrillImpactShakeAmplitude);
                                spawnMidBoss3FistImpactEffect(
                                    boss->drillX,
                                    boss->drillY,
                                    boss->drillWidth,
                                    boss->drillHeight);
                                spawnSepiaCollisionRubble(
                                    boss->drillX,
                                    boss->drillY,
                                    boss->drillWidth,
                                    boss->drillHeight,
                                    SepiaRubbleSource::MidBoss3Drill);
                                boss->drillActive = false;
                            }
                            else if (hitSolid)
                            {
                                if (boss->drillVelocityY >= 0.0f)
                                {
                                    while (rectIntersectsSolid(nextX, nextY, boss->drillWidth, boss->drillHeight) && nextY > 0.0f)
                                    {
                                        nextY -= 2.0f;
                                    }
                                    boss->drillX = std::clamp(nextX, 0.0f, std::max(0.0f, mapWidth - boss->drillWidth));
                                    boss->drillY = std::clamp(nextY, 0.0f, std::max(0.0f, mapHeight - boss->drillHeight));
                                    boss->drillGroundRush = true;
                                    boss->drillVelocityX = static_cast<float>(boss->drillDirection) * boss->params.drillRushSpeed;
                                    boss->drillVelocityY = 0.0f;
                                    boss->drillAimX = static_cast<float>(boss->drillDirection);
                                    boss->drillAimY = 0.0f;
                                }
                                else
                                {
                                    constexpr float kDrillImpactHitStopSeconds = 0.105f;
                                    constexpr float kDrillImpactShakeSeconds = 0.40f;
                                    constexpr float kDrillImpactShakeAmplitude = 56.0f;
                                    flow.hitStopRemaining = std::max(flow.hitStopRemaining, kDrillImpactHitStopSeconds);
                                    flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kDrillImpactShakeSeconds);
                                    flow.screenShakeDuration = std::max(flow.screenShakeDuration, kDrillImpactShakeSeconds);
                                    flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kDrillImpactShakeAmplitude);
                                    spawnMidBoss3FistImpactEffect(
                                        boss->drillX,
                                        boss->drillY,
                                        boss->drillWidth,
                                        boss->drillHeight);
                                    spawnSepiaCollisionRubble(
                                        boss->drillX,
                                        boss->drillY,
                                        boss->drillWidth,
                                        boss->drillHeight,
                                        SepiaRubbleSource::MidBoss3Drill);
                                    boss->drillActive = false;
                                }
                            }
                            else
                            {
                                boss->drillX = nextX;
                                boss->drillY = nextY;
                            }
                        }

                        if (boss->drillActive)
                        {
                            const float drillHitInsetX = boss->drillWidth * 0.14f;
                            const float drillHitInsetY = boss->drillHeight * 0.22f;
                            TransformComponent drillRect(
                                boss->drillX + drillHitInsetX,
                                boss->drillY + drillHitInsetY,
                                std::max(1.0f, boss->drillWidth - drillHitInsetX * 2.0f),
                                std::max(1.0f, boss->drillHeight - drillHitInsetY * 2.0f));
                            for (const auto& target : entities)
                            {
                                if (!target ||
                                    target.get() == entity ||
                                    target.get() == player ||
                                    isEntityPendingRemove(target.get()))
                                {
                                    continue;
                                }

                                if (auto* targetFist = target->GetComponent<MidBoss3FistComponent>())
                                {
                                    auto* targetTransform = target->GetComponent<TransformComponent>();
                                    if (targetFist->ownerBoss == entity ||
                                        !targetTransform ||
                                        !IntersectsBounds(drillRect, *targetTransform))
                                    {
                                        continue;
                                    }

                                    breakFistAtCollision(*targetFist, *targetTransform);
                                    continue;
                                }

                                const auto* targetTransform = target->GetComponent<TransformComponent>();
                                if (!targetTransform || !IntersectsBounds(drillRect, *targetTransform))
                                {
                                    continue;
                                }

                                if (HasTag(*target, kTagEnemy))
                                {
                                    const auto* targetEnemy = target->GetComponent<EnemyComponent>();
                                    if (targetEnemy && targetEnemy->GetArchetype() == EnemyArchetype::MidBoss3)
                                    {
                                        continue;
                                    }
                                }

                                if (isSepiaObject(*target) || isSepiaDriveObject(*target))
                                {
                                    continue;
                                }

                                if (isFloorObject(*target))
                                {
                                    ++boss->drillFloorObjectHits;
                                    if (boss->drillFloorObjectHits >= 2)
                                    {
                                        queueDestroyEntity(target.get(), SepiaRubbleSource::MidBoss3Drill);
                                    }
                                    continue;
                                }

                                if (isBreakableObject(*target))
                                {
                                    queueDestroyEntity(target.get(), SepiaRubbleSource::MidBoss3Drill);
                                }
                            }

                            if (!boss->drillDamageApplied &&
                                player &&
                                playerTransform &&
                                IntersectsBounds(drillRect, *playerTransform))
                            {
                                handlePlayerDamage(entity, 2, "MidBoss3 drill damaged player");
                                boss->drillDamageApplied = true;
                            }
                        }
                    }

                    const bool drillOut =
                        boss->drillX + boss->drillWidth < 0.0f ||
                        boss->drillX > mapWidth ||
                        boss->drillY > mapHeight + boss->drillHeight;
                    if (drillOut || !boss->drillActive || boss->stateTimer >= boss->params.drillFormTime + boss->params.drillWaitTime + boss->params.drillCooldownTime + 3.0f)
                    {
                        boss->drillActive = false;
                        for (Entity* fistEntity : boss->fistEntities)
                        {
                            auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                            if (!fist)
                            {
                                continue;
                            }
                            fist->state = MidBoss3FistState::Broken;
                            fist->broken = true;
                            fist->captureJammerActive = false;
                        }
                        boss->state = MidBoss3State::AttackCooldown;
                        boss->stateTimer = 0.0f;
                    }
                }
            }
            else if (boss->state == MidBoss3State::AttackCooldown)
            {
                bool allFistsDone = true;
                for (Entity* fistEntity : boss->fistEntities)
                {
                    const auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                    if (fist &&
                        fist->state != MidBoss3FistState::Docked &&
                        fist->state != MidBoss3FistState::Broken)
                    {
                        allFistsDone = false;
                        break;
                    }
                }
                const float cooldown = boss->cooldownAttack == 4
                    ? boss->params.drillCooldownTime
                    : (boss->cooldownAttack == 2 ? boss->params.meteorCooldownTime : boss->params.launcherCooldownTime);
                if (boss->stateTimer >= cooldown && allFistsDone)
                {
                    const int finishedAttack = boss->cooldownAttack;
                    boss->state = MidBoss3State::Move;
                    boss->stateTimer = 0.0f;
                    boss->moveTimer = 0.0f;
                    boss->moveStep = 0;
                    boss->moving = false;
                    boss->launcherShotsFired = 0;
                    boss->meteorShotsFired = 0;
                    boss->reloadStartedForMove = false;
                    boss->reloadStartMoveStep = -1;
                    switch (finishedAttack)
                    {
                    case 1:
                        boss->nextFlowAttack = 3;
                        boss->movePattern = 1;
                        boss->chooseMoveSideFromStageCenter = false;
                        boss->reloadStartMoveStep = 0;
                        break;
                    case 2:
                        boss->nextFlowAttack = 1;
                        boss->movePattern = 0;
                        boss->chooseMoveSideFromStageCenter = true;
                        boss->reloadStartMoveStep = 1;
                        break;
                    case 3:
                        boss->nextFlowAttack = 4;
                        boss->movePattern = 0;
                        boss->chooseMoveSideFromStageCenter = true;
                        boss->reloadStartMoveStep = 1;
                        break;
                    case 4:
                        boss->nextFlowAttack = 2;
                        boss->movePattern = 1;
                        boss->chooseMoveSideFromStageCenter = false;
                        boss->reloadStartMoveStep = 0;
                        break;
                    default:
                        boss->nextFlowAttack = 2;
                        boss->movePattern = 0;
                        boss->chooseMoveSideFromStageCenter = true;
                        boss->reloadStartMoveStep = 0;
                        break;
                    }
                    boss->cooldownAttack = 0;
                    boss->launcherPrepared = false;
                    boss->drillActive = false;
                    boss->drillFormed = false;
                    boss->drillGroundRush = false;
                    boss->drillDamageApplied = false;
                }
            }

            const float bossWave = std::sin(boss->idleTimer * boss->params.idleFloatSpeed);
            transform->x = boss->homeX + boss->damageMotionOffsetX;
            transform->y = boss->homeY + bossWave * boss->params.idleFloatAmplitude + boss->damageMotionOffsetY;

            for (Entity* fistEntity : boss->fistEntities)
            {
                auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
                auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
                if (!fist || !fistTransform)
                {
                    continue;
                }
                const float dockX = getFistDockX(*fist, *fistTransform, transform->x);
                const float dockY = getFistDockY(*fist, *fistTransform);
                const float fistWidth = fistTransform->width * fistTransform->scale;
                const float fistHeight = fistTransform->height * fistTransform->scale;
                if (fist->impactAttackActive)
                {
                    fist->impactAttackRemaining -= deltaTime;
                    if (fist->impactAttackRemaining <= 0.0f)
                    {
                        fist->impactAttackActive = false;
                    }
                    else if (!fist->impactDamageApplied && player && playerTransform)
                    {
                        TransformComponent impactRect(
                            fist->impactAttackX,
                            fist->impactAttackY,
                            fist->impactAttackWidth,
                            fist->impactAttackHeight);
                        if (IntersectsBounds(impactRect, *playerTransform))
                        {
                            handlePlayerDamage(fistEntity, 1, "MidBoss3 meteor impact damaged player");
                            fist->impactDamageApplied = true;
                        }
                    }
                }
                const float rawLauncherStartX = boss->launcherDirection < 0
                    ? stageCenterX + kTileSize * 0.7f
                    : stageCenterX - fistWidth - kTileSize * 0.7f;
                const float launcherStartX = std::clamp(rawLauncherStartX, 0.0f, std::max(0.0f, mapWidth - fistWidth));
                const float launcherStartY = (fist->fistIndex == 0 || fist->fistIndex == 2)
                    ? boss->launcherLowerLaneY
                    : boss->launcherUpperLaneY;
                int meteorSlot = 1;
                if (fist->fistIndex == 2)
                {
                    meteorSlot = 2;
                }
                else if (fist->fistIndex == 1)
                {
                    meteorSlot = 3;
                }
                else if (fist->fistIndex == 3)
                {
                    meteorSlot = 4;
                }
                if (boss->state == MidBoss3State::LauncherMeteorFist)
                {
                    if (fist->fistIndex == 0)
                    {
                        meteorSlot = 2;
                    }
                    else if (fist->fistIndex == 1)
                    {
                        meteorSlot = 4;
                    }
                }
                const auto findMeteorTargetCenterX = [&](int slot) -> float
                {
                    struct MeteorRun
                    {
                        int start = 0;
                        int width = 0;
                    };

                    const int columnCount = std::max(1, static_cast<int>(mapWidth / kTileSize));
                    const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                    const auto runCenterX = [&](int startColumn, int width) -> float
                    {
                        return (static_cast<float>(startColumn) + static_cast<float>(width) * 0.5f) * kTileSize;
                    };

                    for (int row = rowCount - 1; row >= 0; --row)
                    {
                        std::vector<MeteorRun> gaps;
                        for (int column = 0; column < columnCount;)
                        {
                            if (isSolidTile(column, row))
                            {
                                ++column;
                                continue;
                            }

                            const int startColumn = column;
                            while (column < columnCount && !isSolidTile(column, row))
                            {
                                ++column;
                            }

                            const int gapWidth = column - startColumn;
                            if (gapWidth >= 4)
                            {
                                gaps.push_back({ startColumn, gapWidth });
                            }
                        }

                        if (gaps.size() >= 5)
                        {
                            const float centerColumn = stageCenterX / kTileSize;
                            int centerGapIndex = 0;
                            float bestCenterDistance = std::numeric_limits<float>::max();
                            for (int index = 0; index < static_cast<int>(gaps.size()); ++index)
                            {
                                const float gapCenterColumn =
                                    static_cast<float>(gaps[index].start) + static_cast<float>(gaps[index].width) * 0.5f;
                                const float distance = std::fabs(gapCenterColumn - centerColumn);
                                if (distance < bestCenterDistance)
                                {
                                    bestCenterDistance = distance;
                                    centerGapIndex = index;
                                }
                            }

                            const int safeSlot = std::clamp(slot - 1, 0, 3);
                            if (boss->meteorDirection < 0 && centerGapIndex >= 2)
                            {
                                const MeteorRun& outerGap = gaps[centerGapIndex - 2];
                                const MeteorRun& innerGap = gaps[centerGapIndex - 1];
                                if (safeSlot == 0)
                                {
                                    return runCenterX(outerGap.start, 4);
                                }
                                if (safeSlot == 1)
                                {
                                    return runCenterX(outerGap.start + outerGap.width, 4);
                                }
                                if (safeSlot == 2)
                                {
                                    return runCenterX(innerGap.start, 4);
                                }
                                return runCenterX(innerGap.start + innerGap.width, 4);
                            }
                            if (boss->meteorDirection >= 0 && centerGapIndex + 2 < static_cast<int>(gaps.size()))
                            {
                                const MeteorRun& innerGap = gaps[centerGapIndex + 1];
                                const MeteorRun& outerGap = gaps[centerGapIndex + 2];
                                if (safeSlot == 0)
                                {
                                    return runCenterX(outerGap.start, 4);
                                }
                                if (safeSlot == 1)
                                {
                                    return runCenterX(outerGap.start - 4, 4);
                                }
                                if (safeSlot == 2)
                                {
                                    return runCenterX(innerGap.start, 4);
                                }
                                return runCenterX(innerGap.start - 4, 4);
                            }
                        }
                    }

                    const float stageHalfWidth = mapWidth * 0.5f;
                    return boss->meteorDirection < 0
                        ? stageHalfWidth * (static_cast<float>(slot) / 5.0f)
                        : mapWidth - stageHalfWidth * (static_cast<float>(slot) / 5.0f);
                };
                const auto findMeteorGroundY = [&](float centerX) -> float
                {
                    const int centerColumn = std::clamp(
                        static_cast<int>(centerX / kTileSize),
                        0,
                        std::max(0, static_cast<int>(mapWidth / kTileSize) - 1));
                    const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                    for (int row = rowCount / 2; row < rowCount; ++row)
                    {
                        if (isSolidTile(centerColumn, row))
                        {
                            return static_cast<float>(row) * kTileSize;
                        }
                    }
                    return mapHeight;
                };
                const auto initialMeteorCenterX = [&]() -> float
                {
                    if (boss->state == MidBoss3State::LauncherMeteorFist)
                    {
                        const float offsets[2] = { -3.4f, 3.4f };
                        const int index = std::clamp(fist->fistIndex, 0, 1);
                        return playerCenterX + offsets[index] * kTileSize;
                    }
                    const float offsets[4] = { -7.6f, -2.8f, 2.8f, 7.6f };
                    const int index = std::clamp(fist->fistIndex, 0, 3);
                    return playerCenterX + offsets[index] * kTileSize;
                };
                const float meteorCenterX = (boss->state == MidBoss3State::MeteorFist ||
                    boss->state == MidBoss3State::LauncherMeteorFist)
                    ? initialMeteorCenterX()
                    : findMeteorTargetCenterX(meteorSlot);
                const float meteorArenaMinCenterX = std::max(
                    fistWidth * 0.5f + kTileSize,
                    stageCenterX - kTileSize * 15.0f);
                const float meteorArenaMaxCenterX = std::min(
                    mapWidth - fistWidth * 0.5f - kTileSize,
                    stageCenterX + kTileSize * 15.0f);
                const float safeMeteorCenterX = std::clamp(
                    meteorCenterX,
                    meteorArenaMinCenterX,
                    std::max(meteorArenaMinCenterX, meteorArenaMaxCenterX));
                const float meteorStartX = std::clamp(
                    safeMeteorCenterX - fistWidth * 0.5f,
                    0.0f,
                    std::max(0.0f, mapWidth - fistWidth));
                const float meteorGroundY = findMeteorGroundY(safeMeteorCenterX);
                const bool outerMeteorFist = fist->fistIndex == 0 || fist->fistIndex == 3;
                const float meteorStartOffsetGrid =
                    (outerMeteorFist ? 7.8f : 11.4f) +
                    (boss->state == MidBoss3State::LauncherMeteorFist ? 3.0f : 0.0f);
                const float rawMeteorStartY = std::clamp(
                    meteorGroundY - fistHeight - meteorStartOffsetGrid * kTileSize,
                    0.0f,
                    std::max(0.0f, mapHeight - fistHeight));
                const auto rectIntersectsSolidTileOnly = [&](float x, float y, float width, float height) -> bool
                {
                    if (x < 0.0f || y < 0.0f || x + width > mapWidth || y + height > mapHeight)
                    {
                        return true;
                    }

                    const int columnCount = std::max(1, static_cast<int>(mapWidth / kTileSize));
                    const int rowCount = std::max(1, static_cast<int>(mapHeight / kTileSize));
                    const int leftColumn = static_cast<int>(std::floor(x / kTileSize));
                    const int rightColumn = static_cast<int>(std::floor((x + width - 1.0f) / kTileSize));
                    const int topRow = static_cast<int>(std::floor(y / kTileSize));
                    const int bottomRow = static_cast<int>(std::floor((y + height - 1.0f) / kTileSize));
                    for (int row = topRow; row <= bottomRow; ++row)
                    {
                        for (int column = leftColumn; column <= rightColumn; ++column)
                        {
                            if (column < 0 || column >= columnCount || row < 0 || row >= rowCount)
                            {
                                return true;
                            }
                            if (isSolidTile(column, row))
                            {
                                return true;
                            }
                        }
                    }
                    return false;
                };
                const auto findSafeMeteorStartPosition = [&](float desiredX, float desiredY) -> std::pair<float, float>
                {
                    const float maxX = std::max(0.0f, mapWidth - fistWidth);
                    const float maxY = std::max(0.0f, mapHeight - fistHeight);
                    const float safeX = std::clamp(desiredX, kTileSize, std::max(kTileSize, maxX - kTileSize));
                    for (int yStep = 0; yStep <= 28; ++yStep)
                    {
                        const float candidateY = std::clamp(
                            desiredY + static_cast<float>(yStep) * kTileSize * 0.5f,
                            0.0f,
                            maxY);
                        if (!rectIntersectsSolidTileOnly(safeX, candidateY, fistWidth, fistHeight))
                        {
                            return { safeX, candidateY };
                        }
                    }
                    return { safeX, std::clamp(desiredY, 0.0f, maxY) };
                };
                const auto safeMeteorStart = findSafeMeteorStartPosition(meteorStartX, rawMeteorStartY);
                const float safeMeteorStartX = safeMeteorStart.first;
                const float meteorStartY = safeMeteorStart.second;
                const auto handleFistObjectCollision = [&](bool breakOnStageObject = true) -> bool
                {
                    Entity* hitObject = getProjectileObjectCollision(*fistTransform, fistEntity);
                    if (!hitObject)
                    {
                        return false;
                    }

                    if (hitObject == entity || hitObject->GetComponent<MidBoss3Component>())
                    {
                        return false;
                    }

                    if (isSepiaObject(*hitObject) || isSepiaDriveObject(*hitObject))
                    {
                        return false;
                    }

                    if (auto* otherFist = hitObject->GetComponent<MidBoss3FistComponent>())
                    {
                        auto* otherTransform = hitObject->GetComponent<TransformComponent>();
                        const bool otherIsActive =
                            otherFist->state == MidBoss3FistState::Launching ||
                            otherFist->state == MidBoss3FistState::MeteorFalling;
                        if (otherTransform && otherFist->ownerBoss != entity && otherIsActive)
                        {
                            breakFistAtCollision(*otherFist, *otherTransform);
                            breakFistAtCollision(*fist, *fistTransform);
                            return true;
                        }
                        return false;
                    }

                    if (HasTag(*hitObject, kTagEnemy))
                    {
                        const auto* targetEnemy = hitObject->GetComponent<EnemyComponent>();
                        if (targetEnemy && targetEnemy->GetArchetype() == EnemyArchetype::MidBoss3)
                        {
                            return false;
                        }
                    }

                    if (isFloorObject(*hitObject))
                    {
                        if (!breakOnStageObject)
                        {
                            return false;
                        }
                        breakFistAtCollision(*fist, *fistTransform);
                        return true;
                    }

                    if (HasTag(*hitObject, "Shutter"))
                    {
                        if (!breakOnStageObject)
                        {
                            return false;
                        }
                        breakFistAtCollision(*fist, *fistTransform);
                        return true;
                    }

                    if (isBreakableObject(*hitObject))
                    {
                        if (!breakOnStageObject)
                        {
                            return false;
                        }
                        queueDestroyEntity(hitObject, SepiaRubbleSource::MidBoss3Fist);
                        breakFistAtCollision(*fist, *fistTransform);
                        return true;
                    }

                    return false;
                };

                if (fist->state == MidBoss3FistState::Docked)
                {
                    const float followStep = boss->params.fistReturnSpeed * deltaTime;
                    fistTransform->x = moveToward(fistTransform->x, dockX, followStep);
                    fistTransform->y = moveToward(fistTransform->y, dockY, followStep);
                    fistTransform->rotation = 0.0f;
                }
                else if (fist->state == MidBoss3FistState::LauncherReady)
                {
                    const float followStep = boss->params.fistReturnSpeed * deltaTime;
                    fistTransform->x = moveToward(fistTransform->x, launcherStartX, followStep);
                    fistTransform->y = moveToward(fistTransform->y, launcherStartY, followStep);
                    fistTransform->rotation = 0.0f;
                    fist->atAttackStart =
                        std::fabs(fistTransform->x - launcherStartX) <= boss->params.fistPreLaunchShakeAmplitude + 1.0f &&
                        std::fabs(fistTransform->y - launcherStartY) <= boss->params.fistPreLaunchShakeAmplitude + 1.0f;
                    if (fist->atAttackStart)
                    {
                        fist->attackReadyTimer += deltaTime;
                        const float phase = boss->idleTimer * 104.0f + static_cast<float>(fist->fistIndex) * 2.17f;
                        const float shakeY = std::sin(phase) * boss->params.fistPreLaunchShakeAmplitude;
                        fistTransform->x = launcherStartX;
                        fistTransform->y = launcherStartY + shakeY;
                    }
                    else
                    {
                        fist->attackReadyTimer = 0.0f;
                    }
                }
                else if (fist->state == MidBoss3FistState::MeteorReady)
                {
                    if (!fist->meteorHoldInitialized)
                    {
                        fist->meteorHoldX = safeMeteorStartX;
                        fist->meteorHoldY = meteorStartY;
                        fist->meteorTargetGroundY = meteorGroundY;
                        fist->meteorHoldInitialized = true;
                    }
                    if (rectIntersectsSolidTileOnly(fist->meteorHoldX, fist->meteorHoldY, fistWidth, fistHeight))
                    {
                        const auto correctedMeteorStart = findSafeMeteorStartPosition(fist->meteorHoldX, fist->meteorHoldY);
                        fist->meteorHoldX = correctedMeteorStart.first;
                        fist->meteorHoldY = correctedMeteorStart.second;
                        fist->meteorTargetGroundY = findMeteorGroundY(fist->meteorHoldX + fistWidth * 0.5f);
                    }
                    const float followStep = boss->params.fistReturnSpeed * deltaTime;
                    fistTransform->x = moveToward(fistTransform->x, fist->meteorHoldX, followStep);
                    fistTransform->y = moveToward(fistTransform->y, fist->meteorHoldY, followStep);
                    const bool reachedAttackStart =
                        std::fabs(fistTransform->x - fist->meteorHoldX) <= boss->params.fistPreLaunchShakeAmplitude + 1.0f &&
                        std::fabs(fistTransform->y - fist->meteorHoldY) <= boss->params.fistPreLaunchShakeAmplitude + 1.0f;
                    fist->atAttackStart = reachedAttackStart;
                    if (fist->atAttackStart)
                    {
                        const float fistCenterX = fistTransform->x + fistWidth * 0.5f;
                        const float fistCenterY = fistTransform->y + fistHeight * 0.5f;
                        const float aimDx = playerCenterX - fistCenterX;
                        const float aimDy = playerCenterY - fistCenterY;
                        const float aimLength = std::max(0.001f, std::hypot(aimDx, aimDy));
                        fist->meteorAimX = aimDx / aimLength;
                        fist->meteorAimY = aimDy / aimLength;
                        fistTransform->rotation = std::atan2(fist->meteorAimY, fist->meteorAimX) + 3.14159265f;
                        fist->attackReadyTimer += deltaTime;
                    }
                    else
                    {
                        fistTransform->rotation = -1.5707963f;
                        fist->attackReadyTimer = 0.0f;
                    }
                }
                else if (fist->state == MidBoss3FistState::DrillForming)
                {
                    const int slotX = fist->fistIndex % 2;
                    const int slotY = fist->fistIndex / 2;
                    const float cellWidth = boss->drillWidth * 0.5f;
                    const float cellHeight = boss->drillHeight * 0.5f;
                    const float targetX =
                        boss->drillX + cellWidth * static_cast<float>(slotX) + cellWidth * 0.5f - fistWidth * 0.5f;
                    const float targetY =
                        boss->drillY + cellHeight * static_cast<float>(slotY) + cellHeight * 0.5f - fistHeight * 0.5f;
                    const float followStep = boss->params.fistReturnSpeed * 1.35f * deltaTime;
                    fistTransform->x = moveToward(fistTransform->x, targetX, followStep);
                    fistTransform->y = moveToward(fistTransform->y, targetY, followStep);
                    fistTransform->rotation = boss->drillGroundRush ? 0.0f : std::atan2(boss->drillAimY, boss->drillAimX);
                    if (auto* tint = fistEntity->GetComponent<TintComponent>())
                    {
                        tint->a = boss->drillFormed ? 0.0f : 1.0f;
                    }
                }
                else if (fist->state == MidBoss3FistState::Launching)
                {
                    fist->launchTimer += deltaTime;
                    const float previousFistX = fistTransform->x;
                    const float previousFistY = fistTransform->y;
                    fistTransform->x += fist->velocityX * deltaTime;
                    fistTransform->y += fist->velocityY * deltaTime;
                    fistTransform->rotation = 0.0f;
                    const float launchDirection = fist->velocityX >= 0.0f ? 1.0f : -1.0f;
                    const float acceleratedSpeed = std::min(
                        boss->params.launcherFistMaxSpeed,
                        std::fabs(fist->velocityX) + boss->params.launcherFistAcceleration * deltaTime);
                    fist->velocityX = launchDirection * acceleratedSpeed;

                    const auto sweptLauncherHitSolid = [&]() -> bool
                    {
                        const float travelX = fistTransform->x - previousFistX;
                        const float travelY = fistTransform->y - previousFistY;
                        const float travelLength = std::max(std::fabs(travelX), std::fabs(travelY));
                        const int steps = std::max(1, static_cast<int>(std::ceil(travelLength / std::max(1.0f, kTileSize * 0.35f))));
                        for (int step = 1; step <= steps; ++step)
                        {
                            const float t = static_cast<float>(step) / static_cast<float>(steps);
                            const float sampleX = previousFistX + travelX * t;
                            const float sampleY = previousFistY + travelY * t;
                            if (rectLeadingSideIntersectsSolid(sampleX, sampleY, fistWidth, fistHeight, fist->velocityX, fistEntity))
                            {
                                return true;
                            }
                        }
                        return false;
                    };
                    const bool hitSolidTile = sweptLauncherHitSolid();
                    const bool outOfBounds =
                        fistTransform->x < -fistWidth ||
                        fistTransform->x > mapWidth + fistWidth ||
                        fistTransform->y < -fistHeight ||
                        fistTransform->y > mapHeight + fistHeight;

                    if (!fist->damageApplied &&
                        player &&
                        playerTransform &&
                        IntersectsBounds(*fistTransform, *playerTransform))
                    {
                        handlePlayerDamage(fistEntity, 1, "MidBoss3 launcher fist damaged player");
                        fist->damageApplied = true;
                    }

                    const bool hitObject = handleFistObjectCollision();
                    if (!hitObject && (hitSolidTile || outOfBounds))
                    {
                        if (hitSolidTile)
                        {
                            constexpr float kFistImpactHitStopSeconds = 0.075f;
                            constexpr float kFistImpactShakeSeconds = 0.32f;
                            constexpr float kFistImpactShakeAmplitude = 38.0f;
                            flow.hitStopRemaining = std::max(flow.hitStopRemaining, kFistImpactHitStopSeconds);
                            flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kFistImpactShakeSeconds);
                            flow.screenShakeDuration = std::max(flow.screenShakeDuration, kFistImpactShakeSeconds);
                            flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kFistImpactShakeAmplitude);
                            spawnMidBoss3FistImpactEffect(
                                fistTransform->x,
                                fistTransform->y,
                                fistWidth,
                                fistHeight);
                            spawnSepiaCollisionRubble(
                                fistTransform->x,
                                fistTransform->y,
                                fistWidth,
                                fistHeight,
                                SepiaRubbleSource::MidBoss3Fist);
                        }
                        fist->state = MidBoss3FistState::Broken;
                        fist->velocityX = 0.0f;
                        fist->velocityY = 0.0f;
                        fist->broken = true;
                        fist->captureJammerActive = false;
                        if (auto* tint = fistEntity->GetComponent<TintComponent>())
                        {
                            tint->a = 0.0f;
                        }
                    }
                }
                else if (fist->state == MidBoss3FistState::MeteorFalling)
                {
                    fist->launchTimer += deltaTime;
                    const float previousFistX = fistTransform->x;
                    const float previousFistY = fistTransform->y;
                    fistTransform->x += fist->velocityX * deltaTime;
                    fistTransform->y += fist->velocityY * deltaTime;
                    fistTransform->rotation = std::atan2(fist->velocityY, fist->velocityX) + 3.14159265f;

                    const float previousBottom = previousFistY + fistHeight;
                    const float currentBottom = fistTransform->y + fistHeight;
                    const float targetGroundY = fist->meteorTargetGroundY > 0.0f
                        ? fist->meteorTargetGroundY
                        : meteorGroundY;
                    const bool hitGroundTile =
                        fist->velocityY > 0.0f &&
                        previousBottom < targetGroundY &&
                        currentBottom >= targetGroundY;
                    const bool outOfBounds =
                        fistTransform->x < -fistWidth ||
                        fistTransform->x > mapWidth + fistWidth ||
                        fistTransform->y < -fistHeight ||
                        fistTransform->y > mapHeight + fistHeight;

                    if (!fist->damageApplied &&
                        player &&
                        playerTransform &&
                        IntersectsBounds(*fistTransform, *playerTransform))
                    {
                        handlePlayerDamage(fistEntity, 1, "MidBoss3 meteor fist damaged player");
                        fist->damageApplied = true;
                    }

                    const bool hitObject = handleFistObjectCollision(false);
                    if (!hitObject && (hitGroundTile || outOfBounds))
                    {
                        const float impactCenterX = fistTransform->x + fistWidth * 0.5f;
                        const float impactCenterY = fistTransform->y + fistHeight * 0.5f;
                        fist->impactAttackX = std::clamp(
                            impactCenterX - kTileSize * 2.0f,
                            0.0f,
                            std::max(0.0f, mapWidth - kTileSize * 4.0f));
                        fist->impactAttackY = std::clamp(impactCenterY - kTileSize, 0.0f, std::max(0.0f, mapHeight - kTileSize * 2.0f));
                        fist->impactAttackWidth = kTileSize * 4.0f;
                        fist->impactAttackHeight = kTileSize * 2.0f;
                        fist->impactAttackRemaining = 0.16f;
                        fist->impactAttackActive = true;
                        fist->impactDamageApplied = false;
                        if (hitGroundTile)
                        {
                            constexpr float kFistImpactHitStopSeconds = 0.075f;
                            constexpr float kFistImpactShakeSeconds = 0.32f;
                            constexpr float kFistImpactShakeAmplitude = 38.0f;
                            flow.hitStopRemaining = std::max(flow.hitStopRemaining, kFistImpactHitStopSeconds);
                            flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kFistImpactShakeSeconds);
                            flow.screenShakeDuration = std::max(flow.screenShakeDuration, kFistImpactShakeSeconds);
                            flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kFistImpactShakeAmplitude);
                            spawnMidBoss3FistImpactEffect(
                                fist->impactAttackX,
                                fist->impactAttackY,
                                fist->impactAttackWidth,
                                fist->impactAttackHeight);
                            spawnSepiaCollisionRubble(
                                fist->impactAttackX,
                                fist->impactAttackY,
                                fist->impactAttackWidth,
                                fist->impactAttackHeight,
                                SepiaRubbleSource::MidBoss3Fist);
                        }

                        if (!fist->damageApplied && player && playerTransform)
                        {
                            TransformComponent impactRect(
                                fist->impactAttackX,
                                fist->impactAttackY,
                                fist->impactAttackWidth,
                                fist->impactAttackHeight);
                            if (IntersectsBounds(impactRect, *playerTransform))
                            {
                                handlePlayerDamage(fistEntity, 1, "MidBoss3 meteor impact damaged player");
                                fist->damageApplied = true;
                                fist->impactDamageApplied = true;
                            }
                        }

                        fist->state = MidBoss3FistState::Broken;
                        fist->velocityX = 0.0f;
                        fist->velocityY = 0.0f;
                        fist->broken = true;
                        fist->captureJammerActive = false;
                        if (auto* tint = fistEntity->GetComponent<TintComponent>())
                        {
                            tint->a = 0.0f;
                        }
                    }
                }
                else if (fist->state == MidBoss3FistState::Reloading)
                {
                    const float progress = smoothStep(boss->reloadTimer / std::max(0.01f, boss->params.fistReloadTime));
                    fistTransform->x = fist->reloadStartX + (dockX - fist->reloadStartX) * progress;
                    fistTransform->y = fist->reloadStartY + (dockY - fist->reloadStartY) * progress;
                    fistTransform->rotation = 0.0f;
                    fist->broken = false;
                    fist->captureJammerActive = false;
                    if (auto* tint = fistEntity->GetComponent<TintComponent>())
                    {
                        tint->a = 1.0f;
                    }
                }
                else if (fist->state == MidBoss3FistState::Broken)
                {
                    fist->captureJammerActive = false;
                    if (auto* tint = fistEntity->GetComponent<TintComponent>())
                    {
                        tint->a = 0.0f;
                    }
                }
                const bool contactDamageState = false;
                if (contactDamageState &&
                    !fist->damageApplied &&
                    player &&
                    playerTransform &&
                    IntersectsBounds(*fistTransform, *playerTransform))
                {
                    handlePlayerDamage(fistEntity, 1, "MidBoss3 fist contact damaged player");
                    fist->damageApplied = true;
                }
                if (auto* fistSprite = fistEntity->GetComponent<SpriteRenderComponent>())
                {
                    const bool usesAimedRotation =
                        fist->state == MidBoss3FistState::MeteorReady ||
                        fist->state == MidBoss3FistState::MeteorFalling;
                    const bool usesLockedLauncherFacing =
                        fist->state == MidBoss3FistState::LauncherReady ||
                        fist->state == MidBoss3FistState::Launching;
                    const bool facingRight = usesAimedRotation
                        ? false
                        : (usesLockedLauncherFacing
                            ? fist->lockedFacingRight
                            : (fist->velocityX > 0.0f || (boss->facingRight && std::fabs(fist->velocityX) <= 0.01f)));
                    fistSprite->SetFlipX(facingRight);
                }
            }
            if (boss->state == MidBoss3State::Move && boss->moving)
            {
                playMidBoss3Clip(boss->moveTargetX >= boss->moveStartX ? "move_right" : "move_left");
            }
            else
            {
                playMidBoss3Clip("idle");
            }
            continue;
        }

        if (enemy->knockbackActive)
        {
            enemy->knockbackTimer += flow.lastDeltaTime;
            const float progress = std::min(1.0f, enemy->knockbackTimer / enemy->knockbackDuration);
            const float moveProgress = std::sin(progress * 3.1415926f * 0.5f);
            transform->x = enemy->knockbackStartX + (enemy->knockbackTargetX - enemy->knockbackStartX) * moveProgress;
            transform->y = enemy->knockbackStartY - std::sin(progress * 3.1415926f) * enemy->knockbackHeight * 48.0f;
            if (progress >= 1.0f)
            {
                transform->x = enemy->knockbackTargetX;
                transform->y = enemy->knockbackStartY;
                enemy->knockbackActive = false;
                enemy->knockbackTimer = 0.0f;
            }
            continue;
        }

        if (enemy->GetArchetype() == EnemyArchetype::Walker)
        {
            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::fabs(dx);
            bool walkerMoved = false;
            constexpr float kWalkerSpeed = 150.0f;
            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            constexpr float kWalkerStopDistance = 48.0f;
            constexpr float kWalkerAttackActiveSeconds = 0.18f;
            constexpr float kWalkerAttackFps = 18.0f;
            constexpr int kWalkerAttackFirstFrame = 24;
            constexpr int kWalkerAttackChargeFrame = 28;
            constexpr float kWalkerAttackChargeSeconds = 0.20f;
            constexpr int kWalkerAttackHitFrame = 32;
            constexpr int kWalkerAttackLastFrame = 39;
            constexpr int kWalkerAttackCaptureStartFrame = kWalkerAttackFirstFrame;
            constexpr int kWalkerAttackCaptureEndFrame = kWalkerAttackLastFrame;
            constexpr float kWalkerAttackFlashSeconds = 0.18f;
            constexpr float kWalkerVisualScale = 1.55f;
            constexpr float kWalkerVisualOffsetY = -22.0f;

            const float enemyWidth = transform->width * transform->scale;
            const float playerWidth = playerTransform->width * playerTransform->scale;
            const float enemyCenterX = transform->x + enemyWidth * 0.5f;
            const float playerCenterX = playerTransform->x + playerWidth * 0.5f;
            const float centerDx = playerCenterX - enemyCenterX;
            const float horizontalGap = centerDx >= 0.0f
                ? playerTransform->x - (transform->x + enemyWidth)
                : transform->x - (playerTransform->x + playerWidth);
            const bool inAttackRange = horizontalGap <= kWalkerStopDistance;
            const bool inDetectRange = dist < enemy->detectRange && std::fabs(dy) < enemy->detectHeight;

            const bool onGround = applyEnemyGravity(*enemy, *transform, kGravity, kMaxFallSpeed);
            if (onGround)
            {
                enemy->velocityY = 0.0f;
            }

            if (inDetectRange && enemy->GetAIState() != EnemyComponent::AIState::Attack)
            {
                enemy->facing = centerDx > 0.0f
                    ? EnemyComponent::FacingDirection::Right
                    : EnemyComponent::FacingDirection::Left;
            }

            if (enemy->attackRectActive)
            {
                enemy->attackRectRemaining -= flow.lastDeltaTime;
                if (enemy->attackRectRemaining <= 0.0f)
                {
                    enemy->attackRectActive = false;
                }
            }
            if (enemy->attackFlashRemaining > 0.0f)
            {
                enemy->attackFlashRemaining = std::max(0.0f, enemy->attackFlashRemaining - flow.lastDeltaTime);
            }

            switch (enemy->GetAIState())
            {
            case EnemyComponent::AIState::Idle:
                enemy->attackCaptureWindowActive = false;
                enemy->attackWarningProgress = 0.0f;
                if (inDetectRange)
                {
                    enemy->attackTimer = enemy->attackCooldown;
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                break;
            case EnemyComponent::AIState::Chase:
                enemy->attackCaptureWindowActive = false;
                enemy->attackWarningProgress = 0.0f;
                enemy->attackTimer = std::min(enemy->attackCooldown, enemy->attackTimer + flow.lastDeltaTime);
                if (inAttackRange)
                {
                    enemy->facing = centerDx > 0.0f
                        ? EnemyComponent::FacingDirection::Right
                        : EnemyComponent::FacingDirection::Left;

                    if (enemy->attackTimer >= enemy->attackCooldown)
                    {
                        enemy->attackTimer = 0.0f;
                        enemy->attackFrameTriggered = false;
                        enemy->SetAIState(EnemyComponent::AIState::Attack);
                        if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>())
                        {
                            animation->SetPlaybackSpeed(1.0f);
                            animation->Play("attack", true);
                        }
                    }
                }
                else if (!inDetectRange)
                {
                    enemy->attackTimer = enemy->attackCooldown;
                    enemy->SetAIState(EnemyComponent::AIState::Idle);
                }
                else
                {
                    transform->x += (centerDx > 0.0f ? 1.0f : -1.0f) * kWalkerSpeed * flow.lastDeltaTime;
                    snapToGround(*transform);
                    walkerMoved = true;
                }
                break;
            case EnemyComponent::AIState::Attack:
                enemy->attackTimer += flow.lastDeltaTime;
                if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>())
                {
                    int attackFrame = animation->GetCurrentFrameIndex();
                    const float chargeStartSeconds =
                        static_cast<float>(kWalkerAttackChargeFrame - kWalkerAttackFirstFrame) / kWalkerAttackFps;
                    const float chargeElapsedSeconds = std::max(0.0f, enemy->attackTimer - chargeStartSeconds);
                    const bool charging =
                        attackFrame >= kWalkerAttackChargeFrame &&
                        !enemy->attackFrameTriggered &&
                        chargeElapsedSeconds < kWalkerAttackChargeSeconds;
                    if (charging)
                    {
                        animation->SetCurrentLocalFrameIndex(kWalkerAttackChargeFrame - kWalkerAttackFirstFrame);
                        animation->SetPlaybackSpeed(0.0f);
                        attackFrame = kWalkerAttackChargeFrame;
                        if (auto* sprite = entity->GetComponent<SpriteRenderComponent>())
                        {
                            const float baseOffsetX = transform->width * (1.0f - kWalkerVisualScale) * 0.5f;
                            const float shakePhase = enemy->attackTimer * 72.0f;
                            const float shakeX = std::sin(shakePhase) * 2.2f + std::sin(shakePhase * 1.7f) * 0.8f;
                            const float shakeY = std::cos(shakePhase * 1.23f) * 1.4f;
                            sprite->SetRenderOffset(baseOffsetX + shakeX, kWalkerVisualOffsetY + shakeY);
                        }
                    }
                    else
                    {
                        animation->SetPlaybackSpeed(1.0f);
                        if (auto* sprite = entity->GetComponent<SpriteRenderComponent>())
                        {
                            sprite->SetRenderOffset(
                                transform->width * (1.0f - kWalkerVisualScale) * 0.5f,
                                kWalkerVisualOffsetY);
                        }
                    }

                    enemy->attackWarningProgress = std::clamp(
                        charging
                            ? std::max(
                                static_cast<float>(attackFrame - kWalkerAttackFirstFrame) /
                                    static_cast<float>(kWalkerAttackHitFrame - kWalkerAttackFirstFrame),
                                chargeElapsedSeconds / kWalkerAttackChargeSeconds)
                            : static_cast<float>(attackFrame - kWalkerAttackFirstFrame) /
                                static_cast<float>(kWalkerAttackHitFrame - kWalkerAttackFirstFrame),
                        0.0f,
                        1.0f);
                    enemy->attackCaptureWindowActive =
                        attackFrame >= kWalkerAttackCaptureStartFrame &&
                        attackFrame <= kWalkerAttackCaptureEndFrame;
                    if (!enemy->attackFrameTriggered && attackFrame >= kWalkerAttackHitFrame)
                    {
                        enemy->attackFrameTriggered = true;
                        enemy->attackFlashRemaining = kWalkerAttackFlashSeconds;
                        const float attackWidth = 64.0f;
                        const float attackHeight = 60.0f;
                        const float attackForwardOffset = 16.0f;
                        const float attackOffsetY = transform->height * transform->scale * -0.1f;

                        enemy->attackRectX = enemy->facing == EnemyComponent::FacingDirection::Right
                            ? transform->x + transform->width * transform->scale + attackForwardOffset
                            : transform->x - attackWidth - attackForwardOffset;
                        enemy->attackRectY = transform->y + attackOffsetY;
                        enemy->attackRectWidth = attackWidth;
                        enemy->attackRectHeight = attackHeight;
                        enemy->attackRectRemaining = kWalkerAttackActiveSeconds;
                        enemy->attackRectActive = true;
                    }

                    if (attackFrame >= kWalkerAttackLastFrame)
                    {
                        animation->SetPlaybackSpeed(1.0f);
                        enemy->attackTimer = 0.0f;
                        enemy->attackFrameTriggered = false;
                        enemy->attackCaptureWindowActive = false;
                        enemy->attackWarningProgress = 0.0f;
                        enemy->attackRectActive = false;
                        enemy->SetAIState(EnemyComponent::AIState::Chase);
                    }
                }
                else if (enemy->attackTimer >= enemy->attackCooldown)
                {
                    enemy->attackTimer = 0.0f;
                    enemy->attackFrameTriggered = false;
                    enemy->attackCaptureWindowActive = false;
                    enemy->attackWarningProgress = 0.0f;
                    enemy->attackRectActive = false;
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                break;
            }

            UpdateWalkerSpriteAnimation(*entity, *enemy, walkerMoved);
        }
        else if (enemy->GetArchetype() == EnemyArchetype::Ranged)
        {
            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            const bool onGround = applyEnemyGravity(*enemy, *transform, kGravity, kMaxFallSpeed);
            if (onGround)
            {
                enemy->velocityY = 0.0f;
            }

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            const bool facingRight = enemy->facing == EnemyComponent::FacingDirection::Right;
            const bool playerInFacingDirection = facingRight ? dx >= 0.0f : dx <= 0.0f;
            const bool inDetectRange = playerInFacingDirection && dist < enemy->detectRange && std::fabs(dy) < enemy->detectHeight;
            if (!inDetectRange && enemy->GetAIState() != EnemyComponent::AIState::Attack)
            {
                enemy->attackTimer = enemy->attackCooldown;
                enemy->attackFrameTriggered = false;
                enemy->SetAIState(EnemyComponent::AIState::Idle);
                PlayRangedSpriteAnimation(*entity, "idle");
                continue;
            }

            auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>();
            const auto startRangedAttack = [&]()
                {
                    enemy->attackTimer = 0.0f;
                    enemy->attackFrameTriggered = false;
                    enemy->SetAIState(EnemyComponent::AIState::Attack);
                    PlayRangedSpriteAnimation(*entity, "attack", true);
                };

            if (enemy->GetAIState() != EnemyComponent::AIState::Attack)
            {
                PlayRangedSpriteAnimation(*entity, "idle");
                enemy->attackTimer += flow.lastDeltaTime;

                if (enemy->attackTimer >= enemy->attackCooldown)
                {
                    startRangedAttack();
                }
                continue;
            }

            PlayRangedSpriteAnimation(*entity, "attack");
            const int attackFrame = animation ? animation->GetCurrentFrameIndex() : 0;
            constexpr int kEnemy2AttackFireFrame = 38;
            constexpr int kEnemy2AttackLastFrame = 79;

            if (!enemy->attackFrameTriggered && attackFrame >= kEnemy2AttackFireFrame)
            {
                enemy->attackFrameTriggered = true;

                constexpr float kBulletSpeed = 450.0f;
                const float shotDirection = facingRight ? 1.0f : -1.0f;
                const float velX = shotDirection * kBulletSpeed;
                const float velY = 0.0f;

                auto bullet = std::make_unique<Entity>();
                bullet->AddComponent<TagComponent>(kTagBullet);
                bullet->AddComponent<TransformComponent>(
                    transform->x + (facingRight ? transform->width * transform->scale : -24.0f),
                    transform->y + 24.0f,
                    48.0f, 24.0f);
                bullet->AddComponent<TintComponent>(1.0f, 0.9f, 0.2f, 1.0f);
                bullet->AddComponent<SpriteRenderComponent>(tileTexture);
                bullet->AddComponent<ProjectileComponent>(velX, velY, 1);
                playEnemyGun(*entity);
                newBullets.push_back(std::move(bullet));
            }

            if (attackFrame >= kEnemy2AttackLastFrame)
            {
                enemy->attackFrameTriggered = false;
                enemy->SetAIState(EnemyComponent::AIState::Chase);
                PlayRangedSpriteAnimation(*entity, "idle", true);
            }
        }
        else if (enemy->GetArchetype() == EnemyArchetype::Charger)
        {
		constexpr float kChargerSpeed = 240.0f;
		constexpr float kGravity = 1900.0f;
		constexpr float kMaxFallSpeed = 980.0f;
		constexpr float kTileSize = 48.0f;

            if (applyEnemyGravity(*enemy, *transform, kGravity, kMaxFallSpeed))
            {
                enemy->velocityY = 0.0f;
            }

            const float chargerCenterX = transform->x + transform->width * transform->scale * 0.5f;
            const float chargerCenterY = transform->y + transform->height * transform->scale * 0.5f;
            const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
            const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
            const float dx = playerCenterX - chargerCenterX;
            const float dy = playerCenterY - chargerCenterY;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (enemy->GetAIState() == EnemyComponent::AIState::Idle && dist <= enemy->detectRange)
            {
                enemy->facing = dx >= 0.0f
                    ? EnemyComponent::FacingDirection::Right
                    : EnemyComponent::FacingDirection::Left;
                enemy->SetAIState(EnemyComponent::AIState::Chase);
            }

            if (enemy->GetAIState() == EnemyComponent::AIState::Chase)
            {
                const float direction = enemy->facing == EnemyComponent::FacingDirection::Right ? 1.0f : -1.0f;
                const float nextX = transform->x + direction * kChargerSpeed * flow.lastDeltaTime;
                const float width = transform->width * transform->scale;
                const float height = transform->height * transform->scale;
                const float sideX = direction > 0.0f ? nextX + width - 2.0f : nextX + 2.0f;
                const int sideColumn = static_cast<int>(sideX / kTileSize);
                const int rowTop = static_cast<int>((transform->y + 4.0f) / kTileSize);
                const int rowBottom = static_cast<int>((transform->y + height - 4.0f) / kTileSize);
                bool blockedByStep = false;
                for (int row = rowTop; row <= rowBottom; ++row)
                {
                    if (isSolidTile(sideColumn, row))
                    {
                        blockedByStep = true;
                        break;
                    }
                }
                if (!blockedByStep)
                {
                    transform->x = nextX;
                }
            }
        }
        else if (enemy->GetArchetype() == EnemyArchetype::Ghost)
        {
            auto* ghost = entity->GetComponent<GhostComponent>();
            if (!ghost) continue;

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

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

            if (dist < blaster->detectRange && blaster->shotsRemaining == 0
                && blaster->cooldownTimer >= blaster->cooldown)
            {
                blaster->shotsRemaining = blaster->burstCount;
                blaster->burstTimer = 0.0f;
                blaster->cooldownTimer = 0.0f;
            }

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
                proj.sourceEntity = entity;
                newBullets.push_back(std::move(bullet));
            }
        }
        else if (enemy->GetArchetype() == EnemyArchetype::ShieldBoss)
        {
            auto* boss = entity->GetComponent<ShieldBossComponent>();
            if (!boss) continue;
            if (boss->deathAnimationActive)
            {
                UpdateShieldBossSpriteAnimation(*entity, *boss);
                continue;
            }

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const bool inDetectRange = std::fabs(dx) < boss->detectRange
                && std::fabs(dy) < boss->detectHeight;

		constexpr float kGravity = 1900.0f;
		constexpr float kMaxFallSpeed = 980.0f;
		// 後ろジャンプ�E高さを保ったまま重力を強め、素早く着地させめE
		constexpr float kBackJumpGravityScale = 1.6f;
		constexpr float kBackJumpGravity = kGravity * kBackJumpGravityScale;
		constexpr float kBackJumpMaxFallSpeed = kMaxFallSpeed * kBackJumpGravityScale;
		constexpr float kTileSize = 48.0f;
            constexpr float kShieldBossWidth = kTileSize * 4.0f;
            constexpr float kShieldBossHeight = kTileSize * 4.0f;
            constexpr float kShieldWidth = kTileSize;
            constexpr float kShieldHeight = kTileSize * 4.0f;
            constexpr float kShieldRaiseOffsetY = kTileSize * 0.5f;
            constexpr float kIntroDropHeight = kTileSize * 7.0f;
            constexpr float kIntroDropSpeed = 1100.0f;
            constexpr int kAttack01BoostStartFrame = 60;
            constexpr int kAttack01BoostEndFrame = 132;
            constexpr int kAttack01EndFrame = 179;
            constexpr float kRushAccelerationSeconds = 1.4f;
            constexpr float kRushStartSpeedMultiplier = 0.55f;
            constexpr float kRushMaxSpeedMultiplier = 2.0f;
            constexpr int kAttack02AscendEndFrame = 40;
            constexpr int kAttack02HoverEndFrame = 75;
            constexpr int kAttack02ShieldDropStartFrame = 111;
            constexpr int kAttack02ImpactFrame = 121;
            constexpr int kAttack02EndFrame = 164;
            constexpr int kAttack02ShieldDropFrame = 125;
            constexpr int kAppearRoarSoundFrame = 90;
            constexpr float kAttack02RecoverySeconds =
                static_cast<float>(kAttack02EndFrame - kAttack02ImpactFrame) / 30.0f;
            constexpr float kAttack02SlamDropSpeedScale = 1.45f;
            // 叩きつけ後�E本体も、盾と同じ正規化速度で地面へ戻します、E
            constexpr float kAttack02BossReturnSeconds =
                static_cast<float>(kAttack02ImpactFrame - kAttack02ShieldDropStartFrame) /
                (30.0f * kAttack02SlamDropSpeedScale);
            transform->width = kShieldBossWidth;
            transform->height = kShieldBossHeight;
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
            if (shieldTransform)
            {
                shieldTransform->width = kShieldWidth;
                shieldTransform->height = kShieldHeight;
            }

            auto resetShieldToGuard = [&]()
            {
                if (!shieldComp)
                {
                    return;
                }
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
                    shieldTransform->width = kShieldWidth;
                    shieldTransform->height = kShieldHeight;
                    shieldTransform->rotation = 0.0f;
                }
                ApplyBossShieldGuardTint(boss->shieldEntity, shieldTint);
                boss->slamShieldVisualLocked = false;
                if (auto* shieldAnimation = boss->shieldEntity
                    ? boss->shieldEntity->GetComponent<SpriteSheetAnimationComponent>()
                    : nullptr)
                {
                    shieldAnimation->SetPlaybackSpeed(1.0f);
                }
            };

            auto startRushAttack = [&](ShieldBossAttackPhase attackPhase)
            {
                const float playerCenterX =
                    playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                const float bossCenterX = transform->x + bossWidth * 0.5f;
                boss->attackPhase = attackPhase;
                boss->facing =
                    playerCenterX > bossCenterX ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                boss->state = ShieldBossState::Rush;
                boss->stateTimer = 0.0f;
                boss->rushBoostElapsed = 0.0f;
                boss->rushBoostSoundPlayed = false;
                resetShieldToGuard();
            };

            auto startBackJump = [&](ShieldBossAttackPhase attackPhase)
            {
                constexpr int kLeftTargetColumn = 10;
                constexpr int kRightTargetColumn = 30;
                const float playerCenterX =
                    playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                const float arenaCenterX =
                    (static_cast<float>(kLeftTargetColumn + kRightTargetColumn) * 0.5f) * kTileSize;
                const float leftTargetX = static_cast<float>(kLeftTargetColumn) * kTileSize;
                const float rightTargetX = static_cast<float>(kRightTargetColumn) * kTileSize;
                const float selectedTargetX =
                    playerCenterX < arenaCenterX ? rightTargetX : leftTargetX;
                const float jumpHeightPx = boss->jumpHeight * kTileSize;

                boss->attackPhase = attackPhase;
                boss->jumpStartX = transform->x;
                boss->jumpStartY = transform->y;
                boss->targetX = std::clamp(selectedTargetX, 0.0f, std::max(0.0f, mapWidth - bossWidth));
                boss->targetY = enemy->spawnY;
                boss->facing =
                    boss->targetX < playerCenterX ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                // 叩きつけ時と同じ高さまで上がる�E速を、E��力から送E��します、E
                bossVelocityY = -std::sqrt(2.0f * kBackJumpGravity * jumpHeightPx);
                bossVelocityX = 0.0f;
                boss->returningHomeJump = false;
                boss->state = ShieldBossState::Jump;
                boss->stateTimer = 0.0f;
                resetShieldToGuard();
            };

            auto getAttackFrame = [&](const char* clipName) -> int
            {
                const auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>();
                if (!animation || animation->GetCurrentClipName() != clipName)
                {
                    return 0;
                }
                return animation->GetCurrentLocalFrameIndex();
            };

            auto showShieldDuringAttack02 = [&]()
            {
                if (shieldTint)
                {
                    shieldTint->r = 1.0f;
                    shieldTint->g = 1.0f;
                    shieldTint->b = 1.0f;
                    shieldTint->a = 1.0f;
                }
            };

            struct ShieldSheetAnchor
            {
                int frame;
                float centerX;
                float centerY;
            };

            auto sampleAttack02ShieldAnchor = [](int frame, float& centerX, float& centerY)
            {
                // Values come from the authored attack02 shield sheet alpha bounds.
                constexpr ShieldSheetAnchor anchors[] =
                {
                    { 0, 90.5f, 122.0f },
                    { 20, 110.5f, 85.0f },
                    { 40, 149.0f, 57.0f },
                    { 75, 149.0f, 56.0f },
                    { 90, 188.5f, 64.0f },
                    { 100, 121.0f, 48.5f },
                    { 111, 60.0f, 158.0f },
                    { 121, 67.0f, 153.0f },
                    { 125, 64.0f, 133.0f },
                };

                if (frame <= anchors[0].frame)
                {
                    centerX = anchors[0].centerX;
                    centerY = anchors[0].centerY;
                    return;
                }

                constexpr int anchorCount = static_cast<int>(sizeof(anchors) / sizeof(anchors[0]));
                for (int i = 1; i < anchorCount; ++i)
                {
                    if (frame > anchors[i].frame)
                    {
                        continue;
                    }

                    const ShieldSheetAnchor& prev = anchors[i - 1];
                    const ShieldSheetAnchor& next = anchors[i];
                    const float frameRange = static_cast<float>(next.frame - prev.frame);
                    const float t = frameRange > 0.0f
                        ? std::clamp(static_cast<float>(frame - prev.frame) / frameRange, 0.0f, 1.0f)
                        : 1.0f;
                    centerX = prev.centerX + (next.centerX - prev.centerX) * t;
                    centerY = prev.centerY + (next.centerY - prev.centerY) * t;
                    return;
                }

                centerX = anchors[anchorCount - 1].centerX;
                centerY = anchors[anchorCount - 1].centerY;
            };

            auto applyAttack02ShieldSheetAnchor = [&](int attackFrame)
            {
                if (!shieldTransform)
                {
                    return;
                }

                constexpr float kAttack02CellWidth = 240.0f;
                constexpr float kAttack02CellHeight = 195.0f;
                constexpr float kAttack02BodyLeft = 90.0f;
                constexpr float kAttack02BodyTop = 70.0f;
                constexpr float kAttack02BodyWidth = 114.0f;
                constexpr float kAttack02BodyHeight = 102.0f;
                constexpr float kShieldBaseCellWidth = 241.8f;
                constexpr float kShieldBaseCellHeight = 188.0f;
                constexpr float kShieldBaseBodyWidth = 169.0f;
                constexpr float kShieldBaseBodyHeight = 123.0f;
                constexpr float kAttack02ShieldVisualScale = 1.3f;

                float sheetCenterX = 0.0f;
                float sheetCenterY = 0.0f;
                sampleAttack02ShieldAnchor(attackFrame, sheetCenterX, sheetCenterY);

                const float hitboxWidth = transform->width * transform->scale;
                const float hitboxHeight = transform->height * transform->scale;
                const float bodyVisualWidth = hitboxWidth * (kAttack02CellWidth / kAttack02BodyWidth);
                const float bodyVisualHeight = hitboxHeight * (kAttack02CellHeight / kAttack02BodyHeight);
                const bool flipRight = boss->facing == ShieldBossFacing::Right;
                const float bodyLeft = flipRight
                    ? kAttack02CellWidth - (kAttack02BodyLeft + kAttack02BodyWidth)
                    : kAttack02BodyLeft;
                const float canvasX = transform->x - bodyLeft / kAttack02CellWidth * bodyVisualWidth;
                const float canvasY = transform->y - kAttack02BodyTop / kAttack02CellHeight * bodyVisualHeight;
                const float shieldDrawWidth = hitboxWidth * (kShieldBaseCellWidth / kShieldBaseBodyWidth) * kAttack02ShieldVisualScale;
                const float shieldDrawHeight = hitboxHeight * (kShieldBaseCellHeight / kShieldBaseBodyHeight) * kAttack02ShieldVisualScale;
                const float sampledX = flipRight ? (kAttack02CellWidth - sheetCenterX) : sheetCenterX;
                const float worldCenterX = canvasX + sampledX / kAttack02CellWidth * shieldDrawWidth;
                const float worldCenterY = canvasY + sheetCenterY / kAttack02CellHeight * shieldDrawHeight;
                const float shieldW = shieldTransform->width * shieldTransform->scale;
                const float shieldH = shieldTransform->height * shieldTransform->scale;

                shieldTransform->x = worldCenterX - shieldW * 0.5f;
                shieldTransform->y = worldCenterY - shieldH * 0.5f;
                shieldTransform->rotation = 0.0f;
                boss->hoverShieldX = shieldTransform->x;
                boss->hoverShieldY = shieldTransform->y;
            };

            if (boss->introDropActive || boss->appearAnimationActive)
            {
                if (auto* tint = entity->GetComponent<TintComponent>())
                {
                    tint->a = 1.0f;
                }
                if (shieldTint)
                {
                    shieldTint->a = 0.0f;
                }
                if (boss->introDropActive)
                {
                    transform->y += kIntroDropSpeed * flow.lastDeltaTime;
                    if (snapToGround(*transform) || transform->y >= enemy->spawnY)
                    {
                        transform->y = enemy->spawnY;
                        boss->introDropActive = false;
                        bossVelocityY = 0.0f;
                        const float groundY = transform->y + transform->height * transform->scale;
                        spawnLightLandingEffect(
                            transform->x + transform->width * transform->scale * 0.5f,
                            groundY,
                            transform->width * transform->scale * 1.25f);
                    }
                }
                UpdateShieldBossSpriteAnimation(*entity, *boss);
                if (boss->appearAnimationActive && !boss->roarSoundPlayed)
                {
                    if (const auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>();
                        animation &&
                        animation->GetCurrentClipName() == "appear" &&
                        animation->GetCurrentLocalFrameIndex() >= kAppearRoarSoundFrame)
                    {
                        boss->roarSoundPlayed = true;
                        playShieldBossRoar(*entity);
                    }
                }
                if (!boss->introDropActive && !boss->appearAnimationActive && !boss->roarPlayed)
                {
                    boss->roarPlayed = true;
                    boss->roarAnimationActive = true;
                    boss->roarTimer = 0.0f;
                    boss->stateTimer = 0.0f;
                    const float groundY = transform->y + transform->height * transform->scale;
                    spawnBossRoarEffect(
                        transform->x + transform->width * transform->scale * 0.5f,
                        groundY,
                        transform->width * transform->scale * 1.65f);
                    UpdateShieldBossSpriteAnimation(*entity, *boss);
                }
                continue;
            }

            if (boss->roarAnimationActive)
            {
                boss->roarTimer += flow.lastDeltaTime;
                if (shieldTint)
                {
                    shieldTint->a = 0.0f;
                }
                UpdateShieldBossSpriteAnimation(*entity, *boss);
                continue;
            }

            auto findHitNonWallObject = [&]() -> Entity*
            {
                for (Entity* other : interactionEntities)
                {
                    if (!other || other == entity || other == boss->shieldEntity)
                    {
                        continue;
                    }
                    const auto* otherTransform = other->GetComponent<TransformComponent>();
                    if (!otherTransform)
                    {
                        continue;
                    }
                    if (IntersectsBounds(*transform, *otherTransform) ||
                        (shieldTransform && IntersectsBounds(*shieldTransform, *otherTransform)))
                    {
                        return other;
                    }
                }
                return nullptr;
            };

            auto removeObjectsUnderShield = [&]()
            {
                if (!shieldTransform)
                {
                    return;
                }
                for (Entity* other : interactionEntities)
                {
                    if (!other || other == entity || other == boss->shieldEntity)
                    {
                        continue;
                    }
                    if (HasTag(*other, "Player"))
                    {
                        continue;
                    }
                    const auto* otherTransform = other->GetComponent<TransformComponent>();
                    if (!otherTransform || !IntersectsBounds(*shieldTransform, *otherTransform))
                    {
                        continue;
                    }
                    if (std::find(entitiesToRemove.begin(), entitiesToRemove.end(), other) == entitiesToRemove.end())
                    {
                        entitiesToRemove.push_back(other);
                    }
                }
            };

            auto spawnSlamImpact = [&]()
            {
                if (!(shieldComp && shieldTransform))
                {
                    return;
                }

                const float shockW = kTileSize * 8.0f;
                const float shockH = kTileSize * 3.0f;
                auto shockwave = std::make_unique<Entity>();
                shockwave->AddComponent<TagComponent>("BossShockwave");
                const float shockGroundY = shieldTransform->y + shieldTransform->height * shieldTransform->scale;
                shockwave->AddComponent<TransformComponent>(
                    shieldTransform->x + shieldTransform->width * shieldTransform->scale * 0.5f - shockW * 0.5f,
                    shockGroundY - kTileSize * 2.0f,
                    shockW,
                    shockH);
                shockwave->AddComponent<TintComponent>(0.18f, 0.95f, 1.0f, 0.75f);
                shockwave->AddComponent<SpriteRenderComponent>(tileTexture);
                auto& shockComp = shockwave->AddComponent<ShieldShockwaveComponent>();
                shockComp.ownerBoss = entity;
                shockComp.damage = 1;
                shockComp.lifetime = 0.25f;
                newShields.push_back(std::move(shockwave));

                flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, 0.24f);
                flow.screenShakeDuration = std::max(flow.screenShakeDuration, 0.24f);
                flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, 24.0f);
                spawnSlamImpactEffect(
                    shieldTransform->x + shieldTransform->width * shieldTransform->scale * 0.5f,
                    shockGroundY,
                    shockW);

                shieldComp->contactDamage = 0;
                removeObjectsUnderShield();
            };

            auto playShieldDropSoundOnce = [&]()
            {
                if (boss->shieldDropSoundPlayed)
                {
                    return;
                }

                // 盾落下SEは、叩きつけが地面に当たって盾が消える瞬間に合わせる、E
                boss->shieldDropSoundPlayed = true;
                playShieldBossCue(*entity, "boss_forest_shield_drop");
            };

            auto startBossKnockback = [&](float direction)
            {
                constexpr float kBossKnockbackHitStopSeconds = 0.12f;
                constexpr float kBossKnockbackShakeSeconds = 0.34f;
                constexpr float kBossKnockbackShakeAmplitude = 44.0f;
                boss->knockbackActive = true;
                boss->knockbackTimer = 0.0f;
                boss->knockbackStartX = transform->x;
                boss->knockbackStartY = transform->y;
                boss->knockbackTargetX = transform->x - direction * (kTileSize * 3.0f);
                flow.hitStopRemaining = std::max(flow.hitStopRemaining, kBossKnockbackHitStopSeconds);
                flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kBossKnockbackShakeSeconds);
                flow.screenShakeDuration = std::max(flow.screenShakeDuration, kBossKnockbackShakeSeconds);
                flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kBossKnockbackShakeAmplitude);
                stopShieldBossCue(*entity, "boss_forest_boost");
                playShieldBossCue(*entity, "boss_forest_knockback");
                if (shieldComp && shieldTransform)
                {
                    shieldComp->attached = true;
                    shieldComp->attackType = ShieldAttackType::None;
                    shieldComp->velocityX = 0.0f;
                    shieldComp->velocityY = 0.0f;
                    shieldComp->rotationSpeed = 0.0f;
                    shieldComp->gravityEnabled = false;
                    shieldTransform->width = kShieldWidth;
                    shieldTransform->height = kShieldHeight;
                    shieldTransform->rotation = 0.0f;
                }
            };

            if (boss->knockbackActive)
            {
                boss->knockbackTimer += flow.lastDeltaTime;
                const float progress = std::min(1.0f, boss->knockbackTimer / boss->knockbackDuration);
                const float moveProgress = std::sin(progress * 3.1415926f * 0.5f);
                transform->x = boss->knockbackStartX + (boss->knockbackTargetX - boss->knockbackStartX) * moveProgress;
                transform->y = boss->knockbackStartY - std::sin(progress * 3.1415926f) * boss->knockbackHeight * kTileSize;
                if (progress >= 1.0f)
                {
                    transform->x = boss->knockbackTargetX;
                    transform->y = boss->knockbackStartY;
                    spawnLightLandingEffect(
                        transform->x + transform->width * transform->scale * 0.5f,
                        transform->y + transform->height * transform->scale,
                        transform->width * transform->scale);
                    boss->knockbackActive = false;
                    boss->knockbackTimer = 0.0f;
                }
            }

            auto syncAttachedShieldToBoss = [&]()
            {
                if (!shieldComp || !shieldTransform)
                {
                    return;
                }
                if (boss->knockbackActive)
                {
                    shieldComp->attached = true;
                    shieldComp->attackType = ShieldAttackType::None;
                    shieldComp->velocityX = 0.0f;
                    shieldComp->velocityY = 0.0f;
                    shieldComp->rotationSpeed = 0.0f;
                    shieldComp->gravityEnabled = false;
                    shieldTransform->width = kShieldWidth;
                    shieldTransform->height = kShieldHeight;
                    shieldTransform->rotation = 0.0f;
                }
                if (!shieldComp->attached)
                {
                    return;
                }
                const float shieldW = shieldTransform->width * shieldTransform->scale;
                const float shieldH = shieldTransform->height * shieldTransform->scale;
                const auto getGuardOverlapX = [&]() -> float
                {
                    if (boss->state == ShieldBossState::Idle ||
                        boss->state == ShieldBossState::Detect ||
                        boss->state == ShieldBossState::Rush ||
                        boss->state == ShieldBossState::RushCooldown ||
                        boss->state == ShieldBossState::Cooldown)
                    {
                        return kShieldWidth * 2.5f;
                    }
                    return kShieldWidth * 1.5f;
                };
                const float guardOverlapX = getGuardOverlapX();
                ApplyBossShieldGuardTint(boss->shieldEntity, shieldTint);

                if (boss->state == ShieldBossState::JumpAscend)
                {
                    shieldTransform->x = boss->facing == ShieldBossFacing::Right
                        ? transform->x + bossWidth - guardOverlapX
                        : transform->x - shieldW + guardOverlapX;
                    shieldTransform->y = transform->y - shieldH;
                }
                else if (boss->state == ShieldBossState::AirHover)
                {
                    shieldTransform->x = boss->hoverShieldX;
                    shieldTransform->y = boss->hoverShieldY;
                }
                else if (boss->state == ShieldBossState::Rush)
                {
                    shieldTransform->x = boss->facing == ShieldBossFacing::Right
                        ? transform->x + bossWidth - guardOverlapX
                        : transform->x - shieldW + guardOverlapX;
                    shieldTransform->y = transform->y - kShieldRaiseOffsetY;
                }
                else
                {
                    shieldTransform->x = boss->facing == ShieldBossFacing::Right
                        ? transform->x + bossWidth - guardOverlapX
                        : transform->x - shieldW + guardOverlapX;
                    shieldTransform->y = transform->y - kShieldRaiseOffsetY;
                    shieldTransform->rotation = 0.0f;
                }
            };

            syncAttachedShieldToBoss();
            UpdateShieldBossSpriteAnimation(*entity, *boss);

            if (boss->state == ShieldBossState::Idle ||
                boss->state == ShieldBossState::Detect ||
                boss->state == ShieldBossState::RushCooldown ||
                boss->state == ShieldBossState::Cooldown)
            {
                if (!boss->knockbackActive)
                {
                    bossVelocityY = std::min(kMaxFallSpeed, bossVelocityY + kGravity * flow.lastDeltaTime);
                    transform->y += bossVelocityY * flow.lastDeltaTime;
                    const bool onGround = snapToGround(*transform);
                    if (onGround) bossVelocityY = 0.0f;
                }
            }

            if (boss->knockbackActive)
            {
                continue;
            }

            bool rushEndedThisFrame = false;
            if (boss->state == ShieldBossState::Rush)
            {
                const float dir = boss->facing == ShieldBossFacing::Right ? 1.0f : -1.0f;
                const int attackFrame = getAttackFrame("attack01");
                const bool rushBoostActive =
                    attackFrame >= kAttack01BoostStartFrame &&
                    attackFrame <= kAttack01BoostEndFrame;
                if (rushBoostActive)
                {
                    boss->rushBoostElapsed += flow.lastDeltaTime;
                    if (!boss->rushBoostSoundPlayed)
                    {
                        boss->rushBoostSoundPlayed = true;
                        playShieldBossCue(*entity, "boss_forest_boost");
                    }
                    // アニメーションフレームではなく、実際の突E��経過時間で滑らかに加速する、E
                    const float rushAccelProgress = std::clamp(
                        boss->rushBoostElapsed / kRushAccelerationSeconds,
                        0.0f,
                        1.0f);
                    const float rushAccelEase = rushAccelProgress * rushAccelProgress * (3.0f - 2.0f * rushAccelProgress);
                    const float rushSpeedMultiplier = std::lerp(
                        kRushStartSpeedMultiplier,
                        kRushMaxSpeedMultiplier,
                        rushAccelEase);
                    transform->x += dir * boss->rushSpeed * rushSpeedMultiplier * flow.lastDeltaTime;
                    constexpr float kRushSmokeInterval = 0.034f;
                    const int previousSmokeStep = static_cast<int>(boss->stateTimer / kRushSmokeInterval);
                    const int nextSmokeStep = static_cast<int>((boss->stateTimer + flow.lastDeltaTime) / kRushSmokeInterval);
                    if (nextSmokeStep != previousSmokeStep)
                    {
                        const float smokeX = transform->x + bossWidth * 0.5f - dir * bossWidth * 0.24f;
                        const float smokeY = transform->y + bossHeight;
                        spawnRushSmokeEffect(smokeX, smokeY, dir);
                    }
                }
                syncAttachedShieldToBoss();

                bool hitPlayer = false;
                if (rushBoostActive && playerTransform)
                {
                    hitPlayer = IntersectsBounds(*transform, *playerTransform)
                        || (shieldTransform && IntersectsBounds(*shieldTransform, *playerTransform));
                }

                const int rowTop = static_cast<int>((transform->y + 4.0f) / kTileSize);
                const int rowBottom = static_cast<int>((transform->y + bossHeight - 4.0f) / kTileSize);
                bool hitWall = false;
                if (rushBoostActive && boss->facing == ShieldBossFacing::Right)
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
                else if (rushBoostActive)
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

                Entity* hitObject = rushBoostActive ? findHitNonWallObject() : nullptr;
                const bool rushClipFinished = attackFrame >= kAttack01EndFrame;
                if (hitPlayer || hitObject)
                {
                        if (hitObject && HasTag(*hitObject, "PhotoBox") &&
                            std::find(entitiesToRemove.begin(), entitiesToRemove.end(), hitObject) == entitiesToRemove.end())
                        {
                            entitiesToRemove.push_back(hitObject);
                        }
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                    startBossKnockback(dir);
                    syncAttachedShieldToBoss();
                    rushEndedThisFrame = true;
                }
                else if (hitWall || (rushBoostActive && checkPhotoBoxCollision(*transform, *entity)))
                {
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                    startBossKnockback(dir);
                    syncAttachedShieldToBoss();
                    rushEndedThisFrame = true;
                }
                else if (rushClipFinished)
                {
                    boss->state = ShieldBossState::RushCooldown;
                    boss->stateTimer = 0.0f;
                    stopShieldBossCue(*entity, "boss_forest_boost");
                    syncAttachedShieldToBoss();
                    rushEndedThisFrame = true;
                }

                if (rushEndedThisFrame) continue;
            }

            if (boss->state == ShieldBossState::Jump)
            {
                boss->stateTimer += flow.lastDeltaTime;
                const float jumpHeightPx = boss->jumpHeight * kTileSize;
                const float riseSeconds = std::sqrt(2.0f * jumpHeightPx / kBackJumpGravity);
                const float flightSeconds = std::max(0.01f, riseSeconds * 2.0f);
                const float progress = std::min(1.0f, boss->stateTimer / flightSeconds);
                transform->x = boss->jumpStartX + (boss->targetX - boss->jumpStartX) * progress;
                // 頂点以降も補間せず、E��力加速と最大落下速度で盾と同様に落下させます、E
                bossVelocityY = std::min(
                    kBackJumpMaxFallSpeed,
                    bossVelocityY + kBackJumpGravity * flow.lastDeltaTime);
                transform->y += bossVelocityY * flow.lastDeltaTime;
                if (bossVelocityY >= 0.0f && transform->y >= boss->targetY)
                {
                    transform->x = boss->targetX;
                    transform->y = boss->targetY;
                    bossVelocityY = 0.0f;
                    spawnLightLandingEffect(
                        transform->x + bossWidth * 0.5f,
                        transform->y + bossHeight,
                        bossWidth);
                    boss->returningHomeJump = false;
                    resetShieldToGuard();
                    if (boss->attackPhase == ShieldBossAttackPhase::FirstBackJump)
                    {
                        startRushAttack(ShieldBossAttackPhase::SecondRush);
                    }
                    else if (boss->attackPhase == ShieldBossAttackPhase::FinalBackJump)
                    {
                        startRushAttack(ShieldBossAttackPhase::FirstRush);
                    }
                    else
                    {
                        boss->state = ShieldBossState::Cooldown;
                        boss->stateTimer = 0.0f;
                    }
                }
                continue;
            }

            if (boss->state == ShieldBossState::JumpAscend)
            {
                showShieldDuringAttack02();
                const int attackFrame = getAttackFrame("attack02");
                const float jumpHeightPx = boss->jumpHeight * kTileSize;
                const float ascendProgress = std::min(1.0f,
                    static_cast<float>(attackFrame) / static_cast<float>(kAttack02AscendEndFrame));
                const float easedProgress = std::sin(ascendProgress * 3.1415926f * 0.5f);
                transform->y = boss->targetY - jumpHeightPx * easedProgress;

                transform->x = transform->x + (boss->targetX - transform->x) * std::min(1.0f, flow.lastDeltaTime * 6.0f);
                applyAttack02ShieldSheetAnchor(attackFrame);

                boss->stateTimer += flow.lastDeltaTime;
                if (attackFrame >= kAttack02AscendEndFrame)
                {
                    if (shieldComp && shieldTransform)
                    {
                        boss->hoverShieldX = shieldTransform->x;
                        boss->hoverShieldY = shieldTransform->y;
                        shieldComp->attached = false;
                        shieldComp->gravityEnabled = false;
                        shieldComp->velocityX = 0.0f;
                        shieldComp->velocityY = 0.0f;
                        shieldComp->rotationSpeed = 0.0f;
                        shieldTransform->x = boss->hoverShieldX;
                        shieldTransform->y = boss->hoverShieldY;
                        shieldTransform->rotation = 0.0f;
                    }
                    boss->state = ShieldBossState::AirHover;
                    boss->stateTimer = 0.0f;
                    if (!boss->attack2SoundPlayed)
                    {
                        boss->attack2SoundPlayed = true;
                        playShieldBossCue(*entity, "boss_forest_attack2");
                    }
                }
                continue;
            }

            if (boss->state == ShieldBossState::AirHover)
            {
                showShieldDuringAttack02();
                boss->stateTimer += flow.lastDeltaTime;
                if (shieldTransform)
                {
                    const int attackFrame = getAttackFrame("attack02");
                    applyAttack02ShieldSheetAnchor(attackFrame);
                }
                if (getAttackFrame("attack02") >= kAttack02ShieldDropStartFrame)
                {
                    if (auto* shieldSprite = boss->shieldEntity
                        ? boss->shieldEntity->GetComponent<SpriteRenderComponent>()
                        : nullptr)
                    {
                        boss->slamShieldRenderOffsetX = shieldSprite->GetRenderOffsetX();
                        boss->slamShieldRenderOffsetY = shieldSprite->GetRenderOffsetY();
                        boss->slamShieldVisualLocked = true;
                    }
                    if (auto* shieldAnimation = boss->shieldEntity
                        ? boss->shieldEntity->GetComponent<SpriteSheetAnimationComponent>()
                        : nullptr)
                    {
                        shieldAnimation->Play("attack02");
                        shieldAnimation->SetCurrentLocalFrameIndex(kAttack02ShieldDropFrame);
                        shieldAnimation->SetPlaybackSpeed(0.0f);
                    }
                    boss->state = ShieldBossState::JumpDescend;
                    boss->stateTimer = 0.0f;
                }
                continue;
            }
            if (boss->state == ShieldBossState::JumpDescend)
            {
                showShieldDuringAttack02();
                if (!(shieldComp && shieldTransform))
                {
                    boss->state = ShieldBossState::Cooldown;
                    boss->stateTimer = 0.0f;
                    continue;
                }

                const int attackFrame = getAttackFrame("attack02");
                const float descentProgress = std::clamp(
                    static_cast<float>(attackFrame - kAttack02ShieldDropStartFrame) /
                    static_cast<float>(kAttack02ImpactFrame - kAttack02ShieldDropStartFrame),
                    0.0f,
                    1.0f);
                const float fastDescentProgress = std::clamp(descentProgress * kAttack02SlamDropSpeedScale, 0.0f, 1.0f);
                const bool impactFrameReached = attackFrame >= kAttack02ImpactFrame || fastDescentProgress >= 1.0f;
                const float preImpactProgress = impactFrameReached
                    ? 1.0f
                    : std::min(fastDescentProgress, 0.88f);
                const float easedDescent = preImpactProgress * preImpactProgress * (3.0f - 2.0f * preImpactProgress);
                const float slamTargetY = boss->targetY;
                const float slamWindupY = boss->hoverShieldY;
                shieldTransform->x = boss->hoverShieldX;
                shieldTransform->y = impactFrameReached
                    ? slamTargetY
                    : slamWindupY + (slamTargetY - slamWindupY) * easedDescent;
                // 空中で構えた盾の向きを保ったまま、衝撃波フレームまで縦に落とす、E

                if (impactFrameReached)
                {
                    bossVelocityY = 0.0f;
                    transform->rotation = 0.0f;
                    shieldTransform->y = slamTargetY;
                    boss->hoverShieldX = shieldTransform->x;
                    boss->hoverShieldY = shieldTransform->y;

                    shieldComp->attached = false;
                    shieldComp->attackType = ShieldAttackType::Slam;
                    shieldComp->contactDamage = 2;
                    shieldComp->gravityEnabled = false;
                    shieldComp->velocityX = 0.0f;
                    shieldComp->velocityY = 0.0f;
                    shieldComp->rotationSpeed = 0.0f;
                    shieldTransform->width = kShieldWidth;
                    shieldTransform->height = kShieldHeight;
                    removeObjectsUnderShield();
                    if (shieldTint)
                    {
                        shieldTint->r = 1.0f;
                        shieldTint->g = 0.45f;
                        shieldTint->b = 0.12f;
                        shieldTint->a = 1.0f;
                    }

                    spawnSlamImpact();
                    playShieldDropSoundOnce();
                    if (shieldTint)
                    {
                        shieldTint->a = 0.0f;
                    }
                    boss->slamShieldVisualLocked = false;
                    boss->state = ShieldBossState::SlamPhase2;
                    boss->stateTimer = 0.0f;
                    boss->jumpStartY = transform->y;
                    if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>())
                    {
                        animation->Play("move", true);
                    }
                }
                continue;
            }

            boss->stateTimer += flow.lastDeltaTime;

            switch (boss->state)
            {
            case ShieldBossState::Idle:
                if (inDetectRange || boss->combatStarted)
                {
                    const bool firstDetect = !boss->combatStarted;
                    boss->combatStarted = true;
                    boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                    if (firstDetect && !boss->appearAnimationFinished)
                    {
                        if (auto* tint = entity->GetComponent<TintComponent>())
                        {
                            tint->a = 1.0f;
                        }
                        if (shieldTint)
                        {
                            shieldTint->a = 0.0f;
                        }
                        transform->y = enemy->spawnY - kIntroDropHeight;
                        bossVelocityY = 0.0f;
                        boss->introDropActive = true;
                        boss->appearAnimationActive = true;
                        boss->appearAnimationFinished = false;
                        boss->roarPlayed = false;
                        boss->roarSoundPlayed = false;
                        boss->roarAnimationActive = false;
                        boss->roarTimer = 0.0f;
                        if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>())
                        {
                            animation->Play("appear", true);
                        }
                    }
                    UpdateShieldBossSpriteAnimation(*entity, *boss);
                    boss->state = ShieldBossState::Detect;
                    boss->stateTimer = 0.0f;
                    boss->attackPhase = ShieldBossAttackPhase::FirstRush;
                }
                break;

            case ShieldBossState::Detect:
                boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                if (boss->roarAnimationActive)
                {
                    boss->stateTimer = 0.0f;
                    break;
                }
                if (boss->stateTimer >= 0.5f)
                {
                    startRushAttack(boss->attackPhase);
                }
                break;

            case ShieldBossState::Rush:
                break;

            case ShieldBossState::RushCooldown:
                if (boss->stateTimer >= boss->rushCooldown)
                {
                    if (boss->attackPhase == ShieldBossAttackPhase::FirstRush)
                    {
                        startBackJump(ShieldBossAttackPhase::FirstBackJump);
                    }
                    else
                    {
                        boss->facing = dx > 0.0f ? ShieldBossFacing::Right : ShieldBossFacing::Left;
                        boss->attackPhase = ShieldBossAttackPhase::Slam;
                        const float playerCenterX = playerTransform->x
                            + playerTransform->width * playerTransform->scale * 0.5f;
                        const float jumpShieldWidth = kShieldWidth;
                        const float rawTargetX = boss->facing == ShieldBossFacing::Right
                            ? playerCenterX - bossWidth - jumpShieldWidth * 0.5f
                            : playerCenterX + jumpShieldWidth * 0.5f;
                        constexpr float kSlamLeftLimitX = 6.0f * kTileSize;
                        const float slamRightLimitX = 35.0f * kTileSize - bossWidth;
                        boss->targetX = std::clamp(rawTargetX, kSlamLeftLimitX, slamRightLimitX);
                        boss->targetY = transform->y;
                        bossVelocityY = 0.0f;
                        bossVelocityX = 0.0f;
                        boss->state = ShieldBossState::JumpAscend;
                        boss->stateTimer = 0.0f;
                        boss->slamShieldVisualLocked = false;
                        boss->attack2SoundPlayed = false;
                        boss->shieldDropSoundPlayed = false;
                        showShieldDuringAttack02();
                        if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>())
                        {
                            animation->Play("attack02", true);
                        }
                    if (auto* shieldAnimation = boss->shieldEntity
                        ? boss->shieldEntity->GetComponent<SpriteSheetAnimationComponent>()
                        : nullptr)
                    {
                        shieldAnimation->SetPlaybackSpeed(1.0f);
                        shieldAnimation->Play("attack02", true);
                    }

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
                            shieldTransform->width = kShieldWidth;
                            shieldTransform->height = kShieldHeight;
                            shieldTransform->rotation = 0.0f;
                        }
                    }
                }
                break;
            case ShieldBossState::SlamPhase1:
                spawnSlamImpact();
                playShieldDropSoundOnce();
                if (shieldTint)
                {
                    shieldTint->a = 0.0f;
                }
                boss->slamShieldVisualLocked = false;
                boss->state = ShieldBossState::SlamPhase2;
                boss->stateTimer = 0.0f;
                if (auto* animation = entity->GetComponent<SpriteSheetAnimationComponent>())
                {
                    animation->Play("move", true);
                }
                break;

            case ShieldBossState::SlamPhase2:
                {
                    if (shieldTint)
                    {
                        shieldTint->a = 0.0f;
                    }
                    const float returnProgress = std::min(1.0f, boss->stateTimer / kAttack02BossReturnSeconds);
                    const float easedReturn = returnProgress * returnProgress * (3.0f - 2.0f * returnProgress);
                    transform->y = boss->jumpStartY + (boss->targetY - boss->jumpStartY) * easedReturn;
                    if (shieldTransform)
                    {
                        const float shieldW = shieldTransform->width * shieldTransform->scale;
                        const float guardOverlapX = kShieldWidth * 1.5f;
                        const float guardX = boss->facing == ShieldBossFacing::Right
                            ? transform->x + bossWidth - guardOverlapX
                            : transform->x - shieldW + guardOverlapX;
                        shieldTransform->x = boss->hoverShieldX + (guardX - boss->hoverShieldX) * easedReturn;
                        shieldTransform->y = boss->hoverShieldY + (transform->y - boss->hoverShieldY) * easedReturn;
                        shieldTransform->rotation = 0.0f;
                    }
                }
                if (boss->stateTimer >= kAttack02BossReturnSeconds)
                {
                    transform->y = boss->targetY;
                    boss->state = ShieldBossState::Cooldown;
                    boss->stateTimer = 0.0f;
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
                            shieldTransform->width = kShieldWidth;
                            shieldTransform->height = kShieldHeight;
                            shieldTransform->rotation = 0.0f;
                        }
                        ApplyBossShieldGuardTint(boss->shieldEntity, shieldTint);
                        boss->slamShieldVisualLocked = false;
                        if (auto* shieldAnimation = boss->shieldEntity
                            ? boss->shieldEntity->GetComponent<SpriteSheetAnimationComponent>()
                            : nullptr)
                        {
                            shieldAnimation->SetPlaybackSpeed(1.0f);
                        }
                    }
                }
                break;

            case ShieldBossState::Cooldown:
                if (boss->stateTimer >= boss->slamCooldown)
                {
                    if (boss->attackPhase == ShieldBossAttackPhase::Slam)
                    {
                        startBackJump(ShieldBossAttackPhase::FinalBackJump);
                    }
                    else
                    {
                        boss->state = ShieldBossState::Idle;
                        boss->stateTimer = 0.0f;
                    }
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
    constexpr float kMidBoss2TeleportFlashSeconds = 0.24f;
            constexpr float kBeamFireDuration = 5.2f;
            constexpr float kBeamFireShakeSeconds = 0.16f;
            constexpr float kBeamFireShakeAmplitude = 24.0f;
            constexpr float kMidBoss2TeleportShakeSeconds = 0.12f;
            constexpr float kMidBoss2TeleportShakeAmplitude = 8.0f;
            constexpr float kMidBoss2BeamChargeShakeSeconds = 0.08f;
            constexpr float kMidBoss2BeamChargeShakeAmplitude = 5.0f;
            const auto getMidBoss2LeftX = [&](float centerGridX, float bossWidth)
            {
                return centerGridX * kTileSize - bossWidth * 0.5f;
            };
            const auto pickMidBoss2SpearHoverTarget = [&](bool leftSide, float bossWidth, float bossHeight, float maxBossY, float& outX, float& outY)
            {
                const float arenaMinX = getMidBoss2LeftX(kMidBoss2ArenaCenterMinGridX, bossWidth);
                const float arenaMaxX = getMidBoss2LeftX(kMidBoss2ArenaCenterMaxGridX, bossWidth);
                const auto& slots = leftSide ? boss->params.leftTeleportSlots : boss->params.rightTeleportSlots;
                const int slotIndex = std::clamp(GetRand(2), 0, 2);
                outX = std::clamp(getMidBoss2LeftX(slots[slotIndex].centerGridX, bossWidth), arenaMinX, arenaMaxX);
                outY = std::clamp(
                    mapHeight - bossHeight - (boss->params.teleportHoverBaseGrid + slots[slotIndex].hoverHeightOffsetGrid) * kTileSize,
                    0.0f,
                    maxBossY);
            };
            const auto pickMidBoss2BeamTeleportTarget = [&](bool leftSide, float bossWidth, float bossHeight, float maxBossY, float& outX, float& outY)
            {
                const float arenaMinX = getMidBoss2LeftX(kMidBoss2ArenaCenterMinGridX, bossWidth);
                const float arenaMaxX = getMidBoss2LeftX(kMidBoss2ArenaCenterMaxGridX, bossWidth);
                const auto& slots = leftSide ? boss->params.leftTeleportSlots : boss->params.rightTeleportSlots;
                int bestIndex = 0;
                float bestOffset = std::numeric_limits<float>::infinity();
                for (int index = 0; index < static_cast<int>(slots.size()); ++index)
                {
                    if (slots[index].hoverHeightOffsetGrid < bestOffset)
                    {
                        bestOffset = slots[index].hoverHeightOffsetGrid;
                        bestIndex = index;
                    }
                }

                outX = std::clamp(getMidBoss2LeftX(slots[bestIndex].centerGridX, bossWidth), arenaMinX, arenaMaxX);
                outY = std::clamp(
                    mapHeight - bossHeight - (boss->params.teleportHoverBaseGrid + slots[bestIndex].hoverHeightOffsetGrid) * kTileSize,
                    0.0f,
                    maxBossY);
            };
            const auto destroyOverlappingProtectiveWalls = [&](const TransformComponent& bossTransform)
            {
                for (Entity* candidate : interactionEntities)
                {
                    if (!candidate || !HasTag(*candidate, "ProtectiveWall"))
                    {
                        continue;
                    }

                    auto* wall = candidate->GetComponent<ProtectiveWallComponent>();
                    auto* wallTransform = candidate->GetComponent<TransformComponent>();
                    if (!wall || !wallTransform || wall->IsDestroyed() || !wall->isOn)
                    {
                        continue;
                    }

                    if (!IntersectsBounds(bossTransform, *wallTransform))
                    {
                        continue;
                    }

                    wall->ApplyDamage(wall->GetCurrentDurability());
                }
            };

            if (!boss->initializedHome)
            {
                boss->homeX = transform->x;
                boss->homeY = transform->y;
                boss->initializedHome = true;
            }

            const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
            const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
            const float bossCenterX = transform->x + transform->width * transform->scale * 0.5f;
            const float bossCenterY = transform->y + transform->height * transform->scale * 0.5f;
            const float dx = playerCenterX - bossCenterX;
            const float dy = playerCenterY - bossCenterY;
            const bool inBeamSequence =
                boss->state == MidBoss2State::BeamCharge ||
                boss->state == MidBoss2State::BeamFire ||
                boss->state == MidBoss2State::BeamCooldown;
            if (!inBeamSequence)
            {
                boss->facingRight = dx >= 0.0f;
            }
            else
            {
                boss->facingRight = boss->beamFacingRight;
            }
            const bool visualFacingRight = boss->facingRight;
            const bool beamFacingRight = boss->beamFacingRight;
            if (auto* bossSprite = entity->GetComponent<SpriteRenderComponent>())
            {
                bossSprite->SetFlipX(visualFacingRight);
            }
            boss->attackFlowStep = boss->state == MidBoss2State::BeamCharge || boss->state == MidBoss2State::BeamFire || boss->state == MidBoss2State::BeamCooldown ? 2 : 1;
            boss->captureWindowActive = boss->state == MidBoss2State::BeamFire;
            boss->stateTimer += flow.lastDeltaTime;
            boss->teleportFlashRemaining = std::max(0.0f, boss->teleportFlashRemaining - flow.lastDeltaTime);
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
                turret.fireDirection = beamFacingRight
                    ? LaserTurretFireDirection::Right
                    : LaserTurretFireDirection::Left;
                turret.vertical = false;
                turret.shootsLeft = !beamFacingRight;
                turret.fireToLeft = false;
                turret.active = false;
                newBullets.push_back(std::move(turretEntity));

                auto beamEntity = std::make_unique<Entity>();
                boss->beamEntity = beamEntity.get();
                boss->beamEntity->AddComponent<TagComponent>("LaserBeam");
                boss->beamEntity->AddComponent<TransformComponent>(-10000.0f, -10000.0f, 0.0f, boss->params.beamHeightGrid * kTileSize);
                boss->beamEntity->AddComponent<TintComponent>(0.48f, 0.78f, 1.0f, 0.0f);
                boss->beamEntity->AddComponent<SpriteRenderComponent>(tileTexture);
                boss->beamEntity->AddComponent<LaserBeamComponent>(boss->params.beamDamagePerSecond);
                auto& beamCapture = boss->beamEntity->AddComponent<BossBeamCaptureComponent>();
                beamCapture.captureEnabled = true;
                beamCapture.sourceOnLeft = beamFacingRight;
                beamCapture.visualLeakLength = 12.0f;
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
                    if (auto* beamCapture = beamEntity->GetComponent<BossBeamCaptureComponent>())
                    {
                        beamCapture->captureEnabled = false;
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
                    ? transform->x + bossWidth
                    : transform->x;
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
                        turret->fireDirection = beamFacingRight
                            ? LaserTurretFireDirection::Right
                            : LaserTurretFireDirection::Left;
                        turret->vertical = false;
                        turret->shootsLeft = !beamFacingRight;
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
                        const float beamDrawWidth = std::max(0.0f, beamTransform->width * beamTransform->scale);
                        beamTransform->x = beamFacingRight
                            ? beamOriginX
                            : beamOriginX - beamDrawWidth;
                        beamTransform->y = beamOriginY - beamHeight * 0.5f;
                        beamTransform->height = beamHeight;
                    }
                    if (auto* beamTint = beamEntity->GetComponent<TintComponent>())
                    {
                        beamTint->a = 0.86f;
                    }
                    if (auto* beamDamage = beamEntity->GetComponent<LaserBeamComponent>())
                    {
                        beamDamage->damagePerSecond = boss->params.beamDamagePerSecond;
                    }
                    if (auto* beamCapture = beamEntity->GetComponent<BossBeamCaptureComponent>())
                    {
                        beamCapture->captureEnabled = true;
                        beamCapture->sourceOnLeft = beamFacingRight;
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
                    const float maxBossY = std::max(0.0f, mapHeight - bossHeight);
                    pickMidBoss2SpearHoverTarget(
                        boss->nextSpearStartLeftSide,
                        bossWidth,
                        bossHeight,
                        maxBossY,
                        boss->hoverTargetX,
                        boss->hoverTargetY);
                    boss->state = MidBoss2State::SpearJump;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::SpearJump:
            {
                const float fromX = transform->x;
                const float fromY = transform->y;
                spawnTeleportTrail(
                    fromX,
                    fromY,
                    boss->hoverTargetX,
                    boss->hoverTargetY,
                    transform->width * transform->scale,
                    transform->height * transform->scale,
                    boss->params);
                boss->teleportFlashRemaining = kMidBoss2TeleportFlashSeconds;
                flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kMidBoss2TeleportShakeSeconds);
                flow.screenShakeDuration = std::max(flow.screenShakeDuration, kMidBoss2TeleportShakeSeconds);
                flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kMidBoss2TeleportShakeAmplitude);
                transform->x = boss->hoverTargetX;
                transform->y = boss->hoverTargetY;
                boss->spearShotsFired = 0;
                boss->state = MidBoss2State::SpearThrow;
                boss->stateTimer = 0.0f;
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
                    projectile.sourceEntity = entity;
                    auto& spearComponent = spear->AddComponent<MidBoss2SpearComponent>();
                    spearComponent.launched = false;
                    spearComponent.fadeRemaining = boss->params.spearFadeTime;
                    spearComponent.fadeDuration = boss->params.spearFadeTime;
                    spearComponent.directionX = dirX;
                    spearComponent.directionY = dirY;
                    spearComponent.targetDirectionX = dirX;
                    spearComponent.targetDirectionY = dirY;
                    spearComponent.launchDelay = boss->params.spearInterval;
                    spearComponent.launchTimer = 0.0f;
                    spearComponent.travelDistance = 0.0f;
                    spearComponent.spawnX = spawnX;
                    spearComponent.spawnY = spawnY;
                    boss->lastSpearDirX = dirX;
                    boss->lastSpearDirY = dirY;
                    if (auto* spearTransform = spear->GetComponent<TransformComponent>())
                    {
                        spearTransform->rotation = std::atan2(dirY, dirX);
                    }
                    newBullets.push_back(std::move(spear));
                    playEnemyGun(*entity);
                }

                if (boss->spearShotsFired >= 3)
                {
                    ++boss->spearCycleCount;
                    if (boss->spearCycleCount >= 3)
                    {
                        const float bossWidth = transform->width * transform->scale;
                        const float bossHeight = transform->height * transform->scale;
                        const float maxBossY = std::max(0.0f, mapHeight - bossHeight);
                        const bool playerOnRightSide = playerCenterX >= bossCenterX;
                        const bool beamTeleportLeftSide = playerOnRightSide;
                        boss->beamFacingRight = playerOnRightSide;
                        boss->lastBeamTeleportLeftSide = beamTeleportLeftSide;
                        pickMidBoss2BeamTeleportTarget(beamTeleportLeftSide, bossWidth, bossHeight, maxBossY, boss->beamTargetX, boss->beamTargetY);
                        boss->state = MidBoss2State::BeamCharge;
                    }
                    else
                    {
                        boss->state = MidBoss2State::SpearCooldown;
                    }
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::SpearCooldown:
            {
                boss->cooldownRemaining = std::max(0.0f, boss->params.spearCooldownAfterLanding - boss->stateTimer);
                const float bossWidth = transform->width * transform->scale;
                const float bossHeight = transform->height * transform->scale;
                const float maxBossY = std::max(0.0f, mapHeight - bossHeight);
                if (boss->stateTimer >= boss->params.spearCooldownAfterLanding)
                {
                    const bool nextSpearLeft = (boss->spearCycleCount % 2) == 0
                        ? boss->nextSpearStartLeftSide
                        : !boss->nextSpearStartLeftSide;
                    pickMidBoss2SpearHoverTarget(nextSpearLeft, bossWidth, bossHeight, maxBossY, boss->hoverTargetX, boss->hoverTargetY);
                    boss->state = MidBoss2State::SpearJump;
                    boss->stateTimer = 0.0f;
                }
                break;
            }

            case MidBoss2State::BeamCharge:
                hideBeamEntities();
                if (boss->stateTimer <= flow.lastDeltaTime)
                {
                    boss->beamShockwaveSpawned = false;
                }
                boss->cooldownRemaining = std::max(0.0f, boss->params.beamChargeTime - boss->stateTimer);
                {
                    const float bossWidth = transform->width * transform->scale;
                    const float bossHeight = transform->height * transform->scale;
                    if (boss->stateTimer <= flow.lastDeltaTime)
                    {
                        const float fromX = transform->x;
                        const float fromY = transform->y;
                        spawnTeleportTrail(
                            fromX,
                            fromY,
                            boss->beamTargetX,
                            boss->beamTargetY,
                            bossWidth,
                            bossHeight,
                            boss->params);
                        boss->teleportFlashRemaining = kMidBoss2TeleportFlashSeconds;
                        flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kMidBoss2BeamChargeShakeSeconds);
                        flow.screenShakeDuration = std::max(flow.screenShakeDuration, kMidBoss2BeamChargeShakeSeconds);
                        flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kMidBoss2BeamChargeShakeAmplitude);
                    }
                    transform->x = boss->beamTargetX;
                }
                transform->y = boss->beamTargetY;
                snapToGround(*transform);
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
                if (!boss->beamShockwaveSpawned)
                {
                    const float bossWidth = transform->width * transform->scale;
                    const float bossHeight = transform->height * transform->scale;
                    const float shockCenterX = beamFacingRight ? transform->x + bossWidth : transform->x;
                    const float shockCenterY = transform->y + bossHeight * 0.5f;
                    const float shockRadius = std::max(bossWidth, bossHeight) * 1.15f + kTileSize * 0.5f;
                    spawnBeamShockwave(shockCenterX, shockCenterY, shockRadius, 1.0f, beamFacingRight ? 1.0f : -1.0f);
                    boss->beamShockwaveSpawned = true;
                }
                {
                    transform->x = boss->beamTargetX;
                }
                transform->y = boss->beamTargetY;
                snapToGround(*transform);
                if (boss->stateTimer >= kBeamFireDuration)
                {
                    boss->state = MidBoss2State::BeamCooldown;
                    boss->stateTimer = 0.0f;
                }
                break;

            case MidBoss2State::BeamCooldown:
                hideBeamEntities();
                boss->cooldownRemaining = std::max(0.0f, boss->params.beamCooldownAfterFire - boss->stateTimer);
                if (boss->stateTimer >= boss->params.beamCooldownAfterFire)
                {
                    boss->nextSpearStartLeftSide = !boss->lastBeamTeleportLeftSide;
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
                if (applyEnemyGravity(*enemy, *transform, kGravity, kMaxFallSpeed))
                {
                    enemy->velocityY = 0.0f;
                }
            }
            destroyOverlappingProtectiveWalls(*transform);
        }
    }

    if (!entitiesToRemove.empty())
    {
        entities.erase(
            std::remove_if(
                entities.begin(),
                entities.end(),
                [&](const std::unique_ptr<Entity>& candidate) -> bool
                {
                    if (!candidate)
                    {
                        return false;
                    }
                    return std::find(entitiesToRemove.begin(), entitiesToRemove.end(), candidate.get()) != entitiesToRemove.end();
                }),
            entities.end());
    }

    for (auto& bullet : newBullets)
    {
        entities.push_back(std::move(bullet));
    }

    for (auto& shield : newShields)
    {
        entities.push_back(std::move(shield));
    }

    for (auto& rubble : newRubbles)
    {
        entities.push_back(std::move(rubble));
    }

    flow.goalUnlocked = photo.groups.hasSpawnedCopy || flow.goalUnlockedBySwitch;
}
}
