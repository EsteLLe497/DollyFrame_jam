#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_photo_tray_system.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_world_interaction_system.h"
#include "photo_system.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace game_scene_detail;

namespace
{
    int GetAttackCaptureCount(const PhotoCaptureState& capture)
    {
        if (capture.attackCaptureCount > 0)
        {
            return capture.attackCaptureCount;
        }

        const int countedItems = static_cast<int>(std::count_if(
            capture.items.begin(),
            capture.items.end(),
            [](const CapturedPhotoItem& item)
            {
                return item.enemyAttackPaste;
            }));
        if (countedItems > 0)
        {
            return countedItems;
        }

        return capture.hasPhoto && capture.containsEnemyAttackPaste ? 1 : 0;
    }

    void PreserveSelectedTheme(PhotoCaptureState& capture, PhotoFilterTheme theme)
    {
        capture = PhotoCaptureState{};
        capture.selectedTheme = theme;
    }
}

void GameScene::UpdatePlayerAfterimages(float deltaTime)
{
    game_scene_player_visual_system::UpdateAfterimages(m_player, deltaTime);
}

void GameScene::TrySpawnPlayerAfterimage(const TransformComponent& transform)
{
    game_scene_player_visual_system::TrySpawnAfterimage(m_player, transform);
}

void GameScene::HandlePhotoCapture()
{
    photo_system::HandleCapture(*this);
}

void GameScene::HandlePhotoSpawn()
{
    photo_system::HandleSpawn(*this);
}

void GameScene::TryUseAttackCaptureSlot()
{
    const bool attackPastePressed = Input_IsActionPressed(InputAction::AttackPaste);
    if (!attackPastePressed)
    {
        return;
    }

    Logger::Info(
        std::string("AttackPaste pressed: cameraMode=") +
        (m_flow.cameraMode ? "1" : "0") +
        " placementActive=" + (m_photo.placement.active ? "1" : "0") +
        " pasteActive=" + (m_player.pasteAnimationActive ? "1" : "0") +
        " attackHasPhoto=" + (m_photo.attackCapture.hasPhoto ? "1" : "0") +
        " attackContainsPaste=" + (m_photo.attackCapture.containsEnemyAttackPaste ? "1" : "0") +
        " attackCount=" + std::to_string(GetAttackCaptureCount(m_photo.attackCapture)) +
        " selectedCount=" + std::to_string(GetAttackCaptureCount(m_photo.capture)));

    if (m_flow.cameraMode ||
        m_photo.placement.active ||
        m_player.pasteAnimationActive)
    {
        Logger::Info("AttackPaste blocked: camera/placement/paste animation active");
        return;
    }

    if (!m_photo.attackCapture.hasPhoto || !m_photo.attackCapture.containsEnemyAttackPaste)
    {
        if (m_photo.capture.hasPhoto && m_photo.capture.containsEnemyAttackPaste)
        {
            const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
            const int existingCount = GetAttackCaptureCount(m_photo.attackCapture);
            const int captureCount = GetAttackCaptureCount(m_photo.capture);
            m_photo.attackCapture = m_photo.capture;
            m_photo.attackCapture.attackCaptureCount = existingCount + captureCount;
            PreserveSelectedTheme(m_photo.capture, selectedTheme);
            Logger::Info(
                std::string("AttackPaste promoted current capture: count=") +
                std::to_string(m_photo.attackCapture.attackCaptureCount));
        }
        else if (m_photo.selectedCaptureSlot >= 0 &&
            m_photo.selectedCaptureSlot < static_cast<int>(m_photo.savedCaptures.size()) &&
            m_photo.savedCaptures[m_photo.selectedCaptureSlot].hasPhoto &&
            m_photo.savedCaptures[m_photo.selectedCaptureSlot].containsEnemyAttackPaste)
        {
            m_photo.attackCapture = m_photo.savedCaptures[m_photo.selectedCaptureSlot];
            m_photo.savedCaptures[m_photo.selectedCaptureSlot] = PhotoCaptureState{};
            Logger::Info(
                std::string("AttackPaste loaded from saved slot: slot=") +
                std::to_string(m_photo.selectedCaptureSlot) +
                " count=" + std::to_string(GetAttackCaptureCount(m_photo.attackCapture)));
        }
    }

    if (!m_photo.attackCapture.hasPhoto || !m_photo.attackCapture.containsEnemyAttackPaste)
    {
        Logger::Info("AttackPaste aborted: no enemy-attack capture available");
        return;
    }

    // Keep the selected attack capture available to the normal paste flow.
    // This lets the existing placement/spawn path handle Q-triggered attack shots.
    m_photo.capture = m_photo.attackCapture;

    Entity* player = FindEntityByTag(kTagPlayer);
    if (!player)
    {
        Logger::Warn("AttackPaste aborted: player entity not found");
        return;
    }

    const auto* playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        Logger::Warn("AttackPaste aborted: player transform missing");
        return;
    }

    const CapturedPhotoItem* attackItem = nullptr;
    for (const auto& item : m_photo.attackCapture.items)
    {
        if (item.enemyAttackPaste)
        {
            attackItem = &item;
            break;
        }
    }

    if (!attackItem)
    {
        Logger::Warn("AttackPaste aborted: enemy-attack item missing from capture");
        return;
    }

    const int desiredBossMotionClip =
        attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst ? 1 :
        attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst ? 2 :
        0;
    const CapturedPhotoItem* bossMotionItem = nullptr;
    for (const auto& item : m_photo.attackCapture.items)
    {
        if (!item.enemyAttackPaste &&
            item.spawnArchetype == CapturedSpawnArchetype::None &&
            item.origin == PhotoCopyOrigin::Enemy &&
            (desiredBossMotionClip == 0 || item.bossMotionClip == desiredBossMotionClip))
        {
            bossMotionItem = &item;
            break;
        }
    }
    if (!bossMotionItem && desiredBossMotionClip != 0)
    {
        for (const auto& item : m_photo.attackCapture.items)
        {
            if (!item.enemyAttackPaste &&
                item.spawnArchetype == CapturedSpawnArchetype::None &&
                item.origin == PhotoCopyOrigin::Enemy)
            {
                bossMotionItem = &item;
                break;
            }
        }
    }

    auto finishAttackUse = [&](int remainingAttackCount = -1)
    {
        const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
        m_player.captureAnimationActive = false;
        m_player.captureAnimationReleased = false;
        m_player.pasteAnimationActive = true;
        m_player.pasteAnimationEnemyAttack = true;
        m_player.pasteAnimationReleased = true;
        if (remainingAttackCount >= 0)
        {
            if (remainingAttackCount > 0)
            {
                PhotoCaptureState remainingCapture = m_photo.attackCapture;
                remainingCapture.attackCaptureCount = remainingAttackCount;
                m_photo.attackCapture = remainingCapture;
                m_photo.capture = remainingCapture;
            }
            else
            {
                PreserveSelectedTheme(m_photo.attackCapture, selectedTheme);
                PreserveSelectedTheme(m_photo.capture, selectedTheme);
            }
        }
        else
        {
            PreserveSelectedTheme(m_photo.attackCapture, selectedTheme);
            PreserveSelectedTheme(m_photo.capture, selectedTheme);
        }
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Used captured attack", 0.0f, 0.0f });
    };

    struct AttackAim
    {
        float x = 1.0f;
        float y = 0.0f;
        int direction = 1;
    };

    const auto resolveAttackAimTowardBoss = [&](float fromX, float fromY) -> AttackAim
    {
        const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
        const Entity* bestBoss = nullptr;
        float bestDistanceSq = std::numeric_limits<float>::max();
        for (const auto& candidate : m_world.Entities())
        {
            if (!candidate)
            {
                continue;
            }

            const auto* enemy = candidate->GetComponent<EnemyComponent>();
            const auto* boss = candidate->GetComponent<MidBoss3Component>();
            const auto* transform = candidate->GetComponent<TransformComponent>();
            if (!enemy || !boss || !transform || !enemy->IsEnabled() || enemy->IsDefeated())
            {
                continue;
            }

            const float bossCenterX = transform->x + transform->width * transform->scale * 0.5f;
            const float bossCenterY = transform->y + transform->height * transform->scale * 0.5f;
            const float dx = bossCenterX - fromX;
            const float dy = bossCenterY - fromY;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                bestBoss = candidate.get();
            }
        }

        if (bestBoss)
        {
            const auto* transform = bestBoss->GetComponent<TransformComponent>();
            const float bossCenterX = transform->x + transform->width * transform->scale * 0.5f;
            const float bossCenterY = transform->y + transform->height * transform->scale * 0.5f;
            const float dx = bossCenterX - fromX;
            const float dy = bossCenterY - fromY;
            const float length = std::max(0.001f, std::hypot(dx, dy));
            return {
                dx / length,
                dy / length,
                bossCenterX >= playerCenterX ? 1 : -1,
            };
        }

        const int fallbackDirection = m_player.facingRight ? 1 : -1;
        return { static_cast<float>(fallbackDirection), 0.0f, fallbackDirection };
    };

    const auto resolveAttackAimTowardMidBoss2 = [&](float fromX, float fromY) -> AttackAim
    {
        const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
        const Entity* bestBoss = nullptr;
        float bestDistanceSq = std::numeric_limits<float>::max();
        for (const auto& candidate : m_world.Entities())
        {
            if (!candidate)
            {
                continue;
            }

            const auto* enemy = candidate->GetComponent<EnemyComponent>();
            const auto* boss = candidate->GetComponent<MidBoss2Component>();
            const auto* transform = candidate->GetComponent<TransformComponent>();
            if (!enemy || !boss || !transform || !enemy->IsEnabled() || enemy->IsDefeated())
            {
                continue;
            }

            const float bossCenterX = transform->x + transform->width * transform->scale * 0.5f;
            const float bossCenterY = transform->y + transform->height * transform->scale * 0.5f;
            const float dx = bossCenterX - fromX;
            const float dy = bossCenterY - fromY;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                bestBoss = candidate.get();
            }
        }

        if (bestBoss)
        {
            const auto* transform = bestBoss->GetComponent<TransformComponent>();
            const float bossCenterX = transform->x + transform->width * transform->scale * 0.5f;
            const float bossCenterY = transform->y + transform->height * transform->scale * 0.5f;
            const float dx = bossCenterX - fromX;
            const float dy = bossCenterY - fromY;
            const float length = std::max(0.001f, std::hypot(dx, dy));
            return {
                dx / length,
                dy / length,
                bossCenterX >= playerCenterX ? 1 : -1,
            };
        }

        const int fallbackDirection = m_player.facingRight ? 1 : -1;
        return { static_cast<float>(fallbackDirection), 0.0f, fallbackDirection };
    };

    if (attackItem->spawnArchetype == CapturedSpawnArchetype::MidBoss3FistAttack)
    {
        constexpr float kTileSize = 48.0f;
        constexpr float kFistSpeed = 560.0f;
        constexpr float kFistGap = kTileSize;
        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float playerCenterX = playerTransform->x + playerWidth * 0.5f;
        const float playerCenterY = playerTransform->y + playerHeight * 0.5f;
        const AttackAim placementAim = resolveAttackAimTowardBoss(playerCenterX, playerCenterY);
        const int attackDirection = placementAim.direction;
        const bool facingRight = attackDirection >= 0;
        const float fistW = kTileSize * 3.0f;
        const float fistH = kTileSize * 2.0f;
        const float fistX = facingRight
            ? playerTransform->x + playerWidth + kFistGap
            : playerTransform->x - kFistGap - fistW;
        const float fistY = playerTransform->y + playerHeight * 0.5f - fistH * 0.5f;
        const AttackAim fireAim = resolveAttackAimTowardBoss(
            fistX + fistW * 0.5f,
            fistY + fistH * 0.5f);

        auto fistEntity = std::make_unique<Entity>();
        fistEntity->AddComponent<TagComponent>(kTagBullet);
        fistEntity->AddComponent<TransformComponent>(fistX, fistY, fistW, fistH);
        fistEntity->AddComponent<TintComponent>(0.96f, 0.52f, 0.18f, 1.0f);
        fistEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
        fistEntity->AddComponent<ProjectileComponent>(
            fireAim.x * kFistSpeed,
            fireAim.y * kFistSpeed,
            1,
            ProjectileComponent::Owner::Photo);
        auto& attack = fistEntity->AddComponent<CapturedMidBoss3AttackComponent>(CapturedMidBoss3AttackKind::Fist);
        attack.aimX = fireAim.x;
        attack.aimY = fireAim.y;
        attack.direction = fireAim.direction;
        attack.launched = true;
        if (auto* sprite = fistEntity->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetSourceRect(0.0f, 0.0f, 1.0f, 1.0f);
            sprite->SetFlipX(!facingRight);
        }
        if (auto* transform = fistEntity->GetComponent<TransformComponent>())
        {
            transform->rotation = std::atan2(fireAim.y, fireAim.x);
        }
        m_world.QueueSpawn(std::move(fistEntity));
        finishAttackUse();
        return;
    }

    if (attackItem->spawnArchetype == CapturedSpawnArchetype::MidBoss3DrillAttack)
    {
        constexpr float kTileSize = 48.0f;
        constexpr float kDrillWaitSeconds = 3.0f;
        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float playerCenterX = playerTransform->x + playerWidth * 0.5f;
        const float playerCenterY = playerTransform->y + playerHeight * 0.5f;
        const AttackAim placementAim = resolveAttackAimTowardBoss(playerCenterX, playerCenterY);
        const int attackDirection = placementAim.direction;
        const float drillW = kTileSize * 4.0f;
        const float drillH = kTileSize * 2.0f;
        const float drillX = playerCenterX - drillW * 0.5f;
        const float drillY = std::max(0.0f, playerTransform->y - drillH - kTileSize);

        auto drillEntity = std::make_unique<Entity>();
        drillEntity->AddComponent<TagComponent>(kTagBullet);
        drillEntity->AddComponent<TransformComponent>(drillX, drillY, drillW, drillH);
        drillEntity->AddComponent<TintComponent>(1.0f, 0.55f, 0.18f, 0.92f);
        drillEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
        drillEntity->AddComponent<ProjectileComponent>(0.0f, 0.0f, 2, ProjectileComponent::Owner::Photo);
        auto& attack = drillEntity->AddComponent<CapturedMidBoss3AttackComponent>(CapturedMidBoss3AttackKind::Drill);
        attack.waitRemaining = kDrillWaitSeconds;
        attack.direction = attackDirection;
        attack.aimX = static_cast<float>(attackDirection);
        attack.aimY = 0.0f;
        attack.launched = false;
        attack.groundRush = false;
        attack.attachedToBoss = false;
        attack.bossDamageTimer = 0.0f;
        attack.knockbackRemaining = 0.0f;
        attack.followOffsetX = drillX - playerTransform->x;
        attack.followOffsetY = drillY - playerTransform->y;
        attack.waitBaseX = drillX;
        attack.waitBaseY = drillY;
        attack.waitBaseInitialized = true;
        if (auto* transform = drillEntity->GetComponent<TransformComponent>())
        {
            const AttackAim fireAim = resolveAttackAimTowardBoss(
                transform->x + transform->width * transform->scale * 0.5f,
                transform->y + transform->height * transform->scale * 0.5f);
            attack.aimX = fireAim.x;
            attack.aimY = fireAim.y;
            attack.direction = fireAim.direction;
            transform->rotation = std::atan2(fireAim.y, fireAim.x);
        }
        m_world.QueueSpawn(std::move(drillEntity));
        finishAttackUse();
        return;
    }

    if (attackItem->spawnArchetype == CapturedSpawnArchetype::Projectile && attackItem->spearProjectile)
    {
        constexpr float kTileSize = 48.0f;
        constexpr float kSpearLaunchDelay = 0.14f;
        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float playerCenterX = playerTransform->x + playerWidth * 0.5f;
        const float spearW = attackItem->width > 0.0f ? attackItem->width : kTileSize;
        const float spearH = attackItem->height > 0.0f ? attackItem->height : kTileSize * 2.0f;
        const float kSpearSpawnGap = std::max(12.0f, playerHeight * 0.08f);
        const float spearX = playerCenterX - spearW * 0.5f;
        const float spearY = std::max(0.0f, playerTransform->y - spearH - kSpearSpawnGap);
        const AttackAim fireAim = resolveAttackAimTowardMidBoss2(
            spearX + spearW * 0.5f,
            spearY + spearH * 0.5f);

        auto spearEntity = std::make_unique<Entity>();
        spearEntity->AddComponent<TagComponent>(kTagBullet);
        spearEntity->AddComponent<TransformComponent>(spearX, spearY, spearW, spearH);
        spearEntity->AddComponent<TintComponent>(attackItem->tintR, attackItem->tintG, attackItem->tintB, attackItem->tintA);
        spearEntity->AddComponent<SpriteRenderComponent>(attackItem->textureId >= 0 ? attackItem->textureId : m_tileTexture);
        spearEntity->AddComponent<ProjectileComponent>(0.0f, 0.0f, attackItem->projectileDamage, ProjectileComponent::Owner::Photo);
        auto& spear = spearEntity->AddComponent<MidBoss2SpearComponent>();
        spear.launched = false;
        spear.stuck = false;
        spear.directionX = fireAim.x;
        spear.directionY = fireAim.y;
        spear.targetDirectionX = fireAim.x;
        spear.targetDirectionY = fireAim.y;
        spear.launchDelay = kSpearLaunchDelay;
        spear.launchTimer = 0.0f;
        spear.fadeDuration = 1.0f;
        spear.fadeRemaining = spear.fadeDuration;
        spear.travelDistance = 0.0f;
        if (auto* transform = spearEntity->GetComponent<TransformComponent>())
        {
            transform->rotation = std::atan2(fireAim.y, fireAim.x);
        }
        m_world.QueueSpawn(std::move(spearEntity));
        Logger::Info(
            std::string("AttackPaste spawned MidBoss2 spear: x=") +
            std::to_string(spearX) +
            " y=" + std::to_string(spearY) +
            " dirX=" + std::to_string(fireAim.x) +
            " dirY=" + std::to_string(fireAim.y) +
            " countBefore=" + std::to_string(GetAttackCaptureCount(m_photo.attackCapture)));
        finishAttackUse(std::max(0, GetAttackCaptureCount(m_photo.attackCapture) - 1));
        return;
    }

    if (attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst ||
        attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst)
    {
        constexpr float kTileSize = 48.0f;
        constexpr float kShieldRaiseOffsetY = kTileSize * 0.5f;
        constexpr float kBossRushSpeed = 520.0f;
        constexpr float kBossJumpDescendSpeed = 1800.0f;
        constexpr float kBossSlamShieldVisualWidth = 288.0f;
        constexpr float kBossSlamShieldVisualHeight = 234.0f;
        constexpr int kCapturedBossRushStartFrame = 80;
        constexpr int kCapturedBossSlamStartFrame = 99;
        constexpr float kCapturedBossRushVisualLifetime = 0.5f;
        constexpr float kCapturedBossSlamVisualLifetime = 0.65f;
        const bool rushCapture = attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst;
        const bool slamCapture = attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst;
        const bool spawnRushShieldVisual = rushCapture;
        const bool facingRight = m_player.facingRight;
        const float rushVelocityX = facingRight ? kBossRushSpeed : -kBossRushSpeed;
        const float visualVelocityX = rushCapture ? rushVelocityX : 0.0f;
        const float visualVelocityY = slamCapture ? kBossJumpDescendSpeed : 0.0f;
        const float capturedBossVisualLifetime = rushCapture
            ? kCapturedBossRushVisualLifetime
            : kCapturedBossSlamVisualLifetime;
        const int capturedBossVisualStartFrame = rushCapture
            ? kCapturedBossRushStartFrame
            : kCapturedBossSlamStartFrame;
        const int attackPasteOrder = m_photo.groups.nextPasteOrder++;
        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float playerCenterX = playerTransform->x + playerWidth * 0.5f;
        const float playerFootY = playerTransform->y + playerHeight;

        float shieldW = kTileSize;
        float shieldH = kTileSize * 4.0f;
        float shieldX = facingRight
            ? playerTransform->x + playerWidth
            : playerTransform->x - shieldW;
        float shieldY = playerTransform->y;

        if (attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst)
        {
            shieldX = facingRight
                ? playerTransform->x + playerWidth
                : playerTransform->x - shieldW;
            shieldY = playerFootY - shieldH - kShieldRaiseOffsetY;
        }
        else
        {
            shieldW = kBossSlamShieldVisualWidth;
            shieldH = kBossSlamShieldVisualHeight;
            const float playerFrontX = facingRight
                ? playerTransform->x + playerWidth + kTileSize * 0.5f
                : playerTransform->x - kTileSize * 0.5f;
            shieldX = playerFrontX - shieldW * 0.5f;
            shieldY = playerFootY - kTileSize * 6.0f - shieldH;
        }

        auto shieldEntity = std::make_unique<Entity>();
        shieldEntity->AddComponent<TagComponent>("CapturedShield");
        shieldEntity->AddComponent<PhotoPasteOrderComponent>(attackPasteOrder);
        shieldEntity->AddComponent<TransformComponent>(shieldX, shieldY, shieldW, shieldH);
        shieldEntity->AddComponent<TintComponent>(
            attackItem->tintR,
            attackItem->tintG,
            attackItem->tintB,
            rushCapture ? 0.0f : attackItem->tintA);
        const int actualShieldTextureId = slamCapture
            ? m_assets.GetTexture("boss1_shield_attack02")
            : attackItem->textureId;
        shieldEntity->AddComponent<SpriteRenderComponent>(actualShieldTextureId >= 0 ? actualShieldTextureId : m_tileTexture);
        auto& shieldComp = shieldEntity->AddComponent<ShieldComponent>();
        shieldComp.attached = false;
        shieldComp.photoSpawned = true;
        shieldComp.rotationSpeed = 0.0f;
        shieldComp.velocityX = 0.0f;
        shieldComp.velocityY = 0.0f;
        shieldComp.contactDamage = 1;
        shieldComp.knockbackGrids = 3.0f;
        shieldComp.grounded = false;
        shieldComp.shockwaveSpawned = false;

        if (slamCapture)
        {
            ConfigureBossShieldSpriteAnimation(*shieldEntity);
            if (auto* animation = shieldEntity->GetComponent<SpriteSheetAnimationComponent>())
            {
                animation->Play("attack02", true);
                animation->SetCurrentLocalFrameIndex(kCapturedBossSlamStartFrame);
            }
            if (auto* sprite = shieldEntity->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetFlipX(facingRight);
            }
        }
        else if (auto* sprite = shieldEntity->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetSourceRect(attackItem->sourceX, attackItem->sourceY, attackItem->sourceWidth, attackItem->sourceHeight);
            sprite->SetFlipX(!facingRight);
        }

        if (attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst)
        {
            shieldComp.capturedMode = CapturedShieldMode::RushBurst;
            shieldComp.gravityEnabled = false;
            shieldComp.contactDamage = 2;
            shieldComp.velocityX = rushVelocityX;
            shieldEntity->AddComponent<PhotoCopyLifetimeComponent>(0.5f);
        }
        else
        {
            shieldComp.capturedMode = CapturedShieldMode::JumpBurst;
            shieldComp.gravityEnabled = false;
            shieldComp.contactDamage = 0;
            shieldComp.followPlayer = false;
            shieldComp.hoverDuration = 0.0f;
            shieldComp.descendSpeed = kBossJumpDescendSpeed;
            shieldComp.followOffsetX = (shieldX + shieldW * 0.5f) - playerCenterX;
            shieldComp.followOffsetY = shieldY - playerFootY;
            shieldEntity->AddComponent<PhotoCopyLifetimeComponent>(2.0f);
        }

        m_world.Spawn(std::move(shieldEntity));
        if (spawnRushShieldVisual)
        {
            const bool hasBossMotionItem = bossMotionItem != nullptr;
            const float relativeVisualX = hasBossMotionItem ? bossMotionItem->relativeX - attackItem->relativeX : 0.0f;
            const float relativeVisualY = hasBossMotionItem ? bossMotionItem->relativeY - attackItem->relativeY : 0.0f;
            const float bossVisualWidth = hasBossMotionItem ? bossMotionItem->width : kTileSize * 5.0f;
            const float bossVisualHeight = hasBossMotionItem ? bossMotionItem->height : kTileSize * 4.0625f;
            const float bossVisualX = hasBossMotionItem
                ? shieldX + relativeVisualX
                : shieldX + shieldW * 0.5f - bossVisualWidth * 0.5f;
            const float bossVisualY = hasBossMotionItem
                ? shieldY + relativeVisualY
                : shieldY + shieldH * 0.5f - bossVisualHeight * 0.5f;
            const float visualRotation = hasBossMotionItem ? bossMotionItem->rotation : 0.0f;
            const int shieldTextureId = attackItem->textureId >= 0
                ? attackItem->textureId
                : m_assets.GetTexture(rushCapture ? "boss1_shield_attack01" : "boss1_shield_attack02");
            const char* clipName = rushCapture ? "attack01" : "attack02";

            // 突進キャプチャは攻撃として盾だけを見せるため、本体のコピーは生成しません。
            auto shieldVisualEntity = std::make_unique<Entity>();
            shieldVisualEntity->AddComponent<TagComponent>(kTagPhotoBox);
            shieldVisualEntity->AddComponent<PhotoPasteOrderComponent>(attackPasteOrder);
            shieldVisualEntity->AddComponent<PhotoCopyLayerComponent>(PhotoCopyLayer::Foreground);
            shieldVisualEntity->AddComponent<PhotoCopyRoleComponent>(PhotoCopyRole::Solid);
            shieldVisualEntity->AddComponent<PhotoCopyOriginComponent>(PhotoCopyOrigin::Enemy);
            shieldVisualEntity->AddComponent<PhotoCopyEffectComponent>(attackItem->appliedTheme);
            shieldVisualEntity->AddComponent<PhotoCopyLifetimeComponent>(capturedBossVisualLifetime);
            auto& shieldVisualTransform = shieldVisualEntity->AddComponent<TransformComponent>(
                bossVisualX,
                bossVisualY,
                bossVisualWidth,
                bossVisualHeight);
            shieldVisualTransform.rotation = visualRotation;
            auto& shieldVisualMotion = shieldVisualEntity->AddComponent<PhotoMotionComponent>(visualVelocityX, visualVelocityY);
            shieldVisualMotion.BindTransform(&shieldVisualTransform);
            shieldVisualEntity->AddComponent<TintComponent>(
                attackItem->tintR,
                attackItem->tintG,
                attackItem->tintB,
                attackItem->tintA);
            shieldVisualEntity->AddComponent<SpriteRenderComponent>(
                shieldTextureId >= 0 ? shieldTextureId : m_tileTexture);
            ConfigureBossShieldSpriteAnimation(*shieldVisualEntity);
            if (auto* animation = shieldVisualEntity->GetComponent<SpriteSheetAnimationComponent>())
            {
                animation->Play(clipName, true);
                animation->SetCurrentLocalFrameIndex(capturedBossVisualStartFrame);
            }
            if (auto* sprite = shieldVisualEntity->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetFlipX(facingRight);
            }
            m_world.Spawn(std::move(shieldVisualEntity));
        }
        finishAttackUse();
        return;
    }

    if (attackItem->spawnArchetype != CapturedSpawnArchetype::WalkerMelee)
    {
        return;
    }

    constexpr float kAttackWidth = 48.0f;
    constexpr float kAttackHeight = 60.0f;
    constexpr float kAttackLifetime = 0.4f;
    constexpr float kFaceSideOffset = 8.0f;
    const float playerWidth = playerTransform->width * playerTransform->scale;
    const float playerHeight = playerTransform->height * playerTransform->scale;
    const float attackX = m_player.facingRight
        ? playerTransform->x + playerWidth + kFaceSideOffset
        : playerTransform->x - kAttackWidth - kFaceSideOffset;
    const float attackY = playerTransform->y + playerHeight * 0.22f;

    auto attackEntity = std::make_unique<Entity>();
    attackEntity->AddComponent<TagComponent>("WalkerMeleeAttack");
    auto& attackTransform = attackEntity->AddComponent<TransformComponent>(attackX, attackY, kAttackWidth, kAttackHeight);
    attackTransform.rotation = m_player.facingRight ? 0.0f : 3.14159265f;
    attackEntity->AddComponent<TintComponent>(1.0f, 0.55f, 0.15f, 0.72f);
    attackEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
    attackEntity->AddComponent<PhotoCopyLifetimeComponent>(kAttackLifetime);
    m_world.Spawn(std::move(attackEntity));

    finishAttackUse();
}

void GameScene::StartCameraFlashPulse(float durationSeconds)
{
    if (durationSeconds <= 0.0f)
    {
        return;
    }

    m_ui.cameraFlash.pulseDuration = (std::max)(m_ui.cameraFlash.pulseDuration, durationSeconds);
    m_ui.cameraFlash.pulseRemaining = (std::max)(m_ui.cameraFlash.pulseRemaining, durationSeconds);
}

void GameScene::StoreCapturedPhoto()
{
	// キャプチャした写真にセピア地面アイテムが含まれているか
    bool hasSepiaGroundItem = false;
    for (const auto& item : m_photo.capture.items)
    {
        if (item.spawnArchetype == CapturedSpawnArchetype::SepiaGround ||
            item.sepiaRestoredMarkerObject ||
            (item.spawnArchetype == CapturedSpawnArchetype::FallingRock &&
                item.appliedTheme == PhotoFilterTheme::Sepia))
        {
            hasSepiaGroundItem = true;
            break;
        }
    }

    const bool sepiaDryRun = 
		!hasSepiaGroundItem &&
        !m_photo.capture.containsEnemyAttackPaste &&
        (m_debug.sepiaFilmFilterDryRunEnabled ||
         m_photo.capture.selectedTheme == PhotoFilterTheme::Sepia ||
         m_photo.capture.capturedTheme == PhotoFilterTheme::Sepia);
    if (sepiaDryRun)
    {
        const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
        m_photo.capture = PhotoCaptureState{};
        m_photo.capture.selectedTheme = selectedTheme;
        m_photo.pendingStore = PendingPhotoStoreState{};
        return;
    }

    CommitPendingCapturedPhoto();

    if (m_photo.capture.containsEnemyAttackPaste)
    {
        const int existingCount = GetAttackCaptureCount(m_photo.attackCapture);
        const int captureCount = GetAttackCaptureCount(m_photo.capture);
        const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
        // Enemy attacks use a dedicated one-slot inventory and never enter the photo tray.
        m_photo.attackCapture = m_photo.capture;
        m_photo.attackCapture.attackCaptureCount = existingCount + captureCount;
        m_photo.capture = PhotoCaptureState{}; 
        m_photo.capture.selectedTheme = selectedTheme;
        m_photo.pendingStore = PendingPhotoStoreState{};
        return;
    }

    int slotToStore = -1;
    const int usablePhotoSlots = std::clamp(
        GameSession_Get().photoStorageSlots,
        1,
        static_cast<int>(m_photo.savedCaptures.size()));
    for (int index = 0; index < usablePhotoSlots; ++index)
    {
        if (!m_photo.savedCaptures[index].hasPhoto)
        {
            slotToStore = index;
            break;
        }
    }

    if (slotToStore < 0)
    {
        slotToStore = std::clamp(m_photo.nextCaptureSlot, 0, usablePhotoSlots - 1);
    }

    m_photo.pendingStore.active = true;
    m_photo.pendingStore.commitOnComplete = false;
    m_photo.pendingStore.slotIndex = slotToStore;
    m_photo.pendingStore.capture = m_photo.capture;
    m_photo.savedCaptures[slotToStore] = m_photo.capture;
    m_photo.selectedCaptureSlot = slotToStore;
    m_photo.nextCaptureSlot = (slotToStore + 1) % usablePhotoSlots;
}

void GameScene::CommitPendingCapturedPhoto()
{
    if (!m_photo.pendingStore.active)
    {
        return;
    }
    if (!m_photo.pendingStore.commitOnComplete)
    {
        m_photo.pendingStore = PendingPhotoStoreState{};
        return;
    }
    // 確認
    bool hasSepiaGroundItem = false;
    for (const auto& item : m_photo.pendingStore.capture.items)
    {
        if (item.spawnArchetype == CapturedSpawnArchetype::SepiaGround ||
            item.sepiaRestoredMarkerObject ||
            (item.spawnArchetype == CapturedSpawnArchetype::FallingRock &&
                item.appliedTheme == PhotoFilterTheme::Sepia))
        {
            hasSepiaGroundItem = true;
            break;
        }
    }
    const bool sepiaDryRun =
		!hasSepiaGroundItem &&
        !m_photo.pendingStore.capture.containsEnemyAttackPaste &&
        (m_debug.sepiaFilmFilterDryRunEnabled ||
         m_photo.pendingStore.capture.selectedTheme == PhotoFilterTheme::Sepia ||
         m_photo.pendingStore.capture.capturedTheme == PhotoFilterTheme::Sepia);
    if (sepiaDryRun)
    {
        // Discard queued sepia captures before they can enter the saved slots.
        m_photo.pendingStore = PendingPhotoStoreState{};
        return;
    }

    m_photo.savedCaptures[m_photo.pendingStore.slotIndex] = m_photo.pendingStore.capture;
    m_photo.selectedCaptureSlot = m_photo.pendingStore.slotIndex;
    m_photo.pendingStore = PendingPhotoStoreState{};
}

void GameScene::SetSelectedPhotoSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_photo.savedCaptures.size()))
    {
        return;
    }

    const PhotoCaptureState& storedCapture = m_photo.savedCaptures[slotIndex];
    if (!storedCapture.hasPhoto)
    {
        return;
    }

    const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
    m_photo.capture = storedCapture;
    m_photo.capture.selectedTheme = selectedTheme;
    m_photo.selectedCaptureSlot = slotIndex;
}

void GameScene::ConsumeSelectedPhotoSlot()
{
    CommitPendingCapturedPhoto();

    if (m_photo.capture.containsEnemyAttackPaste)
    {
        return;
    }

    if (m_photo.selectedCaptureSlot < 0 || m_photo.selectedCaptureSlot >= static_cast<int>(m_photo.savedCaptures.size()))
    {
        return;
    }

    PhotoCaptureState& selectedCapture = m_photo.savedCaptures[m_photo.selectedCaptureSlot];
    if (!selectedCapture.hasPhoto)
    {
        return;
    }

    const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
    selectedCapture = PhotoCaptureState{};

    for (int offset = 1; offset <= static_cast<int>(m_photo.savedCaptures.size()); ++offset)
    {
        const int slotIndex = (m_photo.selectedCaptureSlot + offset) % static_cast<int>(m_photo.savedCaptures.size());
        if (!m_photo.savedCaptures[slotIndex].hasPhoto)
        {
            continue;
        }

        SetSelectedPhotoSlot(slotIndex);
        m_photo.capture.selectedTheme = selectedTheme;
        return;
    }

    m_photo.capture = PhotoCaptureState{};
    m_photo.capture.selectedTheme = selectedTheme;
    m_photo.selectedCaptureSlot = 0;
    m_photo.placement.active = false;
    m_photo.placement.valid = false;
}

void GameScene::UpdatePhotoTraySelection()
{
    game_scene_photo_tray_system::UpdateSelection(
        m_photo,
        m_ui.photoTrayReveal,
        [this](int slotIndex)
        {
            SetSelectedPhotoSlot(slotIndex);
        });
}

void GameScene::HandleAttackHits()
{
    Entity* player = FindEntityByTag(kTagPlayer);
    if (!player)
    {
        return;
    }

    const auto* playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    const float playerLeft = playerTransform->x;
    const float playerRight = playerTransform->x + playerTransform->width * playerTransform->scale;
    const float playerTop = playerTransform->y;
    const float playerBottom = playerTransform->y + playerTransform->height * playerTransform->scale;

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::SepiaRubble))
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (enemy && enemy->IsEnabled() && enemy->attackRectActive)
        {
            const float attackLeft = enemy->attackRectX;
            const float attackRight = enemy->attackRectX + enemy->attackRectWidth;
            const float attackTop = enemy->attackRectY;
            const float attackBottom = enemy->attackRectY + enemy->attackRectHeight;

            const bool intersects =
                playerLeft < attackRight &&
                playerRight > attackLeft &&
                playerTop < attackBottom &&
                playerBottom > attackTop;

            if (intersects)
            {
                HandlePlayerDamage(*player, entity, "GameScene player damaged by melee attack");
            }
        }
    }
}

void GameScene::UpdateGoalVisual(float deltaTime)
{
    m_flow.goalPulse += deltaTime;
    if (Entity* goal = FindEntityByTag(kTagGoal))
    {
        if (auto* tint = goal->GetComponent<TintComponent>())
        {
            const float pulse = 0.65f + 0.35f * std::sin(m_flow.goalPulse * 3.2f);
            if (m_flow.goalUnlocked)
            {
                tint->r = 0.32f + pulse * 0.18f;
                tint->g = 0.92f;
                tint->b = 0.42f + pulse * 0.20f;
            }
            else
            {
                tint->r = 0.62f + pulse * 0.10f;
                tint->g = 0.30f;
                tint->b = 0.24f;
            }
            tint->a = 1.0f;
        }
    }
}

void GameScene::RefreshPhotoGroupState()
{
    m_photo.groups.hasSpawnedCopy = FindEntityByTag(kTagPhotoBox) != nullptr;
    int maxGroupId = 0;
    std::vector<int> groups;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity || !HasTag(*entity, kTagPhotoBox))
        {
            continue;
        }

        if (const auto* group = entity->GetComponent<PhotoCopyGroupComponent>())
        {
            maxGroupId = std::max(maxGroupId, group->groupId);
            if (std::find(groups.begin(), groups.end(), group->groupId) == groups.end())
            {
                groups.push_back(group->groupId);
            }
        }
    }

    m_photo.groups.activeGroupCount = static_cast<int>(groups.size());
    m_photo.groups.nextGroupId = std::max(m_photo.groups.nextGroupId, maxGroupId + 1);
}

void GameScene::UpdateSepiaRestoredLifetimes(float deltaTime)
{
    if (deltaTime < 0.0f)
    {
        return;
    }

    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }
        auto* sepiaGroup = entity->GetComponent<SepiaRubbleGroupComponent>();
        if (!sepiaGroup || !sepiaGroup->isRestored)
        {
            continue;
        }

        if (sepiaGroup->restoredLifetime > 0.0f)
        {
            sepiaGroup->restoredLifetime = std::max(0.0f, sepiaGroup->restoredLifetime - deltaTime);
        }

        if (sepiaGroup->restoredLifetime <= 0.0f)
        {
            // Revert tiles in the group's footprint back to empty and clear restored flag.
            if (!sepiaGroup->cellColumns.empty() &&
                sepiaGroup->cellColumns.size() == sepiaGroup->cellRows.size())
            {
                // セル単位で復元したなら、セル単位で戻す（空洞を潰さない）
                for (size_t ci = 0; ci < sepiaGroup->cellColumns.size(); ++ci)
                {
                    const int col = sepiaGroup->cellColumns[ci];
                    const int row = sepiaGroup->cellRows[ci];
                    m_tileMap.SetTile(col, row, 0);
                    m_tileMap.SetMarker(col, row, '<', 0);
                }
            }
            else
            {
                // cell 配列が無い場合は従来どおり min/max 矩形で戻す
                for (int col = sepiaGroup->minColumn; col <= sepiaGroup->maxColumn; ++col)
                {
                    for (int row = sepiaGroup->minRow; row <= sepiaGroup->maxRow; ++row)
                    {
                        m_tileMap.SetTile(col, row, 0);
                        m_tileMap.SetMarker(col, row, '<', 0);
                    }
                }
            }
            if (auto* tint = entity->GetComponent<TintComponent>())
            {
                tint->a = 1.0f;
            }

            sepiaGroup->isRestored = false;
            sepiaGroup->restoredLifetime = 0.0f;
        }
    }
}


