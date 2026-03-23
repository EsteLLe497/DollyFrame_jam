#include "game_scene_internal.h"
#include "game_scene_combat_system.h"
#include "game_scene_player_system.h"
#include "game_scene_player_movement_system.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_photo_tray_system.h"
#include "game_scene_world_interaction_system.h"
#include "photo_system.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kCaptureFinderScaleMin = 1.0f;
    constexpr float kCaptureFinderScaleMax = 2.0f;
    constexpr float kCaptureFinderScaleStep = 0.1f;
    constexpr float kBarrelDebrisLifetime = 0.55f;
    constexpr float kPitRestartFadeDuration = 0.45f;
    constexpr float kTuningPanelX = 24.0f;
    constexpr float kTuningPanelY = 24.0f;
    constexpr float kTuningPanelWidth = 460.0f;
    constexpr float kTuningPanelHeight = 620.0f;
    constexpr float kTuningRowStartY = 124.0f;
    constexpr float kTuningRowHeight = 22.0f;
    constexpr float kTuningSectionGap = 14.0f;
    constexpr float kTuningSectionHeaderHeight = 24.0f;
    constexpr float kTuningMinusButtonX = 314.0f;
    constexpr float kTuningPlusButtonX = 390.0f;
    constexpr float kTuningButtonWidth = 52.0f;
    constexpr float kTuningButtonHeight = 18.0f;

    struct TuningRowLayout
    {
        float y;
        bool isSectionHeader;
    };

    TuningRowLayout GetTuningRowLayout(int index)
    {
        float y = kTuningPanelY + kTuningRowStartY;
        if (index >= 2)
        {
            y += kTuningSectionHeaderHeight + kTuningSectionGap;
        }
        if (index >= 12)
        {
            y += kTuningSectionHeaderHeight + kTuningSectionGap;
        }
        y += static_cast<float>(index) * kTuningRowHeight;
        return { y, false };
    }

    bool IsPointInside(float pointX, float pointY, float x, float y, float width, float height)
    {
        return pointX >= x && pointX <= x + width && pointY >= y && pointY <= y + height;
    }
}

void GameScene::SpawnBarrelBreakEffect(float x, float y, float width, float height)
{
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    constexpr float velocities[][2] =
    {
        { -180.0f, -260.0f },
        { -120.0f, -210.0f },
        { -60.0f, -180.0f },
        {  60.0f, -190.0f },
        {  120.0f, -220.0f },
        {  180.0f, -250.0f },
        { -90.0f, -140.0f },
        {  90.0f, -150.0f },
    };
    constexpr float sizes[] = { 12.0f, 10.0f, 9.0f, 11.0f, 8.0f, 10.0f, 7.0f, 9.0f };
    constexpr float rotations[] = { -0.4f, 0.2f, -0.8f, 0.6f, -0.3f, 0.5f, -0.9f, 0.9f };
    constexpr float rotationSpeeds[] = { -4.0f, 3.2f, -5.4f, 4.6f, -3.8f, 5.0f, -6.2f, 6.0f };
    constexpr float colors[][3] =
    {
        { 0.52f, 0.31f, 0.16f },
        { 0.44f, 0.25f, 0.12f },
        { 0.60f, 0.38f, 0.20f },
        { 0.34f, 0.18f, 0.08f },
    };

    for (int index = 0; index < 8; ++index)
    {
        BarrelDebrisParticle particle;
        particle.x = centerX - sizes[index] * 0.5f;
        particle.y = centerY - sizes[index] * 0.5f;
        particle.velocityX = velocities[index][0];
        particle.velocityY = velocities[index][1];
        particle.size = sizes[index];
        particle.rotation = rotations[index];
        particle.rotationSpeed = rotationSpeeds[index];
        particle.life = kBarrelDebrisLifetime;
        particle.maxLife = kBarrelDebrisLifetime;
        particle.r = colors[index % 4][0];
        particle.g = colors[index % 4][1];
        particle.b = colors[index % 4][2];
        m_effects.barrelDebris.push_back(particle);
    }
}

void GameScene::UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame)
{
    game_scene_player_visual_system::UpdatePresentation(
        m_player,
        player,
        deltaTime,
        moveAxis,
        wasGrounded,
        isDodging,
        landedThisFrame);
}

void GameScene::UpdateTuningPanel()
{
    if (!m_debug.showTuningPanel)
    {
        return;
    }

    auto entries = BuildGameSceneTuningEntries();
    const int kEntryCount = static_cast<int>(entries.size());

    if (Input_IsActionPressed(InputAction::MoveUp))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + kEntryCount - 1) % kEntryCount;
    }
    if (Input_IsActionPressed(InputAction::MoveDown))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + 1) % kEntryCount;
    }

    float delta = 0.0f;
    if (Input_IsActionDown(InputAction::MoveLeft))
    {
        delta -= entries[m_debug.tuningSelection].step;
    }
    if (Input_IsActionDown(InputAction::MoveRight))
    {
        delta += entries[m_debug.tuningSelection].step;
    }

    if (delta != 0.0f)
    {
        *entries[m_debug.tuningSelection].value = std::clamp(
            *entries[m_debug.tuningSelection].value + delta,
            entries[m_debug.tuningSelection].minValue,
            entries[m_debug.tuningSelection].maxValue);
        WriteTuningJsonFile();
    }

    if (!Input_IsMouseLeftPressed())
    {
        return;
    }

    const float mouseX = static_cast<float>(Input_GetMouseX());
    const float mouseY = static_cast<float>(Input_GetMouseY());
    if (!IsPointInside(mouseX, mouseY, kTuningPanelX, kTuningPanelY, kTuningPanelWidth, kTuningPanelHeight))
    {
        return;
    }

    for (int index = 0; index < kEntryCount; ++index)
    {
        const auto layout = GetTuningRowLayout(index);
        const float rowY = layout.y;
        if (IsPointInside(mouseX, mouseY, kTuningPanelX + kTuningMinusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight))
        {
            m_debug.tuningSelection = index;
            *entries[index].value = std::clamp(
                *entries[index].value - entries[index].step,
                entries[index].minValue,
                entries[index].maxValue);
            WriteTuningJsonFile();
            return;
        }
        if (IsPointInside(mouseX, mouseY, kTuningPanelX + kTuningPlusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight))
        {
            m_debug.tuningSelection = index;
            *entries[index].value = std::clamp(
                *entries[index].value + entries[index].step,
                entries[index].minValue,
                entries[index].maxValue);
            WriteTuningJsonFile();
            return;
        }
    }
}

void GameScene::UpdatePlayer(float deltaTime)
{
    if (m_flow.cameraMode)
    {
        const int wheelDelta = GetMouseWheelRotVol();
        const bool dpadUpDown = Input_IsDpadUpDown();
        const bool dpadDownDown = Input_IsDpadDownDown();
        if (wheelDelta != 0 || dpadUpDown || dpadDownDown)
        {
            float scaleStep = 0.0f;
            if (wheelDelta > 0 || dpadUpDown)
            {
                scaleStep += kCaptureFinderScaleStep;
            }
            if (wheelDelta < 0 || dpadDownDown)
            {
                scaleStep -= kCaptureFinderScaleStep;
            }
            m_flow.captureFinderScale = std::clamp(
                m_flow.captureFinderScale + scaleStep,
                kCaptureFinderScaleMin,
                kCaptureFinderScaleMax);
        }
    }

    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    auto* transform = player->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    const bool blockPlayerInput = m_flow.cameraMode || m_photo.placement.active;
    const auto controls = game_scene_player_system::SampleControls(blockPlayerInput);
    const float moveAxis = controls.moveAxis;

    game_scene_player_system::TickDodgeState(m_player, deltaTime);
    UpdatePlayerAfterimages(deltaTime);

    game_scene_player_system::UpdateFacingFromMoveAxis(m_player, moveAxis);

    if (controls.dodgePressed && game_scene_player_system::TryBeginDodge(
        m_player,
        moveAxis,
        GetPlayerDodgeDuration(),
        gPlayerDodgeCooldown))
    {
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Player dodged", 0.0f, 0.0f });
    }

    const bool isDodging = m_player.dodgeRemaining > 0.0f;
    m_player.velocityX = game_scene_player_system::GetHorizontalVelocity(
        m_player,
        moveAxis,
        gPlayerDodgeSpeed,
        gPlayerMoveSpeed);

    const float tileSize = m_tileMap.GetTileSize();
    const float playerWidth = transform->width * transform->scale;
    const float playerHeight = transform->height * transform->scale;
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float previousX = transform->x;
    const float previousY = transform->y;
    const float previousBottom = previousY + playerHeight;
    const bool wasGrounded = IsStandingOnGround(*transform);
    m_player.grounded = wasGrounded;
    if (wasGrounded)
    {
        m_player.coyoteTimeRemaining = gCoyoteTimeSeconds;
    }

    if (wasGrounded && m_player.velocityY > 0.0f)
    {
        m_player.velocityY = 0.0f;
    }

    const bool canJumpNow = !isDodging && controls.jumpPressed && m_player.coyoteTimeRemaining > 0.0f;
    if (canJumpNow)
    {
        m_player.velocityY = gPlayerJumpSpeed;
        m_player.grounded = false;
        m_player.coyoteTimeRemaining = 0.0f;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
    }

    if (m_player.grounded && !canJumpNow)
    {
        m_player.velocityY = 0.0f;
    }
    else
    {
        m_player.velocityY = std::min(gPlayerMaxFallSpeed, m_player.velocityY + gPlayerGravity * deltaTime);
    }
    const float verticalSnapDistance = std::max(gGroundSnapDistance, std::fabs(m_player.velocityY) * deltaTime + 4.0f);
    const game_scene_player_movement_system::PlayerMovementContext movementContext{
        deltaTime,
        tileSize,
        playerWidth,
        playerHeight,
        mapWidth,
        mapHeight,
        previousX,
        previousY,
        previousBottom,
        verticalSnapDistance,
    };
    const float horizontalVelocity = m_player.velocityX;

    game_scene_player_movement_system::ResolveHorizontalTileCollisions(
        *transform,
        m_player,
        movementContext,
        [this, horizontalVelocity](int column, int row)
        {
            return horizontalVelocity > 0.0f
                ? IsTileBlockingFromLeft(column, row)
                : IsTileBlockingFromRight(column, row);
        },
        [this](const TransformComponent& candidate)
        {
            return IntersectsSolidPhotoBox(candidate);
        });

    if (isDodging)
    {
        TrySpawnPlayerAfterimage(*transform);
    }

    std::vector<TransformComponent> photoBoxes;
    GetPhotoBoxBounds(photoBoxes);
    std::vector<TransformComponent> solidObjects;
    GetEntityBoundsByTag("PhotoSource", solidObjects);
    std::vector<TransformComponent> enemyBounds;
    GetEntityBoundsByTag("Enemy", enemyBounds);
    solidObjects.insert(solidObjects.end(), enemyBounds.begin(), enemyBounds.end());
    game_scene_player_movement_system::ResolveHorizontalObjectCollisions(
        *transform,
        m_player,
        movementContext,
        photoBoxes,
        [this](const TransformComponent& candidate)
        {
            return IntersectsSolidPhotoBox(candidate);
        },
        solidObjects);

    if (m_player.velocityY >= 0.0f && wasGrounded)
    {
        if (TrySnapToGround(*transform, verticalSnapDistance))
        {
            m_player.grounded = true;
        }
    }

    game_scene_player_movement_system::ResolveVerticalMotion(
        *transform,
        m_player,
        wasGrounded,
        movementContext,
        photoBoxes,
        solidObjects,
        [this](int column, int row)
        {
            return IsSolidTile(column, row);
        },
        [this](int column, int row)
        {
            return IsPlatformTile(column, row);
        },
        [this](int column, int row)
        {
            return IsSolidTile(column, row) || IsSlopeTile(column, row);
        },
        [this](TransformComponent& targetTransform, float snapDistance)
        {
            return TrySnapToGround(targetTransform, snapDistance);
        },
        [this](const TransformComponent& candidate)
        {
            return IntersectsSolidPhotoBox(candidate);
        });

    const bool landedThisFrame = !wasGrounded && m_player.grounded;
    UpdatePlayerPresentation(*player, deltaTime, moveAxis, wasGrounded, isDodging, landedThisFrame);

    game_scene_player_movement_system::UpdateCameraX(
        m_flow.cameraX,
        transform->x,
        playerWidth,
        mapWidth,
        deltaTime);
}

void GameScene::UpdateBarrels(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag("Player");
    const float mapHeight = GetMapPixelHeight();
    const float tileSize = m_tileMap.GetTileSize();
    const float activationDistance = tileSize * 10.0f;

    auto setBarrelVisible = [](Entity& barrelEntity, bool visible)
    {
        if (auto* tint = barrelEntity.GetComponent<TintComponent>())
        {
            tint->a = visible ? 1.0f : 0.0f;
        }
    };

    auto resetBarrel = [&](Entity& barrelEntity, BarrelComponent& barrel, TransformComponent& transform)
    {
        SpawnBarrelBreakEffect(transform.x, transform.y, transform.width * transform.scale, transform.height * transform.scale);
        m_eventBus.Publish({ EventType::PlaySoundRequest, &barrelEntity, nullptr, "barrel", 0.0f, 0.0f });
        if (!barrel.respawnEnabled)
        {
            barrel.destroyed = true;
        }
        barrel.active = false;
        barrel.cooldownActive = barrel.respawnEnabled && !barrel.destroyed;
        barrel.cooldownRemaining = barrel.cooldownActive ? 3.0f : 0.0f;
        barrel.velocityX = 0.0f;
        barrel.velocityY = 0.0f;
        barrel.grounded = false;
        barrel.accumulatedFallDistance = 0.0f;
        setBarrelVisible(barrelEntity, false);
    };

    auto isBarrelObjectCollision = [&](const Entity& barrelEntity, const TransformComponent& barrelBounds, Entity*& outHit) -> bool
    {
        outHit = nullptr;
        for (const auto& candidate : m_entities)
        {
            if (!candidate || candidate.get() == &barrelEntity)
            {
                continue;
            }

            if (candidate->GetComponent<BarrelComponent>())
            {
                continue;
            }

            if (HasTag(*candidate, "Player") || HasTag(*candidate, "Enemy") || HasTag(*candidate, "Bullet"))
            {
                continue;
            }

            if (HasTag(*candidate, "PhotoBox"))
            {
                const auto* layer = candidate->GetComponent<PhotoCopyLayerComponent>();
                if (layer && layer->layer != PhotoCopyLayer::Foreground)
                {
                    continue;
                }
            }

            const auto* otherTransform = candidate->GetComponent<TransformComponent>();
            if (!otherTransform)
            {
                continue;
            }

            if (IntersectsRect(barrelBounds, *otherTransform))
            {
                outHit = candidate.get();
                return true;
            }
        }

        return false;
    };

    auto intersectsDespawnTile = [&](const TransformComponent& bounds) -> bool
    {
        const float width = bounds.width * bounds.scale;
        const float height = bounds.height * bounds.scale;
        const int left = std::max(0, static_cast<int>((bounds.x + 2.0f) / tileSize));
        const int right = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((bounds.x + width - 2.0f) / tileSize));
        const int top = std::max(0, static_cast<int>((bounds.y + 2.0f) / tileSize));
        const int bottom = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bounds.y + height - 2.0f) / tileSize));
        for (int row = top; row <= bottom; ++row)
        {
            for (int column = left; column <= right; ++column)
            {
                const int tileValue = m_tileMap.GetTile(column, row);
                if (tileValue != 1 && tileValue != TileMap::kPitTileValue)
                {
                    continue;
                }

                TransformComponent tileBounds(
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize,
                    tileSize,
                    tileSize);
                if (IntersectsRect(bounds, tileBounds))
                {
                    return true;
                }
            }
        }
        return false;
    };

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* barrel = entity->GetComponent<BarrelComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!barrel || !transform)
        {
            continue;
        }

        if (barrel->destroyed)
        {
            setBarrelVisible(*entity, false);
            continue;
        }

        const float barrelWidth = transform->width * transform->scale;
        const float barrelHeight = transform->height * transform->scale;

        if (barrel->cooldownActive)
        {
            barrel->cooldownRemaining = std::max(0.0f, barrel->cooldownRemaining - deltaTime);
            if (barrel->cooldownRemaining <= 0.0f)
            {
                barrel->cooldownActive = false;
                transform->x = barrel->spawnX;
                transform->y = barrel->spawnY;
                setBarrelVisible(*entity, true);
            }
        }

        if (!barrel->active)
        {
            barrel->velocityX = 0.0f;
            barrel->velocityY = 0.0f;
            barrel->grounded = false;
            barrel->accumulatedFallDistance = 0.0f;

            if (!barrel->cooldownActive)
            {
                transform->x = barrel->spawnX;
                transform->y = barrel->spawnY;
            }

            if (!barrel->cooldownActive && player)
            {
                const auto* playerTransform = player->GetComponent<TransformComponent>();
                if (playerTransform)
                {
                    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                    const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
                    const float barrelCenterX = transform->x + barrelWidth * 0.5f;
                    const float barrelCenterY = transform->y + barrelHeight * 0.5f;
                    const float dx = playerCenterX - barrelCenterX;
                    const float dy = playerCenterY - barrelCenterY;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance <= activationDistance)
                    {
                        barrel->active = true;
                        setBarrelVisible(*entity, true);
                    }
                }
            }
            else if (!barrel->respawnEnabled)
            {
                barrel->active = true;
                setBarrelVisible(*entity, true);
            }

            continue;
        }

        barrel->velocityY = std::min(barrel->maxFallSpeed, barrel->velocityY + barrel->gravity * deltaTime);
        transform->y += barrel->velocityY * deltaTime;
        const bool canCollideAfterDrop = transform->y >= barrel->spawnY + std::max(8.0f, tileSize * 0.25f);

        if (transform->y + barrelHeight >= mapHeight)
        {
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        if (canCollideAfterDrop && intersectsDespawnTile(*transform))
        {
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        if (player && IntersectsEntity(*entity, *player))
        {
            HandlePlayerDamage(*player, entity.get(), "GameScene player damaged by barrel");
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        bool consumed = false;
        for (const auto& enemyEntity : m_entities)
        {
            if (!enemyEntity || enemyEntity.get() == entity.get())
            {
                continue;
            }

            auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
            if (!enemy || !enemy->IsEnabled() || !IntersectsEntity(*entity, *enemyEntity))
            {
                continue;
            }

            HandleEnemyDamage(*enemyEntity, entity.get(), barrel->contactDamage, "Barrel hit enemy");
            resetBarrel(*entity, *barrel, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

        for (const auto& gimmickEntity : m_entities)
        {
            if (!gimmickEntity || gimmickEntity.get() == entity.get())
            {
                continue;
            }

            auto* gimmick = gimmickEntity->GetComponent<GimmickComponent>();
            if (!gimmick || !gimmick->IsEnabled() || !IntersectsEntity(*entity, *gimmickEntity))
            {
                continue;
            }

            if (gimmick->GetType() == GimmickType::Switch)
            {
                m_flow.goalUnlockedBySwitch = true;
                gimmick->Consume();
            }

            resetBarrel(*entity, *barrel, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

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

void GameScene::StoreCapturedPhoto()
{
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

    m_photo.savedCaptures[slotToStore] = m_photo.capture;
    m_photo.selectedCaptureSlot = slotToStore;
    m_photo.nextCaptureSlot = (slotToStore + 1) % static_cast<int>(m_photo.savedCaptures.size());
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

void GameScene::UpdateEnemies()
{
    Entity* player = FindEntityByTag("Player");
    const TransformComponent* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    game_scene_combat_system::UpdateEnemies(
        m_entities,
        m_tileTexture,
        m_flow,
        m_photo,
        playerTransform,
        // 3/21?C???F?n??X?i?b?v?R?[???o?b?N??bool??p???X(?c?V??r)
        [this](TransformComponent& transform) -> bool
        {
            return SnapEnemyToGround(transform);
        });
}

void GameScene::UpdateBullets()
{
    Entity* player = FindEntityByTag("Player");
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
        // 3/21????F?^?C??????????R?[???o?b?N(?c?V??r)
        [this](float x, float y) -> bool
        {
            const float tileSize = m_tileMap.GetTileSize();
            const int column = static_cast<int>(x / tileSize);
            const int row = static_cast<int>(y / tileSize);
            return IsSolidTile(column, row);
        });
}
void GameScene::HandleAttackHits()
{
    return;
}

void GameScene::UpdateGoalVisual(float deltaTime)
{
    m_flow.goalPulse += deltaTime;
    if (Entity* goal = FindEntityByTag("Goal"))
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

        m_flow.playerTouchingHazard = true;
        HandlePlayerDamage(
            player,
            entity.get(),
            "GameScene player damaged by enemy",
            enemy->GetContactDamage());
    }
}

void GameScene::HandleWorldInteractions()
{
    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    m_flow.playerTouchingHazard = false;
    m_flow.playerTouchingTarget = false;

    if (const auto* playerTransform = player->GetComponent<TransformComponent>())
    {
        if (IntersectsPitTile(*playerTransform))
        {
            StartPitRestart(player, "GameScene player fell into pit tile");
            return;
        }
    }

    HandleEnemyPlayerCollisions(*player);

    game_scene_world_interaction_system::HandleTileInteractions(
        m_flow,
        *player,
        [this](const TransformComponent& transform)
        {
            return IntersectsHazardTile(transform);
        },
        [this](const TransformComponent& transform)
        {
            return IntersectsGoalTile(transform);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        },
        [this](GameEndReason reason)
        {
            QueueResult(reason);
        },
        m_eventBus);

    std::vector<Entity*> consumedGimmicks;
    game_scene_world_interaction_system::HandleEntityInteractions(
        m_entities,
        m_flow,
        *player,
        consumedGimmicks,
        [this](const Entity& a, const Entity& b)
        {
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        },
        [this](GameEndReason reason)
        {
            QueueResult(reason);
        },
        m_eventBus);

    std::vector<Entity*> consumedPickups;
    std::vector<Entity*> defeatedEnemies;
    game_scene_world_interaction_system::HandlePhotoBoxInteractions(
        m_entities,
        m_flow,
        *player,
        consumedPickups,
        defeatedEnemies,
        [this](const Entity& a, const Entity& b)
        {
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        },
        [this](GameEndReason reason)
        {
            QueueResult(reason);
        },
        m_eventBus);

    std::vector<Entity*> entitiesToRemove = consumedGimmicks;
    entitiesToRemove.insert(entitiesToRemove.end(), consumedPickups.begin(), consumedPickups.end());
    game_scene_world_interaction_system::RemoveEntitiesByPointerList(m_entities, entitiesToRemove);

    if (!defeatedEnemies.empty())
    {
        m_eventBus.Publish({ EventType::LogMessage, player, defeatedEnemies.front(), "Invert photo neutralized an enemy", 0.0f, 0.0f });
    }
}

void GameScene::HandleWorldTileInteractions(Entity& player)
{
    auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    if (IntersectsHazardTile(*playerTransform))
    {
        m_flow.playerTouchingHazard = true;
        HandlePlayerDamage(player, nullptr, "GameScene player damaged by hazard tile");
    }

    if (IntersectsPitTile(*playerTransform))
    {
        StartPitRestart(&player, "GameScene player fell into pit tile");
        return;
    }

    if (m_flow.goalUnlocked && IntersectsGoalTile(*playerTransform))
    {
        m_flow.playerTouchingTarget = true;
        if (!m_flow.resultQueued)
        {
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "contact_tone", 0.0f, 0.0f });
            QueueResult(GameEndReason::GoalReached);
        }
    }
}

void GameScene::HandleWorldEntityInteractions(Entity& player, std::vector<Entity*>& consumedGimmicks)
{
    for (const auto& entity : m_entities)
    {
        if (!entity || entity.get() == &player || !IntersectsEntity(player, *entity))
        {
            continue;
        }

        if (const auto* enemy = entity->GetComponent<EnemyComponent>())
        {
            if (enemy->IsEnabled())
            {
                m_flow.playerTouchingHazard = true;
                HandlePlayerDamage(player, entity.get(), "GameScene player damaged by enemy");
            }
        }

        auto* gimmick = entity->GetComponent<GimmickComponent>();
        if (!gimmick || !gimmick->IsEnabled())
        {
            continue;
        }

        switch (gimmick->GetType())
        {
        case GimmickType::Hazard:
            m_flow.playerTouchingHazard = true;
            HandlePlayerDamage(player, entity.get(), "GameScene player damaged by gimmick hazard");
            break;
        case GimmickType::Goal:
            if (m_flow.goalUnlocked && !m_flow.resultQueued)
            {
                m_flow.playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case GimmickType::Pickup:
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up gimmick item", 0.0f, 0.0f });
            gimmick->Consume();
            if (gimmick->IsConsumed())
            {
                consumedGimmicks.push_back(entity.get());
            }
            break;
        case GimmickType::PhotoSource:
        case GimmickType::Gate:
        case GimmickType::Switch:
        default:
            break;
        }
    }
}

void GameScene::HandlePhotoBoxInteractions(Entity& player, std::vector<Entity*>& consumedPickups, std::vector<Entity*>& defeatedEnemies)
{
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        const auto* photoRole = entity->GetComponent<PhotoCopyRoleComponent>();
        const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>();
        if (!photoRole || !IntersectsEntity(player, *entity))
        {
            continue;
        }
        if (photoLayer && photoLayer->layer != PhotoCopyLayer::Foreground)
        {
            continue;
        }

        switch (photoRole->role)
        {
        case PhotoCopyRole::Hazard:
            m_flow.playerTouchingHazard = true;
            HandlePlayerDamage(player, entity.get(), "GameScene player damaged by copied hazard");
            break;
        case PhotoCopyRole::Ally:
            for (const auto& enemyEntity : m_entities)
            {
                if (!enemyEntity || enemyEntity.get() == entity.get())
                {
                    continue;
                }

                auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
                if (!enemy || !enemy->IsEnabled() || !IntersectsEntity(*entity, *enemyEntity))
                {
                    continue;
                }

                enemy->MarkDefeated();
                defeatedEnemies.push_back(enemyEntity.get());
            }
            break;
        case PhotoCopyRole::GoalRelay:
            if (m_flow.goalUnlocked && !m_flow.resultQueued)
            {
                m_flow.playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case PhotoCopyRole::Pickup:
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up copied item", 0.0f, 0.0f });
            consumedPickups.push_back(entity.get());
            break;
        case PhotoCopyRole::Solid:
        default:
            break;
        }
    }
}

void GameScene::RemoveEntitiesByPointerList(const std::vector<Entity*>& entitiesToRemove)
{
    if (entitiesToRemove.empty())
    {
        return;
    }

    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [&](const std::unique_ptr<Entity>& entity)
            {
                return entity && std::find(entitiesToRemove.begin(), entitiesToRemove.end(), entity.get()) != entitiesToRemove.end();
            }),
        m_entities.end());
}

void GameScene::RemoveDefeatedEnemies()
{
    game_scene_world_interaction_system::RemoveDefeatedEnemies(m_entities);
    RefreshPhotoGroupState();
}

void GameScene::RefreshPhotoGroupState()
{
    m_photo.groups.hasSpawnedCopy = FindEntityByTag("PhotoBox") != nullptr;
    int maxGroupId = 0;
    std::vector<int> groups;
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
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
void GameScene::HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage, int amount)
{
    if (game_scene_world_interaction_system::IsPlayerDamageBlocked(
        m_player,
        GetPlayerDodgeDuration(),
        gPlayerDodgeInvincibilitySeconds))
    {
        return;
    }

    auto* health = player.GetComponent<HealthComponent>();
    if (!health)
    {
        return;
    }

    auto* cooldown = player.GetComponent<DamageCooldownComponent>();
    if (cooldown && !cooldown->CanTakeDamage())
    {
        return;
    }

    if (cooldown)
    {
        cooldown->Trigger();
    }
    health->ApplyDamage((std::max)(1, amount));
    GameSession_SetCurrentHp(health->GetCurrentHealth());
    m_eventBus.Publish({ EventType::PlaySoundRequest, &player, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &player, sourceEntity, logMessage, 0.0f, 0.0f });
    if (health->IsDead() && !m_flow.resultQueued)
    {
        QueueResult(GameEndReason::HpZero);
    }
}

void GameScene::HandleEnemyDamage(Entity& enemy, Entity* sourceEntity, int amount, const char* logMessage)
{
    auto* enemyComponent = enemy.GetComponent<EnemyComponent>();
    if (!enemyComponent || !enemyComponent->IsEnabled())
    {
        return;
    }

    if (auto* health = enemy.GetComponent<HealthComponent>())
    {
        health->ApplyDamage(amount);
        if (health->IsDead())
        {
            enemyComponent->MarkDefeated();
        }
    }
    else
    {
        enemyComponent->MarkDefeated();
    }

    m_eventBus.Publish({ EventType::PlaySoundRequest, &enemy, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &enemy, sourceEntity, logMessage, 0.0f, 0.0f });
}

void GameScene::QueueResult(GameEndReason reason)
{
    if (m_flow.resultQueued)
    {
        return;
    }

    m_flow.resultQueued = true;
    GameSession_SetEndReason(reason);
    m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "result", 0.0f, 0.0f });
}

void GameScene::StartPitRestart(Entity* player, const char* logMessage)
{
    if (m_flow.pitRestartActive)
    {
        return;
    }

    m_flow.pitRestartActive = true;
    m_flow.pitRestartTimer = kPitRestartFadeDuration;
    m_flow.cameraMode = false;
    m_photo.placement.active = false;
    m_flow.playerTouchingHazard = true;
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, nullptr, logMessage, 0.0f, 0.0f });
}

