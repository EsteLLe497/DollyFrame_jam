#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_combat_system.h"

#include <array>
#include <cmath>

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
    inline constexpr float kEnemy2IdleFps = 12.0f;
    inline constexpr int kEnemy2AttackSheetColumns = 10;
    inline constexpr int kEnemy2AttackSheetRows = 8;
    inline constexpr int kEnemy2AttackFrameCount = 80;
    inline constexpr float kEnemy2AttackFps = 18.0f;

    constexpr float kEnemyDefeatHitStopSeconds = 0.08f;
    constexpr float kEnemyDefeatShakeSeconds = 0.18f;
    constexpr float kEnemyDefeatShakeAmplitude = 14.0f;

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

    // Enemy2 is left-facing; the shot is emitted on the attack clip's 39th frame.
    sprite->SetFlipX(false);
    animation->DefineClip("idle", resolvedIdleTexture, kEnemy2IdleSheetColumns, kEnemy2IdleSheetRows, 0, kEnemy2IdleFrameCount, kEnemy2IdleFps, true);
    animation->DefineClip("attack", resolvedAttackTexture, kEnemy2AttackSheetColumns, kEnemy2AttackSheetRows, 0, kEnemy2AttackFrameCount, kEnemy2AttackFps, false);
    animation->Play("idle", true);
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
                if (!*it) continue;
                const bool isPhotoBox = HasTag(**it, kTagPhotoBox);
                const bool isGroundedCapturedShield =
                    HasTag(**it, "CapturedShield") &&
                    (*it)->GetComponent<ShieldComponent>() &&
                    (*it)->GetComponent<ShieldComponent>()->photoSpawned &&
                    (*it)->GetComponent<ShieldComponent>()->grounded;
                if (!isPhotoBox && !isGroundedCapturedShield) continue;
                const auto* photoTransform = (*it)->GetComponent<TransformComponent>();
                if (!photoTransform) continue;
                if (IntersectsRect(bossTransform, *photoTransform))
                {
                    if (isPhotoBox)
                    {
                        it = m_entities.erase(it);
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
    case EnemyArchetype::Charger:
        return 10 + (rand() % 21);
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
    std::vector<Entity*> shieldsToRemove;
    std::vector<Entity*> objectsToRemove;

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
                return;
            }
            shieldBoss->knockbackActive = true;
            shieldBoss->knockbackTimer = 0.0f;
            shieldBoss->knockbackStartX = transform.x;
            shieldBoss->knockbackStartY = transform.y;
            shieldBoss->knockbackTargetX = transform.x + direction * distance;
            return;
        }
        enemy.knockbackActive = true;
        enemy.knockbackTimer = 0.0f;
        enemy.knockbackStartX = transform.x;
        enemy.knockbackStartY = transform.y;
        enemy.knockbackTargetX = transform.x + direction * distance;
    };

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
                    for (const auto& target : m_entities)
                    {
                        if (!target || target.get() == entity.get())
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
                        objectsToRemove.push_back(target.get());
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
                            shockwave->AddComponent<TagComponent>("BossShockwave");
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

        if (isBossShield && shield->ownerBoss)
        {
            auto* ownerBoss = shield->ownerBoss->GetComponent<ShieldBossComponent>();
            auto* ownerTransform = shield->ownerBoss->GetComponent<TransformComponent>();
            if (ownerBoss && ownerTransform && ownerBoss->knockbackActive)
            {
                constexpr float kGuardShieldW = 48.0f;
                constexpr float kGuardShieldH = 144.0f;
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
                    ? ownerTransform->x + ownerW
                    : ownerTransform->x - kGuardShieldW;
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
            ApplyHazardDamageToPlayer(*player, entity.get(),
                "BossShield damaged player", shield->contactDamage);
            if (canRemoveNormalCapturedShield)
            { 
                shieldsToRemove.push_back(entity.get());
            }
        }

        bool removeShieldAfterObjectHit = false;
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
                    target.get()) != shield->hitEntities.end();
                if (alreadyHit)
                {
                    if (canRemoveNormalCapturedShield)
                    {
                        removeShieldAfterObjectHit = true;
                        break;
                    }
                    continue;
                }
                shield->hitEntities.push_back(target.get());
            }

            HandleEnemyDamage(*target, entity.get(), shield->contactDamage, "BossShield damaged enemy");

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
            startEnemyKnockback(*target, *enemy, *enemyTransform, dir, shield->knockbackGrids * kTileSize);

            if (canRemoveNormalCapturedShield)
            {
                removeShieldAfterObjectHit = true;
                break;
            }
        }

        if (removeShieldAfterObjectHit)
        {
            shieldsToRemove.push_back(entity.get());
        }
    }

    for (auto& shockwave : spawnedShockwaves)
    {
        m_entities.push_back(std::move(shockwave));
    }

    if (!objectsToRemove.empty())
    {
        m_entities.erase(
            std::remove_if(
                m_entities.begin(),
                m_entities.end(),
                [&](const std::unique_ptr<Entity>& e) -> bool
                {
                    if (!e) return false;
                    return std::find(objectsToRemove.begin(), objectsToRemove.end(), e.get()) != objectsToRemove.end();
                }),
            m_entities.end());
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

            auto* enemyTransform = target->GetComponent<TransformComponent>();
            if (!enemyTransform || !IntersectsRect(*shockTransform, *enemyTransform)) continue;

            if (HasTag(*target, kTagPhotoBox))
            {
                objectsToRemove.push_back(target.get());
                continue;
            }

            auto* enemy = target->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated()) continue;

            const bool alreadyHit = std::find(
                shockwave->hitEntities.begin(),
                shockwave->hitEntities.end(),
                target.get()) != shockwave->hitEntities.end();
            if (alreadyHit) continue;

            HandleEnemyDamage(*target, entity.get(), shockwave->damage, "BossShockwave damaged enemy");
            shockwave->hitEntities.push_back(target.get());

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
            startEnemyKnockback(*target, *enemy, *enemyTransform, dir, shockwave->knockbackGrids * kTileSize);
        }
    }

    if (!objectsToRemove.empty())
    {
        m_entities.erase(
            std::remove_if(
                m_entities.begin(),
                m_entities.end(),
                [&](const std::unique_ptr<Entity>& e) -> bool
                {
                    if (!e) return false;
                    return std::find(objectsToRemove.begin(), objectsToRemove.end(), e.get()) != objectsToRemove.end();
                }),
            m_entities.end());
    }

    if (!shieldsToRemove.empty())
    {
        m_entities.erase(
            std::remove_if(
                m_entities.begin(),
                m_entities.end(),
                [&](const std::unique_ptr<Entity>& e) -> bool
                {
                    if (!e) return false;
                    return std::find(shieldsToRemove.begin(), shieldsToRemove.end(), e.get()) != shieldsToRemove.end();
                }),
            m_entities.end());
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
        if (enemy->GetArchetype() == EnemyArchetype::ShieldBoss)
        {
            enemy->respawnEnabled = false;
            m_flow.shieldBossDefeatedThisScene = true;
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

    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
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
                    for (const auto& candidate : m_entities)
                    {
                        if (!candidate || candidate.get() != shield->ownerBoss)
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
        TriggerEnemyDefeatFeedback(m_flow);
    }
    m_eventBus.Publish({ EventType::PlaySoundRequest, &enemy, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &enemy, sourceEntity, logMessage, 0.0f, 0.0f });
}

