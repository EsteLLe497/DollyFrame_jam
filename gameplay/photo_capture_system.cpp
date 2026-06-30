#include "pch.h"

#include "photo_capture_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "game_scene_internal.h"
#include "photo_filter_rules.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kUnlockedCameraFlashPulseSeconds = 0.28f;
    constexpr float kMidBoss3CapturedFistWidth = 144.0f;
    constexpr float kMidBoss3CapturedFistHeight = 96.0f;
    constexpr float kMidBoss3CapturedDrillWidth = 192.0f;
    constexpr float kMidBoss3CapturedDrillHeight = 96.0f;
    constexpr int kShieldBossRushCaptureStartFrame = 60;

    using OutlinePoint = CapturedPhotoItem::OutlinePoint;

    int GetCapturedTileTextureId(int tileValue, int baseTextureId, int tile2TextureId, int tile3TextureId)
    {
        if (tileValue == 2 && tile2TextureId >= 0)
        {
            return tile2TextureId;
        }
        if (tileValue == 3 && tile3TextureId >= 0)
        {
            return tile3TextureId;
        }
        return baseTextureId;
    }

    void ApplyCapturedTileTint(
        int tileValue,
        int baseTextureId,
        int tile2TextureId,
        int tile3TextureId,
        CapturedPhotoItem& item)
    {
        item.textureId = GetCapturedTileTextureId(tileValue, baseTextureId, tile2TextureId, tile3TextureId);
        if (tileValue == 2 || tileValue == 3)
        {
            item.tintR = 1.0f;
            item.tintG = 1.0f;
            item.tintB = 1.0f;
            item.tintA = 1.0f;
            return;
        }

        GetTileCaptureTint(tileValue, item.tintR, item.tintG, item.tintB, item.tintA);
    }

    bool IsShieldBossRushCaptureReady(const Entity& bossEntity)
    {
        const auto* boss = bossEntity.GetComponent<ShieldBossComponent>();
        if (!boss || boss->state != ShieldBossState::Rush)
        {
            return false;
        }

        const auto* animation = bossEntity.GetComponent<SpriteSheetAnimationComponent>();
        return animation &&
            animation->GetCurrentClipName() == "attack01" &&
            animation->GetCurrentLocalFrameIndex() >= kShieldBossRushCaptureStartFrame;
    }

    OutlinePoint LerpPoint(const OutlinePoint& a, const OutlinePoint& b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
        };
    }

    void ClipPolygonAgainstEdge(
        const std::vector<OutlinePoint>& input,
        std::vector<OutlinePoint>& output,
        auto&& isInside,
        auto&& intersect)
    {
        output.clear();
        if (input.empty())
        {
            return;
        }

        OutlinePoint previous = input.back();
        bool previousInside = isInside(previous);
        for (const OutlinePoint& current : input)
        {
            const bool currentInside = isInside(current);
            if (currentInside != previousInside)
            {
                output.push_back(intersect(previous, current));
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousInside = currentInside;
        }
    }

    bool BuildCapturedOutlineFromEntity(
        const Entity& entity,
        float localLeft,
        float localTop,
        float localWidth,
        float localHeight,
        std::vector<OutlinePoint>& outOutline)
    {
        outOutline.clear();

        const auto* imageCollider = entity.GetComponent<ImageOutlineColliderComponent>();
        if (!imageCollider)
        {
            return false;
        }

        const auto& normalizedOutline = imageCollider->GetNormalizedOutline();
        if (normalizedOutline.size() < 3 || localWidth <= 0.0001f || localHeight <= 0.0001f)
        {
            return false;
        }

        std::vector<OutlinePoint> clipped;
        clipped.reserve(normalizedOutline.size());
        for (const b2Vec2& point : normalizedOutline)
        {
            clipped.push_back({ point.x, point.y });
        }

        std::vector<OutlinePoint> scratch;
        auto clipVertical = [&](float edgeX, bool keepGreater)
        {
            ClipPolygonAgainstEdge(
                clipped,
                scratch,
                [=](const OutlinePoint& point)
                {
                    return keepGreater ? point.x >= edgeX : point.x <= edgeX;
                },
                [=](const OutlinePoint& a, const OutlinePoint& b)
                {
                    const float delta = b.x - a.x;
                    const float t = std::fabs(delta) <= 0.0001f ? 0.0f : (edgeX - a.x) / delta;
                    return LerpPoint(a, b, std::clamp(t, 0.0f, 1.0f));
                });
            clipped.swap(scratch);
        };
        auto clipHorizontal = [&](float edgeY, bool keepGreater)
        {
            ClipPolygonAgainstEdge(
                clipped,
                scratch,
                [=](const OutlinePoint& point)
                {
                    return keepGreater ? point.y >= edgeY : point.y <= edgeY;
                },
                [=](const OutlinePoint& a, const OutlinePoint& b)
                {
                    const float delta = b.y - a.y;
                    const float t = std::fabs(delta) <= 0.0001f ? 0.0f : (edgeY - a.y) / delta;
                    return LerpPoint(a, b, std::clamp(t, 0.0f, 1.0f));
                });
            clipped.swap(scratch);
        };

        clipVertical(localLeft, true);
        clipVertical(localLeft + localWidth, false);
        clipHorizontal(localTop, true);
        clipHorizontal(localTop + localHeight, false);

        if (clipped.size() < 3)
        {
            return false;
        }

        outOutline.reserve(clipped.size());
        for (const OutlinePoint& point : clipped)
        {
            outOutline.push_back({
                (point.x - localLeft) / localWidth,
                (point.y - localTop) / localHeight
            });
        }
        return outOutline.size() >= 3;
    }

    PhotoPlacementRuleGroup ResolvePlacementRuleGroupForCapturedEntity(
        const Entity& entity,
        bool capturedVanishObject,
        bool capturedBarrel,
        bool capturedFallingRock,
        bool capturedBattery,
        bool capturedProjectile,
        bool capturedLaserTurret,
        bool capturedWalker)
    {
        if (capturedVanishObject)
        {
            return PhotoPlacementRuleGroup::Group1;
        }

        if (capturedWalker)  
        {
            return PhotoPlacementRuleGroup::Group3;
        }

        if (capturedBarrel || capturedBattery || capturedProjectile || capturedFallingRock)
        {
            return PhotoPlacementRuleGroup::Group2;
        }

        if (capturedLaserTurret)
        {
            return PhotoPlacementRuleGroup::Group1;
        }

        // Shield captures can be pasted overlapping other objects and then resolve in gameplay.
        if (entity.GetComponent<ShieldComponent>() != nullptr ||
            HasTag(entity, "Shield") || HasTag(entity, "Boss1Shield") || HasTag(entity, "MidBoss1Shield"))
        {
            return PhotoPlacementRuleGroup::Group3;
        }

        return PhotoPlacementRuleGroup::Group1;
    }

    int ResolveSepiaTextureId(const AssetManifest& assets, bool isRestored, int imageNo)
    {
        const std::string baseKey = isRestored ? "sepia_ground" : "sepia_rubble";
        const std::string numberedKey = baseKey + "_" + std::to_string(imageNo);

        const int numbered = assets.GetTexture(numberedKey);
        if (numbered >= 0)
        {
            return numbered;
        }
        return assets.GetTexture(baseKey);
    }

    bool ApplySepiaRestoredMarkerCaptureSpec(
        const SepiaRubbleGroupComponent& sepiaGroup,
        int restoredTextureId,
        int textureId,
        CapturedPhotoItem& item)
    {
        switch (sepiaGroup.restoredMarkerType)
        {
        case 'M':
            item.spawnArchetype = CapturedSpawnArchetype::Log;
            item.textureId = textureId;
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
            item.placementRuleGroup = PhotoPlacementRuleGroup::Group1;
            item.tintR = 0.54f;
            item.tintG = 0.34f;
            item.tintB = 0.16f;
            item.tintA = 1.0f;
            item.sepiaRestoredMarkerObject = true;
            return true;
        case 'S':
            item.spawnArchetype = CapturedSpawnArchetype::FallingRock;
            item.textureId = textureId;
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
            item.placementRuleGroup = PhotoPlacementRuleGroup::Group2;
            item.tintR = 0.6f;
            item.tintG = 0.6f;
            item.tintB = 0.6f;
            item.tintA = 1.0f;
            item.sepiaRestoredMarkerObject = true;
            return true;
        case '+':
            item.spawnArchetype = CapturedSpawnArchetype::SepiaGround;
            item.textureId = restoredTextureId;
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
            item.placementRuleGroup = PhotoPlacementRuleGroup::Group1;
            item.tintR = 1.0f;
            item.tintG = 1.0f;
            item.tintB = 1.0f;
            item.tintA = 1.0f;
            item.sepiaRestoredMarkerObject = true;
            return true;
        default:
            return false;
        }
    }

    bool AppendShieldBossMotionCaptureItem(
        PhotoCaptureState& capture,
        PhotoFilterTheme selectedTheme,
        const ShieldComponent& shield,
        float frameX,
        float frameY,
        std::vector<const Entity*>& capturedBossVisuals)
    {
        const Entity* bossEntity = shield.ownerBoss;
        if (!bossEntity)
        {
            return false;
        }

        if (std::find(capturedBossVisuals.begin(), capturedBossVisuals.end(), bossEntity) != capturedBossVisuals.end())
        {
            return false;
        }

        const auto* boss = bossEntity->GetComponent<ShieldBossComponent>();
        const auto* transform = bossEntity->GetComponent<TransformComponent>();
        const auto* sprite = bossEntity->GetComponent<SpriteRenderComponent>();
        if (!boss ||
            boss->deathAnimationActive ||
            boss->deathAnimationFinished ||
            !transform ||
            !sprite)
        {
            return false;
        }
        if (boss->state == ShieldBossState::Rush && !IsShieldBossRushCaptureReady(*bossEntity))
        {
            return false;
        }

        const float drawWidth = transform->width * transform->scale * sprite->GetRenderScaleX();
        const float drawHeight = transform->height * transform->scale * sprite->GetRenderScaleY();
        if (drawWidth <= 1.0f || drawHeight <= 1.0f)
        {
            return false;
        }

        // 攻撃キャプチャのシールドだけでなく、ボス本体の現在モーションも写真に同梱する。
        CapturedPhotoItem bossItem;
        bossItem.textureId = sprite->GetTextureId();
        bossItem.role = PhotoCopyRole::Hazard;
        bossItem.layer = PhotoCopyLayer::Background;
        bossItem.origin = PhotoCopyOrigin::Enemy;
        bossItem.appliedTheme = selectedTheme;
        bossItem.relativeX = transform->x + sprite->GetRenderOffsetX() - frameX;
        bossItem.relativeY = transform->y + sprite->GetRenderOffsetY() - frameY;
        bossItem.width = drawWidth;
        bossItem.height = drawHeight;
        bossItem.sourceX = sprite->GetSourceX();
        bossItem.sourceY = sprite->GetSourceY();
        bossItem.sourceWidth = sprite->GetSourceWidth();
        bossItem.sourceHeight = sprite->GetSourceHeight();
        bossItem.rotation = transform->rotation + sprite->GetRenderRotationOffset();
        bossItem.flipX = sprite->GetFlipX();
        bossItem.tintA = 0.92f;
        bossItem.spawnArchetype = CapturedSpawnArchetype::None;
        bossItem.enemyAttackPaste = false;
        bossItem.placementRuleGroup = PhotoPlacementRuleGroup::Group3;
        bossItem.bossMotionClip =
            boss->state == ShieldBossState::Rush
            ? 1
            : 2;

        if (const auto* tint = bossEntity->GetComponent<TintComponent>())
        {
            bossItem.tintR = tint->r;
            bossItem.tintG = tint->g;
            bossItem.tintB = tint->b;
            bossItem.tintA = std::min(tint->a, bossItem.tintA);
        }

        capture.items.push_back(bossItem);
        capturedBossVisuals.push_back(bossEntity);
        return true;
    }

    void AppendEntitiesByTag(
        std::vector<Entity*>& outEntities,
        const GameScene& scene,
        EntityTag tag)
    {
        for (Entity* entity : scene.EntitiesByTag(tag))
        {
            if (entity)
            {
                outEntities.push_back(entity);
            }
        }
    }

    bool SpawnRestoredSepiaMarkerObject(
        std::vector<std::unique_ptr<Entity>>& pendingEntities,
        int whiteTexture,
        float tileSize,
        float& restoredLifetimeSeconds,
        char restoredMarkerType,
        int restoredMarkerParameter,
        int column,
        int row)
    {
        if (tileSize <= 0.0f)
        {
            return false;
        }

        switch ((std::toupper(static_cast<unsigned char>(restoredMarkerType))))
        {
        case 'M':
        {
            const float spawnX = static_cast<float>(column) * tileSize;
            const float spawnY = static_cast<float>(row) * tileSize;

            auto log = std::make_unique<Entity>();
            log->AddComponent<TagComponent>(kTagLog);
            log->AddComponent<TransformComponent>(
                spawnX,
                spawnY,
                tileSize * 4.0f,
                tileSize);
            log->AddComponent<TintComponent>(0.54f, 0.34f, 0.16f, 1.0f);
            log->AddComponent<SpriteRenderComponent>(whiteTexture);
            log->AddComponent<ImageOutlineColliderComponent>(
                std::vector<b2Vec2>{
                    { 0.0f, 0.0f },
                    { 1.0f, 0.0f },
                    { 1.0f, 1.0f },
                { 0.0f, 1.0f }},
                0.5f);
            log->AddComponent<BarrelComponent>(
                gBarrelGravity,
                gBarrelMaxFallSpeed,
                0.0f,
                0.0f,
                1,
                99999.0f,
                99999.0f);
            log->AddComponent<PhotoCopyLifetimeComponent>(gPastedObjectLifetimeSeconds);

            if (auto* barrel = log->GetComponent<BarrelComponent>())
            {
                barrel->active = true;
                barrel->respawnEnabled = false;
                barrel->respawnWhenOffscreen = false;
                barrel->spawnX = spawnX;
                barrel->spawnY = spawnY;
            }

            pendingEntities.push_back(std::move(log));
            return true;
        }
        case '+':
        {
            constexpr float kSepiaElevatorWidthTiles = 4.0f;
            constexpr float kSepiaElevatorHeightTiles = 1.0f;
            constexpr float kSepiaElevatorSpeedTilesPerSec = 2.5f;
            constexpr float kSepiaElevatorTopPauseSeconds = 1.0f;
            constexpr float ColorR = 0.42f;
            constexpr float ColorG = 0.46f;
            constexpr float ColorB = 0.52f;
            const float spawnX = static_cast<float>(column) * tileSize;
            const float spawnY = static_cast<float>(row) * tileSize;
            const int moveRangeTiles = restoredMarkerParameter > 0
                ? restoredMarkerParameter : 3;
            const float extraTime = static_cast<float>(moveRangeTiles) * 0.25f;

            auto elevatorEntity = std::make_unique<Entity>();
            elevatorEntity->AddComponent<TagComponent>(kTagSepiaElevator);
            elevatorEntity->AddComponent<TransformComponent>(
                spawnX,
                spawnY,
                tileSize * kSepiaElevatorWidthTiles,
                tileSize * kSepiaElevatorHeightTiles);
            elevatorEntity->AddComponent<TintComponent>(
                ColorR,
                ColorG,
                ColorB,
                1.0f);
            elevatorEntity->AddComponent<SpriteRenderComponent>(whiteTexture);
            elevatorEntity->AddComponent<SepiaElevatorComponent>(
                tileSize * static_cast<float>(moveRangeTiles),
                tileSize * kSepiaElevatorSpeedTilesPerSec,
                kSepiaElevatorTopPauseSeconds);
            elevatorEntity->AddComponent<PhotoCopyLifetimeComponent>(restoredLifetimeSeconds + extraTime);
            restoredLifetimeSeconds = restoredLifetimeSeconds + extraTime;
            pendingEntities.push_back(std::move(elevatorEntity));

            return true;
        }
        default:
            return false;
        }
    }
}

void PhotoCaptureSystem::HandleCapture(GameScene& scene)
{
    if (!Input_IsActionPressed(InputAction::CapturePhoto))
    {
        return;
    }

    if (scene.m_photo.placement.active)
    {
        return;
    }

    if (scene.m_player.captureAnimationActive && !scene.m_player.captureAnimationReleased)
    {
        scene.m_player.captureAnimationReleased = true;
        scene.m_player.pasteAnimationActive = false;
        scene.m_player.pasteAnimationReleased = false;
        scene.m_player.pasteAnimationEnemyAttack = false;
        scene.m_photo.placement.active = false;
        scene.m_photo.placement.valid = false;
        scene.m_player.afterimages.clear();
    }

    if (scene.IsPhotoTrayHit(static_cast<float>(Input_GetMouseX()), static_cast<float>(Input_GetMouseY())))
    {
        return;
    }

    if (scene.m_ui.captureLockoutRemaining > 0.0f)
    {
        return;
    }

    if (scene.m_ui.captureRapidTimer <= 0.0f)
    {
        scene.m_ui.captureRapidCount = 0;
    }

    ++scene.m_ui.captureRapidCount;
    scene.m_ui.captureRapidTimer = gCaptureRapidWindowSeconds;
    if (scene.m_ui.captureRapidCount > static_cast<int>(std::round(gCaptureRapidShotLimit)))
    {
        scene.m_ui.captureLockoutRemaining = gCaptureOverheatLockSeconds;
        scene.m_ui.captureRapidCount = 0;
        scene.m_ui.captureRapidTimer = 0.0f;
        return;
    }

    Entity* player = scene.FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    const auto* playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    // 撮影判定も描画時と同じビュー状態で計算し、ファインダーの見た目と一致させる。
    scene.PrepareFrameRendering();
    scene.GetCaptureFrameRect(*playerTransform, frameX, frameY, frameWidth, frameHeight);
    bool restoredSepiaBackground = false;
    scene.m_flow.cameraMode = false;
    bool hasSepiaRubbleInFrame = false;
    bool hasEnemyAttackCaptureCandidateInFrame = false;
    for (const auto& entity : scene.m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }
        const auto* t = entity->GetComponent<TransformComponent>();
        if (!t)
        {
            continue;
        }
        
        const float overlapW = std::max(0.0f,
            std::min(frameX + frameWidth, t->x + t->width * t->scale) - std::max(frameX, t->x));
        const float overlapH = std::max(0.0f,
            std::min(frameY + frameHeight, t->y + t->height * t->scale) - std::max(frameY, t->y));
        if (overlapW > 0.0f && overlapH > 0.0f)
        {
            if (entity->GetComponent<SepiaRubbleComponent>())
            {
                hasSepiaRubbleInFrame = true;
            }
            if (const auto* midBoss3Fist = entity->GetComponent<MidBoss3FistComponent>())
            {
                const bool fistAttackActive =
                    midBoss3Fist->state == MidBoss3FistState::Launching ||
                    midBoss3Fist->state == MidBoss3FistState::MeteorFalling;
                if (fistAttackActive && !midBoss3Fist->captureJammerActive)
                {
                    hasEnemyAttackCaptureCandidateInFrame = true;
                }
            }
            if (hasSepiaRubbleInFrame && hasEnemyAttackCaptureCandidateInFrame)
            {
                break;
            }
        }
    }

    const bool flashEnabled = scene.m_ui.cameraFlash.unlocked && scene.m_ui.cameraFlash.enabled;
    const bool sepiaDryRun =
        !hasSepiaRubbleInFrame &&
        !hasEnemyAttackCaptureCandidateInFrame &&
        (scene.m_debug.sepiaFilmFilterDryRunEnabled ||
         scene.m_photo.capture.selectedTheme == PhotoFilterTheme::Sepia);

    if (sepiaDryRun)
    {
        const PhotoFilterTheme selectedTheme = scene.m_photo.capture.selectedTheme;
        // Sepia dry-run is visual only; do not keep objects, tiles, attacks, or stored photo data.
        scene.m_photo.capture = PhotoCaptureState{};
        scene.m_photo.capture.selectedTheme = selectedTheme;
        scene.m_photo.pendingStore = PendingPhotoStoreState{};
        scene.m_photo.placement.active = false;
        scene.m_photo.placement.valid = false;
        scene.m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "shutter", 0.0f, 0.0f });
        scene.m_ui.shutterFlashRemaining = gShutterFlashSeconds;
        scene.m_ui.developedPhotoPreviewRemaining = 0.0f;
        if (flashEnabled)
        {
            scene.StartCameraFlashPulse(kUnlockedCameraFlashPulseSeconds);
        }
        return;
    }

    const bool defeatedGhostInFinder = scene.HandleFinderDefeatGhosts(frameX, frameY, frameWidth, frameHeight) > 0;

    scene.m_photo.capture.items.clear();
    scene.m_photo.capture.containsEnemyAttackPaste = false;
    scene.m_photo.capture.attackCaptureCount = 0;
    float capturedMaxRight = 0.0f;
    float capturedMaxBottom = 0.0f;
    CaptureEntitiesInFrame(scene, frameX, frameY, frameWidth, frameHeight, capturedMaxRight, capturedMaxBottom, restoredSepiaBackground);

    CaptureTilesInFrame(scene, frameX, frameY, frameWidth, frameHeight, capturedMaxRight, capturedMaxBottom);

    if (scene.m_photo.capture.items.empty())
    {
        if (flashEnabled || defeatedGhostInFinder || restoredSepiaBackground)
        {
            scene.m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "shutter", 0.0f, 0.0f });
            scene.m_ui.shutterFlashRemaining = gShutterFlashSeconds;
            if (flashEnabled)
            {
                scene.StartCameraFlashPulse(kUnlockedCameraFlashPulseSeconds);
            }
        }
        return;
    }

    FinalizeCapturedPhoto(scene, *player, frameWidth, frameHeight);
}

void PhotoCaptureSystem::CaptureEntitiesInFrame(
    GameScene& scene,
    float frameX,
    float frameY,
    float frameWidth,
    float frameHeight,
    float& capturedMaxRight,
    float& capturedMaxBottom,
    bool& restoredSepiaBackground)
{
    std::vector<Entity*> entitiesToRemove;
    bool capturedMidBoss3FistRubbleAttack = false;
    bool capturedMidBoss3DrillRubbleAttack = false;
    bool capturedMidBoss2Spear = false;
    std::vector<const Entity*> capturedBossVisuals;
    std::vector<Entity*> captureCandidates;
    captureCandidates.reserve(
        scene.EntitiesByTag(EntityTag::Enemy).size() +
        scene.EntitiesByTag(EntityTag::PhotoBox).size() +
        scene.EntitiesByTag(EntityTag::Goal).size() +
        scene.EntitiesByTag(EntityTag::PhotoSource).size() +
        scene.EntitiesByTag(EntityTag::Hazard).size() +
        scene.EntitiesByTag(EntityTag::Battery).size() +
        scene.EntitiesByTag(EntityTag::BatterySwitch).size() +
        scene.EntitiesByTag(EntityTag::Elevator).size() +
        scene.EntitiesByTag(EntityTag::LaserSwitch).size() +
        scene.EntitiesByTag(EntityTag::Shutter).size() +
        scene.EntitiesByTag(EntityTag::ProtectiveWall).size() +
        scene.EntitiesByTag(EntityTag::LaserTurret).size() +
        scene.EntitiesByTag(EntityTag::LaserBeam).size() +
        scene.EntitiesByTag(EntityTag::StageLight).size() +
        scene.EntitiesByTag(EntityTag::MarkerLight).size() +
        scene.EntitiesByTag(EntityTag::SepiaRubble).size() +
        scene.EntitiesByTag(EntityTag::SepiaElevator).size() +
        scene.EntitiesByTag(EntityTag::Filter).size() +
        scene.EntitiesByTag(EntityTag::Bullet).size() +
        scene.EntitiesByTag(EntityTag::Shield).size() +
        scene.EntitiesByTag(EntityTag::BossShield).size() +
        scene.EntitiesByTag(EntityTag::Boss1Shield).size() +
        scene.EntitiesByTag(EntityTag::MidBoss1Shield).size() +
        scene.EntitiesByTag(EntityTag::CapturedShield).size() +
        scene.EntitiesByTag(EntityTag::MidBoss3Fist).size() +
        scene.EntitiesByTag(EntityTag::Barrel).size() +
        scene.EntitiesByTag(EntityTag::Log).size() +
        scene.EntitiesByTag(EntityTag::FallingRock).size() +
        scene.EntitiesByTag(EntityTag::DamagePlatform).size() +
        scene.EntitiesByTag(EntityTag::DamagePlatformSpike).size());
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Enemy);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::PhotoBox);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Goal);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::PhotoSource);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Hazard);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Battery);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::BatterySwitch);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Elevator);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::LaserSwitch);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Shutter);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::ProtectiveWall);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::LaserTurret);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::LaserBeam);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::StageLight);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::MarkerLight);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::SepiaRubble);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::SepiaElevator);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Filter);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Bullet);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Shield);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::BossShield);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Boss1Shield);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::MidBoss1Shield);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::CapturedShield);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::MidBoss3Fist);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Barrel);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::Log);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::FallingRock);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::DamagePlatform);
    AppendEntitiesByTag(captureCandidates, scene, EntityTag::DamagePlatformSpike);

    for (Entity* entity : captureCandidates)
    {
        if (!entity || HasTag(*entity, EntityTag::Player) || HasTag(*entity, EntityTag::DropItem))
        {
            continue;
        }
        if (HasTag(*entity, EntityTag::Enemy))
        {
            const auto* enemyComp = entity->GetComponent<EnemyComponent>();
            if (!enemyComp ||
                enemyComp->GetArchetype() != EnemyArchetype::Walker ||
                !enemyComp->attackCaptureWindowActive)
            {
                continue;
            }
        }
        const auto* bossBeamCapture = entity->GetComponent<BossBeamCaptureComponent>();
        const bool isCapturableBossBeam =
            HasTag(*entity, EntityTag::LaserBeam) &&
            bossBeamCapture &&
            bossBeamCapture->captureEnabled;
        if ((HasTag(*entity, EntityTag::LaserBeam) && !isCapturableBossBeam) ||
            (HasTag(*entity, EntityTag::LaserTurret) &&
                (!bossBeamCapture || !bossBeamCapture->captureEnabled)) ||
            HasTag(*entity, EntityTag::StageLight) ||
            HasTag(*entity, EntityTag::HangingGravityObject))
        {
            continue;
        }
        if (HasTag(*entity, EntityTag::BossShockwave))
        {
            continue;
        }
        const auto* midBoss3Fist = entity->GetComponent<MidBoss3FistComponent>();
        if (midBoss3Fist)
        {
            if (midBoss3Fist->captureJammerActive)
            {
                continue;
            }
            const bool fistAttackActive =
                midBoss3Fist->state == MidBoss3FistState::Launching ||
                midBoss3Fist->state == MidBoss3FistState::MeteorFalling;
            if (!fistAttackActive)
            {
                continue;
            }
        }
        const bool isPhotoBox = HasTag(*entity, EntityTag::PhotoBox);
        if (isPhotoBox)
        {
            const auto* layer = entity->GetComponent<PhotoCopyLayerComponent>();
            if (!layer || layer->layer != PhotoCopyLayer::Foreground)
            {
                continue;
            }

            if (const auto* pasteAnimation = entity->GetComponent<PhotoPasteAnimationComponent>())
            {
                if (!pasteAnimation->IsFinished())
                {
                    continue;
                }
            }
        }

        if (HasTag(*entity, EntityTag::BatterySwitch) ||
            HasTag(*entity, EntityTag::BatteryGenerator) ||
            HasTag(*entity, EntityTag::ConveyorBelt) ||
            HasTag(*entity, EntityTag::Elevator) ||
            HasTag(*entity, EntityTag::LaserSwitch) ||
            HasTag(*entity, EntityTag::Shutter) ||
            HasTag(*entity, EntityTag::ProtectiveWall))
        {
            continue;
        }

        const auto* targetTransform = entity->GetComponent<TransformComponent>();
        const auto* sprite = entity->GetComponent<SpriteRenderComponent>();
        if (!targetTransform || !sprite)
        {
            continue;
        }

        const float targetX = targetTransform->x;
        const float targetY = targetTransform->y;
        const float targetWidth = targetTransform->width * targetTransform->scale;
        const float targetHeight = targetTransform->height * targetTransform->scale;
        const float overlapLeft = (std::max)(frameX, targetX);
        const float overlapTop = (std::max)(frameY, targetY);
        const float overlapRight = (std::min)(frameX + frameWidth, targetX + targetWidth);
        const float overlapBottom = (std::min)(frameY + frameHeight, targetY + targetHeight);
        const float overlapWidth = (std::max)(0.0f, overlapRight - overlapLeft);
        const float overlapHeight = (std::max)(0.0f, overlapBottom - overlapTop);
        if (overlapWidth <= 0.0f || overlapHeight <= 0.0f)
        {
            continue;
        }

        const float localLeft = (overlapLeft - targetX) / targetWidth;
        const float localTop = (overlapTop - targetY) / targetHeight;
        const float localWidth = overlapWidth / targetWidth;
        const float localHeight = overlapHeight / targetHeight;

        CapturedPhotoItem item;
        const bool capturedLog = HasTag(*entity, kTagLog);
        const bool capturedDamagePlatform = HasTag(*entity, kTagDamagePlatform);
        const bool capturedDamagePlatformSpike = HasTag(*entity, kTagDamagePlatformSpike);
        const bool capturedBarrel = entity->GetComponent<BarrelComponent>() != nullptr && !capturedLog;
        const auto* fallingRock = entity->GetComponent<FallingRockComponent>();
        const bool capturedFallingRock = fallingRock != nullptr && !capturedLog;
        if (capturedFallingRock)
        {
            if (fallingRock->rubbleActive || fallingRock->cooldownActive)
            {
                continue;
            }
        }
        const bool capturedBattery = entity->GetComponent<BatteryComponent>() != nullptr && !capturedLog;
        const auto* laserBeam = entity->GetComponent<LaserBeamComponent>();
        const bool capturedBossBeam = isCapturableBossBeam && laserBeam != nullptr;
        const bool capturedLaserTurret = (HasTag(*entity, kTagLaserTurret) ||
            capturedBossBeam) &&
            (!bossBeamCapture || bossBeamCapture->captureEnabled);
        if (HasTag(*entity, kTagLaserTurret) && bossBeamCapture && !bossBeamCapture->captureEnabled)
        {
            continue;
        }
        const auto* vanishOnCapture = entity->GetComponent<VanishOnCaptureComponent>();
        const bool capturedVanishObject = vanishOnCapture && vanishOnCapture->enabled;
        const auto* sepiaRubble = entity->GetComponent<SepiaRubbleComponent>();
        const bool capturedSepiaRubble = sepiaRubble != nullptr;
        const bool capturedMidBoss3FistRubble =
            sepiaRubble && sepiaRubble->source == SepiaRubbleSource::MidBoss3Fist;
        const bool capturedMidBoss3DrillRubble =
            sepiaRubble && sepiaRubble->source == SepiaRubbleSource::MidBoss3Drill;
        const bool capturedFallingRockRubble =
            sepiaRubble && sepiaRubble->source == SepiaRubbleSource::FallingRock;
        if (capturedMidBoss3FistRubble && capturedMidBoss3FistRubbleAttack)
        {
            entitiesToRemove.push_back(entity);
            continue;
        }
        if (capturedMidBoss3DrillRubble && capturedMidBoss3DrillRubbleAttack)
        {
            entitiesToRemove.push_back(entity);
            continue;
        }
        auto* sepiaGroup = entity->GetComponent<SepiaRubbleGroupComponent>();
        const bool capturedNumericSepiaRubble = capturedSepiaRubble && sepiaGroup &&
                   sepiaGroup->markerType == '>' && sepiaGroup->restoredMarkerType == '\0';
        if (sepiaGroup && sepiaGroup->markerType == '<')
        { 
            if (scene.m_photo.capture.selectedTheme == PhotoFilterTheme::Sepia)
            {
                const float tileSize = scene.m_tileMap.GetTileSize();
                const float groupLeft = static_cast<float>(sepiaGroup->minColumn) * tileSize;
                const float groupTop = static_cast<float>(sepiaGroup->minRow) * tileSize;
                const float groupRight = static_cast<float>(sepiaGroup->maxColumn + 1) * tileSize;
                const float groupBottom = static_cast<float>(sepiaGroup->maxRow + 1) * tileSize;

                if (groupLeft >= frameX && groupTop >= frameY && groupRight <= frameX + frameWidth && groupBottom <= frameY + frameHeight)
                {
                    if (sepiaGroup->isRestored && sepiaGroup->restoredLifetime > 0.0f)
                    {
                        continue;
                    }

                    const int tileValueToSet = sepiaGroup->restoredTileValue > 0 ? sepiaGroup->restoredTileValue : 1;
                    float restoredLifetimeSeconds = gPastedObjectLifetimeSeconds;
                    if (!sepiaGroup->cellColumns.empty() &&
                        sepiaGroup->cellColumns.size() == sepiaGroup->cellRows.size() &&
                        sepiaGroup->cellColumns.size() == sepiaGroup->cellRestoredTileValues.size())
                    {
                        const bool hasCellRestoredMarkerTypes =
                            sepiaGroup->cellRestoredMarkerTypes.size() == sepiaGroup->cellColumns.size();

                        const bool hasCellRestoredMarkerParameters =
                            sepiaGroup->cellRestoredMarkerParameters.size() == sepiaGroup->cellColumns.size();

                        for (size_t ci = 0; ci < sepiaGroup->cellColumns.size(); ++ci)
                        {
                            const int column = sepiaGroup->cellColumns[ci];
                            const int row = sepiaGroup->cellRows[ci];

                            const char restoredMarkerType =
                                hasCellRestoredMarkerTypes
                                ? sepiaGroup->cellRestoredMarkerTypes[ci]
                                : '\0';

                            if (restoredMarkerType != '\0')
                            {
                                const int restoredMarkerParameter = hasCellRestoredMarkerParameters
                                ? sepiaGroup->cellRestoredMarkerParameters[ci] : 0;

                                if (SpawnRestoredSepiaMarkerObject(
                                    scene.m_world.PendingEntities(),
                                    scene.m_whiteTexture,
                                    tileSize,
                                    restoredLifetimeSeconds,
                                    restoredMarkerType,
                                    restoredMarkerParameter,
                                    column,
                                    row))
                                {
                                    scene.m_tileMap.SetTile(column, row, 0);
                                    scene.m_tileMap.SetMarker(column, row, restoredMarkerType, restoredMarkerParameter);
                                }
                            }
                            else
                            {
                                const int v = sepiaGroup->cellRestoredTileValues[ci];
                                const int tileValueToSet = (v > 0) ? v : 1;
                                scene.m_tileMap.SetTile(column, row, tileValueToSet);
                            }
                        }
                    }
                    sepiaGroup->isRestored = true;
                    sepiaGroup->restoredLifetime = restoredLifetimeSeconds;
					restoredSepiaBackground = true;
                    if (auto* tint = entity->GetComponent<TintComponent>())
                    {
                        tint->a = 0.0f; 
                    }
                    else
                    {
                        entity->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 0.0f);
                    }
                    // Ensure we add a PhotoPasteAnimationComponent so pasted visuals animate for the configured time.
                    if (!entity->GetComponent<PhotoPasteAnimationComponent>())
                    {
                        entity->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
                    }
                }
            }
            continue;
        }
        if (scene.m_photo.capture.selectedTheme == PhotoFilterTheme::Sepia)
        {
            if (sepiaGroup && sepiaGroup->markerType == '>' &&
                sepiaGroup->restoredMarkerType == '\0' && sepiaGroup->restoredTileValue > 0)
            {
                const int tileValue = sepiaGroup->restoredTileValue;


                item.spawnArchetype = CapturedSpawnArchetype::SepiaGround;
                item.role = GetTileCopyRole(tileValue);
                item.layer = PhotoCopyLayer::Foreground;
                item.origin = GetTileCopyOrigin(tileValue);
                item.appliedTheme = scene.m_photo.capture.selectedTheme;
                item.placementRuleGroup = PhotoPlacementRuleGroup::Group1;
                item.relativeX = overlapLeft - frameX;
                item.relativeY = overlapTop - frameY;
                item.width = overlapWidth;
                item.height = overlapHeight;
                item.rotation = targetTransform->rotation;
                item.sourceX = 0.0f;
                item.sourceY = 0.0f;
                item.sourceWidth = 1.0f;
                item.sourceHeight = 1.0f;
                ApplyCapturedTileTint(
                    tileValue,
                    scene.m_tileTexture,
                    scene.m_tileTexture2,
                    scene.m_tileTexture3,
                    item);
                item.sepiaRestoredTileValue = tileValue;
                item.sourceTileValue = tileValue;

                scene.m_photo.capture.items.push_back(item);
                scene.m_photo.capture.attackCaptureCount += item.enemyAttackPaste ? 1 : 0;
                capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
                capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
                continue;
            }

            if (sepiaGroup && sepiaGroup->markerType == '>' && sepiaGroup->restoredMarkerType != '\0')
            {
                if (ApplySepiaRestoredMarkerCaptureSpec(
                    *sepiaGroup,
                    ResolveSepiaTextureId(scene.m_assets, true, sepiaGroup->imageNo),
                    scene.m_whiteTexture, item))
                {
                    item.relativeX = overlapLeft - frameX;
                    item.relativeY = overlapTop - frameY;
                    item.width = overlapWidth;
                    item.height = overlapHeight;
                    item.rotation = targetTransform->rotation;
                    item.sourceX = sprite->GetSourceX() + sprite->GetSourceWidth() * localLeft;
                    item.sourceY = sprite->GetSourceY() + sprite->GetSourceHeight() * localTop;
                    item.sourceWidth = sprite->GetSourceWidth() * localWidth;
                    item.sourceHeight = sprite->GetSourceHeight() * localHeight;
                    
                    scene.m_photo.capture.items.push_back(item);
                    scene.m_photo.capture.attackCaptureCount += item.enemyAttackPaste ? 1 : 0;
                    scene.m_photo.capture.containsEnemyAttackPaste =
                        scene.m_photo.capture.containsEnemyAttackPaste || item.enemyAttackPaste;

                    capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
                    capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
                }
                continue;
            }
        }
        if (capturedSepiaRubble && scene.m_photo.capture.selectedTheme != PhotoFilterTheme::Sepia)
        {
            continue;
        }
        const auto* tileValueComponent = entity->GetComponent<PhotoCopyTileValueComponent>();
        const auto* damagePlatform = entity->GetComponent<DamagePlatformComponent>();
        const auto* spikeStrip = entity->GetComponent<SpikeStripComponent>();
        const auto* projectile = entity->GetComponent<ProjectileComponent>();
        auto* markerLight = entity->GetComponent<MarkerLightComponent>();
        const bool capturedProjectile = projectile != nullptr;
        const auto* enemyComp = entity->GetComponent<EnemyComponent>();
        const bool capturedWalker = enemyComp && enemyComp->GetArchetype() == EnemyArchetype::Walker;
        const auto* shieldComp = entity->GetComponent<ShieldComponent>();
        const bool capturedShield = shieldComp != nullptr;
        if (capturedShield && shieldComp->photoSpawned && shieldComp->grounded)
        {
            continue;
        }
        if (capturedShield && shieldComp->ownerBoss)
        {
            if (const auto* bossComp = shieldComp->ownerBoss->GetComponent<ShieldBossComponent>())
            {
                if (bossComp->deathAnimationActive || bossComp->deathAnimationFinished)
                {
                    continue;
                }
                if (bossComp->state == ShieldBossState::SlamPhase1 ||
                    bossComp->state == ShieldBossState::SlamPhase2)
                {
                    continue;
                }
            }
        }
        CapturedSpawnArchetype capturedShieldArchetype = CapturedSpawnArchetype::None;
        if (capturedShield)
        {
            if (shieldComp->photoSpawned)
            {
                switch (shieldComp->capturedMode)
                {
                case CapturedShieldMode::RushBurst:
                    capturedShieldArchetype = CapturedSpawnArchetype::ShieldRushBurst;
                    break;
                case CapturedShieldMode::JumpBurst:
                    capturedShieldArchetype = CapturedSpawnArchetype::ShieldJumpBurst;
                    break;
                case CapturedShieldMode::Normal:
                case CapturedShieldMode::None:
                default:
                    capturedShieldArchetype = CapturedSpawnArchetype::ShieldNormal;
                    break;
                }
            }
            else if (shieldComp->ownerBoss)
            {
                if (const auto* bossComp = shieldComp->ownerBoss->GetComponent<ShieldBossComponent>())
                {
                    switch (bossComp->state)
                    {
                    case ShieldBossState::Rush:
                        // 攻撃①のチャージ中も盾だけは通常盾として保存できるようにします。
                        capturedShieldArchetype = IsShieldBossRushCaptureReady(*shieldComp->ownerBoss)
                            ? CapturedSpawnArchetype::ShieldRushBurst
                            : CapturedSpawnArchetype::ShieldNormal;
                        break;
                    case ShieldBossState::JumpAscend:
                    case ShieldBossState::AirHover:
                    case ShieldBossState::JumpDescend:
                        capturedShieldArchetype = CapturedSpawnArchetype::ShieldJumpBurst;
                        break;
                    default:
                        capturedShieldArchetype = CapturedSpawnArchetype::ShieldNormal;
                        break;
                    }
                }
            }
            else
            {
                capturedShieldArchetype = CapturedSpawnArchetype::ShieldNormal;
            }
        }
        const bool capturedShieldAttack =
            capturedShieldArchetype == CapturedSpawnArchetype::ShieldRushBurst ||
            capturedShieldArchetype == CapturedSpawnArchetype::ShieldJumpBurst;
        item.textureId = sprite->GetTextureId();
        item.role = GetEntityCopyRole(*entity);
        item.layer = PhotoCopyLayer::Foreground;
        item.origin = GetEntityCopyOrigin(*entity);
        item.appliedTheme = scene.m_photo.capture.selectedTheme;
        if (capturedMidBoss3FistRubble || midBoss3Fist)
        {
            item.spawnArchetype = CapturedSpawnArchetype::MidBoss3FistAttack;
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Hazard;
            item.textureId = scene.m_whiteTexture;
        }
        else if (capturedMidBoss3DrillRubble)
        {
            item.spawnArchetype = CapturedSpawnArchetype::MidBoss3DrillAttack;
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Hazard;
            item.textureId = scene.m_whiteTexture;
        }
        else if (capturedFallingRockRubble)
        {
            item.spawnArchetype = CapturedSpawnArchetype::FallingRock;
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
            item.textureId = scene.m_whiteTexture;
        }
        else if (capturedFallingRock)
        {
            item.spawnArchetype = CapturedSpawnArchetype::FallingRock;
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
        }
        else if (capturedSepiaRubble)
        {
            item.spawnArchetype = CapturedSpawnArchetype::SepiaGround;
            item.textureId = capturedNumericSepiaRubble
                ? ResolveSepiaTextureId(scene.m_assets, true, sepiaGroup->imageNo)
                : scene.m_assets.GetTexture("sepia_ground");
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
        }
        else if (capturedShield)
        {
            item.spawnArchetype = capturedShieldArchetype;
        }
        else if (capturedBarrel)
        {
            item.spawnArchetype = CapturedSpawnArchetype::Barrel;
        }
        else if (capturedLog)
        {
            item.spawnArchetype = CapturedSpawnArchetype::Log;
        }
        else if (capturedFallingRock)
        {
            item.spawnArchetype = CapturedSpawnArchetype::FallingRock;
        }
        else if (capturedBattery)
        {
            item.spawnArchetype = CapturedSpawnArchetype::Battery;
        }
        else if (capturedProjectile)
        {
            item.spawnArchetype = CapturedSpawnArchetype::Projectile;
        }
        else if (capturedLaserTurret)
        {
            item.spawnArchetype = CapturedSpawnArchetype::LaserTurret;
        }
        else if (capturedWalker)
        {
            item.spawnArchetype = CapturedSpawnArchetype::WalkerMelee;
        }
        else
        {
            item.spawnArchetype = CapturedSpawnArchetype::None;
        }
        item.enemyAttackPaste =
            capturedWalker ||
            capturedShieldAttack ||
            capturedLaserTurret ||
            capturedMidBoss2Spear ||
            midBoss3Fist != nullptr ||
            capturedMidBoss3FistRubble ||
            capturedMidBoss3DrillRubble;
        item.placementRuleGroup = ResolvePlacementRuleGroupForCapturedEntity(
            *entity,
            capturedVanishObject,
            capturedBarrel,
            capturedFallingRock,
            capturedBattery,
            capturedProjectile,
            capturedLaserTurret,
            capturedWalker);
        item.vanishOnCapture = capturedVanishObject;
        item.relativeX = overlapLeft - frameX;
        item.relativeY = overlapTop - frameY;
        item.width = overlapWidth;
        item.height = overlapHeight;
        item.rotation = targetTransform->rotation;
        item.sourceX = sprite->GetSourceX() + sprite->GetSourceWidth() * localLeft;
        item.sourceY = sprite->GetSourceY() + sprite->GetSourceHeight() * localTop;
        item.sourceWidth = sprite->GetSourceWidth() * localWidth;
        item.sourceHeight = sprite->GetSourceHeight() * localHeight;
        item.sourceTileValue = tileValueComponent ? tileValueComponent->tileValue : 0;
        if (capturedNumericSepiaRubble)
        {
            item.sepiaRestoredTileValue = sepiaGroup->restoredTileValue;
        }
        BuildCapturedOutlineFromEntity(*entity, localLeft, localTop, localWidth, localHeight, item.collisionOutline);
        if (damagePlatform)
        {
            item.damagePlatformTileSpan = damagePlatform->tileSpan;
            item.sourceTileValue = 0;
        }
        if (spikeStrip)
        {
            item.spikeStripTileSpan = spikeStrip->tileSpan;
            item.sourceTileValue = 0;
        }
        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            item.tintR = tint->r;
            item.tintG = tint->g;
            item.tintB = tint->b;
            item.tintA = tint->a;
        }
        if (capturedShield)
        {
            constexpr float kCapturedShieldWidth = 240.0f;
            constexpr float kCapturedShieldHeight = 192.0f;
            const float capturedCenterX = overlapLeft + overlapWidth * 0.5f;
            const float capturedCenterY = overlapTop + overlapHeight * 0.5f;
            item.relativeX = capturedCenterX - frameX - kCapturedShieldWidth * 0.5f;
            item.relativeY = capturedCenterY - frameY - kCapturedShieldHeight * 0.5f;
            item.width = kCapturedShieldWidth;
            item.height = kCapturedShieldHeight;
            item.rotation = 0.0f;
            item.sourceX = sprite->GetSourceX();
            item.sourceY = sprite->GetSourceY();
            item.sourceWidth = sprite->GetSourceWidth();
            item.sourceHeight = sprite->GetSourceHeight();
            // 盾単体の写真は、ボスの向きに合わせた左右反転も再現します。
            item.flipX = sprite->GetFlipX();
            item.collisionOutline.clear();
            item.collisionOutline.push_back({ 0.0f, 0.0f });
            item.collisionOutline.push_back({ 1.0f, 0.0f });
            item.collisionOutline.push_back({ 1.0f, 1.0f });
            item.collisionOutline.push_back({ 0.0f, 1.0f });
        }
        if (capturedMidBoss3FistRubble || capturedMidBoss3DrillRubble)
        {
            const float capturedCenterX = overlapLeft + overlapWidth * 0.5f;
            const float capturedCenterY = overlapTop + overlapHeight * 0.5f;
            const float capturedAttackWidth = capturedMidBoss3DrillRubble ? kMidBoss3CapturedDrillWidth : kMidBoss3CapturedFistWidth;
            const float capturedAttackHeight = capturedMidBoss3DrillRubble ? kMidBoss3CapturedDrillHeight : kMidBoss3CapturedFistHeight;
            item.textureId = scene.m_whiteTexture;
            item.relativeX = capturedCenterX - frameX - capturedAttackWidth * 0.5f;
            item.relativeY = capturedCenterY - frameY - capturedAttackHeight * 0.5f;
            item.width = capturedAttackWidth;
            item.height = capturedAttackHeight;
            item.sourceX = 0.0f;
            item.sourceY = 0.0f;
            item.sourceWidth = 1.0f;
            item.sourceHeight = 1.0f;
            item.rotation = 0.0f;
            item.flipX = false;
            item.tintR = 0.96f;
            item.tintG = 0.52f;
            item.tintB = 0.18f;
            item.tintA = 1.0f;
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Hazard;
        }
        else if (capturedFallingRockRubble)
        {
            item.textureId = scene.m_whiteTexture;
            item.sourceX = 0.0f;
            item.sourceY = 0.0f;
            item.sourceWidth = 1.0f;
            item.sourceHeight = 1.0f;
            item.tintA = 1.0f;
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
            item.placementRuleGroup = PhotoPlacementRuleGroup::Group2;
        }
        else if (capturedFallingRock)
        {
            item.tintA = 1.0f;
        }
        else if (midBoss3Fist)
        {
            item.textureId = scene.m_whiteTexture;
            item.sourceX = 0.0f;
            item.sourceY = 0.0f;
            item.sourceWidth = 1.0f;
            item.sourceHeight = 1.0f;
            item.tintR = 0.96f;
            item.tintG = 0.52f;
            item.tintB = 0.18f;
            item.tintA = 1.0f;
        }
        if (markerLight)
        {
            markerLight->activated = !markerLight->activated;
            continue;
        }
        if (capturedProjectile)
        {
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            item.projectileVelocityX = projectile->GetVelocityX();
            item.projectileVelocityY = projectile->GetVelocityY();
            item.projectileDamage = projectile->GetDamage();
            item.rotation = std::atan2(item.projectileVelocityY, item.projectileVelocityX);
            if (const auto* spear = entity->GetComponent<MidBoss2SpearComponent>())
            {
                if (capturedMidBoss2Spear)
                {
                    continue;
                }
                capturedMidBoss2Spear = true;
                item.enemyAttackPaste = true;
                item.spearProjectile = true;
                item.spearStuck = spear->stuck;
                item.spearDirectionX = spear->stuck ? spear->directionX : spear->targetDirectionX;
                item.spearDirectionY = spear->stuck ? spear->directionY : spear->targetDirectionY;
                item.spearTravelDistance = spear->travelDistance;
                Logger::Info(
                    std::string("Captured MidBoss2 spear: stuck=") +
                    (spear->stuck ? "1" : "0") +
                    " travel=" + std::to_string(spear->travelDistance) +
                    " dirX=" + std::to_string(item.spearDirectionX) +
                    " dirY=" + std::to_string(item.spearDirectionY) +
                    " projectileDamage=" + std::to_string(item.projectileDamage));
                if (std::fabs(item.spearDirectionX) > 0.0001f || std::fabs(item.spearDirectionY) > 0.0001f)
                {
                    item.rotation = std::atan2(item.spearDirectionY, item.spearDirectionX);
                }
            }
        }
        else if (capturedWalker)  
        {
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
        }
        else if (midBoss3Fist || capturedMidBoss3FistRubble || capturedMidBoss3DrillRubble)
        {
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
        }
        else if (capturedLaserTurret)
        {
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            if (const auto* laserTurret = entity->GetComponent<LaserTurretComponent>())
            {
                item.laserBeamThickness = laserTurret->beamThickness;
                item.laserDamagePerSecond = laserTurret->damagePerSecond;
                item.laserEnemyKnockbackSpeed = laserTurret->enemyKnockbackSpeed;
            }
            else if (laserBeam)
            {
                item.laserBeamThickness = targetTransform->height * targetTransform->scale;
                item.laserDamagePerSecond = laserBeam->damagePerSecond;
                item.laserEnemyKnockbackSpeed = laserBeam->enemyKnockbackSpeed;
            }
        }
        else if (capturedDamagePlatform)
        {
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
        }
        else if (capturedDamagePlatformSpike || damagePlatform || spikeStrip)
        {
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
        }
        else if (!capturedBarrel && !capturedFallingRock && !capturedBattery && !capturedLaserTurret && !capturedShield && !capturedDamagePlatform && !capturedDamagePlatformSpike && !capturedSepiaRubble && !midBoss3Fist)
        {
            item.role = GetRoleFromTint(item.tintR, item.tintG, item.tintB);
            item.layer = GetLayerFromTint(item.tintR, item.tintG, item.tintB);
        }
        else
        {
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
        }
        if (capturedVanishObject)
        {
            item.origin = PhotoCopyOrigin::Generic;
            item.placementRuleGroup = PhotoPlacementRuleGroup::Group1;
        }

        if (item.role == PhotoCopyRole::Solid)
        {
            item.layer = PhotoCopyLayer::Foreground;
        }

        if (!capturedBarrel && !capturedFallingRock && !capturedBattery && !capturedLaserTurret && !capturedLog && !capturedShield && !capturedDamagePlatform && !capturedDamagePlatformSpike && !isPhotoBox && !capturedVanishObject && !capturedWalker && !capturedSepiaRubble && !midBoss3Fist)
        {
            ApplyPhotoFilterToCapturedTarget(*entity, scene.m_photo.capture.selectedTheme);
        }
        scene.m_photo.capture.items.push_back(item);
        scene.m_photo.capture.attackCaptureCount += item.enemyAttackPaste ? 1 : 0;
        if (capturedShieldAttack && shieldComp)
        {
            if (AppendShieldBossMotionCaptureItem(
                    scene.m_photo.capture,
                    scene.m_photo.capture.selectedTheme,
                    *shieldComp,
                    frameX,
                    frameY,
                    capturedBossVisuals))
            {
                const CapturedPhotoItem& bossItem = scene.m_photo.capture.items.back();
                capturedMaxRight = (std::max)(capturedMaxRight, bossItem.relativeX + bossItem.width);
                capturedMaxBottom = (std::max)(capturedMaxBottom, bossItem.relativeY + bossItem.height);
            }
        }
        if (capturedMidBoss3FistRubble)
        {
            capturedMidBoss3FistRubbleAttack = true;
            entitiesToRemove.push_back(entity);
        }
        if (capturedMidBoss3DrillRubble)
        {
            capturedMidBoss3DrillRubbleAttack = true;
            entitiesToRemove.push_back(entity);
        }
        if (capturedFallingRockRubble)
        {
            entitiesToRemove.push_back(entity);
        }
        scene.m_photo.capture.containsEnemyAttackPaste =
            scene.m_photo.capture.containsEnemyAttackPaste || item.enemyAttackPaste;
        if (item.enemyAttackPaste)
        {
            Logger::Info(
                std::string("Captured attack item appended: archetype=") +
                std::to_string(static_cast<int>(item.spawnArchetype)) +
                " attackCount=" + std::to_string(scene.m_photo.capture.attackCaptureCount) +
                " containsEnemyAttackPaste=" + (scene.m_photo.capture.containsEnemyAttackPaste ? "1" : "0"));
        }
        capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
        capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
        if (capturedVanishObject)
        {
            entitiesToRemove.push_back(entity);
        }
    }
    scene.m_world.RemoveByPointerList(entitiesToRemove);
}

void PhotoCaptureSystem::CaptureTilesInFrame(
    GameScene& scene,
    float frameX,
    float frameY,
    float frameWidth,
    float frameHeight,
    float& capturedMaxRight,
    float& capturedMaxBottom)
{
    const float tileSize = scene.m_tileMap.GetTileSize();
    const int leftColumn = std::max(0, static_cast<int>(frameX / tileSize));
    const int rightColumn = std::min(scene.m_tileMap.GetWidth() - 1, static_cast<int>((frameX + frameWidth - 1.0f) / tileSize));
    const int topRow = std::max(0, static_cast<int>(frameY / tileSize));
    const int bottomRow = std::min(scene.m_tileMap.GetHeight() - 1, static_cast<int>((frameY + frameHeight - 1.0f) / tileSize));

    for (int row = topRow; row <= bottomRow; ++row)
    {
        for (int column = leftColumn; column <= rightColumn; ++column)
        {
            const int tileValue = scene.m_tileMap.GetTile(column, row);
            if (tileValue <= 0)
            {
                continue;
            }

            if (tileValue == 1 || tileValue == 8 || tileValue == 11)
            {
                continue;
            }

            const float tileX = static_cast<float>(column) * tileSize;
            const float tileY = static_cast<float>(row) * tileSize;
            const TileTriangleShape triangle = TileMap::GetTriangleShape(tileValue);
            const float boundsWidth = triangle.isTriangle
                ? static_cast<float>(triangle.widthTiles) * tileSize
                : tileSize;
            const float boundsHeight = triangle.isTriangle
                ? static_cast<float>(triangle.heightTiles) * tileSize
                : tileSize;
            const float overlapLeft = (std::max)(frameX, tileX);
            const float overlapTop = (std::max)(frameY, tileY);
            const float overlapRight = (std::min)(frameX + frameWidth, tileX + boundsWidth);
            const float overlapBottom = (std::min)(frameY + frameHeight, tileY + boundsHeight);
            const float overlapWidth = (std::max)(0.0f, overlapRight - overlapLeft);
            const float overlapHeight = (std::max)(0.0f, overlapBottom - overlapTop);
            if (overlapWidth <= 1.0f || overlapHeight <= 1.0f)
            {
                continue;
            }

            CapturedPhotoItem item;
            item.role = GetTileCopyRole(tileValue);
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = GetTileCopyOrigin(tileValue);
            item.appliedTheme = scene.m_photo.capture.selectedTheme;
            item.placementRuleGroup = PhotoPlacementRuleGroup::Group1;
            item.relativeX = overlapLeft - frameX;
            item.relativeY = overlapTop - frameY;
            item.width = overlapWidth;
            item.height = overlapHeight;
            item.sourceX = 0.0f;
            item.sourceY = 0.0f;
            item.sourceWidth = 1.0f;
            item.sourceHeight = 1.0f;
            ApplyCapturedTileTint(
                tileValue,
                scene.m_tileTexture,
                scene.m_tileTexture2,
                scene.m_tileTexture3,
                item);
            item.sourceTileValue = tileValue;
            scene.m_photo.capture.items.push_back(item);
            scene.m_photo.capture.attackCaptureCount += item.enemyAttackPaste ? 1 : 0;
            capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
            capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
        }
    }
}

void PhotoCaptureSystem::FinalizeCapturedPhoto(GameScene& scene, Entity& player, float frameWidth, float frameHeight)
{
    scene.m_photo.capture.hasPhoto = true;
    scene.m_photo.capture.capturedTheme = scene.m_photo.capture.selectedTheme;
    scene.m_photo.placement.layer = PhotoCopyLayer::Foreground;
    scene.m_photo.placement.flipX = false;
    scene.m_photo.placement.rotation = 0.0f;
    scene.m_photo.capture.width = (std::max)(1.0f, frameWidth);
    scene.m_photo.capture.height = (std::max)(1.0f, frameHeight);
    scene.m_photo.capture.textureId = scene.m_photo.capture.items.front().textureId;
    scene.m_photo.capture.sourceX = scene.m_photo.capture.items.front().sourceX;
    scene.m_photo.capture.sourceY = scene.m_photo.capture.items.front().sourceY;
    scene.m_photo.capture.sourceWidth = scene.m_photo.capture.items.front().sourceWidth;
    scene.m_photo.capture.sourceHeight = scene.m_photo.capture.items.front().sourceHeight;
    scene.m_photo.capture.tintR = scene.m_photo.capture.items.front().tintR;
    scene.m_photo.capture.tintG = scene.m_photo.capture.items.front().tintG;
    scene.m_photo.capture.tintB = scene.m_photo.capture.items.front().tintB;
    scene.m_photo.capture.tintA = scene.m_photo.capture.items.front().tintA;
    if (scene.m_photo.capture.containsEnemyAttackPaste)
    {
        Logger::Info(
            std::string("FinalizeCapturedPhoto attack capture: items=") +
            std::to_string(scene.m_photo.capture.items.size()) +
            " attackCount=" + std::to_string(scene.m_photo.capture.attackCaptureCount) +
            " theme=" + std::to_string(static_cast<int>(scene.m_photo.capture.capturedTheme)));
    }
    scene.StoreCapturedPhoto();

    scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "shutter", 0.0f, 0.0f });
    scene.m_ui.shutterFlashRemaining = gShutterFlashSeconds;
    if (scene.m_ui.cameraFlash.unlocked && scene.m_ui.cameraFlash.enabled)
    {
        scene.StartCameraFlashPulse(kUnlockedCameraFlashPulseSeconds);
    }
    scene.m_eventBus.Publish({ EventType::LogMessage, &player, nullptr, GetPhotoCaptureLogMessage(scene.m_photo.capture.capturedTheme), 0.0f, 0.0f });
    scene.m_ui.developedPhotoPreviewRemaining = scene.m_ui.tuning.developedPhotoPreview.lifetime;
}

