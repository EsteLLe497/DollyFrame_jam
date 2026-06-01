#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_photo_tray_system.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_world_interaction_system.h"
#include "photo_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace game_scene_detail;

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
    if (!Input_IsKeyPressed('Q') ||
        m_flow.cameraMode ||
        m_photo.placement.active ||
        m_player.pasteAnimationActive ||
        !m_photo.attackCapture.hasPhoto ||
        !m_photo.attackCapture.containsEnemyAttackPaste)
    {
        return;
    }

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
        return;
    }

    auto finishAttackUse = [&]()
    {
        m_player.captureAnimationActive = false;
        m_player.captureAnimationReleased = false;
        m_player.pasteAnimationActive = true;
        m_player.pasteAnimationEnemyAttack = true;
        m_player.pasteAnimationReleased = true;
        m_photo.attackCapture = PhotoCaptureState{};
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Used captured attack", 0.0f, 0.0f });
    };

    if (attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst ||
        attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst)
    {
        constexpr float kTileSize = 48.0f;
        constexpr float kBossRushSpeed = 520.0f;
        constexpr float kBossJumpDescendSpeed = 1200.0f;
        const bool facingRight = m_player.facingRight;
        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float playerCenterX = playerTransform->x + playerWidth * 0.5f;
        const float playerFootY = playerTransform->y + playerHeight;

        float shieldW = kTileSize;
        float shieldH = kTileSize * 3.0f;
        float shieldX = facingRight
            ? playerTransform->x + playerWidth
            : playerTransform->x - shieldW;
        float shieldY = playerTransform->y;

        if (attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst)
        {
            shieldW = kTileSize * 2.0f;
            shieldH = kTileSize * 4.0f;
            shieldX = facingRight
                ? playerTransform->x + playerWidth
                : playerTransform->x - shieldW;
            shieldY = playerFootY - shieldH;
        }
        else
        {
            shieldW = kTileSize * 3.0f;
            shieldH = kTileSize;
            const float playerFrontX = facingRight
                ? playerTransform->x + playerWidth + shieldW * 0.5f
                : playerTransform->x - shieldW * 0.5f;
            shieldX = playerFrontX - shieldW * 0.5f;
            shieldY = playerFootY - kTileSize * 6.0f - shieldH;
        }

        auto shieldEntity = std::make_unique<Entity>();
        shieldEntity->AddComponent<TagComponent>("CapturedShield");
        shieldEntity->AddComponent<TransformComponent>(shieldX, shieldY, shieldW, shieldH);
        shieldEntity->AddComponent<TintComponent>(
            attackItem->tintR,
            attackItem->tintG,
            attackItem->tintB,
            attackItem->tintA);
        shieldEntity->AddComponent<SpriteRenderComponent>(attackItem->textureId >= 0 ? attackItem->textureId : m_tileTexture);
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

        if (auto* sprite = shieldEntity->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetSourceRect(attackItem->sourceX, attackItem->sourceY, attackItem->sourceWidth, attackItem->sourceHeight);
            sprite->SetFlipX(!facingRight);
        }

        if (attackItem->spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst)
        {
            shieldComp.capturedMode = CapturedShieldMode::RushBurst;
            shieldComp.gravityEnabled = false;
            shieldComp.contactDamage = 2;
            shieldComp.velocityX = facingRight ? kBossRushSpeed : -kBossRushSpeed;
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

        m_entities.push_back(std::move(shieldEntity));
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
    m_entities.push_back(std::move(attackEntity));

    finishAttackUse();
}

void GameScene::StartCameraFlashPulse(float durationSeconds)
{
    if (durationSeconds <= 0.0f)
    {
        return;
    }

    m_flow.cameraFlash.pulseDuration = (std::max)(m_flow.cameraFlash.pulseDuration, durationSeconds);
    m_flow.cameraFlash.pulseRemaining = (std::max)(m_flow.cameraFlash.pulseRemaining, durationSeconds);
}

void GameScene::StoreCapturedPhoto()
{
	// キャプチャした写真にセピア地面アイテムが含まれているか
    bool hasSepiaGroundItem = false;
    for (const auto& item : m_photo.capture.items)
    {
        if (item.spawnArchetype == CapturedSpawnArchetype::SepiaGround ||
            item.sepiaRestoredMarkerObject)
        {
            hasSepiaGroundItem = true;
            break;
        }
    }

    const bool sepiaDryRun = 
		!hasSepiaGroundItem &&
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
        const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
        // Enemy attacks use a dedicated one-slot inventory and never enter the photo tray.
        m_photo.attackCapture = m_photo.capture;
        m_photo.capture = PhotoCaptureState{};
        m_photo.capture.selectedTheme = selectedTheme;
        m_photo.pendingStore = PendingPhotoStoreState{};
        return;
    }

    int slotToStore = -1;
    for (int index = 0; index < static_cast<int>(m_photo.savedCaptures.size()); ++index)
    {
        if (!m_photo.savedCaptures[index].hasPhoto)
        {
            slotToStore = index;
            break;
        }
    }

    if (slotToStore < 0)
    {
        slotToStore = m_photo.nextCaptureSlot;
    }

    m_photo.pendingStore.active = true;
    m_photo.pendingStore.slotIndex = slotToStore;
    m_photo.pendingStore.capture = m_photo.capture;
    m_photo.selectedCaptureSlot = slotToStore;
    m_photo.nextCaptureSlot = (slotToStore + 1) % static_cast<int>(m_photo.savedCaptures.size());
}

void GameScene::CommitPendingCapturedPhoto()
{
    if (!m_photo.pendingStore.active)
    {
        return;
    }
    // 確認
    bool hasSepiaGroundItem = false;
    for (const auto& item : m_photo.pendingStore.capture.items)
    {
        if (item.spawnArchetype == CapturedSpawnArchetype::SepiaGround)
        {
            hasSepiaGroundItem = true;
            break;
        }
    }
    const bool sepiaDryRun =
		!hasSepiaGroundItem &&
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
        m_flow.photoTrayReveal,
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

    for (const auto& entity : m_entities)
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
                HandlePlayerDamage(*player, entity.get(), "GameScene player damaged by melee attack");
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
    for (const auto& entity : m_entities)
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

    for (const auto& entity : m_entities)
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
