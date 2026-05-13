#include "game_scene_internal.h"
#include "game_scene_combat_system.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kEnemyDefeatHitStopSeconds = 0.08f;
    constexpr float kEnemyDefeatShakeSeconds = 0.18f;
    constexpr float kEnemyDefeatShakeAmplitude = 14.0f;

    void TriggerEnemyDefeatFeedback(GameSceneFlowState& flow)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kEnemyDefeatHitStopSeconds);
        flow.screenShakeRemaining = kEnemyDefeatShakeSeconds;
        flow.screenShakeDuration = kEnemyDefeatShakeSeconds;
        flow.screenShakeAmplitude = kEnemyDefeatShakeAmplitude;
    }
}

void GameScene::UpdateEnemies()
{
    Entity* player = FindEntityByTag(kTagPlayer);
    const TransformComponent* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    game_scene_combat_system::UpdateEnemies(
        m_entities,
        m_tileTexture,
        GetMapPixelWidth(),
        GetMapPixelHeight(),
        m_flow,
        m_photo,
        playerTransform,
        [this](TransformComponent& transform) -> bool
        {
            return SnapEnemyToGround(transform);
        },
        [this](Entity& enemyEntity)
        {
            m_eventBus.Publish({ EventType::PlaySoundRequest, &enemyEntity, nullptr, "enemy_gun", 0.0f, 0.0f });
        },
        
        [this](const TransformComponent& bossTransform, Entity& bossEntity) -> bool
        {
            for (auto it = m_entities.begin(); it != m_entities.end(); ++it)
            {
                if (!*it || !HasTag(**it, kTagPhotoBox)) continue;
                const auto* photoTransform = (*it)->GetComponent<TransformComponent>();
                if (!photoTransform) continue;
                if (IntersectsRect(bossTransform, *photoTransform))
                {
                    it = m_entities.erase(it);
                    return true;
                }
            }
            return false;
        },
        [this](int column, int row) -> bool
        {
            return IsSolidTile(column, row);
        });
}

int GameScene::HandleFinderDefeatGhosts(float frameX, float frameY, float frameWidth, float frameHeight)
{
    TransformComponent finderBounds(frameX, frameY, frameWidth, frameHeight);
    int defeatedGhostCount = 0;

    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagEnemy)) continue;

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
    
    game_scene_combat_system::UpdateBullets(
        m_entities,
        GetMapPixelWidth(),
        GetMapPixelHeight(),
        m_flow.lastDeltaTime,
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
        
        [this](float x, float y) -> bool
        {
            const float tileSize = m_tileMap.GetTileSize();
            const int column = static_cast<int>(x / tileSize);
            const int row = static_cast<int>(y / tileSize);
            return IsSolidTile(column, row);
        });
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
        const float hue = static_cast<float>(rand() % 100) * 0.01f;
        item->AddComponent<TintComponent>(0.96f, 0.76f + hue * 0.2f, 0.10f + hue * 0.3f, 1.0f);
        item->AddComponent<SpriteRenderComponent>(m_whiteTexture);
        item->AddComponent<DropItemComponent>(1, velX, velY);
        m_pendingEntities.push_back(std::move(item));
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

    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagDropItem)) continue;

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
                collected.push_back(entity.get());
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
            collected.push_back(entity.get());
        }
    }

    if (!collected.empty())
    {
        m_entities.erase(
            std::remove_if(
                m_entities.begin(),
                m_entities.end(),
                [&](const std::unique_ptr<Entity>& e) -> bool
                {
                    if (!e) return false;
                    for (Entity* ptr : collected)
                    {
                        if (e.get() == ptr) return true;
                    }
                    return false;
                }),
            m_entities.end());
    }
}

int GameScene::GetEnemyDropCount(EnemyArchetype archetype) const
{
    switch (archetype)
    {
    case EnemyArchetype::Walker:
        return 10;
    case EnemyArchetype::Ranged:
        return 10;
    case EnemyArchetype::ShieldBoss:
    case EnemyArchetype::MidBoss2:
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

    for (const auto& entity : m_entities)
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

        const bool isBossShield = HasTag(*entity, "BossShield");
        const bool isCapturedShield = HasTag(*entity, "CapturedShield");
        if (!isBossShield && !isCapturedShield)
        {
            continue;
        }

        if (shield->photoSpawned)
        {
            switch (shield->capturedMode)
            {
            case CapturedShieldMode::Normal:
                shield->gravityEnabled = true;
                shield->grounded = false;
                shield->velocityY = std::min(kMaxFallSpeed, shield->velocityY + kGravity * deltaTime);
                shieldTransform->x += shield->velocityX * deltaTime;
                shieldTransform->y += shield->velocityY * deltaTime;
                shieldTransform->rotation += shield->rotationSpeed * deltaTime;
                if (TrySnapToGround(*shieldTransform,
                    std::max(gGroundSnapDistance, std::fabs(shield->velocityY) * deltaTime + 4.0f)))
                {
                    shield->grounded = true;
                    shield->velocityY = 0.0f;
                    shield->velocityX *= 0.85f;
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
                    shieldTransform->x = playerCenterX + shield->followOffsetX - shieldWidth * 0.5f;
                    shieldTransform->y = playerFootY + shield->followOffsetY;
                    if (shield->hoverElapsed >= shield->hoverDuration)
                    {
                        shield->followPlayer = false;
                        shield->hitEntities.clear();
                    }
                }
                else
                {
                    shieldTransform->y += shield->descendSpeed * deltaTime;
                    if (TrySnapToGround(*shieldTransform, shield->descendSpeed * deltaTime + 4.0f))
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
                            shockwave->AddComponent<TagComponent>("BossShockwave");
                            shockwave->AddComponent<TransformComponent>(
                                shieldTransform->x + shieldTransform->width * shieldTransform->scale * 0.5f - shockW * 0.5f,
                                shieldTransform->y + shieldTransform->height * shieldTransform->scale - shockH,
                                shockW,
                                shockH);
                            shockwave->AddComponent<TintComponent>(0.18f, 0.95f, 1.0f, 0.75f);
                            auto& shockComp = shockwave->AddComponent<ShieldShockwaveComponent>();
                            shockComp.ownerBoss = shield->ownerBoss;
                            shockComp.damage = 1;
                            shockComp.lifetime = 0.4f;
                            shockComp.damagesPlayer = false;
                            spawnedShockwaves.push_back(std::move(shockwave));
                        }
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

        const float shieldW = shieldTransform->width * shieldTransform->scale;

        if ((!shield->photoSpawned || shield->capturedMode == CapturedShieldMode::Normal) &&
            player && playerTransform && IntersectsRect(*playerTransform, *shieldTransform))
        {
            ApplyHazardDamageToPlayer(*player, entity.get(),
                "BossShield damaged player", shield->contactDamage);
        }

        for (const auto& target : m_entities)
        {
            if (!target || target.get() == entity.get())
            {
                continue;
            }
            if (HasTag(*target, "BossShield") || HasTag(*target, "CapturedShield") || HasTag(*target, "BossShockwave"))
            {
                continue;
            }
            if (target.get() == shield->ownerBoss)
            {
                continue;
            }

            auto* enemy = target->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated())
            {
                continue;
            }

            auto* enemyTransform = target->GetComponent<TransformComponent>();
            if (!enemyTransform || !IntersectsRect(*shieldTransform, *enemyTransform))
            {
                continue;
            }
            if (shield->contactDamage <= 0)
            {
                continue;
            }

            if (shield->photoSpawned)
            {
                const bool alreadyHit = std::find(
                    shield->hitEntities.begin(),
                    shield->hitEntities.end(),
                    target.get()) != shield->hitEntities.end();
                if (alreadyHit)
                {
                    continue;
                }
                shield->hitEntities.push_back(target.get());
            }

            HandleEnemyDamage(*target, entity.get(), shield->contactDamage, "BossShield damaged enemy");

            const float shieldCenterX = shieldTransform->x + shieldW * 0.5f;
            const float enemyCenterX = enemyTransform->x + enemyTransform->width * enemyTransform->scale * 0.5f;
            const float dir = enemyCenterX >= shieldCenterX ? 1.0f : -1.0f;
            enemyTransform->x += dir * shield->knockbackGrids * kTileSize;
        }
    }

    for (auto& shockwave : spawnedShockwaves)
    {
        m_entities.push_back(std::move(shockwave));
    }

    std::vector<Entity*> shockwavesToRemove;
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "BossShockwave")) continue;

        auto* shockwave = entity->GetComponent<ShieldShockwaveComponent>();
        auto* shockTransform = entity->GetComponent<TransformComponent>();
        if (!shockwave || !shockTransform) continue;

        shockwave->elapsed += deltaTime;
        if (shockwave->elapsed >= shockwave->lifetime)
        {
            shockwavesToRemove.push_back(entity.get());
            continue;
        }

        if (shockwave->damagesPlayer &&
            !shockwave->hitPlayer && player && playerTransform
            && IntersectsRect(*playerTransform, *shockTransform))
        {
            ApplyHazardDamageToPlayer(*player, entity.get(),
                "BossShockwave damaged player", shockwave->damage);
            shockwave->hitPlayer = true;
        }

        for (const auto& target : m_entities)
        {
            if (!target || target.get() == entity.get()) continue;
            if (HasTag(*target, "BossShield") || HasTag(*target, "CapturedShield") || HasTag(*target, "BossShockwave")) continue;
            if (target.get() == shockwave->ownerBoss) continue;

            auto* enemy = target->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated()) continue;

            auto* enemyTransform = target->GetComponent<TransformComponent>();
            if (!enemyTransform || !IntersectsRect(*shockTransform, *enemyTransform)) continue;

            const bool alreadyHit = std::find(
                shockwave->hitEntities.begin(),
                shockwave->hitEntities.end(),
                target.get()) != shockwave->hitEntities.end();
            if (alreadyHit) continue;

            HandleEnemyDamage(*target, entity.get(), shockwave->damage, "BossShockwave damaged enemy");
            shockwave->hitEntities.push_back(target.get());

            const float shockCenterX = shockTransform->x + shockTransform->width * shockTransform->scale * 0.5f;
            const float enemyCenterX = enemyTransform->x + enemyTransform->width * enemyTransform->scale * 0.5f;
            const float dir = enemyCenterX >= shockCenterX ? 1.0f : -1.0f;
            enemyTransform->x += dir * shockwave->knockbackGrids * kTileSize;
        }
    }

    if (!shockwavesToRemove.empty())
    {
        m_entities.erase(
            std::remove_if(
                m_entities.begin(),
                m_entities.end(),
                [&](const std::unique_ptr<Entity>& e) -> bool
                {
                    if (!e) return false;
                    for (Entity* ptr : shockwavesToRemove)
                    {
                        if (e.get() == ptr) return true;
                    }
                    return false;
                }),
            m_entities.end());
    }
}
void GameScene::HandleEnemyPlayerCollisions(Entity& player)
{
    const auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    for (const auto& entity : m_entities)
    {
        if (!entity || entity.get() == &player)
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
            entity.get(),
            "GameScene player damaged by enemy",
            enemy->GetContactDamage());
    }
}

void GameScene::HandleWalkerMeleeAttackCollisions(Entity& player)
{
    const auto* playerTransform = player.GetComponent<TransformComponent>();

    for (const auto& entity : m_entities)
    {
        if (!entity) continue;

        const auto* tag = entity->GetComponent<TagComponent>();
        if (!tag || tag->tag != "WalkerMeleeAttack") continue;

        const auto* meleeTransform = entity->GetComponent<TransformComponent>();
        if (!meleeTransform) continue;

        // プレイヤーへのダメージ
        if (playerTransform && IntersectsRect(*playerTransform, *meleeTransform))
        {
            ApplyHazardDamageToPlayer(
                player,
                entity.get(),
                "GameScene player damaged by WalkerMeleeAttack",
                1);
        }

        // 敵へのダメージ
        for (const auto& target : m_entities)
        {
            if (!target) continue;

            auto* enemy = target->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated()) continue;

            const auto* enemyTransform = target->GetComponent<TransformComponent>();
            if (!enemyTransform) continue;

            if (!IntersectsRect(*meleeTransform, *enemyTransform)) continue;

            HandleEnemyDamage(*target, entity.get(), 1, "WalkerMeleeAttack hit enemy");
        }
    }
}

void GameScene::RemoveDefeatedEnemies()
{
    const float cameraLeft = m_flow.cameraX - 48.0f;
    const float cameraRight = m_flow.cameraX + gCameraViewWidth + 48.0f;

    for (const auto& entity : m_entities)
    {
        if (!entity) continue;

        auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsDefeated()) continue;
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

    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                const auto* enemy = entity ? entity->GetComponent<EnemyComponent>() : nullptr;
                if (enemy && enemy->IsDefeated() && !enemy->respawnEnabled)
                {
                    return true;
                }
                const auto* lifetime = entity ? entity->GetComponent<PhotoCopyLifetimeComponent>() : nullptr;
                if (!lifetime)
                {
                    return false;
                }
                return lifetime->IsExpired();
            }),
        m_entities.end());

    RefreshPhotoGroupState();
}

void GameScene::HandleEnemyDamage(Entity& enemy, Entity* sourceEntity, int amount, const char* logMessage)
{
    auto* enemyComponent = enemy.GetComponent<EnemyComponent>();
    if (!enemyComponent || !enemyComponent->IsEnabled() || enemyComponent->IsDefeated())
    {
        return;
    }

    auto* damageFlash = enemy.GetComponent<DamageCooldownComponent>();
    if (!damageFlash)
    {
        damageFlash = &enemy.AddComponent<DamageCooldownComponent>(0.18f);
    }
    damageFlash->Trigger();

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
            enemyComponent->MarkDefeated();
            defeatedThisHit = true;
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
        TriggerEnemyDefeatFeedback(m_flow);
    }
    m_eventBus.Publish({ EventType::PlaySoundRequest, &enemy, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &enemy, sourceEntity, logMessage, 0.0f, 0.0f });
}

