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
    const bool sepiaDryRun =
        m_debug.sepiaFilmFilterDryRunEnabled ||
        m_photo.capture.selectedTheme == PhotoFilterTheme::Sepia ||
        m_photo.capture.capturedTheme == PhotoFilterTheme::Sepia;
    if (sepiaDryRun)
    {
        // Sepia dry-run is preview-only, so never queue a capture for storage.
        const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
        m_photo.capture = PhotoCaptureState{};
        m_photo.capture.selectedTheme = selectedTheme;
        m_photo.pendingStore = PendingPhotoStoreState{};
        return;
    }

    CommitPendingCapturedPhoto();

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
    const bool sepiaDryRun =
        m_debug.sepiaFilmFilterDryRunEnabled ||
        m_photo.pendingStore.capture.selectedTheme == PhotoFilterTheme::Sepia ||
        m_photo.pendingStore.capture.capturedTheme == PhotoFilterTheme::Sepia;
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
