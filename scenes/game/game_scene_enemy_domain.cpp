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
        // photoBox陦晉ｪ√メ繧ｧ繝・け繧ｳ繝ｼ繝ｫ繝舌ャ繧ｯ
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
        return 50;
    default:
        return 5;
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

        // 謦・ｴ譎ゅ↓髱櫁｡ｨ遉ｺ・・ｽ薙◆繧雁愛螳夂┌蜉ｹ蛹厄ｼ・判髱｢螟悶↓遘ｻ蜍・
        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            tint->a = 0.0f;
        }
        enemy->SetEnabled(false);
        // 逕ｻ髱｢螟悶↓遘ｻ蜍輔＆縺帙※蠖薙◆繧雁愛螳壹ｒ蝗樣∩
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
    bool defeatedThisHit = false;
    if (auto* health = enemy.GetComponent<HealthComponent>())
    {
        health->ApplyDamage(amount);
        if (health->IsDead())
        {
            enemyComponent->MarkDefeated();
            defeatedThisHit = true;
            
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

