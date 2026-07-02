#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_photo_storage_layout.h"
#include "game_scene_render_ui_helpers.h"
#include "game_viewport.h"
#include "photo_system.h"
#include "photo_shared.h"
#include "photo_filter_rules.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

#include "DxLib.h"
#include <texture.h>

using namespace game_scene_detail;

namespace
{
    constexpr float kPitRestartFadeDuration = 0.45f;
    constexpr float kStageTransitionFadeOutDuration = 0.45f;
    constexpr float kStageTransitionFadeInDuration = 1.10f;
    constexpr float kCaptureFinderBaseTilesX = 23.0f;

    float GetCaptureFinderScreenScale(float tileSize)
    {
        const float targetWorldWidth = std::max(1.0f, tileSize * kCaptureFinderBaseTilesX);
        const float baseCameraZoomMultiplier =
            std::max(1.0f, static_cast<float>(SCREEN_WIDTH) / targetWorldWidth);
        return game_viewport::ComputeViewScale(
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            gCameraViewWidth,
            gCameraViewHeight,
            baseCameraZoomMultiplier);
    }

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
    constexpr int kDarknessOverlayActiveLightLimit = 6;

    struct BackgroundPartPlacement
    {
        const char* textureKey;
        float worldX;
        float worldY;
        float width;
        float height;
        float parallax; // 1.0 = 通常の足場と同じ動き
    };

    //// 仮配置。座標は後で調整
    //constexpr BackgroundPartPlacement kBackgroundParts[] =
    //{
    //    { "bg_parts_tree_01",  320.0f, 480.0f, 192.0f, 256.0f, 1.0f },
    //    { "bg_parts_rock_01",  860.0f, 620.0f, 128.0f,  96.0f, 1.0f },
    //    { "bg_parts_grass_01", 540.0f, 700.0f, 160.0f,  64.0f, 1.0f },
    //};

    float SmoothStep01(float t);

    struct OverlayLightSource
    {
        float centerX = 0.0f;
        float centerY = 0.0f;
        float shapeType = 0.0f;
        float innerRadius = 0.0f;
        float outerRadius = 0.0f;
        float extentX = 0.0f;
        float extentY = 0.0f;
        float intensity = 1.0f;
        float colorR = 1.0f;
        float colorG = 1.0f;
        float colorB = 1.0f;
        float priority = 0.0f;
    };

    struct DarknessOverlayContext
    {
        float viewOriginX = 0.0f;
        float viewOriginY = 0.0f;
        float cameraX = 0.0f;
        float cameraY = 0.0f;
        float viewScale = 1.0f;
        float playerLightScreenX = 0.0f;
        float playerLightScreenY = 0.0f;
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        int maxDarknessAlpha = 0;
        float tileSize = 0.0f;
    };

    float WorldToOverlayScreenX(const DarknessOverlayContext& ctx, float worldX)
    {
        return ctx.viewOriginX + (worldX - ctx.cameraX) * ctx.viewScale;
    }

    float WorldToOverlayScreenY(const DarknessOverlayContext& ctx, float worldY)
    {
        return ctx.viewOriginY + (worldY - ctx.cameraY) * ctx.viewScale;
    }

    bool IsOverlayLightVisible(const OverlayLightSource& light, const DarknessOverlayContext& ctx)
    {
        const float extentX = light.shapeType >= 0.5f ? light.extentX : 0.0f;
        const float extentY = light.shapeType >= 0.5f ? light.extentY : 0.0f;
        const float radius = light.outerRadius;
        return light.centerX + extentX + radius >= static_cast<float>(ctx.left) &&
            light.centerX - extentX - radius <= static_cast<float>(ctx.right) &&
            light.centerY + extentY + radius >= static_cast<float>(ctx.top) &&
            light.centerY - extentY - radius <= static_cast<float>(ctx.bottom);
    }

    void AppendOverlayLight(
        std::vector<OverlayLightSource>& overlayLights,
        OverlayLightSource light,
        const DarknessOverlayContext& ctx,
        float basePriority)
    {
        if (!IsOverlayLightVisible(light, ctx))
        {
            return;
        }

        const float dx = light.centerX - ctx.playerLightScreenX;
        const float dy = light.centerY - ctx.playerLightScreenY;
        const float distancePenalty = std::sqrt(dx * dx + dy * dy) * 0.08f;
        light.priority = basePriority + light.outerRadius * light.intensity - distancePenalty;
        overlayLights.push_back(light);
    }

    template <typename Fn>
    void ForEachCaptureTargetCandidate(
        const GameScene& scene,
        const TransformComponent& captureFrame,
        Fn&& fn)
    {
        const auto& photoBoxEntities = scene.EntitiesByTag(EntityTag::PhotoBox);
        const auto& goalEntities = scene.EntitiesByTag(EntityTag::Goal);
        const auto& photoSourceEntities = scene.EntitiesByTag(EntityTag::PhotoSource);
        const auto& hazardEntities = scene.EntitiesByTag(EntityTag::Hazard);
        const auto& bulletEntities = scene.EntitiesByTag(EntityTag::Bullet);
        const auto& dropItemEntities = scene.EntitiesByTag(EntityTag::DropItem);
        const auto& batteryEntities = scene.EntitiesByTag(EntityTag::Battery);
        const auto& logEntities = scene.EntitiesByTag(EntityTag::Log);
        const auto& damagePlatformEntities = scene.EntitiesByTag(EntityTag::DamagePlatform);
        const auto& damagePlatformSpikeEntities = scene.EntitiesByTag(EntityTag::DamagePlatformSpike);
        const auto& laserTurretEntities = scene.EntitiesByTag(EntityTag::LaserTurret);
        const auto& markerLightEntities = scene.EntitiesByTag(EntityTag::MarkerLight);
        const auto& sepiaRubbleEntities = scene.EntitiesByTag(EntityTag::SepiaRubble);
        const auto& sepiaElevatorEntities = scene.EntitiesByTag(EntityTag::SepiaElevator);
        const auto& filterEntities = scene.EntitiesByTag(EntityTag::Filter);
        const auto& barrelEntities = scene.EntitiesByTag(EntityTag::Barrel);
        const auto& shieldEntities = scene.EntitiesByTag(EntityTag::Shield);
        const auto& bossShieldEntities = scene.EntitiesByTag(EntityTag::BossShield);
        const auto& boss1ShieldEntities = scene.EntitiesByTag(EntityTag::Boss1Shield);
        const auto& midBoss1ShieldEntities = scene.EntitiesByTag(EntityTag::MidBoss1Shield);
        const auto& capturedShieldEntities = scene.EntitiesByTag(EntityTag::CapturedShield);
        const auto& walkerMeleeAttackEntities = scene.EntitiesByTag(EntityTag::WalkerMeleeAttack);
        const auto& bossShockwaveEntities = scene.EntitiesByTag(EntityTag::BossShockwave);

        auto considerCaptureTarget = [&](Entity* entity)
        {
            if (!entity)
            {
                return;
            }

            if (HasTag(*entity, EntityTag::Player) ||
                HasTag(*entity, EntityTag::Enemy) ||
                HasTag(*entity, EntityTag::BatterySwitch) ||
                HasTag(*entity, EntityTag::Elevator) ||
                HasTag(*entity, EntityTag::LaserSwitch) ||
                HasTag(*entity, EntityTag::Shutter) ||
                HasTag(*entity, EntityTag::LaserBeam) ||
                HasTag(*entity, EntityTag::StageLight))
            {
                return;
            }

            if (HasTag(*entity, EntityTag::PhotoBox))
            {
                const auto* layer = entity->GetComponent<PhotoCopyLayerComponent>();
                if (!layer || layer->layer != PhotoCopyLayer::Foreground)
                {
                    return;
                }

                if (const auto* pasteAnimation = entity->GetComponent<PhotoPasteAnimationComponent>())
                {
                    if (!pasteAnimation->IsFinished())
                    {
                        return;
                    }
                }
            }

            if (const auto* shield = entity->GetComponent<ShieldComponent>())
            {
                if (shield->ownerBoss)
                {
                    if (const auto* boss = shield->ownerBoss->GetComponent<ShieldBossComponent>())
                    {
                        if (boss->deathAnimationActive || boss->deathAnimationFinished)
                        {
                            return;
                        }
                    }
                }
            }

            const auto* transform = entity->GetComponent<TransformComponent>();
            const auto* sprite = entity->GetComponent<SpriteRenderComponent>();
            if (!transform || !sprite || !IntersectsRect(captureFrame, *transform))
            {
                return;
            }

            fn(entity, *transform);
        };

        auto scanCandidates = [&](const std::vector<Entity*>& entities)
        {
            for (Entity* entity : entities)
            {
                considerCaptureTarget(entity);
            }
        };

        scanCandidates(photoBoxEntities);
        scanCandidates(goalEntities);
        scanCandidates(photoSourceEntities);
        scanCandidates(hazardEntities);
        scanCandidates(bulletEntities);
        scanCandidates(dropItemEntities);
        scanCandidates(batteryEntities);
        scanCandidates(logEntities);
        scanCandidates(damagePlatformEntities);
        scanCandidates(damagePlatformSpikeEntities);
        scanCandidates(laserTurretEntities);
        scanCandidates(markerLightEntities);
        scanCandidates(sepiaRubbleEntities);
        scanCandidates(sepiaElevatorEntities);
        scanCandidates(filterEntities);
        scanCandidates(barrelEntities);
        scanCandidates(shieldEntities);
        scanCandidates(bossShieldEntities);
        scanCandidates(boss1ShieldEntities);
        scanCandidates(midBoss1ShieldEntities);
        scanCandidates(capturedShieldEntities);
        scanCandidates(walkerMeleeAttackEntities);
        scanCandidates(bossShockwaveEntities);
    }

    void AddPlayerOverlayLight(std::vector<OverlayLightSource>& overlayLights, const DarknessOverlayContext& ctx, float innerRadius, float outerRadius)
    {
        AppendOverlayLight(
            overlayLights,
            {
                ctx.playerLightScreenX,
                ctx.playerLightScreenY,
                0.0f,
                innerRadius,
                outerRadius,
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f },
            ctx,
            100000.0f);
    }

    void CollectMarkerLightOverlayLights(
        const std::vector<Entity*>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        for (Entity* entity : entities)
        {
            if (!entity) continue;
            const auto* extraLight = entity->GetComponent<MarkerLightComponent>();
            const auto* extraTransform = entity->GetComponent<TransformComponent>();
            if (!extraLight || !extraTransform || !extraLight->activated)
            {
                continue;
            }

            const float extraCenterWorldX = extraTransform->x + extraTransform->width * extraTransform->scale * 0.5f;
            const float extraCenterWorldY = extraTransform->y + extraTransform->height * extraTransform->scale * 0.5f;
            const float extraOuterRadius = extraLight->radius * ctx.viewScale;
            AppendOverlayLight(
                overlayLights,
                {
                    WorldToOverlayScreenX(ctx, extraCenterWorldX),
                    WorldToOverlayScreenY(ctx, extraCenterWorldY),
                    0.0f,
                    extraOuterRadius * 0.44f,
                    extraOuterRadius,
                    0.0f,
                    0.0f,
                    extraLight->intensity,
                    1.0f,
                    1.0f,
                    1.0f },
                ctx,
                12.0f);
        }
    };

    void CollectStageLightOverlayLights(
        const std::vector<Entity*>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        for (Entity* entity : entities)
        {
            if (!entity) continue;
            const auto* stageLight = entity->GetComponent<StageLightComponent>();
            const auto* stageTransform = entity->GetComponent<TransformComponent>();
            if (!stageLight || !stageTransform || !stageLight->enabled)
            {
                continue;
            }

            const float stageCenterWorldX = stageTransform->x + stageTransform->width * stageTransform->scale * 0.5f;
            const float beamLength = stageLight->beamLength * stageTransform->scale * ctx.viewScale;
            const float beamTopWidth = std::max(stageLight->beamTopWidth, stageTransform->width) * stageTransform->scale * ctx.viewScale;
            const float beamBottomWidth = stageLight->beamBottomWidth * stageTransform->scale * ctx.viewScale;
            const float beamFeather = std::max(stageLight->beamFeather * stageTransform->scale * ctx.viewScale, 4.0f * ctx.viewScale);
            const float sourceY = WorldToOverlayScreenY(ctx, stageTransform->y + stageTransform->height * stageTransform->scale);
            AppendOverlayLight(
                overlayLights,
                {
                    WorldToOverlayScreenX(ctx, stageCenterWorldX),
                    sourceY + beamLength * 0.5f,
                    2.0f,
                    beamTopWidth * 0.5f,
                    beamFeather,
                    beamBottomWidth * 0.5f,
                    beamLength * 0.5f,
                    stageLight->intensity,
                    stageLight->r,
                    stageLight->g,
                    stageLight->b },
                ctx,
                18.0f);
        }
    }

    void CollectBatteryOverlayLights(
        const std::vector<Entity*>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        const float batteryOuterRadius = ctx.tileSize * ctx.viewScale;
        for (Entity* entity : entities)
        {
            if (!entity) continue;
            const auto* battery = entity->GetComponent<BatteryComponent>();
            const auto* batteryTransform = entity->GetComponent<TransformComponent>();
            if (!battery || !batteryTransform)
            {
                continue;
            }

            const float batteryCenterWorldX = batteryTransform->x + batteryTransform->width * batteryTransform->scale * 0.5f;
            const float batteryCenterWorldY = batteryTransform->y + batteryTransform->height * batteryTransform->scale * 0.5f;
            AppendOverlayLight(
                overlayLights,
                {
                    WorldToOverlayScreenX(ctx, batteryCenterWorldX),
                    WorldToOverlayScreenY(ctx, batteryCenterWorldY),
                    0.0f,
                    batteryOuterRadius * 0.44f,
                    batteryOuterRadius,
                    0.0f,
                    0.0f,
                    1.0f,
                    0.38f,
                    0.88f,
                    1.0f },
                ctx,
                4.0f);
        }
    }

    void CollectLaserBeamOverlayLights(
        const std::vector<Entity*>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        const float laserFeather = ctx.tileSize * ctx.viewScale * 0.6f;
        for (Entity* entity : entities)
        {
            if (!entity) continue;
            const auto* beamTransform = entity->GetComponent<TransformComponent>();
            if (!beamTransform || beamTransform->width <= 0.0f || beamTransform->height <= 0.0f)
            {
                continue;
            }

            const float beamWidth = beamTransform->width * beamTransform->scale;
            const float beamHeight = beamTransform->height * beamTransform->scale;
            const float beamShortSize = std::max(beamWidth, beamHeight) > 0.0f
                ? std::min(beamWidth, beamHeight)
                : 0.0f;
            const float beamGlowHalfWidth = std::max(beamShortSize * ctx.viewScale * 1.6f, ctx.tileSize * ctx.viewScale * 0.22f);
            const float beamCenterX = beamTransform->x + beamWidth * 0.5f;
            const float beamCenterY = beamTransform->y + beamHeight * 0.5f;
            AppendOverlayLight(
                overlayLights,
                {
                    WorldToOverlayScreenX(ctx, beamCenterX),
                    WorldToOverlayScreenY(ctx, beamCenterY),
                    1.0f,
                    0.0f,
                    laserFeather,
                    std::max(beamWidth * ctx.viewScale * 0.5f, beamGlowHalfWidth),
                    std::max(beamHeight * ctx.viewScale * 0.5f, beamGlowHalfWidth),
                    0.96f,
                    1.0f,
                    0.22f,
                    0.18f },
                ctx,
                18.0f);
        }
    }

    void CollectBlasterBulletOverlayLights(
        const std::vector<Entity*>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        const float blasterBulletOuterRadius = ctx.tileSize * ctx.viewScale * 0.9f;
        for (Entity* entity : entities)
        {
            if (!entity) continue;
            const auto* projectile = entity->GetComponent<ProjectileComponent>();
            const auto* bulletTransform = entity->GetComponent<TransformComponent>();
            if (!projectile || !bulletTransform || projectile->GetOwner() != ProjectileComponent::Owner::BlasterRobot)
            {
                continue;
            }

            const float bulletCenterWorldX = bulletTransform->x + bulletTransform->width * bulletTransform->scale * 0.5f;
            const float bulletCenterWorldY = bulletTransform->y + bulletTransform->height * bulletTransform->scale * 0.5f;
            AppendOverlayLight(
                overlayLights,
                {
                    WorldToOverlayScreenX(ctx, bulletCenterWorldX),
                    WorldToOverlayScreenY(ctx, bulletCenterWorldY),
                    0.0f,
                    blasterBulletOuterRadius * 0.22f,
                    blasterBulletOuterRadius,
                    0.0f,
                    0.0f,
                    0.92f,
                    0.34f,
                    1.0f,
                    0.66f },
                ctx,
                8.0f);
        }
    }

    void CollectDarknessOverlayLights(
        const std::vector<Entity*>& markerLightEntities,
        const std::vector<Entity*>& stageLightEntities,
        const std::vector<Entity*>& batteryEntities,
        const std::vector<Entity*>& laserBeamEntities,
        const std::vector<Entity*>& blasterBulletEntities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        AddPlayerOverlayLight(overlayLights, ctx, 74.0f * ctx.viewScale, 170.0f * ctx.viewScale);
        CollectMarkerLightOverlayLights(markerLightEntities, ctx, overlayLights);
        CollectStageLightOverlayLights(stageLightEntities, ctx, overlayLights);

        if (ctx.tileSize > 0.0f)
        {
            CollectBatteryOverlayLights(batteryEntities, ctx, overlayLights);
            CollectLaserBeamOverlayLights(laserBeamEntities, ctx, overlayLights);
            CollectBlasterBulletOverlayLights(blasterBulletEntities, ctx, overlayLights);
        }
    };

    DarknessOverlayParams BuildDarknessOverlayParams(
        const DarknessOverlayContext& ctx,
        const std::vector<OverlayLightSource>& overlayLights)
    {
        DarknessOverlayParams params;
        params.enabled = true;
        params.lightCount = static_cast<int>(overlayLights.size());
        for (int lightIndex = 0; lightIndex < params.lightCount; ++lightIndex)
        {
            params.lights[lightIndex].centerX = overlayLights[static_cast<size_t>(lightIndex)].centerX;
            params.lights[lightIndex].centerY = overlayLights[static_cast<size_t>(lightIndex)].centerY;
            params.lights[lightIndex].shapeType = overlayLights[static_cast<size_t>(lightIndex)].shapeType;
            params.lights[lightIndex].innerRadius = overlayLights[static_cast<size_t>(lightIndex)].innerRadius;
            params.lights[lightIndex].outerRadius = overlayLights[static_cast<size_t>(lightIndex)].outerRadius;
            params.lights[lightIndex].extentX = overlayLights[static_cast<size_t>(lightIndex)].extentX;
            params.lights[lightIndex].extentY = overlayLights[static_cast<size_t>(lightIndex)].extentY;
            params.lights[lightIndex].intensity = overlayLights[static_cast<size_t>(lightIndex)].intensity;
            params.lights[lightIndex].colorR = overlayLights[static_cast<size_t>(lightIndex)].colorR;
            params.lights[lightIndex].colorG = overlayLights[static_cast<size_t>(lightIndex)].colorG;
            params.lights[lightIndex].colorB = overlayLights[static_cast<size_t>(lightIndex)].colorB;
        }
        params.darknessOpacity = static_cast<float>(ctx.maxDarknessAlpha) / 255.0f;
        params.viewLeft = static_cast<float>(ctx.left);
        params.viewTop = static_cast<float>(ctx.top);
        params.viewRight = static_cast<float>(ctx.right);
        params.viewBottom = static_cast<float>(ctx.bottom);
        params.colorR = 2.0f / 255.0f;
        params.colorG = 2.0f / 255.0f;
        params.colorB = 4.0f / 255.0f;
        return params;
    }

    void DrawDarknessOverlayRect(const DarknessOverlayContext& ctx, float x0, float y0, float x1, float y1, int alpha)
    {
        const int rectLeft = static_cast<int>(std::floor((std::min)(x0, x1)));
        const int rectTop = static_cast<int>(std::floor((std::min)(y0, y1)));
        const int rectRight = static_cast<int>(std::ceil((std::max)(x0, x1)));
        const int rectBottom = static_cast<int>(std::ceil((std::max)(y0, y1)));
        if (rectRight <= rectLeft || rectBottom <= rectTop || alpha <= 0)
        {
            return;
        }

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(alpha, 0, 255));
        DrawBox(rectLeft, rectTop, rectRight, rectBottom, GetColor(2, 2, 4), TRUE);
        static_cast<void>(ctx);
    }

    void DrawDarknessOverlaySingleCircleFallback(
        const DarknessOverlayContext& ctx,
        const OverlayLightSource& light,
        int maxDarknessAlpha)
    {
        const float lightCenterX = light.centerX;
        const float lightCenterY = light.centerY;
        const float innerRadius = light.innerRadius;
        const float outerRadius = light.outerRadius;
        const float outerRadiusSq = outerRadius * outerRadius;
        const float innerRadiusSq = innerRadius * innerRadius;
        constexpr int kStripeHeight = 2;
        constexpr int kSoftBandSegments = 18;

        DrawDarknessOverlayRect(ctx, static_cast<float>(ctx.left), static_cast<float>(ctx.top), static_cast<float>(ctx.right), lightCenterY - outerRadius, maxDarknessAlpha);
        DrawDarknessOverlayRect(ctx, static_cast<float>(ctx.left), lightCenterY + outerRadius, static_cast<float>(ctx.right), static_cast<float>(ctx.bottom), maxDarknessAlpha);

        const int bandStartY = (std::max)(ctx.top, static_cast<int>(std::floor(lightCenterY - outerRadius)));
        const int bandEndY = (std::min)(ctx.bottom, static_cast<int>(std::ceil(lightCenterY + outerRadius)));
        for (int bandTop = bandStartY; bandTop < bandEndY; bandTop += kStripeHeight)
        {
            const int bandBottom = (std::min)(bandEndY, bandTop + kStripeHeight);
            const float bandCenterY = (static_cast<float>(bandTop) + static_cast<float>(bandBottom)) * 0.5f;
            const float dy = std::fabs(bandCenterY - lightCenterY);
            if (dy >= outerRadius)
            {
                DrawDarknessOverlayRect(ctx, static_cast<float>(ctx.left), static_cast<float>(bandTop), static_cast<float>(ctx.right), static_cast<float>(bandBottom), maxDarknessAlpha);
                continue;
            }

            const float outerDx = std::sqrt((std::max)(0.0f, outerRadiusSq - dy * dy));
            const float innerDx = dy < innerRadius
                ? std::sqrt((std::max)(0.0f, innerRadiusSq - dy * dy))
                : 0.0f;

            DrawDarknessOverlayRect(ctx, static_cast<float>(ctx.left), static_cast<float>(bandTop), lightCenterX - outerDx, static_cast<float>(bandBottom), maxDarknessAlpha);
            DrawDarknessOverlayRect(ctx, lightCenterX + outerDx, static_cast<float>(bandTop), static_cast<float>(ctx.right), static_cast<float>(bandBottom), maxDarknessAlpha);

            const float softWidth = (std::max)(0.0f, outerDx - innerDx);
            if (softWidth <= 0.5f)
            {
                continue;
            }

            for (int segmentIndex = 0; segmentIndex < kSoftBandSegments; ++segmentIndex)
            {
                const float t0 = static_cast<float>(segmentIndex) / static_cast<float>(kSoftBandSegments);
                const float t1 = static_cast<float>(segmentIndex + 1) / static_cast<float>(kSoftBandSegments);
                const float dx0 = innerDx + softWidth * t0;
                const float dx1 = innerDx + softWidth * t1;
                const float dxMid = (dx0 + dx1) * 0.5f;
                const float radiusAtMid = std::sqrt(dxMid * dxMid + dy * dy);
                const float normalized = Clamp01((radiusAtMid - innerRadius) / (outerRadius - innerRadius));
                const float eased = normalized * normalized * (3.0f - 2.0f * normalized);
                const float edgeWeighted = eased * eased;
                const int alpha = static_cast<int>(std::round(edgeWeighted * static_cast<float>(maxDarknessAlpha)));
                if (alpha <= 0)
                {
                    continue;
                }

                DrawDarknessOverlayRect(ctx, lightCenterX - dx1, static_cast<float>(bandTop), lightCenterX - dx0, static_cast<float>(bandBottom), alpha);
                DrawDarknessOverlayRect(ctx, lightCenterX + dx0, static_cast<float>(bandTop), lightCenterX + dx1, static_cast<float>(bandBottom), alpha);
            }
        }
    }

    void DrawDarknessOverlayFallback(
        const DarknessOverlayContext& ctx,
        const std::vector<OverlayLightSource>& overlayLights,
        int maxDarknessAlpha)
    {
        if (overlayLights.size() == 1 && overlayLights[0].shapeType < 0.5f)
        {
            DrawDarknessOverlaySingleCircleFallback(ctx, overlayLights[0], maxDarknessAlpha);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            return;
        }

        constexpr int kStripeHeight = 2;
        constexpr int kSoftBandSegments = 18;
        std::vector<float> xBreaks;
        xBreaks.reserve(2 + overlayLights.size() * (4 + kSoftBandSegments * 2));

        for (int bandTop = ctx.top; bandTop < ctx.bottom; bandTop += kStripeHeight)
        {
            const int bandBottom = (std::min)(ctx.bottom, bandTop + kStripeHeight);
            const float bandCenterY = (static_cast<float>(bandTop) + static_cast<float>(bandBottom)) * 0.5f;
            xBreaks.clear();
            xBreaks.push_back(static_cast<float>(ctx.left));
            xBreaks.push_back(static_cast<float>(ctx.right));

            for (const OverlayLightSource& light : overlayLights)
            {
                if (light.shapeType >= 1.5f)
                {
                    const float halfLength = std::max(0.001f, light.extentY);
                    const float feather = light.outerRadius;
                    if (std::fabs(bandCenterY - light.centerY) > halfLength + feather)
                    {
                        continue;
                    }

                    const float topY = light.centerY - halfLength;
                    const float normalizedY = Clamp01((bandCenterY - topY) / (halfLength * 2.0f));
                    const float halfWidth = std::lerp(light.innerRadius, light.extentX, SmoothStep01(normalizedY));
                    xBreaks.push_back(light.centerX - halfWidth - feather);
                    xBreaks.push_back(light.centerX - halfWidth);
                    xBreaks.push_back(light.centerX + halfWidth);
                    xBreaks.push_back(light.centerX + halfWidth + feather);
                }
                else if (light.shapeType >= 0.5f)
                {
                    const float halfHeight = light.extentY;
                    const float feather = light.outerRadius;
                    if (std::fabs(bandCenterY - light.centerY) > halfHeight + feather)
                    {
                        continue;
                    }

                    xBreaks.push_back(light.centerX - light.extentX - feather);
                    xBreaks.push_back(light.centerX - light.extentX);
                    xBreaks.push_back(light.centerX + light.extentX);
                    xBreaks.push_back(light.centerX + light.extentX + feather);
                }
                else
                {
                    const float dy = std::fabs(bandCenterY - light.centerY);
                    if (dy >= light.outerRadius)
                    {
                        continue;
                    }

                    const float outerDx = std::sqrt((std::max)(0.0f, light.outerRadius * light.outerRadius - dy * dy));
                    xBreaks.push_back(light.centerX - outerDx);
                    xBreaks.push_back(light.centerX + outerDx);

                    if (dy < light.innerRadius)
                    {
                        const float innerDx = std::sqrt((std::max)(0.0f, light.innerRadius * light.innerRadius - dy * dy));
                        xBreaks.push_back(light.centerX - innerDx);
                        xBreaks.push_back(light.centerX + innerDx);

                        const float softWidth = (std::max)(0.0f, outerDx - innerDx);
                        if (softWidth > 0.5f)
                        {
                            for (int segmentIndex = 1; segmentIndex < kSoftBandSegments; ++segmentIndex)
                            {
                                const float t = static_cast<float>(segmentIndex) / static_cast<float>(kSoftBandSegments);
                                const float dx = innerDx + softWidth * t;
                                xBreaks.push_back(light.centerX - dx);
                                xBreaks.push_back(light.centerX + dx);
                            }
                        }
                    }
                    else
                    {
                        for (int segmentIndex = 1; segmentIndex < kSoftBandSegments; ++segmentIndex)
                        {
                            const float t = static_cast<float>(segmentIndex) / static_cast<float>(kSoftBandSegments);
                            const float dx = outerDx * t;
                            xBreaks.push_back(light.centerX - dx);
                            xBreaks.push_back(light.centerX + dx);
                        }
                    }
                }
            }

            std::sort(xBreaks.begin(), xBreaks.end());
            xBreaks.erase(
                std::unique(
                    xBreaks.begin(),
                    xBreaks.end(),
                    [](float a, float b)
                    {
                        return std::fabs(a - b) <= 0.5f;
                    }),
                xBreaks.end());

            for (size_t index = 1; index < xBreaks.size(); ++index)
            {
                const float x0 = xBreaks[index - 1];
                const float x1 = xBreaks[index];
                if (x1 - x0 <= 0.5f)
                {
                    continue;
                }

                const float sampleX = (x0 + x1) * 0.5f;
                int alpha = maxDarknessAlpha;
                for (const OverlayLightSource& light : overlayLights)
                {
                    if (light.shapeType >= 1.5f)
                    {
                        const float halfLength = std::max(0.001f, light.extentY);
                        const float feather = std::max(0.001f, light.outerRadius);
                        const float topY = light.centerY - halfLength;
                        const float bottomY = light.centerY + halfLength;
                        const float normalizedY = Clamp01((bandCenterY - topY) / (halfLength * 2.0f));
                        const float halfWidth = std::lerp(light.innerRadius, light.extentX, SmoothStep01(normalizedY));
                        const float dx = std::fabs(sampleX - light.centerX) - halfWidth;
                        const float dyTop = topY - bandCenterY;
                        const float dyBottom = bandCenterY - bottomY;
                        const float outsideX = std::max(dx, 0.0f);
                        const float outsideY = std::max(std::max(dyTop, dyBottom), 0.0f);
                        const float outsideDistance = std::sqrt(outsideX * outsideX + outsideY * outsideY);
                        if (outsideDistance >= feather)
                        {
                            continue;
                        }
                        if (outsideDistance <= 0.0f)
                        {
                            alpha = 0;
                            break;
                        }

                        const float normalized = Clamp01(outsideDistance / feather);
                        const float eased = normalized * normalized * (3.0f - 2.0f * normalized);
                        const float edgeWeighted = eased * eased;
                        const int candidateAlpha = static_cast<int>(std::round(edgeWeighted * static_cast<float>(maxDarknessAlpha) / std::max(0.001f, light.intensity)));
                        alpha = (std::min)(alpha, std::clamp(candidateAlpha, 0, maxDarknessAlpha));
                    }
                    else if (light.shapeType >= 0.5f)
                    {
                        const float dx = std::fabs(sampleX - light.centerX) - light.extentX;
                        const float dy = std::fabs(bandCenterY - light.centerY) - light.extentY;
                        const float outsideX = (std::max)(dx, 0.0f);
                        const float outsideY = (std::max)(dy, 0.0f);
                        const float outsideDistance = std::sqrt(outsideX * outsideX + outsideY * outsideY);
                        const float insideDistance = (std::min)((std::max)(dx, dy), 0.0f);
                        const float signedDistance = outsideDistance + insideDistance;
                        if (signedDistance >= light.outerRadius)
                        {
                            continue;
                        }
                        if (signedDistance <= 0.0f)
                        {
                            alpha = 0;
                            break;
                        }

                        const float normalized = Clamp01(signedDistance / light.outerRadius);
                        const float eased = normalized * normalized * (3.0f - 2.0f * normalized);
                        const float edgeWeighted = eased * eased;
                        const int candidateAlpha = static_cast<int>(std::round(edgeWeighted * static_cast<float>(maxDarknessAlpha) / std::max(0.001f, light.intensity)));
                        alpha = (std::min)(alpha, std::clamp(candidateAlpha, 0, maxDarknessAlpha));
                    }
                    else
                    {
                        const float dx = sampleX - light.centerX;
                        const float dy = bandCenterY - light.centerY;
                        const float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance >= light.outerRadius)
                        {
                            continue;
                        }
                        if (distance <= light.innerRadius)
                        {
                            alpha = 0;
                            break;
                        }

                        const float normalized = Clamp01((distance - light.innerRadius) / (light.outerRadius - light.innerRadius));
                        const float eased = normalized * normalized * (3.0f - 2.0f * normalized);
                        const float edgeWeighted = eased * eased;
                        const int candidateAlpha = static_cast<int>(std::round(edgeWeighted * static_cast<float>(maxDarknessAlpha) / std::max(0.001f, light.intensity)));
                        alpha = (std::min)(alpha, std::clamp(candidateAlpha, 0, maxDarknessAlpha));
                    }
                }

                DrawDarknessOverlayRect(ctx, x0, static_cast<float>(bandTop), x1, static_cast<float>(bandBottom), alpha);
            }
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void RenderDarknessOverlayViaDirectX(
        const DarknessOverlayContext& ctx,
        const std::vector<OverlayLightSource>& overlayLights)
    {
        const DarknessOverlayParams params = BuildDarknessOverlayParams(ctx, overlayLights);
        DirectXSetDarknessOverlay(params);
        DirectXDrawDarknessOverlay();
    }

    unsigned int BuildSepiaNoiseSeed(int x, int y, int frame)
    {
        unsigned int value = static_cast<unsigned int>(x) * 1973u;
        value ^= static_cast<unsigned int>(y) * 9277u;
        value ^= static_cast<unsigned int>(frame) * 26699u;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        return value;
    }

    void DrawSepiaFilmNoise(int left, int top, int right, int bottom, int frame)
    {
        constexpr int kNoiseCell = 4;
        if (right <= left || bottom <= top)
        {
            return;
        }

        // Sparse black dots mimic coarse film grain without per-pixel work.
        for (int y = top; y < bottom; y += kNoiseCell)
        {
            for (int x = left; x < right; x += kNoiseCell)
            {
                const unsigned int seed = BuildSepiaNoiseSeed(x / kNoiseCell, y / kNoiseCell, frame);
                const int grain = static_cast<int>(seed & 31u);
                if (grain < 15)
                {
                    continue;
                }

                const int alpha = 9 + grain;
                const int dotSize = ((seed >> 6) & 3u) == 0u ? 2 : 1;
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
                DrawBox(
                    x,
                    y,
                    std::min(x + dotSize, right),
                    std::min(y + dotSize, bottom),
                    GetColor(0, 0, 0),
                    TRUE);
            }
        }
    }

    void DrawSepiaFilmScratches(int left, int top, int right, int bottom, int frame)
    {
        const int width = right - left;
        if (width <= 0 || bottom <= top)
        {
            return;
        }

        // Long black vertical scratches sell the worn film look.
        constexpr int kScratchCount = 7;
        for (int index = 0; index < kScratchCount; ++index)
        {
            const unsigned int seed = BuildSepiaNoiseSeed(index * 23, frame / 2, 17);
            const int x = left + static_cast<int>(seed % static_cast<unsigned int>(width));
            const int y0 = top + static_cast<int>((seed >> 8) % 34u) - 18;
            const int length = 70 + static_cast<int>((seed >> 14) % 220u);
            const int alpha = 18 + static_cast<int>((seed >> 22) % 34u);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            DrawLine(x, y0, x + static_cast<int>((seed >> 5) % 3u) - 1, std::min(y0 + length, bottom), GetColor(0, 0, 0), 1);
        }
    }

    void DrawSepiaFilmDust(int left, int top, int right, int bottom, int frame)
    {
        const int width = right - left;
        const int height = bottom - top;
        if (width <= 0 || height <= 0)
        {
            return;
        }

        // A few larger dark stains create the faded, dirty film overlay.
        constexpr int kDustCount = 16;
        for (int index = 0; index < kDustCount; ++index)
        {
            const unsigned int seed = BuildSepiaNoiseSeed(index * 41, frame / 3, 73);
            const int x = left + static_cast<int>(seed % static_cast<unsigned int>(width));
            const int y = top + static_cast<int>((seed >> 9) % static_cast<unsigned int>(height));
            const int radius = 1 + static_cast<int>((seed >> 18) % 3u);
            const int alpha = 10 + static_cast<int>((seed >> 24) % 22u);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            DrawCircle(x, y, radius, GetColor(0, 0, 0), TRUE);
        }
    }

    void DrawSepiaFilmUnevenFade(int left, int top, int right, int bottom, int frame)
    {
        const int height = bottom - top;
        if (right <= left || height <= 0)
        {
            return;
        }

        // Wide translucent bands make the finder feel bleached and unstable.
        constexpr int kBandCount = 4;
        for (int index = 0; index < kBandCount; ++index)
        {
            const unsigned int seed = BuildSepiaNoiseSeed(index * 13, frame / 4, 109);
            const int bandTop = top + static_cast<int>(seed % static_cast<unsigned int>(height));
            const int bandHeight = 18 + static_cast<int>((seed >> 11) % 56u);
            const int alpha = 10 + static_cast<int>((seed >> 21) % 18u);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            DrawBox(left, bandTop, right, std::min(bandTop + bandHeight, bottom), GetColor(238, 202, 142), TRUE);
        }
    }

    float GetTuningRowY(int index)
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
        return y + static_cast<float>(index) * kTuningRowHeight;
    }

    void DrawTuningButton(float x, float y, float width, float height, const char* label, bool highlighted)
    {
        DrawBox(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(x + width)),
            static_cast<int>(std::round(y + height)),
            highlighted ? GetColor(228, 196, 120) : GetColor(48, 60, 78),
            TRUE);
        DrawBox(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(x + width)),
            static_cast<int>(std::round(y + height)),
            GetColor(220, 230, 244),
            FALSE);
        DrawString(
            static_cast<int>(std::round(x + 18.0f)),
            static_cast<int>(std::round(y + 2.0f)),
            label,
            highlighted ? GetColor(18, 18, 22) : GetColor(232, 238, 245));
    }

    float SmoothStep01(float t)
    {
        const float x = Clamp01(t);
        return x * x * (3.0f - 2.0f * x);
    }
}

void GameScene::DrawStageDarknessOverlay() const
{
    if (!m_lifecycle.darknessStageEnabled)
    {
        return;
    }

    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float viewHeight = GetViewHeight();
    const int left = static_cast<int>(std::floor(viewOriginX));
    const int top = static_cast<int>(std::floor(viewOriginY));
    const int right = static_cast<int>(std::ceil(viewOriginX + viewWidth));
    const int bottom = static_cast<int>(std::ceil(viewOriginY + viewHeight));
    constexpr int kBaseDarknessAlpha = 252;

    const Entity* player = FindEntityByTag(kTagPlayer);
    const auto* transform = player ? player->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
    {
        const DarknessOverlayContext noPlayerCtx{
            viewOriginX,
            viewOriginY,
            m_flow.cameraX,
            m_flow.cameraY,
            GetViewScale(),
            0.0f,
            0.0f,
            left,
            top,
            right,
            bottom,
            kBaseDarknessAlpha,
            m_tileMap.GetTileSize() };
        DrawDarknessOverlayRect(
            noPlayerCtx,
            static_cast<float>(left),
            static_cast<float>(top),
            static_cast<float>(right),
            static_cast<float>(bottom),
            kBaseDarknessAlpha);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        return;
    }

    const float viewScale = GetViewScale();
    int maxDarknessAlpha = kBaseDarknessAlpha;

    if (m_ui.cameraFlash.enabled && m_ui.cameraFlash.pulseRemaining > 0.0f && m_ui.cameraFlash.pulseDuration > 0.0f)
    {
        const float flashT = Clamp01(m_ui.cameraFlash.pulseRemaining / m_ui.cameraFlash.pulseDuration);
        const float flashEase = flashT * flashT * (3.0f - 2.0f * flashT);
        maxDarknessAlpha = static_cast<int>(std::round(std::lerp(228.0f, 160.0f, flashEase)));
    }

    DarknessOverlayContext ctx;
    ctx.viewOriginX = viewOriginX;
    ctx.viewOriginY = viewOriginY;
    ctx.cameraX = m_flow.cameraX;
    ctx.cameraY = m_flow.cameraY;
    ctx.viewScale = viewScale;
    ctx.playerLightScreenX = viewOriginX + (transform->x + transform->width * transform->scale * 0.5f - m_flow.cameraX) * viewScale;
    ctx.playerLightScreenY = viewOriginY + (transform->y + transform->height * transform->scale * 0.5f - m_flow.cameraY) * viewScale - 16.0f * viewScale;
    ctx.left = left;
    ctx.top = top;
    ctx.right = right;
    ctx.bottom = bottom;
    ctx.maxDarknessAlpha = maxDarknessAlpha;
    ctx.tileSize = m_tileMap.GetTileSize();

    std::vector<OverlayLightSource> overlayLights;
    overlayLights.reserve(kMaxDarknessOverlayLights * 2);
    CollectDarknessOverlayLights(
        m_world.EntitiesByTag(EntityTag::MarkerLight),
        m_world.EntitiesByTag(EntityTag::StageLight),
        m_world.EntitiesByTag(EntityTag::Battery),
        m_world.EntitiesByTag(EntityTag::LaserBeam),
        m_world.EntitiesByTag(EntityTag::Bullet),
        ctx,
        overlayLights);

    const int renderedLightLimit = (std::min)(kMaxDarknessOverlayLights, kDarknessOverlayActiveLightLimit);
    if (overlayLights.size() > static_cast<size_t>(renderedLightLimit))
    {
        const auto priorityCompare = [](const OverlayLightSource& a, const OverlayLightSource& b)
        {
            return a.priority > b.priority;
        };
        std::nth_element(
            overlayLights.begin(),
            overlayLights.begin() + renderedLightLimit,
            overlayLights.end(),
            priorityCompare);
        overlayLights.resize(renderedLightLimit);
        std::sort(overlayLights.begin(), overlayLights.end(), priorityCompare);
    }

    if (DirectXHasDarknessOverlay())
    {
        RenderDarknessOverlayViaDirectX(ctx, overlayLights);
        return;
    }

    DrawDarknessOverlayFallback(ctx, overlayLights, maxDarknessAlpha);
}

void GameScene::DrawSepiaFilmFilterOverlay() const
{
    if (m_photo.placement.active)
    {
        return;
    }

    const bool enabled =
        m_debug.sepiaFilmFilterDryRunEnabled ||
        m_photo.capture.selectedTheme == PhotoFilterTheme::Sepia;
    if (!enabled)
    {
        return;
    }

    const Entity* player = FindEntityByTag(kTagPlayer);
    const auto* transform = player ? player->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
    {
        return;
    }

    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(*transform, frameX, frameY, frameWidth, frameHeight);

    const float viewScale = GetViewScale();
    const float finderUiScale = GetCaptureFinderScreenScale(m_tileMap.GetTileSize());
    const float drawWidth = gCaptureFrameWidthPx * m_ui.captureFinderScale * finderUiScale;
    const float drawHeight = gCaptureFrameHeightPx * m_ui.captureFinderScale * finderUiScale;
    const float drawX = static_cast<float>(Input_GetMouseX()) - drawWidth * 0.5f;
    const float drawY = static_cast<float>(Input_GetMouseY()) - drawHeight * 0.5f;
    const int left = static_cast<int>(std::round(drawX));
    const int top = static_cast<int>(std::round(drawY));
    const int right = static_cast<int>(std::round(drawX + drawWidth));
    const int bottom = static_cast<int>(std::round(drawY + drawHeight));
    if (right <= left || bottom <= top)
    {
        return;
    }

    const int frame = GetNowCount() / 33;
    const int effectLeft = left;
    const int effectTop = top;
    const int effectRight = right;
    const int effectBottom = bottom;
    if (effectRight <= effectLeft || effectBottom <= effectTop)
    {
        return;
    }

    // Fill under the finder frame so no gap appears, without drawing outside it.
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 44);
    DrawBox(effectLeft, effectTop, effectRight, effectBottom, GetColor(205, 178, 68), TRUE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 82);
    DrawBox(effectLeft, effectTop, effectRight, effectBottom, GetColor(255, 252, 220), TRUE);

    DrawSepiaFilmUnevenFade(effectLeft, effectTop, effectRight, effectBottom, frame);
    DrawSepiaFilmNoise(effectLeft, effectTop, effectRight, effectBottom, frame);
    DrawSepiaFilmDust(effectLeft, effectTop, effectRight, effectBottom, frame);
    DrawSepiaFilmScratches(effectLeft, effectTop, effectRight, effectBottom, frame);
    // フレーム内の瓦礫を足場テクスチャでプレビュー描画
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::SepiaRubble))
    {
        if (!entity)
        {
            continue;
        }
        
        if (const auto* group = entity->GetComponent<SepiaRubbleGroupComponent>())
        {
            if (group->isRestored)
            {
                continue;
            }
        }
		const auto* t = entity->GetComponent<TransformComponent>();
        if (!t)
        {
            continue;
        }
        const float overlapLeft = std::max(frameX, t->x);
        const float overlapTop = std::max(frameY, t->y);
        const float overlapRight = std::min(frameX + frameWidth, t->x + t->width * t->scale);
        const float overlapBot = std::min(frameY + frameHeight, t->y + t->height * t->scale);
        if (overlapRight - overlapLeft <= 1.0f || overlapBot - overlapTop <= 1.0f)
        {
            continue;
        }
        // フレームと重なった瓦礫部分のみを描画
		const float objectWorldX = t->width * t->scale;
		const float objectWorldY = t->height * t->scale;
        if (objectWorldX <= 0.0f || objectWorldY <= 0.0f)
        {
            continue;
        }
        const float overlapWidth = overlapRight - overlapLeft;
        const float overlapHeight = overlapBot - overlapTop;
        const float drawEntityX = GetViewOriginX() + (overlapLeft - m_flow.cameraX) * viewScale;
        const float drawEntityY = GetViewOriginY() + (overlapTop - m_flow.cameraY) * viewScale;
        const float drawEntityW = overlapWidth * viewScale;
        const float drawEntityH = overlapHeight * viewScale;
        const float sourceX = std::clamp((overlapLeft - t->x) / objectWorldX, 0.0f, 1.0f);
        const float sourceY = std::clamp((overlapTop - t->y) / objectWorldY, 0.0f, 1.0f);
        const float sourceW = std::clamp(overlapWidth / objectWorldX, 0.0f, 1.0f - sourceX);
        const float sourceH = std::clamp(overlapHeight / objectWorldY, 0.0f, 1.0f - sourceY);
        const auto* rubble = entity->GetComponent<SepiaRubbleComponent>();
        const bool attackRubble =
            rubble &&
            (rubble->source == SepiaRubbleSource::MidBoss3Fist ||
                rubble->source == SepiaRubbleSource::MidBoss3Drill);
        Shader_ResetStyle();
        if (attackRubble)
        {
            Shader_SetTint(0.96f, 0.52f, 0.18f, 0.95f);
        }
        else
        {
            Shader_SetTint(1.0f, 1.0f, 1.0f, 0.9f);
        }
        SpriteDraw(
            attackRubble ? m_whiteTexture : m_assets.GetTexture("sepia_ground"),
            drawEntityX, drawEntityY,
            drawEntityW, drawEntityH,
            attackRubble ? 0.0f : sourceX,
            attackRubble ? 0.0f : sourceY,
            attackRubble ? 1.0f : sourceW,
            attackRubble ? 1.0f : sourceH);

    }
    Shader_ResetStyle();
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void GameScene::DrawShieldBossSlamVignetteOverlay() const
{
    float strength = 0.0f;
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        const auto* boss = entity->GetComponent<ShieldBossComponent>();
        if (!enemy ||
            enemy->GetArchetype() != EnemyArchetype::ShieldBoss ||
            !enemy->IsEnabled() ||
            !boss ||
            !boss->shieldEntity)
        {
            continue;
        }

        const auto* shieldTint = boss->shieldEntity->GetComponent<TintComponent>();
        const float shieldAlpha = shieldTint ? shieldTint->a : 1.0f;
        if (shieldAlpha <= 0.05f)
        {
            continue;
        }

        float stateStrength = 0.0f;
        switch (boss->state)
        {
        case ShieldBossState::JumpAscend:
            stateStrength = 0.50f + 0.25f * Clamp01(boss->stateTimer / 0.28f);
            break;
        case ShieldBossState::AirHover:
            stateStrength = 0.80f + 0.08f * std::sin(boss->stateTimer * 18.0f);
            break;
        case ShieldBossState::JumpDescend:
            stateStrength = 0.70f;
            break;
        default:
            break;
        }

        strength = std::max(strength, stateStrength * Clamp01(shieldAlpha));
    }

    if (strength <= 0.01f)
    {
        return;
    }

    const float screenW = static_cast<float>(SCREEN_WIDTH);
    const float screenH = static_cast<float>(SCREEN_HEIGHT);
    const float shortSide = std::min(screenW, screenH);
    const int outerColor = GetColor(8, 10, 14);
    const int innerColor = GetColor(30, 24, 20);

    const auto drawBand = [](float x, float y, float width, float height, int alpha, int color)
    {
        if (alpha <= 0 || width <= 0.0f || height <= 0.0f)
        {
            return;
        }
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(alpha, 0, 255));
        DrawBoxAA(x, y, x + width, y + height, color, TRUE);
    };

    const float edge0 = shortSide * 0.055f;
    const float edge1 = shortSide * 0.115f;
    const float edge2 = shortSide * 0.205f;
    const int outerAlpha = static_cast<int>(std::round(118.0f * strength));
    const int midAlpha = static_cast<int>(std::round(72.0f * strength));
    const int innerAlpha = static_cast<int>(std::round(34.0f * strength));

    drawBand(0.0f, 0.0f, screenW, edge2, innerAlpha, innerColor);
    drawBand(0.0f, screenH - edge2, screenW, edge2, innerAlpha, innerColor);
    drawBand(0.0f, 0.0f, edge2, screenH, innerAlpha, innerColor);
    drawBand(screenW - edge2, 0.0f, edge2, screenH, innerAlpha, innerColor);

    drawBand(0.0f, 0.0f, screenW, edge1, midAlpha, outerColor);
    drawBand(0.0f, screenH - edge1, screenW, edge1, midAlpha, outerColor);
    drawBand(0.0f, 0.0f, edge1, screenH, midAlpha, outerColor);
    drawBand(screenW - edge1, 0.0f, edge1, screenH, midAlpha, outerColor);

    drawBand(0.0f, 0.0f, screenW, edge0, outerAlpha, outerColor);
    drawBand(0.0f, screenH - edge0, screenW, edge0, outerAlpha, outerColor);
    drawBand(0.0f, 0.0f, edge0, screenH, outerAlpha, outerColor);
    drawBand(screenW - edge0, 0.0f, edge0, screenH, outerAlpha, outerColor);

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void GameScene::DrawShieldBossIntroCurtainOverlay() const
{
    const float progress = Clamp01(m_render.shieldBossIntroCurtainProgress);
    if (progress <= 0.001f)
    {
        return;
    }

    // 登場演出中だけ、上下から1グリッド程度の黒幕を滑らかに出し入れする。
    const float easedProgress = progress * progress * (3.0f - 2.0f * progress);
    const float tileSize = std::max(1.0f, m_tileMap.GetTileSize());
    const int maxCurtainHeight = std::clamp(
        static_cast<int>(std::round(tileSize * GetViewScale())),
        32,
        120);
    const int curtainHeight = static_cast<int>(std::round(static_cast<float>(maxCurtainHeight) * easedProgress));
    if (curtainHeight <= 0)
    {
        return;
    }
    const int curtainColor = GetColor(0, 0, 0);

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(0, 0, SCREEN_WIDTH, curtainHeight, curtainColor, TRUE);
    DrawBox(0, SCREEN_HEIGHT - curtainHeight, SCREEN_WIDTH, SCREEN_HEIGHT, curtainColor, TRUE);
}

void GameScene::DrawMarkerLightOutlines() const
{
    if (!m_lifecycle.darknessStageEnabled)
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const unsigned int outlineColor = GetColor(248, 248, 252);

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::MarkerLight))
    {
        if (!entity) continue;
        const auto* markerLight = entity->GetComponent<MarkerLightComponent>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!markerLight || !transform || markerLight->activated)
        {
            continue;
        }

        const float drawX = viewOriginX + (transform->x - m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + (transform->y - m_flow.cameraY) * viewScale;
        const float drawWidth = transform->width * transform->scale * viewScale;
        const float drawHeight = transform->height * transform->scale * viewScale;
        const float centerX = drawX + drawWidth * 0.5f;
        const float centerY = drawY + drawHeight * 0.5f;

        float topLeftX = drawX;
        float topLeftY = drawY;
        float topRightX = drawX + drawWidth;
        float topRightY = drawY;
        float bottomRightX = drawX + drawWidth;
        float bottomRightY = drawY + drawHeight;
        float bottomLeftX = drawX;
        float bottomLeftY = drawY + drawHeight;
        RotatePoint(centerX, centerY, transform->rotation, topLeftX, topLeftY);
        RotatePoint(centerX, centerY, transform->rotation, topRightX, topRightY);
        RotatePoint(centerX, centerY, transform->rotation, bottomRightX, bottomRightY);
        RotatePoint(centerX, centerY, transform->rotation, bottomLeftX, bottomLeftY);

        DrawLine(
            static_cast<int>(std::round(topLeftX)),
            static_cast<int>(std::round(topLeftY)),
            static_cast<int>(std::round(topRightX)),
            static_cast<int>(std::round(topRightY)),
            outlineColor);
        DrawLine(
            static_cast<int>(std::round(topRightX)),
            static_cast<int>(std::round(topRightY)),
            static_cast<int>(std::round(bottomRightX)),
            static_cast<int>(std::round(bottomRightY)),
            outlineColor);
        DrawLine(
            static_cast<int>(std::round(bottomRightX)),
            static_cast<int>(std::round(bottomRightY)),
            static_cast<int>(std::round(bottomLeftX)),
            static_cast<int>(std::round(bottomLeftY)),
            outlineColor);
        DrawLine(
            static_cast<int>(std::round(bottomLeftX)),
            static_cast<int>(std::round(bottomLeftY)),
            static_cast<int>(std::round(topLeftX)),
            static_cast<int>(std::round(topLeftY)),
            outlineColor);
    }
}

void GameScene::DrawPitRestartOverlay() const
{
    const bool hasPitFade = m_flow.pitRestartActive || m_flow.pitRestartFadeInTimer > 0.0f;
    const bool hasStageTransitionFade = m_flow.stageTransitionActive || m_flow.stageTransitionFadeInTimer > 0.0f;
    if (!hasPitFade && !hasStageTransitionFade)
    {
        return;
    }

    float pitAlpha = 0.0f;
    if (m_flow.pitRestartActive)
    {
        const float progress = Clamp01(1.0f - (m_flow.pitRestartTimer / kPitRestartFadeDuration));
        pitAlpha = 0.35f + progress * 0.65f; // Fade-out to black before respawn.
    }
    else if (m_flow.pitRestartFadeInTimer > 0.0f)
    {
        const float progress = Clamp01(m_flow.pitRestartFadeInTimer / kPitRestartFadeDuration);
        pitAlpha = progress; // Fade-in from black after respawn.
    }

    float stageTransitionAlpha = 0.0f;
    if (m_flow.stageTransitionActive)
    {
        const float progress = Clamp01(1.0f - (m_flow.stageTransitionTimer / kStageTransitionFadeOutDuration));
        stageTransitionAlpha = 0.35f + progress * 0.65f;
    }
    else if (m_flow.stageTransitionFadeInTimer > 0.0f)
    {
        const float elapsed = Clamp01(1.0f - (m_flow.stageTransitionFadeInTimer / kStageTransitionFadeInDuration));
        stageTransitionAlpha = 1.0f - SmoothStep01(elapsed);
    }

    const float alpha = std::max(pitAlpha, stageTransitionAlpha);

    Shader_ResetStyle();
    Shader_SetTint(0.01f, 0.01f, 0.02f, alpha);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_ResetStyle();
}

void GameScene::DrawCaptureOverlay() const
{
    if (m_photo.placement.active)
    {
        return;
    }

    const Entity* player = FindEntityByTag(kTagPlayer);
    if (!player)
    {
        return;
    }

    const auto* transform = player->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(*transform, frameX, frameY, frameWidth, frameHeight);

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float viewHeight = GetViewHeight();
    const float overlayLeft = 0.0f;
    const float overlayTop = 0.0f;
    const float overlayWidth = static_cast<float>(SCREEN_WIDTH);
    const float overlayHeight = static_cast<float>(SCREEN_HEIGHT);
    // ファインダー本体はカメラから分離し、常にマウス中心のスクリーンUIとして描画する。
    const float finderUiScale = GetCaptureFinderScreenScale(m_tileMap.GetTileSize());
    const float drawWidth = gCaptureFrameWidthPx * m_ui.captureFinderScale * finderUiScale;
    const float drawHeight = gCaptureFrameHeightPx * m_ui.captureFinderScale * finderUiScale;
    const float drawX = static_cast<float>(Input_GetMouseX()) - drawWidth * 0.5f;
    const float drawY = static_cast<float>(Input_GetMouseY()) - drawHeight * 0.5f;
    const int left = static_cast<int>(std::round(drawX));
    const int top = static_cast<int>(std::round(drawY));
    const int right = static_cast<int>(std::round(drawX + drawWidth));
    const int bottom = static_cast<int>(std::round(drawY + drawHeight));

    const auto& captureUi = m_ui.tuning.captureOverlay;
    const float shutterT = Clamp01(m_ui.shutterFlashRemaining / gShutterFlashSeconds);
    const float frameInset = captureUi.frameInset * shutterT * finderUiScale;
    const float innerX = drawX + frameInset;
    const float innerY = drawY + frameInset;
    const float innerWidth = std::max(8.0f, drawWidth - frameInset * 2.0f);
    const float innerHeight = std::max(8.0f, drawHeight - frameInset * 2.0f);
    float overlayR = 1.0f;
    float overlayG = 1.0f;
    float overlayB = 1.0f;
    GetPhotoFilterThemeOverlayColor(m_photo.capture.selectedTheme, overlayR, overlayG, overlayB);
    const unsigned int frameColor = GetColor(
        static_cast<int>(std::round(overlayR * 255.0f)),
        static_cast<int>(std::round(overlayG * 255.0f)),
        static_cast<int>(std::round(overlayB * 255.0f)));
    const unsigned int guideColor = GetColor(
        static_cast<int>(std::round(overlayR * 200.0f)),
        static_cast<int>(std::round(overlayG * 200.0f)),
        static_cast<int>(std::round(overlayB * 200.0f)));

    const auto drawVignetteBand = [&](float x, float y, float width, float height, float alpha, float r, float g, float b)
    {
        Shader_ResetStyle();
        Shader_SetTint(r, g, b, alpha);
        SpriteDraw(m_whiteTexture, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f);
    };

    const auto drawFrameBand = [&](float x, float y, float width, float height, float alpha)
    {
        Shader_ResetStyle();
        Shader_SetTint(overlayR, overlayG, overlayB, alpha);
        SpriteDraw(m_whiteTexture, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f);
    };

    const auto drawCornerFrame = [&](int frameLeft, int frameTop, int frameRight, int frameBottom, int thickness, int cornerLength, unsigned int color)
    {
        for (int offset = 0; offset < thickness; ++offset)
        {
            DrawLine(frameLeft, frameTop + offset, frameLeft + cornerLength, frameTop + offset, color);
            DrawLine(frameLeft + offset, frameTop, frameLeft + offset, frameTop + cornerLength, color);
            DrawLine(frameRight - cornerLength, frameTop + offset, frameRight, frameTop + offset, color);
            DrawLine(frameRight - offset, frameTop, frameRight - offset, frameTop + cornerLength, color);
            DrawLine(frameLeft, frameBottom - offset, frameLeft + cornerLength, frameBottom - offset, color);
            DrawLine(frameLeft + offset, frameBottom - cornerLength, frameLeft + offset, frameBottom, color);
            DrawLine(frameRight - cornerLength, frameBottom - offset, frameRight, frameBottom - offset, color);
            DrawLine(frameRight - offset, frameBottom - cornerLength, frameRight - offset, frameBottom, color);
        }
    };

    const int cornerLength = std::max(18, static_cast<int>(std::round(captureUi.cornerLength * finderUiScale)));
    const int cornerThickness = std::max(2, static_cast<int>(std::round(captureUi.cornerThickness + shutterT * 2.0f)));
    const int guideInset = std::max(12, static_cast<int>(std::round(captureUi.guideInset * finderUiScale)));
    const unsigned int gridColor = GetColor(242, 246, 252);

    if (m_flow.cameraMode)
    {
        const float vignetteAlpha = 0.15f + shutterT * 0.08f;
        const float edge0 = captureUi.vignetteEdge0 * finderUiScale;
        const float edge1 = captureUi.vignetteEdge1 * finderUiScale;
        const float edge2 = captureUi.vignetteEdge2 * finderUiScale;
        const float edge3 = captureUi.vignetteEdge3 * finderUiScale;
        const float topBottomBoost = captureUi.vignetteBoost;
        drawVignetteBand(overlayLeft, overlayTop, overlayWidth, edge3, vignetteAlpha * topBottomBoost, 0.01f, 0.015f, 0.025f);
        drawVignetteBand(overlayLeft, overlayTop + overlayHeight - edge3, overlayWidth, edge3, vignetteAlpha * topBottomBoost, 0.01f, 0.015f, 0.025f);
        drawVignetteBand(overlayLeft, overlayTop, edge3, overlayHeight, vignetteAlpha, 0.01f, 0.015f, 0.025f);
        drawVignetteBand(overlayLeft + overlayWidth - edge3, overlayTop, edge3, overlayHeight, vignetteAlpha, 0.01f, 0.015f, 0.025f);

        drawVignetteBand(overlayLeft, overlayTop, overlayWidth, edge2, vignetteAlpha * 0.82f * topBottomBoost, 0.02f, 0.02f, 0.035f);
        drawVignetteBand(overlayLeft, overlayTop + overlayHeight - edge2, overlayWidth, edge2, vignetteAlpha * 0.82f * topBottomBoost, 0.02f, 0.02f, 0.035f);
        drawVignetteBand(overlayLeft, overlayTop, edge2, overlayHeight, vignetteAlpha * 0.82f, 0.02f, 0.02f, 0.035f);
        drawVignetteBand(overlayLeft + overlayWidth - edge2, overlayTop, edge2, overlayHeight, vignetteAlpha * 0.82f, 0.02f, 0.02f, 0.035f);

        drawVignetteBand(overlayLeft, overlayTop, overlayWidth, edge1, vignetteAlpha * 0.60f * topBottomBoost, 0.03f, 0.028f, 0.05f);
        drawVignetteBand(overlayLeft, overlayTop + overlayHeight - edge1, overlayWidth, edge1, vignetteAlpha * 0.60f * topBottomBoost, 0.03f, 0.028f, 0.05f);
        drawVignetteBand(overlayLeft, overlayTop, edge1, overlayHeight, vignetteAlpha * 0.60f, 0.03f, 0.028f, 0.05f);
        drawVignetteBand(overlayLeft + overlayWidth - edge1, overlayTop, edge1, overlayHeight, vignetteAlpha * 0.60f, 0.03f, 0.028f, 0.05f);

        drawVignetteBand(overlayLeft, overlayTop, overlayWidth, edge0, vignetteAlpha * 0.38f * topBottomBoost, 0.04f, 0.035f, 0.06f);
        drawVignetteBand(overlayLeft, overlayTop + overlayHeight - edge0, overlayWidth, edge0, vignetteAlpha * 0.38f * topBottomBoost, 0.04f, 0.035f, 0.06f);
        drawVignetteBand(overlayLeft, overlayTop, edge0, overlayHeight, vignetteAlpha * 0.38f, 0.04f, 0.035f, 0.06f);
        drawVignetteBand(overlayLeft + overlayWidth - edge0, overlayTop, edge0, overlayHeight, vignetteAlpha * 0.38f, 0.04f, 0.035f, 0.06f);
    }

    drawFrameBand(innerX, innerY, innerWidth, innerHeight, 0.10f + shutterT * 0.18f);
    const float frameBandThickness = std::max(4.0f, captureUi.frameBandThickness * finderUiScale);
    drawFrameBand(drawX, drawY, drawWidth, frameBandThickness, 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX, drawY + drawHeight - frameBandThickness, drawWidth, frameBandThickness, 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX, drawY, frameBandThickness, drawHeight, 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX + drawWidth - frameBandThickness, drawY, frameBandThickness, drawHeight, 0.30f + shutterT * 0.16f);

    const Entity* bestTarget = FindCaptureTarget(*transform);

    ForEachCaptureTargetCandidate(*this, *transform, [&](Entity* entity, const TransformComponent& targetTransform)
    {
        const auto* photoOrigin = entity ? entity->GetComponent<PhotoCopyOriginComponent>() : nullptr;
        const bool movingEnemyAttackVisual =
            entity &&
            entity->GetComponent<PhotoMotionComponent>() &&
            photoOrigin &&
            photoOrigin->origin == PhotoCopyOrigin::Enemy;
        const bool hidesCaptureHitboxVisual =
            entity &&
            (HasTag(*entity, EntityTag::BossShield) ||
                HasTag(*entity, EntityTag::Boss1Shield) ||
                HasTag(*entity, EntityTag::MidBoss1Shield) ||
                HasTag(*entity, EntityTag::CapturedShield) ||
                HasTag(*entity, EntityTag::WalkerMeleeAttack) ||
                HasTag(*entity, EntityTag::BossShockwave) ||
                movingEnemyAttackVisual);
        if (hidesCaptureHitboxVisual)
        {
            // 攻撃や盾の当たり判定は残し、たたき台の白い判定表示だけ隠す。
            return;
        }

        const float targetLeft = targetTransform.x;
        const float targetTop = targetTransform.y;
        const float targetRight = targetTransform.x + targetTransform.width * targetTransform.scale;
        const float targetBottom = targetTransform.y + targetTransform.height * targetTransform.scale;
        const float overlapLeft = std::max(frameX, targetLeft);
        const float overlapTop = std::max(frameY, targetTop);
        const float overlapRight = std::min(frameX + frameWidth, targetRight);
        const float overlapBottom = std::min(frameY + frameHeight, targetBottom);
        const float overlapWidth = overlapRight - overlapLeft;
        const float overlapHeight = overlapBottom - overlapTop;
        if (overlapWidth <= 0.0f || overlapHeight <= 0.0f)
        {
            return;
        }

        const float glowX = viewOriginX + (overlapLeft - m_flow.cameraX) * viewScale;
        const float glowY = viewOriginY + (overlapTop - m_flow.cameraY) * viewScale;
        const float glowWidth = overlapWidth * viewScale;
        const float glowHeight = overlapHeight * viewScale;
        const bool isBestTarget = entity == bestTarget;

        Shader_ResetStyle();
        Shader_SetBlendMode(ShaderBlendMode2D::Additive);
        Shader_SetTint(1.0f, 1.0f, 1.0f, isBestTarget ? 0.42f : 0.24f);
        SpriteDraw(m_whiteTexture, glowX, glowY, glowWidth, glowHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        if (isBestTarget)
        {
            Shader_SetTint(1.0f, 1.0f, 1.0f, 0.16f);
            SpriteDraw(m_whiteTexture, glowX - 2.0f, glowY - 2.0f, glowWidth + 4.0f, glowHeight + 4.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        }
        Shader_ResetStyle();
    });

    drawCornerFrame(left, top, right, bottom, cornerThickness, cornerLength, frameColor);

    const int centerX = (left + right) / 2;
    const int centerY = (top + bottom) / 2;
    DrawLine(centerX - guideInset, centerY, centerX + guideInset, centerY, guideColor);
    DrawLine(centerX, centerY - guideInset, centerX, centerY + guideInset, guideColor);
    DrawBox(left + guideInset, top + guideInset, right - guideInset, bottom - guideInset, guideColor, FALSE);

    if (m_ui.captureLockoutRemaining > 0.0f || m_ui.captureRapidCount > 0)
    {
        const int limitCount = std::max(1, static_cast<int>(std::round(gCaptureRapidShotLimit)));
        const int currentCount = std::clamp(m_ui.captureRapidCount, 0, limitCount);
        const float warningWidth = captureUi.warningPanelWidth;
        const float warningHeight = captureUi.warningPanelHeight;
        const float warningX = captureUi.warningPanelX;
        const float warningY = captureUi.warningPanelY;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 210);
        DrawBox(
            static_cast<int>(std::round(warningX)),
            static_cast<int>(std::round(warningY)),
            static_cast<int>(std::round(warningX + warningWidth)),
            static_cast<int>(std::round(warningY + warningHeight)),
            GetColor(22, 12, 14),
            TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawBox(
            static_cast<int>(std::round(warningX)),
            static_cast<int>(std::round(warningY)),
            static_cast<int>(std::round(warningX + warningWidth)),
            static_cast<int>(std::round(warningY + warningHeight)),
            m_ui.captureLockoutRemaining > 0.0f ? GetColor(255, 92, 72) : GetColor(255, 220, 116),
            FALSE);
        DrawString(
            static_cast<int>(std::round(warningX + captureUi.warningTitleX)),
            static_cast<int>(std::round(warningY + captureUi.warningTitleY)),
            m_ui.captureLockoutRemaining > 0.0f ? "撮影ロック" : "撮影数",
            m_ui.captureLockoutRemaining > 0.0f ? GetColor(255, 228, 220) : GetColor(255, 244, 214));
        DrawFormatString(
            static_cast<int>(std::round(warningX + captureUi.warningCountX)),
            static_cast<int>(std::round(warningY + captureUi.warningCountY)),
            GetColor(236, 246, 255),
            "%d / %d",
            currentCount,
            limitCount);
        if (m_ui.captureLockoutRemaining > 0.0f)
        {
            DrawFormatString(
                static_cast<int>(std::round(warningX + captureUi.warningTimerX)),
                static_cast<int>(std::round(warningY + captureUi.warningCountY)),
                GetColor(255, 196, 196),
                "%.1fs",
                m_ui.captureLockoutRemaining);
        }
    }

    if (m_ui.shutterFlashRemaining > 0.0f)
    {
        Shader_ResetStyle();
        Shader_SetTint(overlayR, overlayG, overlayB, 0.10f + shutterT * 0.55f);
        SpriteDraw(m_whiteTexture, overlayLeft, overlayTop, overlayWidth, overlayHeight, 0.0f, 0.0f, 1.0f, 1.0f);

        const int pulseInset = static_cast<int>(std::round(captureUi.pulseInset * shutterT * finderUiScale));
        drawCornerFrame(
            left + pulseInset,
            top + pulseInset,
            right - pulseInset,
            bottom - pulseInset,
            std::max(2, cornerThickness + 1),
            std::max(12, cornerLength - pulseInset / 2),
            frameColor);
    }

    Shader_ResetStyle();
}

void GameScene::DrawTuningPanel()
{
    if (!m_debug.showTuningPanel)
    {
        return;
    }

    const auto entries = BuildGameSceneTuningEntries();
    DrawBox(
        static_cast<int>(std::round(kTuningPanelX)),
        static_cast<int>(std::round(kTuningPanelY)),
        static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth)),
        static_cast<int>(std::round(kTuningPanelY + kTuningPanelHeight)),
        GetColor(12, 16, 22),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(kTuningPanelX)),
        static_cast<int>(std::round(kTuningPanelY)),
        static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth)),
        static_cast<int>(std::round(kTuningPanelY + kTuningPanelHeight)),
        GetColor(220, 230, 244),
        FALSE);

    DrawString(
        static_cast<int>(kTuningPanelX + 16.0f),
        static_cast<int>(kTuningPanelY + 14.0f),
        "ゲーム調整",
        GetColor(245, 248, 255));
    DrawString(
        static_cast<int>(kTuningPanelX + 16.0f),
        static_cast<int>(kTuningPanelY + 42.0f),
        "F1 で閉じる  矢印で調整  +/- クリックで assets/tuning.json に保存",
        GetColor(178, 198, 220));

    const auto drawSectionHeader = [](float y, const char* label)
    {
        DrawBox(
            static_cast<int>(std::round(kTuningPanelX + 14.0f)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth - 14.0f)),
            static_cast<int>(std::round(y + kTuningSectionHeaderHeight - 6.0f)),
            GetColor(28, 36, 48),
            TRUE);
        DrawString(
            static_cast<int>(std::round(kTuningPanelX + 24.0f)),
            static_cast<int>(std::round(y + 2.0f)),
            label,
            GetColor(255, 228, 164));
    };

    drawSectionHeader(kTuningPanelY + 76.0f, "カメラ");
    drawSectionHeader(kTuningPanelY + 76.0f + (2.0f * kTuningRowHeight) + kTuningSectionHeaderHeight + kTuningSectionGap, "プレイヤー");
    drawSectionHeader(kTuningPanelY + 76.0f + (12.0f * kTuningRowHeight) + (kTuningSectionHeaderHeight + kTuningSectionGap) * 2.0f, "写真");

    for (int index = 0; index < static_cast<int>(entries.size()); ++index)
    {
        const float rowY = GetTuningRowY(index);
        const bool selected = index == m_debug.tuningSelection;
        const int labelColor = selected ? GetColor(255, 236, 178) : GetColor(228, 234, 242);
        const int valueColor = selected ? GetColor(255, 236, 178) : GetColor(172, 196, 220);

        if (selected)
        {
            DrawBox(
                static_cast<int>(std::round(kTuningPanelX + 12.0f)),
                static_cast<int>(std::round(rowY - 1.0f)),
                static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth - 12.0f)),
                static_cast<int>(std::round(rowY + kTuningButtonHeight + 1.0f)),
                GetColor(38, 48, 62),
                TRUE);
        }

        DrawString(
            static_cast<int>(std::round(kTuningPanelX + 20.0f)),
            static_cast<int>(std::round(rowY + 1.0f)),
            entries[index].label,
            labelColor);
        DrawFormatString(
            static_cast<int>(std::round(kTuningPanelX + 190.0f)),
            static_cast<int>(std::round(rowY + 1.0f)),
            valueColor,
            "%.2f",
            *entries[index].value);

        DrawTuningButton(kTuningPanelX + kTuningMinusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight, "-", selected);
        DrawTuningButton(kTuningPanelX + kTuningPlusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight, "+", selected);
    }

    DrawFormatString(
        static_cast<int>(kTuningPanelX + 20.0f),
        static_cast<int>(kTuningPanelY + kTuningPanelHeight - 34.0f),
        GetColor(176, 208, 228),
        "Dodge Time: %.2f",
        GetPlayerDodgeDuration());

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const int cursorOuter = GetColor(255, 242, 170);
    const int cursorInner = GetColor(18, 22, 28);
    DrawCircle(mouseX, mouseY, 7, cursorOuter, FALSE);
    DrawCircle(mouseX, mouseY, 2, cursorOuter, TRUE);
    DrawLine(mouseX - 10, mouseY, mouseX + 10, mouseY, cursorInner);
    DrawLine(mouseX, mouseY - 10, mouseX, mouseY + 10, cursorInner);
    DrawLine(mouseX - 9, mouseY, mouseX + 9, mouseY, cursorOuter);
    DrawLine(mouseX, mouseY - 9, mouseX, mouseY + 9, cursorOuter);
}

void GameScene::DrawUiAdjustmentWindow()
{
    ImGui::SetNextWindowSize(ImVec2(540.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UI調整"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("ゲームUIの位置・大きさ・間隔・演出をリアルタイム調整できます。");
    ImGui::TextUnformatted("各カテゴリを開いて値を変更してください。");

    if (ImGui::Button("UI設定を保存"))
    {
        SaveUiTuningState();
    }
    ImGui::SameLine();
    if (ImGui::Button("保存値を再読込"))
    {
        LoadUiTuningState();
    }
    ImGui::SameLine();
    if (ImGui::Button("HPを現在値に同期"))
    {
        if (const Entity* player = FindEntityByTag(kTagPlayer))
        {
            if (const auto* health = player->GetComponent<HealthComponent>())
            {
                const int maxHp = std::max(1, health->GetMaxHealth());
                const float ratio = std::clamp(static_cast<float>(health->GetCurrentHealth()) / static_cast<float>(maxHp), 0.0f, 1.0f);
                m_ui.hpDisplayRatio = ratio;
                m_ui.hpDamageLagRatio = ratio;
                m_ui.hpDamageFlash = 0.0f;
                m_ui.hpUiInitialized = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("すべて初期値に戻す"))
    {
        m_ui.tuning = GameSceneUiTuningState{};
        m_ui.captureFinderScale = 1.0f;
        m_ui.photoTrayReveal = 1.0f;
        m_ui.hpDisplayRatio = 1.0f;
        m_ui.hpDamageLagRatio = 1.0f;
        m_ui.hpDamageFlash = 0.0f;
        m_ui.cameraFlash.enabled = true;
        m_ui.cameraFlash.pulseRemaining = 0.0f;
        m_ui.cameraFlash.pulseDuration = 0.0f;
        m_ui.hpUiInitialized = false;
    }

    if (!m_debug.saveStatusMessage.empty() && m_debug.saveStatusTimer > 0.0f)
    {
        ImGui::TextWrapped("%s", m_debug.saveStatusMessage.c_str());
    }

    auto drag = [](const char* label, float& value, float speed, float minValue, float maxValue)
    {
        ImGui::DragFloat(label, &value, speed, minValue, maxValue, "%.2f");
    };

    auto& tuning = m_ui.tuning;

    DrawTutorialAdjustmentPanel();

    if (ImGui::CollapsingHeader("撮影ファインダー", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drag("現在の倍率##finder", m_ui.captureFinderScale, 0.01f, 0.1f, 5.0f);
        drag("基準幅(px)##finder", gCaptureFrameWidthPx, 1.0f, 16.0f, 2048.0f);
        drag("基準高さ(px)##finder", gCaptureFrameHeightPx, 1.0f, 16.0f, 2048.0f);
        drag("最小倍率##finder", tuning.captureFinder.scaleMin, 0.01f, 0.1f, 5.0f);
        drag("最大倍率##finder", tuning.captureFinder.scaleMax, 0.01f, 0.1f, 5.0f);
        drag("倍率変更量##finder", tuning.captureFinder.scaleStep, 0.01f, 0.01f, 1.0f);
        drag("ズーム追従速度##finder", tuning.captureFinder.zoomBlendResponse, 0.1f, 0.1f, 30.0f);
    }

    if (ImGui::CollapsingHeader("撮影オーバーレイ"))
    {
        drag("枠の内側余白##overlay", tuning.captureOverlay.frameInset, 1.0f, 0.0f, 200.0f);
        drag("コーナー線の長さ##overlay", tuning.captureOverlay.cornerLength, 1.0f, 0.0f, 300.0f);
        drag("コーナー線の太さ##overlay", tuning.captureOverlay.cornerThickness, 0.1f, 0.5f, 20.0f);
        drag("ガイド余白##overlay", tuning.captureOverlay.guideInset, 1.0f, 0.0f, 300.0f);
        drag("外枠帯の太さ##overlay", tuning.captureOverlay.frameBandThickness, 1.0f, 0.0f, 100.0f);
        drag("暗幕 境界1##overlay", tuning.captureOverlay.vignetteEdge0, 1.0f, 0.0f, 500.0f);
        drag("暗幕 境界2##overlay", tuning.captureOverlay.vignetteEdge1, 1.0f, 0.0f, 500.0f);
        drag("暗幕 境界3##overlay", tuning.captureOverlay.vignetteEdge2, 1.0f, 0.0f, 500.0f);
        drag("暗幕 境界4##overlay", tuning.captureOverlay.vignetteEdge3, 1.0f, 0.0f, 500.0f);
        drag("暗幕の強さ##overlay", tuning.captureOverlay.vignetteBoost, 0.01f, 0.0f, 3.0f);
        drag("警告X##overlay", tuning.captureOverlay.warningPanelX, 1.0f, -500.0f, 2000.0f);
        drag("警告Y##overlay", tuning.captureOverlay.warningPanelY, 1.0f, -500.0f, 2000.0f);
        drag("警告幅##overlay", tuning.captureOverlay.warningPanelWidth, 1.0f, 10.0f, 1000.0f);
        drag("警告高さ##overlay", tuning.captureOverlay.warningPanelHeight, 1.0f, 10.0f, 500.0f);
        drag("警告タイトルX##overlay", tuning.captureOverlay.warningTitleX, 1.0f, -200.0f, 500.0f);
        drag("警告タイトルY##overlay", tuning.captureOverlay.warningTitleY, 1.0f, -200.0f, 500.0f);
        drag("警告カウントX##overlay", tuning.captureOverlay.warningCountX, 1.0f, -200.0f, 500.0f);
        drag("警告カウントY##overlay", tuning.captureOverlay.warningCountY, 1.0f, -200.0f, 500.0f);
        drag("警告タイマーX##overlay", tuning.captureOverlay.warningTimerX, 1.0f, -200.0f, 500.0f);
        drag("パルス余白##overlay", tuning.captureOverlay.pulseInset, 1.0f, 0.0f, 300.0f);
    }

    if (ImGui::CollapsingHeader("保存写真トレイ", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("表示率##tray", &m_ui.photoTrayReveal, 0.0f, 1.0f, "%.2f");
        drag("先頭X##tray", tuning.photoTray.slotStartX, 1.0f, -1000.0f, 3000.0f);
        drag("先頭Y##tray", tuning.photoTray.slotStartY, 1.0f, -1000.0f, 3000.0f);
        drag("写真枠の幅##tray", tuning.photoTray.slotWidth, 1.0f, 10.0f, 1000.0f);
        drag("写真枠の高さ##tray", tuning.photoTray.slotHeight, 1.0f, 10.0f, 1000.0f);
        drag("写真同士の間隔##tray", tuning.photoTray.slotGapX, 1.0f, -500.0f, 1000.0f);
        drag("写真内側余白##tray", tuning.photoTray.previewPadding, 1.0f, 0.0f, 200.0f);
        drag("保存写真の表示倍率##tray", tuning.photoTray.previewScale, 0.01f, 0.05f, 5.0f);
        drag("空欄文字X##tray", tuning.photoTray.emptyTextX, 1.0f, -200.0f, 500.0f);
        drag("空欄文字Y##tray", tuning.photoTray.emptyTextY, 1.0f, -200.0f, 500.0f);
        drag("ロック文字X##tray", tuning.photoTray.lockTextX, 1.0f, -200.0f, 500.0f);
        drag("ロック文字Y##tray", tuning.photoTray.lockTextY, 1.0f, -200.0f, 500.0f);
        drag("開閉速度##tray", tuning.photoTray.revealSpeed, 0.1f, 0.1f, 50.0f);
        drag("クリック有効しきい値##tray", tuning.photoTray.revealThreshold, 0.01f, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("現像写真プレビュー"))
    {
        auto& preview = tuning.developedPhotoPreview;
        drag("表示時間##preview", preview.lifetime, 0.05f, 0.1f, 20.0f);
        drag("カード幅##preview", preview.cardWidth, 1.0f, 20.0f, 1000.0f);
        drag("カード高さ##preview", preview.cardHeight, 1.0f, 20.0f, 1000.0f);
        drag("右余白##preview", preview.cardRightMargin, 1.0f, -500.0f, 1000.0f);
        drag("開始Yオフセット##preview", preview.cardStartYOffset, 1.0f, -500.0f, 1000.0f);
        drag("停止Y##preview", preview.cardCruiseY, 1.0f, -500.0f, 2000.0f);
        drag("影オフセット##preview", preview.cardShadowOffset, 1.0f, 0.0f, 100.0f);
        drag("輪郭オフセット##preview", preview.cardOutlineOffset, 1.0f, 0.0f, 100.0f);
        drag("写真余白##preview", preview.frameInset, 1.0f, 0.0f, 300.0f);
        drag("写真高さ##preview", preview.imageHeight, 1.0f, 10.0f, 1000.0f);
        drag("上帯高さ##preview", preview.imageTopStripHeight, 1.0f, 0.0f, 300.0f);
        drag("中帯Y##preview", preview.imageMiddleStripY, 1.0f, 0.0f, 500.0f);
        drag("上昇時間比##preview", preview.cardRiseEase, 0.01f, 0.01f, 1.0f);
        drag("停止開始##preview", preview.cardPauseStart, 0.01f, 0.0f, 1.0f);
        drag("停止終了##preview", preview.cardPauseEnd, 0.01f, 0.0f, 1.0f);
        drag("停止揺れ幅##preview", preview.cardPauseAmplitude, 0.5f, 0.0f, 200.0f);
        drag("飛び出し量##preview", preview.cardOvershootY, 0.5f, 0.0f, 200.0f);
        drag("写真ポップ倍率##preview", preview.popScale, 0.001f, 0.0f, 1.0f);
        drag("光球開始X##preview", preview.orbLaunchXOffset, 1.0f, -500.0f, 500.0f);
        drag("光球開始Y比率##preview", preview.orbLaunchYOffset, 0.01f, -2.0f, 2.0f);
        drag("光球制御点1Y##preview", preview.orbControl1YOffset, 1.0f, -1000.0f, 1000.0f);
        drag("光球制御点2X##preview", preview.orbControl2XOffset, 1.0f, -1000.0f, 1000.0f);
        drag("光球制御点2Y##preview", preview.orbControl2YOffset, 1.0f, -1000.0f, 1000.0f);
    }

    if (ImGui::CollapsingHeader("プレイヤーHP", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto& hp = tuning.hp;
        ImGui::SliderFloat("HP表示率##hp", &m_ui.hpDisplayRatio, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("遅延ダメージ率##hp", &m_ui.hpDamageLagRatio, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("被弾フラッシュ##hp", &m_ui.hpDamageFlash, 0.0f, 1.0f, "%.2f");
        drag("先頭X##hp", hp.slotStartX, 1.0f, -1000.0f, 3000.0f);
        drag("先頭Y##hp", hp.slotStartY, 1.0f, -1000.0f, 3000.0f);
        drag("スロット幅##hp", hp.slotWidth, 1.0f, 10.0f, 500.0f);
        drag("スロット高さ##hp", hp.slotHeight, 1.0f, 10.0f, 500.0f);
        drag("ハート同士の間隔##hp", hp.slotGapX, 1.0f, -300.0f, 500.0f);
        drag("ハートサイズ##hp", hp.heartSize, 1.0f, 4.0f, 500.0f);
        drag("ハートYオフセット##hp", hp.heartYOffset, 1.0f, -300.0f, 500.0f);
        drag("影X##hp", hp.heartShadowOffsetX, 0.5f, -100.0f, 100.0f);
        drag("影Y##hp", hp.heartShadowOffsetY, 0.5f, -100.0f, 100.0f);
        drag("発光拡張##hp", hp.heartGlowExpand, 0.5f, 0.0f, 100.0f);
        drag("遅延発光拡張##hp", hp.heartLagGlowExpand, 0.5f, 0.0f, 100.0f);
        drag("ラベルX##hp", hp.labelOffsetX, 1.0f, -1000.0f, 1000.0f);
        drag("ラベルY##hp", hp.labelOffsetY, 1.0f, -500.0f, 500.0f);
        drag("HP文字Y##hp", hp.hpTextOffsetY, 1.0f, -500.0f, 500.0f);
    }

    if (ImGui::CollapsingHeader("部品HUD"))
    {
        auto& parts = tuning.partsHud;
        drag("幅##parts", parts.panelWidth, 1.0f, 10.0f, 1000.0f);
        drag("高さ##parts", parts.panelHeight, 1.0f, 10.0f, 500.0f);
        drag("右余白##parts", parts.marginRight, 1.0f, -500.0f, 1000.0f);
        drag("下余白##parts", parts.marginBottom, 1.0f, -500.0f, 1000.0f);
        drag("アイコンX##parts", parts.iconX, 1.0f, -300.0f, 500.0f);
        drag("アイコンY##parts", parts.iconY, 1.0f, -300.0f, 500.0f);
        drag("アイコンサイズ##parts", parts.iconSize, 1.0f, 1.0f, 300.0f);
        drag("アイコン内側##parts", parts.iconInnerInset, 1.0f, 0.0f, 100.0f);
        drag("ラベルX##parts", parts.labelX, 1.0f, -300.0f, 500.0f);
        drag("ラベルY##parts", parts.labelY, 1.0f, -300.0f, 500.0f);
        drag("数値Y##parts", parts.valueY, 1.0f, -300.0f, 500.0f);
    }

    if (ImGui::CollapsingHeader("ボスHP"))
    {
        auto& boss = tuning.bossHp;
        drag("バー幅##boss", boss.panelWidth, 1.0f, 20.0f, 2000.0f);
        drag("バー高さ##boss", boss.barHeight, 1.0f, 2.0f, 300.0f);
        drag("パネル余白##boss", boss.panelPadding, 1.0f, 0.0f, 300.0f);
        drag("上余白##boss", boss.marginTop, 1.0f, -500.0f, 2000.0f);
        drag("追加高さ##boss", boss.panelExtraHeight, 1.0f, 0.0f, 500.0f);
        drag("タイトルY##boss", boss.titleOffsetY, 1.0f, -300.0f, 500.0f);
        drag("HP文字Y##boss", boss.hpTextOffsetY, 1.0f, -300.0f, 500.0f);
    }

    if (ImGui::CollapsingHeader("攻撃写真スロット"))
    {
        auto& attack = tuning.attackCapture;
        drag("X##attack", attack.panelX, 1.0f, -1000.0f, 3000.0f);
        drag("Y##attack", attack.panelY, 1.0f, -1000.0f, 3000.0f);
        drag("パネルサイズ##attack", attack.panelSize, 1.0f, 10.0f, 1000.0f);
        drag("アイコン半径##attack", attack.iconRadius, 1.0f, 1.0f, 500.0f);
        drag("タイトルX##attack", attack.titleX, 1.0f, -500.0f, 500.0f);
        drag("タイトルY##attack", attack.titleY, 1.0f, -500.0f, 500.0f);
        drag("個数表示 右余白##attack", attack.countRightOffset, 1.0f, -500.0f, 500.0f);
        drag("個数表示 下余白##attack", attack.countBottomOffset, 1.0f, -500.0f, 500.0f);
    }

    if (ImGui::CollapsingHeader("フィルターパネル"))
    {
        auto& filter = tuning.filterPanel;
        drag("幅##filter", filter.panelWidth, 1.0f, 10.0f, 1000.0f);
        drag("高さ##filter", filter.panelHeight, 1.0f, 10.0f, 500.0f);
        drag("右余白##filter", filter.marginRight, 1.0f, -500.0f, 1000.0f);
        drag("上余白##filter", filter.marginTop, 1.0f, -500.0f, 1000.0f);
        drag("色見本X##filter", filter.swatchX, 1.0f, -300.0f, 500.0f);
        drag("色見本Y##filter", filter.swatchY, 1.0f, -300.0f, 500.0f);
        drag("色見本サイズ##filter", filter.swatchSize, 1.0f, 1.0f, 300.0f);
        drag("タイトルX##filter", filter.titleX, 1.0f, -300.0f, 500.0f);
        drag("タイトルY##filter", filter.titleY, 1.0f, -300.0f, 500.0f);
        drag("効果文Y##filter", filter.effectY, 1.0f, -300.0f, 500.0f);
        drag("ヒントX##filter", filter.hintX, 1.0f, -300.0f, 500.0f);
        drag("ヒントY##filter", filter.hintY, 1.0f, -300.0f, 500.0f);
    }

    if (ImGui::CollapsingHeader("一時停止メニュー"))
    {
        auto& menu = tuning.escapeMenu;
        drag("幅##menu", menu.panelWidth, 1.0f, 100.0f, 2000.0f);
        drag("高さ##menu", menu.panelHeight, 1.0f, 100.0f, 2000.0f);
        drag("項目開始Y##menu", menu.rowStartOffset, 1.0f, 0.0f, 1000.0f);
        drag("項目高さ##menu", menu.rowHeight, 1.0f, 4.0f, 200.0f);
        drag("項目左右余白##menu", menu.rowPaddingX, 1.0f, 0.0f, 500.0f);
        drag("項目下余白##menu", menu.rowBottomInset, 1.0f, 0.0f, 100.0f);
    }

    if (ImGui::CollapsingHeader("商人UI"))
    {
        auto& merchant = tuning.merchant;
        drag("パネル幅##merchant", merchant.panelWidth, 1.0f, 100.0f, 2500.0f);
        drag("パネル高さ##merchant", merchant.panelHeight, 1.0f, 100.0f, 2000.0f);
        drag("商品行高さ##merchant", merchant.rowHeight, 1.0f, 10.0f, 500.0f);
        drag("リスト左##merchant", merchant.listLeftOffset, 1.0f, -500.0f, 2000.0f);
        drag("リスト上##merchant", merchant.listTopOffset, 1.0f, -500.0f, 2000.0f);
        drag("リスト右##merchant", merchant.listRightOffset, 1.0f, -500.0f, 2500.0f);
        drag("詳細左##merchant", merchant.detailLeftOffset, 1.0f, -500.0f, 2500.0f);
        drag("詳細上##merchant", merchant.detailTopOffset, 1.0f, -500.0f, 2000.0f);
        drag("詳細下余白##merchant", merchant.detailBottomOffset, 1.0f, -500.0f, 2000.0f);
        drag("接触表示 半幅##merchant", merchant.promptHalfWidth, 1.0f, 10.0f, 1000.0f);
        drag("接触表示 高さ##merchant", merchant.promptHeight, 1.0f, 10.0f, 500.0f);
        drag("接触表示 文字X##merchant", merchant.promptTextX, 1.0f, -300.0f, 500.0f);
        drag("接触表示 文字Y##merchant", merchant.promptTextY, 1.0f, -300.0f, 500.0f);
        drag("接触表示 浮遊量##merchant", merchant.promptRiseOffsetY, 0.5f, 0.0f, 100.0f);
        drag("接触表示 点滅速度##merchant", merchant.promptPulseSpeed, 0.1f, 0.0f, 30.0f);
    }

    if (ImGui::CollapsingHeader("バッテリースイッチ表示"))
    {
        auto& counter = tuning.batteryCounter;
        drag("幅##battery", counter.panelWidth, 1.0f, 10.0f, 500.0f);
        drag("高さ##battery", counter.panelHeight, 1.0f, 10.0f, 300.0f);
        drag("上方向オフセット##battery", counter.offsetY, 1.0f, -500.0f, 1000.0f);
        drag("タイル距離倍率##battery", counter.tileOffsetMultiplier, 0.1f, -10.0f, 20.0f);
    }

    if (ImGui::CollapsingHeader("ステージガイド"))
    {
        drag("X##guide", tuning.stageGuide.x, 1.0f, -1000.0f, 3000.0f);
        drag("下からの距離##guide", tuning.stageGuide.yOffsetFromBottom, 1.0f, -1000.0f, 3000.0f);
    }

    if (ImGui::CollapsingHeader("マップエディター"))
    {
        drag("左##editor", tuning.mapEditor.panelLeft, 1.0f, -1000.0f, 3000.0f);
        drag("上##editor", tuning.mapEditor.panelTop, 1.0f, -1000.0f, 3000.0f);
        drag("右##editor", tuning.mapEditor.panelRight, 1.0f, -1000.0f, 3000.0f);
        drag("下##editor", tuning.mapEditor.panelBottom, 1.0f, -1000.0f, 3000.0f);
    }

    if (ImGui::CollapsingHeader("カメラフラッシュ"))
    {
        ImGui::Checkbox("有効##flash", &m_ui.cameraFlash.enabled);
        drag("残り時間##flash", m_ui.cameraFlash.pulseRemaining, 0.01f, 0.0f, 10.0f);
        drag("継続時間##flash", m_ui.cameraFlash.pulseDuration, 0.01f, 0.0f, 10.0f);
    }

    ImGui::End();
}

void GameScene::DrawDevelopedPhotoPreview() const
{
    if (m_ui.developedPhotoPreviewRemaining <= 0.0f || !m_photo.pendingStore.active || m_photo.pendingStore.capture.items.empty())
    {
        return;
    }

    const PhotoCaptureState& previewCapture = m_photo.pendingStore.capture;
    const float previewLifetime = std::max(0.01f, m_ui.tuning.developedPhotoPreview.lifetime);
    const float progress = 1.0f - Clamp01(m_ui.developedPhotoPreviewRemaining / previewLifetime);
    const float cardPhaseT = Clamp01(progress / std::max(0.01f, m_ui.tuning.developedPhotoPreview.cardRiseEase));
    const float orbPhaseT = Clamp01((progress - 0.14f) / 0.30f);
    const float orbArriveT = Clamp01((progress - 0.34f) / 0.10f);
    const float finalFade = Clamp01(m_ui.developedPhotoPreviewRemaining / std::max(0.01f, previewLifetime * 0.081f));

    float accentR = 0.32f;
    float accentG = 0.92f;
    float accentB = 1.0f;
    GetPhotoFilterThemeOverlayColor(previewCapture.capturedTheme, accentR, accentG, accentB);

    const int targetSlotIndex = std::clamp(
        m_photo.pendingStore.slotIndex,
        0,
        2);
    const UiLayoutRect targetSlot = MakePhotoTraySlotRect(m_ui.tuning, targetSlotIndex);
    const float targetCenterX = targetSlot.x + targetSlot.width * 0.5f;
    const float targetCenterY = targetSlot.y + targetSlot.height * 0.5f;

    const auto& previewUi = m_ui.tuning.developedPhotoPreview;
    const float photoWidth = previewUi.cardWidth;
    const float photoHeight = previewUi.cardHeight;
    const float frameInset = previewUi.frameInset;
    const float imageWidth = photoWidth - frameInset * 2.0f;
    const float imageHeight = previewUi.imageHeight;
    const float baseX = static_cast<float>(SCREEN_WIDTH) - photoWidth - previewUi.cardRightMargin;
    const float startY = static_cast<float>(SCREEN_HEIGHT) + previewUi.cardStartYOffset;
    const float cardCruiseY = previewUi.cardCruiseY;

    const float cardRiseT = Clamp01(cardPhaseT / std::max(0.01f, previewUi.cardRiseEase));
    const float riseEase = cardRiseT * cardRiseT * (3.0f - 2.0f * cardRiseT);
    const float overshootT = std::sin(cardRiseT * 3.14159265f);
    const float settleScale = 0.95f + std::sin(cardRiseT * 1.57079632f) * 0.05f;
    const float pauseSlow = cardPhaseT >= previewUi.cardPauseStart && cardPhaseT <= previewUi.cardPauseEnd
        ? std::sin(((cardPhaseT - previewUi.cardPauseStart) / std::max(0.01f, previewUi.cardPauseEnd - previewUi.cardPauseStart)) * 3.14159265f)
        : 0.0f;
    const float cardX = baseX + (1.0f - riseEase) * previewUi.cardShadowOffset * 0.5f;
    const float cardY = std::lerp(startY, cardCruiseY, riseEase) - overshootT * previewUi.cardOvershootY + pauseSlow * previewUi.cardPauseAmplitude;
    const float cardAlpha = Clamp01(1.0f - Clamp01((progress - 0.22f) / 0.12f)) * finalFade;

    if (cardAlpha > 0.0f)
    {
        const float visibleHeight = photoHeight * settleScale;
        const float previewScaleOut = 1.0f;
        const float imageScaleOut = settleScale;

        DrawBox(
            static_cast<int>(std::round(cardX + previewUi.cardShadowOffset)),
            static_cast<int>(std::round(cardY + previewUi.cardOutlineOffset)),
            static_cast<int>(std::round(cardX + photoWidth + previewUi.cardShadowOffset)),
            static_cast<int>(std::round(cardY + visibleHeight + previewUi.cardOutlineOffset)),
            GetColor(16, 18, 24),
            TRUE);

        Shader_ResetStyle();
        Shader_SetTint(accentR, accentG, accentB, 0.14f * cardAlpha);
        SpriteDraw(
            m_whiteTexture,
            cardX - previewUi.cardShadowOffset,
            cardY - previewUi.cardShadowOffset,
            photoWidth + previewUi.cardShadowOffset * 2.0f,
            visibleHeight + previewUi.cardShadowOffset * 2.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f);
        Shader_SetTint(0.98f, 0.96f, 0.90f, 0.96f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY, photoWidth, visibleHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(accentR, accentG, accentB, 0.20f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY, photoWidth, previewUi.imageTopStripHeight * imageScaleOut, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(0.92f, 0.88f, 0.74f, 0.12f * cardAlpha);
        SpriteDraw(
            m_whiteTexture,
            cardX,
            cardY + previewUi.imageMiddleStripY * imageScaleOut,
            photoWidth,
            10.0f * imageScaleOut,
            0.0f,
            0.0f,
            1.0f,
            1.0f);

        const float photoX = cardX + frameInset * previewScaleOut;
        const float photoY = cardY + frameInset * imageScaleOut;
        Shader_SetTint(0.12f, 0.14f, 0.18f, 0.88f * cardAlpha);
        SpriteDraw(m_whiteTexture, photoX - 3.0f, photoY - 3.0f, imageWidth + 6.0f, imageHeight * imageScaleOut + 6.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        const float previewScale = std::min(
            imageWidth / std::max(1.0f, previewCapture.width),
            imageHeight / std::max(1.0f, previewCapture.height));
        const float previewPop = settleScale + overshootT * previewUi.popScale;
        const float scaledContentWidth = previewCapture.width * previewScale * previewPop;
        const float scaledContentHeight = previewCapture.height * previewScale * previewPop * settleScale;
        const float contentOffsetX = photoX + (imageWidth - scaledContentWidth) * 0.5f;
        const float contentOffsetY = photoY + (imageHeight * imageScaleOut - scaledContentHeight) * 0.5f;

        Shader_SetTint(0.10f, 0.12f, 0.14f, 0.95f * cardAlpha);
        SpriteDraw(m_whiteTexture, photoX, photoY, imageWidth, imageHeight * imageScaleOut, 0.0f, 0.0f, 1.0f, 1.0f);

        for (const auto& item : previewCapture.items)
        {
            DrawCapturedPreviewItem(
                m_tileTexture,
                item,
                contentOffsetX + item.relativeX * previewScale * previewPop,
                contentOffsetY + item.relativeY * previewScale * previewPop * settleScale,
                item.width * previewScale * previewPop,
                item.height * previewScale * previewPop * settleScale,
                cardAlpha);
        }

        DrawBox(
            static_cast<int>(std::round(photoX)),
            static_cast<int>(std::round(photoY)),
            static_cast<int>(std::round(photoX + imageWidth)),
            static_cast<int>(std::round(photoY + imageHeight * imageScaleOut)),
            GetColor(215, 205, 180),
            FALSE);
        DrawString(
            static_cast<int>(cardX + 18.0f),
            static_cast<int>(cardY + visibleHeight + 22.0f),
            "Captured",
            GetColor(62, 56, 48));
        DrawFormatString(
            static_cast<int>(cardX + 18.0f),
            static_cast<int>(cardY + visibleHeight + 46.0f),
            GetColor(110, 96, 78),
            "%s  %.0fx%.0f",
            GetPhotoFilterThemeLabel(previewCapture.capturedTheme),
            previewCapture.width,
            previewCapture.height);
        Shader_ResetStyle();
    }

    const float launchX = cardX + photoWidth * 0.5f - previewUi.orbLaunchXOffset;
    const float launchY = cardCruiseY + photoHeight * previewUi.orbLaunchYOffset;
    const float control1X = launchX + previewUi.orbLaunchXOffset * 0.333f;
    const float control1Y = launchY - previewUi.orbControl1YOffset;
    const float control2X = targetCenterX + previewUi.orbControl2XOffset;
    const float control2Y = targetCenterY - previewUi.orbControl2YOffset;
    const float easedTravel = orbPhaseT * orbPhaseT * (3.0f - 2.0f * orbPhaseT);
    const float invT = 1.0f - easedTravel;
    const float orbX = invT * invT * invT * launchX
        + 3.0f * invT * invT * easedTravel * control1X
        + 3.0f * invT * easedTravel * easedTravel * control2X
        + easedTravel * easedTravel * easedTravel * targetCenterX;
    const float orbY = invT * invT * invT * launchY
        + 3.0f * invT * invT * easedTravel * control1Y
        + 3.0f * invT * easedTravel * easedTravel * control2Y
        + easedTravel * easedTravel * easedTravel * targetCenterY;

    const float derivX = 3.0f * invT * invT * (control1X - launchX)
        + 6.0f * invT * easedTravel * (control2X - control1X)
        + 3.0f * easedTravel * easedTravel * (targetCenterX - control2X);
    const float derivY = 3.0f * invT * invT * (control1Y - launchY)
        + 6.0f * invT * easedTravel * (control2Y - control1Y)
        + 3.0f * easedTravel * easedTravel * (targetCenterY - control2Y);
    const float derivLength = std::sqrt(derivX * derivX + derivY * derivY);
    const float tangentX = derivLength > 0.001f ? derivX / derivLength : 0.0f;
    const float tangentY = derivLength > 0.001f ? derivY / derivLength : -1.0f;
    const float normalY = tangentX;

    const float orbAppear = Clamp01((progress - 0.13f) / 0.03f);
    const float orbAlpha = orbAppear * finalFade;
    if (orbAlpha <= 0.0f)
    {
        return;
    }

    const float pulse = 1.0f + std::sin(progress * 21.0f) * 0.08f;
    const float orbRadius = std::lerp(14.0f, 7.0f, orbArriveT) * pulse;
    const int orbColor = GetColor(
        static_cast<int>(std::round(120.0f + accentR * 135.0f)),
        static_cast<int>(std::round(118.0f + accentG * 122.0f)),
        static_cast<int>(std::round(140.0f + accentB * 112.0f)));
    const int coreColor = GetColor(255, 248, 228);
    const int sparkColor = GetColor(
        static_cast<int>(std::round(188.0f + accentR * 67.0f)),
        static_cast<int>(std::round(186.0f + accentG * 69.0f)),
        static_cast<int>(std::round(198.0f + accentB * 57.0f)));

    for (int i = 0; i < 8; ++i)
    {
        const float sampleT = Clamp01(orbPhaseT - i * 0.060f);
        const float sampleEase = sampleT * sampleT * (3.0f - 2.0f * sampleT);
        const float sampleInvT = 1.0f - sampleEase;
        const float sampleX = sampleInvT * sampleInvT * sampleInvT * launchX
            + 3.0f * sampleInvT * sampleInvT * sampleEase * control1X
            + 3.0f * sampleInvT * sampleEase * sampleEase * control2X
            + sampleEase * sampleEase * sampleEase * targetCenterX;
        const float sampleY = sampleInvT * sampleInvT * sampleInvT * launchY
            + 3.0f * sampleInvT * sampleInvT * sampleEase * control1Y
            + 3.0f * sampleInvT * sampleEase * sampleEase * control2Y
            + sampleEase * sampleEase * sampleEase * targetCenterY;
        const float sampleAlpha = orbAlpha * (1.0f - static_cast<float>(i) / 8.5f);
        const float sampleRadius = orbRadius * (1.6f - i * 0.10f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(108.0f * sampleAlpha)));
        DrawCircle(
            static_cast<int>(std::round(sampleX)),
            static_cast<int>(std::round(sampleY)),
            static_cast<int>(std::round(sampleRadius)),
            orbColor,
            TRUE);
    }

    for (int i = 0; i < 20; ++i)
    {
        const float orbitT = static_cast<float>(i) / 20.0f;
        const float angle = progress * 12.0f + orbitT * 6.2831853f;
        const float orbitRadius = orbRadius * (1.0f + std::sin(progress * 7.0f + orbitT * 11.0f) * 0.26f);
        const float particleX = orbX + std::cos(angle) * orbitRadius + tangentX * (orbitT - 0.5f) * 15.0f * (1.0f - orbArriveT);
        const float particleY = orbY + std::sin(angle) * orbitRadius * 0.58f + normalY * (orbitT - 0.5f) * 10.0f;
        const float particleRadius = std::lerp(3.2f, 1.0f, orbitT) * (1.0f - orbArriveT * 0.35f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round((122.0f - orbitT * 56.0f) * orbAlpha)));
        DrawCircle(
            static_cast<int>(std::round(particleX)),
            static_cast<int>(std::round(particleY)),
            static_cast<int>(std::max(1.0f, particleRadius)),
            sparkColor,
            TRUE);
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(164.0f * orbAlpha)));
    DrawCircle(
        static_cast<int>(std::round(orbX)),
        static_cast<int>(std::round(orbY)),
        static_cast<int>(std::round(orbRadius * 1.9f)),
        orbColor,
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(236.0f * orbAlpha)));
    DrawCircle(
        static_cast<int>(std::round(orbX)),
        static_cast<int>(std::round(orbY)),
        static_cast<int>(std::round(orbRadius)),
        coreColor,
        TRUE);
    DrawCircle(
        static_cast<int>(std::round(orbX - orbRadius * 0.34f)),
        static_cast<int>(std::round(orbY - orbRadius * 0.36f)),
        static_cast<int>(std::round(orbRadius * 0.34f)),
        GetColor(255, 255, 255),
        TRUE);

    if (orbArriveT > 0.0f)
    {
        const float ringRadius = std::lerp(10.0f, 24.0f, orbArriveT);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(132.0f * orbAlpha * (1.0f - orbArriveT))));
        DrawCircle(
            static_cast<int>(std::round(targetCenterX)),
            static_cast<int>(std::round(targetCenterY)),
            static_cast<int>(std::round(ringRadius)),
            orbColor,
            FALSE);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

bool GameScene::IsPhotoTrayHit(float screenX, float screenY) const
{
    if (m_ui.photoTrayReveal <= m_ui.tuning.photoTray.revealThreshold)
    {
        return false;
    }

    for (int slotIndex = 0; slotIndex < 3; ++slotIndex)
    {
        const UiLayoutRect slot = MakePhotoTraySlotRect(m_ui.tuning, slotIndex);
        if (IsPointInRect(screenX, screenY, slot))
        {
            return true;
        }
    }

    return false;
}

void GameScene::DrawPhotoStorageTray() const
{
    if (m_ui.photoTrayReveal <= 0.01f)
    {
        return;
    }

    const GameSessionState& session = GameSession_Get();
    const auto& photoTray = m_ui.tuning.photoTray;
    const int unlockedSlotCount = std::clamp(session.photoStorageSlots, 0, 3);
    const int trayAlpha = static_cast<int>(std::round(255.0f * std::clamp(m_ui.photoTrayReveal, 0.0f, 1.0f)));
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, trayAlpha);

    for (int slotIndex = 0; slotIndex < 3; ++slotIndex)
    {
        const UiLayoutRect slot = MakePhotoTraySlotRect(m_ui.tuning, slotIndex);
        const bool slotIsPending = m_photo.pendingStore.active && m_photo.pendingStore.slotIndex == slotIndex;
        const PhotoCaptureState& storedCapture = slotIsPending ? m_photo.pendingStore.capture : m_photo.savedCaptures[slotIndex];
        const bool unlocked = slotIndex < unlockedSlotCount;
        const bool selected = unlocked && slotIndex == m_photo.selectedCaptureSlot;
        const unsigned int fillColor = !unlocked
            ? GetColor(10, 14, 18)
            : (selected ? GetColor(30, 42, 56) : GetColor(16, 22, 30));
        const unsigned int outlineColor = !unlocked
            ? GetColor(92, 100, 110)
            : (selected ? GetColor(255, 234, 156) : GetColor(188, 204, 224));

        DrawBox(
            static_cast<int>(std::round(slot.x)),
            static_cast<int>(std::round(slot.y)),
            static_cast<int>(std::round(slot.x + slot.width)),
            static_cast<int>(std::round(slot.y + slot.height)),
            fillColor,
            TRUE);
        DrawBox(
            static_cast<int>(std::round(slot.x)),
            static_cast<int>(std::round(slot.y)),
            static_cast<int>(std::round(slot.x + slot.width)),
            static_cast<int>(std::round(slot.y + slot.height)),
            outlineColor,
            FALSE);

        if (!unlocked)
        {
            DrawString(
                static_cast<int>(std::round(slot.x + photoTray.lockTextX)),
                static_cast<int>(std::round(slot.y + photoTray.lockTextY)),
                "LOCK",
                GetColor(160, 166, 174));
            continue;
        }

        if (!storedCapture.hasPhoto || storedCapture.items.empty())
        {
            DrawString(
                static_cast<int>(std::round(slot.x + photoTray.emptyTextX)),
                static_cast<int>(std::round(slot.y + photoTray.emptyTextY)),
                "EMPTY",
                GetColor(146, 156, 170));
            continue;
        }

        const float previewWidth = (slot.width - photoTray.previewPadding * 2.0f);
        const float previewHeight = (slot.height - photoTray.previewPadding * 2.0f);
        const float previewX = slot.x + (slot.width - previewWidth) * 0.5f;
        const float previewY = slot.y + (slot.height - previewHeight) * 0.5f;
        const float scale = std::min(
            previewWidth / std::max(1.0f, storedCapture.width),
            previewHeight / std::max(1.0f, storedCapture.height));
        const float previewScale = std::max(0.01f, photoTray.previewScale);
        const float finalScale = scale * previewScale;
        const float contentX = previewX + (previewWidth - storedCapture.width * finalScale) * 0.5f;
        const float contentY = previewY + (previewHeight - storedCapture.height * finalScale) * 0.5f;

        DrawBox(
            static_cast<int>(std::round(previewX)),
            static_cast<int>(std::round(previewY)),
            static_cast<int>(std::round(previewX + previewWidth)),
            static_cast<int>(std::round(previewY + previewHeight)),
            GetColor(10, 14, 18),
            TRUE);

        for (const auto& item : storedCapture.items)
        {
            DrawCapturedPreviewItem(
                m_tileTexture,
                item,
                contentX + item.relativeX * finalScale,
                contentY + item.relativeY * finalScale,
                item.width * finalScale,
                item.height * finalScale,
                1.0f);
        }

        DrawBox(
            static_cast<int>(std::round(previewX)),
            static_cast<int>(std::round(previewY)),
            static_cast<int>(std::round(previewX + previewWidth)),
            static_cast<int>(std::round(previewY + previewHeight)),
            GetColor(215, 205, 180),
            FALSE);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void GameScene::DrawPhotoPlacementPreview() const
{
    photo_system::DrawPlacementPreview(*this);
}

void GameScene::DrawBackdrop() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float viewHeight = GetViewHeight();

    DrawBackdropBaseInView(viewOriginX, viewOriginY, viewWidth, viewHeight, viewScale);

    if (m_debug.showBackdropGrid)
    {
        DrawBackdropGridInView(viewOriginX, viewOriginY, viewWidth, viewHeight, viewScale);
    }

    DrawBackdropFrameInView(viewOriginX, viewOriginY, viewWidth, viewHeight);

    DrawCameraWorldInView(viewOriginX, viewOriginY, viewScale);

    DrawPhotoFilterPanelInView();

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

// DrawBackdropBaseInView を置き換える（該当関数全体）
void GameScene::DrawBackdropBaseInView(
    float viewOriginX,
    float viewOriginY,
    float viewWidth,
    float viewHeight,
    float viewScale) const
{
    int bgTexture = m_camera.backdropTextureId >= 0 ? m_camera.backdropTextureId : m_assets.GetTexture("forest_bg");
    int bg1Texture = m_camera.backdropTexture1Id >= 0 ? m_camera.backdropTexture1Id : m_assets.GetTexture("forest_fg");
    int bg2Texture = m_assets.GetTexture("forest1_bg");
    if (bgTexture < 0) bgTexture = m_assets.GetTexture("forest_bg");
    if (bg1Texture < 0) bg1Texture = m_assets.GetTexture("forest_fg");
    if (bg2Texture < 0) bg2Texture = m_assets.GetTexture("forest1_bg");

    if (bgTexture < 0 || bg1Texture < 0)
    {
        return;
    }

    const int texW = TextureGetWidth(bgTexture);
    const int texH = TextureGetHeight(bgTexture);
    if (texW <= 0 || texH <= 0) return;

    const int texW1 = TextureGetWidth(bg1Texture);
    const int texH1 = TextureGetHeight(bg1Texture);
    if (texW1 <= 0 || texH1 <= 0) return;

    const float drawW = viewWidth;
    const float drawH = viewHeight;
    const float drawW1 = viewWidth;
    const float drawH1 = viewHeight;

    // パララックス（例）
    const float parallaxX = 0.45f;
    const float parallaxY = 0.45f;
    const float parallaxX1 = 0.85f;
    const float parallaxY1 = 0.45f;

    // カメラ位置→UVオフセット（0..1）
    auto calcScroll = [](float worldPos, float parallax, float texSize)->float
    {
        if (parallax == 0.0f) return 0.0f;
        float v = std::fmod((worldPos * parallax) / texSize, 1.0f);
        if (v < 0.0f) v += 1.0f;
        return v;
    };

    const float scrollU = calcScroll(m_flow.cameraX, parallaxX, static_cast<float>(texW));
    const float scrollV = calcScroll(-m_flow.cameraY, parallaxY, static_cast<float>(texH));

    const float scrollU1 = calcScroll(m_flow.cameraX, parallaxX1, static_cast<float>(texW1));
    const float scrollV1 = 0.0f;//calcScroll(-m_flow.cameraY, parallaxY1, static_cast<float>(texH1));
    // view に対する UV のスパン（= 画面幅 / テクスチャ幅）
    const float uSpan = drawW / static_cast<float>(texW);
    const float vSpan = drawH / static_cast<float>(texH);
    const float uSpan1 = drawW1 / static_cast<float>(texW1);
    const float vSpan1 = drawH1 / static_cast<float>(texH1);

    // Y を下にずらすオフセット（必要なら）
    //const float bg1OffsetY = 24.0f * viewScale;
    const float bg1OffsetY = 0.0f;

    // 内部：1回分のuv塊を四分割して描くヘルパー（tx,tyは [0..inf) を許容し、小数部で扱う）
    const auto drawTiledChunk = [&](int textureId, float destX, float destY, float destW, float destH, float tx, float ty, float tw, float th)
    {
        // normalize to fractional part in [0,1)
        tx = std::fmod(tx, 1.0f);
        if (tx < 0.0f) tx += 1.0f;
        ty = std::fmod(ty, 1.0f);
        if (ty < 0.0f) ty += 1.0f;

        if (tw <= 0.0f || th <= 0.0f) return;

        const float u1 = std::min(1.0f - tx, tw);
        const float u2 = tw - u1;
        const float v1 = std::min(1.0f - ty, th);
        const float v2 = th - v1;

        const float w1 = destW * (u1 / tw);
        const float w2 = destW - w1;
        const float h1 = destH * (v1 / th);
        const float h2 = destH - h1;

        // 左上
        if (w1 > 0.5f && h1 > 0.5f)
        {
            SpriteDraw(textureId, destX, destY, w1, h1, tx, ty, u1, v1, false, 0.0f);
        }
        // 右上
        if (u2 > 0.0001f && w2 > 0.5f && h1 > 0.5f)
        {
            SpriteDraw(textureId, destX + w1, destY, w2, h1, 0.0f, ty, u2, v1, false, 0.0f);
        }
        // 左下
        if (v2 > 0.0001f && h2 > 0.5f && w1 > 0.5f)
        {
            SpriteDraw(textureId, destX, destY + h1, w1, h2, tx, 0.0f, u1, v2, false, 0.0f);
        }
        // 右下
        if (u2 > 0.0001f && v2 > 0.0001f && w2 > 0.5f && h2 > 0.5f)
        {
            SpriteDraw(textureId, destX + w1, destY + h1, w2, h2, 0.0f, 0.0f, u2, v2, false, 0.0f);
        }
    };

    // 汎用：uSpan/vSpan が 1 を超える場合に回数分タイルして描画するループ
    const auto drawTiledRepeating = [&](int textureId, float destX, float destY, float destW, float destH, float baseTx, float baseTy, float totalTw, float totalTh)
    {
        const int repsX = std::max(1, static_cast<int>(std::ceil(totalTw)));
        const int repsY = std::max(1, static_cast<int>(std::ceil(totalTh)));

        float accuY = 0.0f;
        for (int iy = 0; iy < repsY; ++iy)
        {
            const float thPart = std::min(1.0f, totalTh - static_cast<float>(iy));
            const float destHPart = destH * (thPart / totalTh);
            float accuX = 0.0f;
            for (int ix = 0; ix < repsX; ++ix)
            {
                const float twPart = std::min(1.0f, totalTw - static_cast<float>(ix));
                const float destWPart = destW * (twPart / totalTw);

                const float tileTx = baseTx + static_cast<float>(ix);
                const float tileTy = baseTy + static_cast<float>(iy);

                drawTiledChunk(textureId, destX + accuX, destY + accuY, destWPart, destHPart, tileTx, tileTy, twPart, thPart);

                accuX += destWPart;
            }
            accuY += destHPart;
        }
    };

    //// 背景（奥）を描画
    //drawTiledRepeating(bgTexture, viewOriginX, viewOriginY, drawW, drawH, scrollU, scrollV, uSpan, vSpan);
    // 
    // 背景前景（手前）を描画（Y を下にオフセット）
    //drawTiledRepeating(bg1Texture, viewOriginX, viewOriginY + bg1OffsetY, drawW1, drawH1, scrollU1, scrollV1, uSpan1, vSpan1);
    // 背景前景（手前）を描画（Y を下にオフセット）
    drawTiledRepeating(bg1Texture, viewOriginX, viewOriginY + bg1OffsetY, drawW1, drawH1, scrollU1, scrollV1, uSpan1, vSpan1);

    // BG_Forest.png だけ少し光度を落とす黒膜
    constexpr int kForestBaseDarkenAlpha = 75; // 0-255。32=薄め、64=やや暗め
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, kForestBaseDarkenAlpha);
    DrawBox(
        static_cast<int>(std::round(viewOriginX)),
        static_cast<int>(std::round(viewOriginY + bg1OffsetY)),
        static_cast<int>(std::round(viewOriginX + drawW1)),
        static_cast<int>(std::round(viewOriginY + bg1OffsetY + drawH1)),
        GetColor(0, 0, 0),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    //int bg2Texture = m_assets.GetTexture("forest1_bg");
    if (bg2Texture >= 0)
    {
        const int texW2 = TextureGetWidth(bg2Texture);
        const int texH2 = TextureGetHeight(bg2Texture);
        if (texW2 > 0 && texH2 > 0)
        {
            const float parallaxX2 = 0.65f; // 好みで調整
            const float scrollU2 = calcScroll(m_flow.cameraX, parallaxX2, static_cast<float>(texW2));
            const float scrollV2 = 0.0f;//calcScroll(-m_flow.cameraY, 0.45f, static_cast<float>(texH2));
            const float uSpan2 = viewWidth / static_cast<float>(texW2);
            const float vSpan2 = viewHeight / static_cast<float>(texH2);
            drawTiledRepeating(bg2Texture, viewOriginX, viewOriginY, viewWidth, viewHeight, scrollU2, scrollV2, uSpan2, vSpan2);
        }
    }

    int bg3Texture = m_assets.GetTexture("forest2_bg");
    if (bg3Texture >= 0)
    {
        const int texW3 = TextureGetWidth(bg3Texture);
        const int texH3 = TextureGetHeight(bg3Texture);
        if (texW3 > 0 && texH3 > 0)
        {
            const float parallaxX3 = 0.70f; // 好みで調整
            const float scrollU3 = calcScroll(m_flow.cameraX, parallaxX3, static_cast<float>(texW3));
            const float scrollV3 = 0.0f;//calcScroll(-m_flow.cameraY, 0.45f, static_cast<float>(texH3));
            const float uSpan3 = viewWidth / static_cast<float>(texW3);
            const float vSpan3 = viewHeight / static_cast<float>(texH3);
            drawTiledRepeating(bg3Texture, viewOriginX, viewOriginY, viewWidth, viewHeight, scrollU3, scrollV3, uSpan3, vSpan3);
        }
    }

    int bg4Texture = m_assets.GetTexture("forest3_bg");
    if (bg4Texture >= 0)
    {
        const int texW4 = TextureGetWidth(bg4Texture);
        const int texH4 = TextureGetHeight(bg4Texture);
        if (texW4 > 0 && texH4 > 0)
        {
            // scrollU1をそのまま使う（BG_Forest.pngと完全同期）
            const float uSpan4 = viewWidth / static_cast<float>(texW4);
            const float vSpan4 = viewHeight / static_cast<float>(texH4);
            drawTiledRepeating(bg4Texture, viewOriginX, viewOriginY, viewWidth, viewHeight, scrollU1, scrollV1, uSpan4, vSpan4);
        }
    }
}

void GameScene::DrawStageTransitionMarkersInView(float viewOriginX, float viewOriginY, float viewScale) const
{
    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    const int columnStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraX / tileSize)) - 1);
    const int rowStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraY / tileSize)) - 1);
    const int columnEnd = std::min(m_tileMap.GetWidth() - 1, static_cast<int>(std::ceil((m_flow.cameraX + gCameraViewWidth) / tileSize)) + 1);
    const int rowEnd = std::min(m_tileMap.GetHeight() - 1, static_cast<int>(std::ceil((m_flow.cameraY + gCameraViewHeight) / tileSize)) + 1);

    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker == '\0')
            {
                continue;
            }

            const StageTransitionLink* transition = nullptr;
            for (const StageTransitionLink& link : gStageTransitionLinks)
            {
                const bool sourceMatches = link.sourceMapCsv == "*" || link.sourceMapCsv == m_lifecycle.currentMapCsvPath;
                if (sourceMatches && link.marker == marker)
                {
                    transition = &link;
                    break;
                }
            }
            if (!transition)
            {
                continue;
            }

            const float worldX = static_cast<float>(column) * tileSize;
            const float worldY = static_cast<float>(row) * tileSize;
            const int left = static_cast<int>(std::round(viewOriginX + (worldX - m_flow.cameraX) * viewScale));
            const int top = static_cast<int>(std::round(viewOriginY + (worldY - m_flow.cameraY) * viewScale));
            const int right = static_cast<int>(std::round(viewOriginX + (worldX + tileSize - m_flow.cameraX) * viewScale));
            const int bottom = static_cast<int>(std::round(viewOriginY + (worldY + tileSize - m_flow.cameraY) * viewScale));

            if (marker == '=')
            {
                // 遷移オブジェクト本体は描かず、右半分へ向かって濃くなる影だけを表示する。
                constexpr int kShadowBandCount = 8;
                constexpr int kShadowMinAlpha = 24;
                constexpr int kShadowMaxAlpha = 176;
                const int shadowLeft = left + (right - left) / 2;
                const int shadowWidth = (std::max)(1, right - shadowLeft);
                for (int bandIndex = 0; bandIndex < kShadowBandCount; ++bandIndex)
                {
                    const float bandStartRate =
                        static_cast<float>(bandIndex) / static_cast<float>(kShadowBandCount);
                    const float bandEndRate =
                        static_cast<float>(bandIndex + 1) / static_cast<float>(kShadowBandCount);
                    const int bandLeft = shadowLeft + static_cast<int>(std::floor(shadowWidth * bandStartRate));
                    const int bandRight = shadowLeft + static_cast<int>(std::ceil(shadowWidth * bandEndRate));
                    const int alpha = static_cast<int>(std::round(std::lerp(
                        static_cast<float>(kShadowMinAlpha),
                        static_cast<float>(kShadowMaxAlpha),
                        bandEndRate)));
                    SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
                    DrawBox(bandLeft, top, bandRight, bottom, GetColor(0, 0, 0), TRUE);
                }
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                continue;
            }

            DrawBox(left, top, right, bottom, GetColor(255, 210, 90), FALSE);

            const std::string destName = GetMapDisplayName(transition->destinationMapCsv);
            DrawFormatString(left, top - 16, GetColor(180, 240, 255), "→ %s", destName.c_str());
        }
    }
}

void GameScene::DrawBackdropGridInView(float viewOriginX, float viewOriginY, float viewWidth, float viewHeight, float viewScale) const
{
    const float worldLeft = m_flow.cameraX;
    const float worldRight = m_flow.cameraX + gCameraViewWidth;
    const float gridSpacing = m_tileMap.GetTileSize();
    if (gridSpacing <= 0.0f)
    {
        return;
    }

    const unsigned int majorColor = GetColor(72, 188, 128);
    const unsigned int minorColor = GetColor(38, 112, 82);

    for (float worldX = std::floor(worldLeft / gridSpacing) * gridSpacing; worldX <= worldRight; worldX += gridSpacing)
    {
        const float screenX = viewOriginX + (worldX - m_flow.cameraX) * viewScale;
        const int x = static_cast<int>(std::round(screenX));
        const bool major = std::fmod(std::fabs(worldX), gridSpacing * 4.0f) < 0.5f ||
            (gridSpacing * 4.0f - std::fmod(std::fabs(worldX), gridSpacing * 4.0f)) < 0.5f;
        DrawLine(x, static_cast<int>(viewOriginY), x, static_cast<int>(viewOriginY + viewHeight), major ? majorColor : minorColor);
        if (!major)
        {
            DrawLine(x + 1, static_cast<int>(viewOriginY), x + 1, static_cast<int>(viewOriginY + viewHeight), GetColor(22, 56, 40));
        }
    }

    const float worldTop = m_flow.cameraY;
    const float worldBottom = m_flow.cameraY + gCameraViewHeight;
    for (float worldY = std::floor(worldTop / gridSpacing) * gridSpacing; worldY <= worldBottom; worldY += gridSpacing)
    {
        const float screenY = viewOriginY + (worldY - m_flow.cameraY) * viewScale;
        const int y = static_cast<int>(std::round(screenY));
        const bool major = std::fmod(worldY, gridSpacing * 4.0f) < 0.5f ||
            (gridSpacing * 4.0f - std::fmod(worldY, gridSpacing * 4.0f)) < 0.5f;
        DrawLine(static_cast<int>(viewOriginX), y, static_cast<int>(viewOriginX + viewWidth), y, major ? majorColor : minorColor);
        if (!major)
        {
            DrawLine(static_cast<int>(viewOriginX), y + 1, static_cast<int>(viewOriginX + viewWidth), y + 1, GetColor(22, 56, 40));
        }
    }
}

void GameScene::DrawBackdropFrameInView(float viewOriginX, float viewOriginY, float viewWidth, float viewHeight) const
{
    const float panelRight = viewOriginX + viewWidth;
    const float panelBottom = viewOriginY + viewHeight;
    Shader_SetTint(0.18f, 0.18f, 0.22f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY - 10.0f, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, panelBottom, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, panelRight, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
}

//void GameScene::DrawBackgroundPartsInView(float viewOriginX, float viewOriginY, float viewScale) const//パーツ
//{
//    for (const auto& part : kBackgroundParts)
//    {
//        const int textureId = m_assets.GetTexture(part.textureKey);
//        if (textureId < 0)
//        {
//            continue;
//        }
//
//        const float screenX = viewOriginX + (part.worldX - m_flow.cameraX * part.parallax) * viewScale;
//        const float screenY = viewOriginY + (part.worldY - m_flow.cameraY * part.parallax) * viewScale;
//        const float drawW = part.width * viewScale;
//        const float drawH = part.height * viewScale;
//
//        Shader_ResetStyle();
//        SpriteDraw(textureId, screenX, screenY, drawW, drawH, 0.0f, 0.0f, 1.0f, 1.0f);
//    }
//    Shader_ResetStyle();
//}

void GameScene::DrawCameraWorldInView(float viewOriginX, float viewOriginY, float viewScale) const
{
    const TileMapViewport viewport
    {
        viewOriginX,
        viewOriginY,
        GetViewWidth(),
        GetViewHeight()
    };

    //DrawBackgroundPartsInView(viewOriginX, viewOriginY, viewScale); // ★これを追加し直す

    // 写真カメラを含む現在の表示範囲だけにタイル描画を制限する。
    m_tileMap.Draw(
        m_tileTexture,
        viewOriginX - m_flow.cameraX * viewScale,
        viewOriginY - m_flow.cameraY * viewScale,
        viewport,
        viewScale,
        m_tileTexture2,
        m_tileTexture3,
        m_tileTexture4);
    DrawStageTransitionMarkersInView(viewOriginX, viewOriginY, viewScale);
    DrawMapEditorMarkersInView(viewOriginX, viewOriginY, viewScale);
    DrawMidBoss2TeleportSlotsInView(viewOriginX, viewOriginY, viewScale);
    DrawStageGuideInView();
}

void GameScene::DrawMapEditorMarkersInView(float viewOriginX, float viewOriginY, float viewScale) const
{
    if (!m_mapEditor.active)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    const int columnStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraX / tileSize)) - 1);
    const int rowStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraY / tileSize)) - 1);
    const int columnEnd = std::min(m_tileMap.GetWidth() - 1, static_cast<int>(std::ceil((m_flow.cameraX + gCameraViewWidth) / tileSize)) + 1);
    const int rowEnd = std::min(m_tileMap.GetHeight() - 1, static_cast<int>(std::ceil((m_flow.cameraY + gCameraViewHeight) / tileSize)) + 1);

    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker == '\0')
            {
                continue;
            }

            const RgbColor color = GetEditorMarkerColor(marker);

            const float worldX = static_cast<float>(column) * tileSize;
            const float worldY = static_cast<float>(row) * tileSize;
            const int left = static_cast<int>(std::round(viewOriginX + (worldX - m_flow.cameraX) * viewScale));
            const int top = static_cast<int>(std::round(viewOriginY + (worldY - m_flow.cameraY) * viewScale));
            const int right = static_cast<int>(std::round(viewOriginX + (worldX + tileSize - m_flow.cameraX) * viewScale));
            const int bottom = static_cast<int>(std::round(viewOriginY + (worldY + tileSize - m_flow.cameraY) * viewScale));

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 88);
            DrawBox(left, top, right, bottom, GetColor(color.r, color.g, color.b), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawBox(left, top, right, bottom, GetColor(color.r, color.g, color.b), FALSE);
        }
    }
}

void GameScene::DrawMidBoss2TeleportSlotsInView(float viewOriginX, float viewOriginY, float viewScale) const
{
    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    const float mapHeight = GetMapPixelHeight();
    constexpr float kMidBoss2JumpCenterGridX = 36.0f;
    constexpr float kMidBoss2ArenaHalfWidthGrid = 18.0f;

    const auto getMidBoss2LeftX = [&](float centerGridX, float bossWidth)
    {
        return centerGridX * tileSize - bossWidth * 0.5f;
    };

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        const auto* boss = entity->GetComponent<MidBoss2Component>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss2 || !transform)
        {
            continue;
        }

        if (!enemy->IsEnabled() || enemy->IsDefeated() || boss->state == MidBoss2State::Dead)
        {
            continue;
        }

        const float bossWidth = transform->width * transform->scale;
        const float bossHeight = transform->height * transform->scale;
        if (bossWidth <= 0.0f || bossHeight <= 0.0f)
        {
            continue;
        }

        const float arenaMinX = getMidBoss2LeftX(kMidBoss2JumpCenterGridX - kMidBoss2ArenaHalfWidthGrid, bossWidth);
        const float arenaMaxX = getMidBoss2LeftX(kMidBoss2JumpCenterGridX + kMidBoss2ArenaHalfWidthGrid, bossWidth);
        const float maxBossY = std::max(0.0f, mapHeight - bossHeight);

        const auto drawSlotSet = [&](const char* sideLabel, const std::array<MidBoss2Component::TeleportSlotConfig, 3>& slots)
        {
            int lowestSlotIndex = 0;
            float lowestOffset = slots[0].hoverHeightOffsetGrid;
            for (int index = 1; index < static_cast<int>(slots.size()); ++index)
            {
                if (slots[static_cast<size_t>(index)].hoverHeightOffsetGrid < lowestOffset)
                {
                    lowestOffset = slots[static_cast<size_t>(index)].hoverHeightOffsetGrid;
                    lowestSlotIndex = index;
                }
            }

            for (int index = 0; index < static_cast<int>(slots.size()); ++index)
            {
                const auto& slot = slots[static_cast<size_t>(index)];
                const float rawX = getMidBoss2LeftX(slot.centerGridX, bossWidth);
                const float rawY = mapHeight - bossHeight - (boss->params.teleportHoverBaseGrid + slot.hoverHeightOffsetGrid) * tileSize;
                const float targetX = std::clamp(rawX, arenaMinX, arenaMaxX);
                const float targetY = std::clamp(rawY, 0.0f, maxBossY);
                const bool xClamped = std::fabs(targetX - rawX) > 0.1f;
                const bool yClamped = std::fabs(targetY - rawY) > 0.1f;
                const bool beamTarget = index == lowestSlotIndex;

                const int left = static_cast<int>(std::round(viewOriginX + (targetX - m_flow.cameraX) * viewScale));
                const int top = static_cast<int>(std::round(viewOriginY + (targetY - m_flow.cameraY) * viewScale));
                const int right = static_cast<int>(std::round(viewOriginX + (targetX + bossWidth - m_flow.cameraX) * viewScale));
                const int bottom = static_cast<int>(std::round(viewOriginY + (targetY + bossHeight - m_flow.cameraY) * viewScale));
                if (right <= left || bottom <= top)
                {
                    continue;
                }

                const int centerX = (left + right) / 2;
                const int centerY = (top + bottom) / 2;
                const bool isLeftSide = sideLabel[0] == 'L';
                const unsigned int sideFillColor = isLeftSide ? GetColor(78, 220, 255) : GetColor(255, 152, 84);
                const unsigned int sideOutlineColor = isLeftSide ? GetColor(112, 242, 255) : GetColor(255, 202, 142);
                const unsigned int fillColor = beamTarget ? GetColor(255, 214, 120) : sideFillColor;
                const unsigned int outlineColor = beamTarget
                    ? GetColor(255, 246, 200)
                    : xClamped || yClamped
                        ? GetColor(255, 120, 108)
                        : sideOutlineColor;

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, beamTarget ? 68 : 36);
                DrawBox(left, top, right, bottom, fillColor, TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

                DrawBox(left, top, right, bottom, outlineColor, FALSE);
                DrawCircle(centerX, centerY, std::max(4, static_cast<int>(std::round(5.0f * viewScale))), outlineColor, FALSE);
                DrawLine(centerX - 8, centerY, centerX + 8, centerY, outlineColor);
                DrawLine(centerX, centerY - 8, centerX, centerY + 8, outlineColor);

                const char* clampTag = (xClamped || yClamped) ? " CLAMP" : "";
                const char* beamTag = beamTarget ? " BEAM" : "";
                DrawFormatString(
                    left,
                    std::max(0, top - 18),
                    outlineColor,
                    "%s%d%s%s  H=%.2f",
                    sideLabel,
                    index + 1,
                    beamTag,
                    clampTag,
                    boss->params.teleportHoverBaseGrid + slot.hoverHeightOffsetGrid);
            }
        };

        drawSlotSet("L", boss->params.leftTeleportSlots);
        drawSlotSet("R", boss->params.rightTeleportSlots);
    }
}

void GameScene::DrawStageGuideInView() const
{
    if (const Entity* player = FindEntityByTag(kTagPlayer))
    {
        if (const auto* transform = player->GetComponent<TransformComponent>())
        {
            const auto& guideUi = m_ui.tuning.stageGuide;
            DrawString(
                static_cast<int>(std::round(guideUi.x)),
                static_cast<int>(std::round(SCREEN_HEIGHT - guideUi.yOffsetFromBottom)),
                GetStageGuideText(transform->x),
                GetColor(238, 244, 255));
        }
    }
}

void GameScene::DrawPhotoFilterPanelInView() const
{
    float filterR = 1.0f;
    float filterG = 1.0f;
    float filterB = 1.0f;
    GetPhotoFilterThemeOverlayColor(m_photo.capture.selectedTheme, filterR, filterG, filterB);

    const auto& filterUi = m_ui.tuning.filterPanel;
    const int panelWidth = static_cast<int>(std::round(filterUi.panelWidth));
    const int panelHeight = static_cast<int>(std::round(filterUi.panelHeight));
    const int panelX = SCREEN_WIDTH - panelWidth - static_cast<int>(std::round(filterUi.marginRight));
    const int panelY = static_cast<int>(std::round(filterUi.marginTop));
    DrawBox(panelX, panelY, panelX + panelWidth, panelY + panelHeight, GetColor(14, 18, 24), TRUE);
    DrawBox(panelX, panelY, panelX + panelWidth, panelY + panelHeight, GetColor(220, 228, 236), FALSE);
    DrawBox(
        panelX + static_cast<int>(std::round(filterUi.swatchX)),
        panelY + static_cast<int>(std::round(filterUi.swatchY)),
        panelX + static_cast<int>(std::round(filterUi.swatchX + filterUi.swatchSize)),
        panelY + static_cast<int>(std::round(filterUi.swatchY + filterUi.swatchSize)),
        GetColor(
            static_cast<int>(filterR * 255.0f),
            static_cast<int>(filterG * 255.0f),
            static_cast<int>(filterB * 255.0f)),
        TRUE);
    DrawFormatString(
        panelX + static_cast<int>(std::round(filterUi.titleX)),
        panelY + static_cast<int>(std::round(filterUi.titleY)),
        GetColor(245, 248, 255),
        "フィルター: %s",
        GetPhotoFilterThemeLabel(m_photo.capture.selectedTheme));
    DrawFormatString(
        panelX + static_cast<int>(std::round(filterUi.titleX)),
        panelY + static_cast<int>(std::round(filterUi.effectY)),
        GetColor(180, 210, 235),
        "%s",
        GetPhotoFilterThemeEffectText(m_photo.capture.selectedTheme));
    DrawFormatString(
        panelX + static_cast<int>(std::round(filterUi.hintX)),
        panelY + static_cast<int>(std::round(filterUi.hintY)),
        GetColor(205, 220, 235),
        "C で切り替え  1 なし  2 暖色  3 寒色  4 反転  5 セピア");
}

void GameScene::GetCaptureFrameRect(const TransformComponent& playerTransform, float& x, float& y, float& width, float& height) const
{
    static_cast<void>(playerTransform);
    const float viewScale = std::max(0.0001f, GetViewScale());
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();

    // ファインダーはスクリーンUIとして扱い、カメラ倍率に左右されない画面pxを維持する。
    const float finderUiScale = GetCaptureFinderScreenScale(m_tileMap.GetTileSize());
    width = gCaptureFrameWidthPx * m_ui.captureFinderScale * finderUiScale / viewScale;
    height = gCaptureFrameHeightPx * m_ui.captureFinderScale * finderUiScale / viewScale;

    // Cursor-centered finder: the visible frame and actual capture bounds must match.
    const float cursorWorldX = m_flow.cameraX + (static_cast<float>(Input_GetMouseX()) - viewOriginX) / viewScale;
    const float cursorWorldY = m_flow.cameraY + (static_cast<float>(Input_GetMouseY()) - viewOriginY) / viewScale;
    x = cursorWorldX - width * 0.5f;
    y = cursorWorldY - height * 0.5f;
}

Entity* GameScene::FindCaptureTarget(const TransformComponent& playerTransform) const
{
    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(playerTransform, frameX, frameY, frameWidth, frameHeight);

    TransformComponent captureFrame(frameX, frameY, frameWidth, frameHeight);
    Entity* bestTarget = nullptr;
    float bestDistance = 1000000.0f;

    ForEachCaptureTargetCandidate(*this, captureFrame, [&](Entity* entity, const TransformComponent& transform)
    {
        const float targetCenterX = transform.x + transform.width * transform.scale * 0.5f;
        const float playerCenterX = playerTransform.x + playerTransform.width * playerTransform.scale * 0.5f;
        const float distance = std::fabs(targetCenterX - playerCenterX);
        if (!bestTarget || distance < bestDistance)
        {
            bestTarget = entity;
            bestDistance = distance;
        }
    });

    return bestTarget;
}

void GameScene::DrawBatterySwitchCounters() const
{
    const auto& counterUi = m_ui.tuning.batteryCounter;
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float tileOffsetY = m_tileMap.GetTileSize() * counterUi.tileOffsetMultiplier * viewScale;

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::BatterySwitch))
    {
        if (!entity) continue;
        const auto* transform = entity->GetComponent<TransformComponent>();
        const auto* batterySwitch = entity->GetComponent<BatterySwitchComponent>();
        if (!transform || !batterySwitch)
        {
            continue;
        }

        const float drawX = viewOriginX + (transform->x - m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + (transform->y - m_flow.cameraY) * viewScale;
        const float drawWidth = transform->width * transform->scale * viewScale;
        const float panelWidth = counterUi.panelWidth;
        const float panelHeight = counterUi.panelHeight;
        const float panelX = drawX + drawWidth * 0.5f - panelWidth * 0.5f;
        const float panelY = drawY - tileOffsetY - counterUi.offsetY;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 208);
        DrawBox(
            static_cast<int>(std::round(panelX)),
            static_cast<int>(std::round(panelY)),
            static_cast<int>(std::round(panelX + panelWidth)),
            static_cast<int>(std::round(panelY + panelHeight)),
            GetColor(18, 20, 24),
            TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        DrawBox(
            static_cast<int>(std::round(panelX)),
            static_cast<int>(std::round(panelY)),
            static_cast<int>(std::round(panelX + panelWidth)),
            static_cast<int>(std::round(panelY + panelHeight)),
            batterySwitch->isPressed
                ? GetColor(180, 255, 196) : GetColor(242, 226, 190),
            FALSE);

        if (batterySwitch->pressMode == SwitchPressMode::Player)
        {
            DrawString(
                static_cast<int>(std::round(panelX + 7.0f)),
                static_cast<int>(std::round(panelY + 4.0f)),
                "PLAYER",
                GetColor(255, 244, 220));
        }
        else
        {
            DrawFormatString(
                static_cast<int>(std::round(panelX + 10.0f)),
                static_cast<int>(std::round(panelY + 4.0f)),
                batterySwitch->isPressed ? GetColor(220, 255, 228) : GetColor(255, 244, 220),
                "%d/%d",
                batterySwitch->insertedBatteryCount,
                batterySwitch->requiredBatteryCount);
        }
    }
}

