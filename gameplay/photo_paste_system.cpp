#include "photo_paste_system.h"

#include "game_scene_internal.h"
#include "photo_system_bridge.h"
#include "photo_filter_rules.h"
#include "DxLib.h"
#include <cmath>

using namespace game_scene_detail;

namespace
{
    constexpr int kMaxPhotoGroups = 3;
    constexpr float kArchetypePhotoFrameLifetimeSeconds = 0.45f;
    constexpr float kPlacementRotateSpeed = 2.4f;
    constexpr float kPadDeadZone = 0.18f;
    constexpr float kPadCursorMaxSpeed = 920.0f;
    constexpr float kPadCursorResponse = 14.0f;
    constexpr float kPadCursorDamping = 10.0f;
    constexpr float kPlacementInvalidFlashSeconds = 0.22f;
    constexpr float kPlacementConfirmFlashSeconds = 0.14f;
    constexpr float kValidPreviewPulseHz = 2.2f;
    constexpr float kValidPreviewOutlineMin = 1.5f;
    constexpr float kValidPreviewOutlineMax = 2.2f;
    constexpr float kValidPreviewTintAlphaMin = 0.46f;
    constexpr float kValidPreviewTintAlphaMax = 0.62f;

    float NormalizeAngleRadians(float radians)
    {
        const float twoPi = 6.2831853072f;
        if (std::isnan(radians) || std::isinf(radians))
        {
            return 0.0f;
        }

        radians = std::fmod(radians, twoPi);
        if (radians > 3.1415926536f)
        {
            radians -= twoPi;
        }
        else if (radians < -3.1415926536f)
        {
            radians += twoPi;
        }
        return radians;
    }
}

void PhotoPasteSystem::HandleSpawn(GameScene& scene)
{
    scene.m_photo.placement.valid = false;

    if (!scene.m_flow.cameraMode &&
        scene.m_photo.capture.hasPhoto &&
        (Input_IsActionPressed(InputAction::HoldPlacement) || Input_IsNorthButtonPressed()))
    {
        scene.m_photo.placement.active = !scene.m_photo.placement.active;
        if (scene.m_photo.placement.active)
        {
            scene.m_flow.cameraMode = false;
            ++scene.m_photo.placement.sessionId;
        }
        else
        {
            scene.m_photo.placement.valid = false;
            scene.m_photo.placement.blockedByUi = false;
        }
    }

    if (!scene.m_photo.capture.hasPhoto || !scene.m_photo.placement.active)
    {
        scene.m_photo.placement.blockedByUi = false;
        return;
    }

    if (Input_IsActionPressed(InputAction::Cancel))
    {
        scene.m_photo.placement.active = false;
        scene.m_photo.placement.valid = false;
        scene.m_photo.placement.blockedByUi = false;
        return;
    }

    if (Input_IsActionPressed(InputAction::FlipPlacement))
    {
        scene.m_photo.placement.flipX = !scene.m_photo.placement.flipX;
    }
    if (Input_IsActionPressed(InputAction::ToggleBridgePlacement))
    {
        scene.m_photo.placement.bridgeEnabled = !scene.m_photo.placement.bridgeEnabled;
    }
    if (Input_IsActionDown(InputAction::RotatePlacementLeft))
    {
        scene.m_photo.placement.rotation -= kPlacementRotateSpeed / 60.0f;
    }
    if (Input_IsActionDown(InputAction::RotatePlacementRight))
    {
        scene.m_photo.placement.rotation += kPlacementRotateSpeed / 60.0f;
    }

    const bool leftTriggerDown = Input_IsLeftTriggerDown();
    const bool rightTriggerDown = Input_IsRightTriggerDown();

    if (!(leftTriggerDown || rightTriggerDown))
    {
        scene.m_photo.placement.rotation += Input_GetAxis(InputAxis::Rotate) * (kPlacementRotateSpeed / 60.0f);
    }

    scene.m_photo.placement.rotation = NormalizeAngleRadians(scene.m_photo.placement.rotation);

    Entity* player = scene.FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    float spawnX = 0.0f;
    float spawnY = 0.0f;
    float spawnWidth = 0.0f;
    float spawnHeight = 0.0f;
    if (!UpdatePlacementPreview(scene, spawnX, spawnY, spawnWidth, spawnHeight))
    {
        return;
    }

    SpawnPhotoGroup(scene, *player, spawnX, spawnY, spawnWidth);
}

void PhotoPasteSystem::DrawPlacementPreview(const GameScene& scene)
{
    if (!scene.m_photo.placement.active || scene.m_photo.capture.items.empty())
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    float previewWidth = 0.0f;
    float previewHeight = 0.0f;
    std::vector<CapturedPhotoItem> previewItems = photo_system_bridge::BuildPlacementItemsBridge(
        scene.m_photo.capture,
        scene.m_photo.placement,
        scene.m_whiteTexture,
        previewWidth,
        previewHeight);
    const bool pulseEnabled = scene.m_debug.effectPlacementPulseEnabled;
    const float timeSeconds = static_cast<float>(GetNowCount()) / 1000.0f;
    const float pulse01 = pulseEnabled ? (0.5f + 0.5f * std::sin(timeSeconds * 6.2831853072f * kValidPreviewPulseHz)) : 0.5f;
    const float validOutlineThickness = pulseEnabled
        ? (kValidPreviewOutlineMin + (kValidPreviewOutlineMax - kValidPreviewOutlineMin) * pulse01)
        : 1.8f;
    const float validTintAlpha = pulseEnabled
        ? (kValidPreviewTintAlphaMin + (kValidPreviewTintAlphaMax - kValidPreviewTintAlphaMin) * pulse01)
        : 0.55f;

    for (const auto& item : previewItems)
    {
        CapturedPhotoItem previewItem = item;
        photo_system_bridge::ApplyPreviewFilterThemeBridge(previewItem);
        const float drawX = viewOriginX + ((scene.m_photo.placement.x + item.relativeX) - scene.m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + ((scene.m_photo.placement.y + item.relativeY) - scene.m_flow.cameraY) * viewScale;
        const float drawWidth = item.width * viewScale;
        const float drawHeight = item.height * viewScale;

        Shader_ResetStyle();
        if (scene.m_photo.placement.valid)
        {
            float outlineR = 0.32f;
            float outlineG = 0.92f;
            float outlineB = 1.0f;
            GetPhotoFilterThemePreviewOutlineColor(previewItem.appliedTheme, outlineR, outlineG, outlineB);
            const float themeBoost = previewItem.appliedTheme == PhotoFilterTheme::None ? 0.0f : 0.2f;
            Shader_SetOutline(outlineR, outlineG, outlineB, 1.0f, validOutlineThickness + themeBoost);
            Shader_SetTint(previewItem.tintR, previewItem.tintG, previewItem.tintB, validTintAlpha);
        }
        else
        {
            Shader_SetOutline(1.0f, 0.24f, 0.24f, 1.0f, 1.6f);
            Shader_SetTint(1.0f, 0.24f, 0.24f, 0.42f);
        }

        photo_system_bridge::DrawCapturedPhotoItemBridge(
            scene.m_tileTexture,
            item,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            scene.m_photo.placement.valid ? 0.55f : 0.42f);

        if (item.spawnArchetype == CapturedSpawnArchetype::Barrel)
        {
            Shader_ResetStyle();
            if (scene.m_photo.placement.valid)
            {
                Shader_SetOutline(0.34f, 1.0f, 0.48f, 1.0f, 1.8f);
                Shader_SetTint(0.10f, 0.30f, 0.14f, 0.12f);
            }
            else
            {
                Shader_SetOutline(1.0f, 0.24f, 0.24f, 1.0f, 1.8f);
                Shader_SetTint(0.30f, 0.10f, 0.10f, 0.12f);
            }
            SpriteDraw(scene.m_whiteTexture, drawX, drawY, drawWidth, drawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    if (scene.m_photo.placement.valid && pulseEnabled)
    {
        const float outerX = viewOriginX + (scene.m_photo.placement.x - scene.m_flow.cameraX) * viewScale;
        const float outerY = viewOriginY + (scene.m_photo.placement.y - scene.m_flow.cameraY) * viewScale;
        const float outerW = previewWidth * viewScale;
        const float outerH = previewHeight * viewScale;
        const int pad = std::max(2, static_cast<int>(std::round(2.0f + pulse01 * 3.0f)));
        const int alpha = static_cast<int>(std::round(120.0f + pulse01 * 95.0f));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawBox(
            static_cast<int>(std::round(outerX)) - pad,
            static_cast<int>(std::round(outerY)) - pad,
            static_cast<int>(std::round(outerX + outerW)) + pad,
            static_cast<int>(std::round(outerY + outerH)) + pad,
            GetColor(128, 245, 190),
            FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    DrawFormatString(
        static_cast<int>(viewOriginX + 24.0f),
        static_cast<int>(viewOriginY + 24.0f),
        GetColor(230, 240, 255),
        "Filter:%s  Flip:%s  Bridge:%s",
        GetPhotoFilterThemeLabel(scene.m_photo.capture.capturedTheme),
        scene.m_photo.placement.flipX ? "On" : "Off",
        scene.m_photo.placement.bridgeEnabled ? "On" : "Off");
    DrawFormatString(
        static_cast<int>(viewOriginX + 24.0f),
        static_cast<int>(viewOriginY + 48.0f),
        GetColor(190, 220, 255),
        "Solid in world  Groups:%d/3  Rot:%.0f  Keys:F/B/Z/X Esc:Cancel",
        scene.m_photo.groups.activeGroupCount,
        scene.m_photo.placement.rotation * 57.2957795f);

    const float centerWorldX = scene.m_photo.placement.x + previewWidth * 0.5f;
    const float centerWorldY = scene.m_photo.placement.y + previewHeight * 0.5f;
    const int centerScreenX = static_cast<int>(std::round(viewOriginX + (centerWorldX - scene.m_flow.cameraX) * viewScale));
    const int centerScreenY = static_cast<int>(std::round(viewOriginY + (centerWorldY - scene.m_flow.cameraY) * viewScale));
    const int reticleColor = scene.m_photo.placement.valid ? GetColor(90, 240, 180) : GetColor(255, 100, 100);
    DrawCircle(centerScreenX, centerScreenY, 8, reticleColor, FALSE);
    DrawLine(centerScreenX - 12, centerScreenY, centerScreenX + 12, centerScreenY, reticleColor, 1);
    DrawLine(centerScreenX, centerScreenY - 12, centerScreenX, centerScreenY + 12, reticleColor, 1);

    int statusColor = GetColor(90, 235, 150);
    const char* statusText = "Place: LMB / RT";
    if (!scene.m_photo.placement.valid)
    {
        statusColor = GetColor(255, 120, 120);
        statusText = scene.m_photo.placement.blockedByUi ? "Cannot place: cursor is over photo tray" : "Cannot place: blocked by rule";
    }
    if (scene.m_photo.placement.invalidFlashRemaining > 0.0f)
    {
        statusColor = GetColor(255, 72, 72);
    }
    else if (scene.m_photo.placement.confirmFlashRemaining > 0.0f)
    {
        statusColor = GetColor(120, 255, 180);
    }
    DrawFormatString(
        static_cast<int>(viewOriginX + 24.0f),
        static_cast<int>(viewOriginY + 72.0f),
        statusColor,
        "%s",
        statusText);

    Shader_ResetStyle();
}

bool PhotoPasteSystem::UpdatePlacementPreview(
    GameScene& scene,
    float& spawnX,
    float& spawnY,
    float& spawnWidth,
    float& spawnHeight)
{
    float placementWidth = 0.0f;
    float placementHeight = 0.0f;
    static_cast<void>(photo_system_bridge::BuildPlacementItemsBridge(
        scene.m_photo.capture,
        scene.m_photo.placement,
        scene.m_whiteTexture,
        placementWidth,
        placementHeight));
    spawnWidth = placementWidth;
    spawnHeight = placementHeight;
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();

    const float mapWidth = scene.GetMapPixelWidth();
    const float mapHeight = scene.GetMapPixelHeight();
    static float padCursorWorldX = 0.0f;
    static float padCursorWorldY = 0.0f;
    static float padCursorVelocityX = 0.0f;
    static float padCursorVelocityY = 0.0f;
    static unsigned int lastTimeMs = 0;
    static bool initialized = false;
    static int lastSessionId = -1;
    static int lastMouseX = Input_GetMouseX();
    static int lastMouseY = Input_GetMouseY();

    const float cursorStartWorldX = scene.m_flow.cameraX + gCameraViewWidth * 0.5f;
    const float cursorStartWorldY = scene.m_flow.cameraY + gCameraViewHeight * 0.5f;

    if (!initialized)
    {
        padCursorWorldX = cursorStartWorldX;
        padCursorWorldY = cursorStartWorldY;
        initialized = true;
    }

    if (lastSessionId != scene.m_photo.placement.sessionId)
    {
        padCursorWorldX = cursorStartWorldX;
        padCursorWorldY = cursorStartWorldY;
        padCursorVelocityX = 0.0f;
        padCursorVelocityY = 0.0f;
        lastSessionId = scene.m_photo.placement.sessionId;
    }

    const unsigned int nowMs = static_cast<unsigned int>(GetNowCount());
    const float dt = lastTimeMs ? (static_cast<float>(nowMs - lastTimeMs) / 1000.0f) : (1.0f / 60.0f);
    lastTimeMs = nowMs;
    scene.m_photo.placement.invalidFlashRemaining = std::max(0.0f, scene.m_photo.placement.invalidFlashRemaining - dt);
    scene.m_photo.placement.confirmFlashRemaining = std::max(0.0f, scene.m_photo.placement.confirmFlashRemaining - dt);

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const bool mouseMoved = mouseX != lastMouseX || mouseY != lastMouseY;
    lastMouseX = mouseX;
    lastMouseY = mouseY;

    if (mouseMoved)
    {
        padCursorWorldX = ((static_cast<float>(mouseX) - viewOriginX) / viewScale) + scene.m_flow.cameraX;
        padCursorWorldY = ((static_cast<float>(mouseY) - viewOriginY) / viewScale) + scene.m_flow.cameraY;
        padCursorVelocityX = 0.0f;
        padCursorVelocityY = 0.0f;
    }

    const float rightX = Input_GetRightStickX();
    const float rightY = Input_GetRightStickY();
    const float magnitude = std::sqrt(rightX * rightX + rightY * rightY);
    const bool padActive = Input_IsGamepadConnected() && magnitude > kPadDeadZone;
    if (padActive)
    {
        const float normalizedMagnitude = std::clamp((magnitude - kPadDeadZone) / (1.0f - kPadDeadZone), 0.0f, 1.0f);
        const float curvedMagnitude = normalizedMagnitude * normalizedMagnitude;
        const float scale = curvedMagnitude / magnitude;
        const float desiredVelocityX = rightX * scale * kPadCursorMaxSpeed;
        const float desiredVelocityY = rightY * scale * kPadCursorMaxSpeed;
        const float response = std::min(1.0f, dt * kPadCursorResponse);
        padCursorVelocityX += (desiredVelocityX - padCursorVelocityX) * response;
        padCursorVelocityY += (desiredVelocityY - padCursorVelocityY) * response;
    }
    else
    {
        const float damping = std::max(0.0f, 1.0f - dt * kPadCursorDamping);
        padCursorVelocityX *= damping;
        padCursorVelocityY *= damping;
    }

    padCursorWorldX += padCursorVelocityX * dt;
    padCursorWorldY += padCursorVelocityY * dt;
    padCursorWorldX = std::clamp(padCursorWorldX, 0.0f, std::max(0.0f, mapWidth));
    padCursorWorldY = std::clamp(padCursorWorldY, 0.0f, std::max(0.0f, mapHeight));

    const float cursorWorldX = padCursorWorldX;
    const float cursorWorldY = padCursorWorldY;
    spawnX = cursorWorldX - spawnWidth * 0.5f;
    spawnY = std::clamp(cursorWorldY - spawnHeight * 0.5f, 0.0f, std::max(0.0f, mapHeight - spawnHeight));

    scene.m_photo.placement.active = true;
    scene.m_photo.placement.x = spawnX;
    scene.m_photo.placement.y = spawnY;
    scene.m_photo.placement.width = spawnWidth;
    scene.m_photo.placement.height = spawnHeight;
    scene.m_photo.placement.valid = scene.IsPhotoPlacementValid(spawnX, spawnY, spawnWidth, spawnHeight);

    const float cursorScreenX = viewOriginX + (cursorWorldX - scene.m_flow.cameraX) * viewScale;
    const float cursorScreenY = viewOriginY + (cursorWorldY - scene.m_flow.cameraY) * viewScale;
    const bool confirmPressed = Input_IsActionPressed(InputAction::ConfirmPlacement);
    const bool blockedByTray = scene.IsPhotoTrayHit(cursorScreenX, cursorScreenY);
    scene.m_photo.placement.blockedByUi = blockedByTray;

    if (confirmPressed && (!scene.m_photo.placement.valid || blockedByTray))
    {
        scene.m_photo.placement.invalidFlashRemaining = kPlacementInvalidFlashSeconds;
        scene.m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "cant_paste", 0.0f, 0.0f });
        return false;
    }

    if (confirmPressed && scene.m_photo.placement.valid)
    {
        scene.m_photo.placement.confirmFlashRemaining = kPlacementConfirmFlashSeconds;
        return true;
    }

    return false;
}

void PhotoPasteSystem::SpawnPhotoGroup(
    GameScene& scene,
    Entity& player,
    float spawnX,
    float spawnY,
    float spawnWidth)
{
    static_cast<void>(spawnWidth);
    float rotatedSpawnWidth = 0.0f;
    float rotatedSpawnHeight = 0.0f;
    std::vector<CapturedPhotoItem> spawnedItems = photo_system_bridge::BuildPlacementItemsBridge(
        scene.m_photo.capture,
        scene.m_photo.placement,
        scene.m_whiteTexture,
        rotatedSpawnWidth,
        rotatedSpawnHeight);
    static_cast<void>(rotatedSpawnWidth);
    static_cast<void>(rotatedSpawnHeight);

    if (scene.m_photo.groups.activeGroupCount >= kMaxPhotoGroups)
    {
        const int groupToRemove = scene.m_photo.groups.nextGroupId - scene.m_photo.groups.activeGroupCount;
        scene.m_entities.erase(
            std::remove_if(
                scene.m_entities.begin(),
                scene.m_entities.end(),
                [&](const std::unique_ptr<Entity>& entity)
                {
                    if (!entity || !HasTag(*entity, "PhotoBox"))
                    {
                        return false;
                    }
                    const auto* group = entity->GetComponent<PhotoCopyGroupComponent>();
                    return group && group->groupId == groupToRemove;
                }),
            scene.m_entities.end());
        scene.m_photo.groups.activeGroupCount = std::max(0, scene.m_photo.groups.activeGroupCount - 1);
    }

    const int groupId = scene.m_photo.groups.nextGroupId++;
    const int pasteOrder = scene.m_photo.groups.nextPasteOrder++;
    Entity* lastSpawnedEntity = nullptr;
    int spawnedPhotoBoxCount = 0;
    for (const auto& item : spawnedItems)
    {
        if (item.spawnArchetype == CapturedSpawnArchetype::Barrel)
        {
            auto barrelEntity = std::make_unique<Entity>();
            Entity* spawnedBarrel = barrelEntity.get();
            lastSpawnedEntity = spawnedBarrel;
            spawnedBarrel->AddComponent<TagComponent>("Barrel");
            spawnedBarrel->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedBarrel->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            spawnedBarrel->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
            spawnedBarrel->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            spawnedBarrel->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
            spawnedBarrel->AddComponent<BarrelComponent>(
                gBarrelGravity,
                gBarrelMaxFallSpeed,
                gBarrelRollSpeed,
                gBarrelGroundFriction,
                gBarrelContactDamage,
                gBarrelBreakMinFallDistance,
                gBarrelBreakMinImpactSpeed);
            spawnedBarrel->AddComponent<PhotoCopyLifetimeComponent>(gPastedObjectLifetimeSeconds);
            spawnedBarrel->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
            if (auto* sprite = spawnedBarrel->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedBarrel->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            if (auto* barrel = spawnedBarrel->GetComponent<BarrelComponent>())
            {
                barrel->spawnX = spawnX + item.relativeX;
                barrel->spawnY = spawnY + item.relativeY;
                barrel->respawnEnabled = false;
                barrel->active = true;
            }
            scene.m_entities.push_back(std::move(barrelEntity));
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::Projectile)
        {
            auto bulletEntity = std::make_unique<Entity>();
            Entity* spawnedBullet = bulletEntity.get();
            lastSpawnedEntity = spawnedBullet;
            spawnedBullet->AddComponent<TagComponent>("Bullet");
            spawnedBullet->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedBullet->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            spawnedBullet->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
            spawnedBullet->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            spawnedBullet->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
            spawnedBullet->AddComponent<ProjectileComponent>(item.projectileVelocityX, item.projectileVelocityY, item.projectileDamage, ProjectileComponent::Owner::Photo);
            if (auto* sprite = spawnedBullet->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedBullet->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            scene.m_entities.push_back(std::move(bulletEntity));
            continue;
        }

        auto entity = std::make_unique<Entity>();
        lastSpawnedEntity = entity.get();
        ++spawnedPhotoBoxCount;
        lastSpawnedEntity->AddComponent<TagComponent>("PhotoBox");
        lastSpawnedEntity->AddComponent<PhotoCopyGroupComponent>(groupId);
        lastSpawnedEntity->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
        lastSpawnedEntity->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
        lastSpawnedEntity->AddComponent<PhotoCopyRoleComponent>(item.role);
        lastSpawnedEntity->AddComponent<PhotoCopyOriginComponent>(item.origin);
        if (item.vanishOnCapture)
        {
            lastSpawnedEntity->AddComponent<VanishOnCaptureComponent>(true);
        }
        if (item.origin == PhotoCopyOrigin::Tile && item.sourceTileValue > 0)
        {
            lastSpawnedEntity->AddComponent<PhotoCopyTileValueComponent>(item.sourceTileValue);
        }
        lastSpawnedEntity->AddComponent<PhotoCopyEffectComponent>(item.appliedTheme);
        const PhotoCopyLayer spawnedLayer =
            item.layer == PhotoCopyLayer::Shadow ? PhotoCopyLayer::Shadow :
            item.layer == PhotoCopyLayer::Background ? PhotoCopyLayer::Background :
            scene.m_photo.placement.layer;
        lastSpawnedEntity->AddComponent<PhotoCopyLayerComponent>(spawnedLayer);
        const float lifetimeSeconds =
            (spawnedLayer == PhotoCopyLayer::Background)
            ? kArchetypePhotoFrameLifetimeSeconds
            : gPastedObjectLifetimeSeconds;
        lastSpawnedEntity->AddComponent<PhotoCopyLifetimeComponent>(lifetimeSeconds);
        lastSpawnedEntity->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
        lastSpawnedEntity->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
        lastSpawnedEntity->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
        if (auto* sprite = lastSpawnedEntity->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
            sprite->SetFlipX(item.flipX);
        }
        if (auto* transform = lastSpawnedEntity->GetComponent<TransformComponent>())
        {
            transform->rotation = item.rotation;
        }
        if (!item.collisionOutline.empty())
        {
            std::vector<b2Vec2> normalizedOutline;
            normalizedOutline.reserve(item.collisionOutline.size());
            for (const auto& point : item.collisionOutline)
            {
                normalizedOutline.push_back({ point.x, point.y });
            }
            lastSpawnedEntity->AddComponent<ImageOutlineColliderComponent>(std::move(normalizedOutline), 0.2f);
        }
        ApplyPhotoFilterToPhotoBox(*lastSpawnedEntity, item.appliedTheme);
        scene.m_entities.push_back(std::move(entity));
    }

    if (spawnedPhotoBoxCount > 0)
    {
        scene.m_photo.groups.activeGroupCount = std::min(kMaxPhotoGroups, scene.m_photo.groups.activeGroupCount + 1);
        scene.m_photo.groups.hasSpawnedCopy = true;
    }
    scene.ConsumeSelectedPhotoSlot();
    scene.m_photo.placement.active = false;
    scene.m_photo.placement.valid = false;
    scene.m_photo.placement.blockedByUi = false;
    scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, lastSpawnedEntity, "test_tone", 0.0f, 0.0f });
    scene.m_eventBus.Publish({ EventType::LogMessage, &player, lastSpawnedEntity, "Spawned filtered reconstruction", 0.0f, 0.0f });
}
