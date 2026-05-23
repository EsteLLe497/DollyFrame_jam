#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_render_ui_helpers.h"
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

    constexpr int kPhotoTraySlotCount = 3;
    constexpr float kPhotoTraySlotWidth = 170.0f;
    constexpr float kPhotoTraySlotHeight = 92.0f;
    constexpr float kPhotoTraySlotGap = 18.0f;
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
        const std::vector<std::unique_ptr<Entity>>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        for (const auto& entity : entities)
        {
            if (!entity || !HasTag(*entity, kTagMarkerLight))
            {
                continue;
            }

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
    }

    void CollectStageLightOverlayLights(
        const std::vector<std::unique_ptr<Entity>>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        for (const auto& entity : entities)
        {
            if (!entity || !HasTag(*entity, kTagStageLight))
            {
                continue;
            }

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
        const std::vector<std::unique_ptr<Entity>>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        const float batteryOuterRadius = ctx.tileSize * ctx.viewScale;
        for (const auto& entity : entities)
        {
            if (!entity || !HasTag(*entity, kTagBattery))
            {
                continue;
            }
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
        const std::vector<std::unique_ptr<Entity>>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        const float laserFeather = ctx.tileSize * ctx.viewScale * 0.6f;
        for (const auto& entity : entities)
        {
            if (!entity || !HasTag(*entity, kTagLaserBeam))
            {
                continue;
            }
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
        const std::vector<std::unique_ptr<Entity>>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        const float blasterBulletOuterRadius = ctx.tileSize * ctx.viewScale * 0.9f;
        for (const auto& entity : entities)
        {
            if (!entity || !HasTag(*entity, kTagBullet))
            {
                continue;
            }
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
        const std::vector<std::unique_ptr<Entity>>& entities,
        const DarknessOverlayContext& ctx,
        std::vector<OverlayLightSource>& overlayLights)
    {
        AddPlayerOverlayLight(overlayLights, ctx, 74.0f * ctx.viewScale, 170.0f * ctx.viewScale);
        CollectMarkerLightOverlayLights(entities, ctx, overlayLights);
        CollectStageLightOverlayLights(entities, ctx, overlayLights);

        if (ctx.tileSize > 0.0f)
        {
            CollectBatteryOverlayLights(entities, ctx, overlayLights);
            CollectLaserBeamOverlayLights(entities, ctx, overlayLights);
            CollectBlasterBulletOverlayLights(entities, ctx, overlayLights);
        }
    }

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
    if (!m_darknessStageEnabled)
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

    if (m_flow.cameraFlash.enabled && m_flow.cameraFlash.pulseRemaining > 0.0f && m_flow.cameraFlash.pulseDuration > 0.0f)
    {
        const float flashT = Clamp01(m_flow.cameraFlash.pulseRemaining / m_flow.cameraFlash.pulseDuration);
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
    CollectDarknessOverlayLights(m_entities, ctx, overlayLights);

    const int renderedLightLimit = (std::min)(kMaxDarknessOverlayLights, kDarknessOverlayActiveLightLimit);
    if (overlayLights.size() > static_cast<size_t>(renderedLightLimit))
    {
        std::partial_sort(
            overlayLights.begin(),
            overlayLights.begin() + renderedLightLimit,
            overlayLights.end(),
            [](const OverlayLightSource& a, const OverlayLightSource& b)
            {
                return a.priority > b.priority;
            });
        overlayLights.resize(renderedLightLimit);
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
    const bool enabled =
        m_debug.sepiaFilmFilterDryRunEnabled ||
        m_photo.capture.selectedTheme == PhotoFilterTheme::Sepia;
    if (!enabled || !m_flow.cameraMode)
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
    const float drawX = GetViewOriginX() + (frameX - m_flow.cameraX) * viewScale;
    const float drawY = GetViewOriginY() + (frameY - m_flow.cameraY) * viewScale;
    const int left = static_cast<int>(std::round(drawX));
    const int top = static_cast<int>(std::round(drawY));
    const int right = static_cast<int>(std::round(drawX + frameWidth * viewScale));
    const int bottom = static_cast<int>(std::round(drawY + frameHeight * viewScale));
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
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagSepiaRubble))
        {
            continue;
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
        Shader_ResetStyle();
        Shader_SetTint(1.0f, 1.0f, 1.0f, 0.9f);
        SpriteDraw(
            m_assets.GetTexture("sepia_ground"),
            drawEntityX, drawEntityY,
            drawEntityW, drawEntityH,
            sourceX, sourceY, sourceW, sourceH);

    }
    Shader_ResetStyle();
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void GameScene::DrawMarkerLightOutlines() const
{
    if (!m_darknessStageEnabled)
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const unsigned int outlineColor = GetColor(248, 248, 252);

    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagMarkerLight))
        {
            continue;
        }

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
    if (!m_flow.cameraMode && m_flow.shutterFlashRemaining <= 0.0f)
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
    const float drawX = viewOriginX + (frameX - m_flow.cameraX) * viewScale;
    const float drawY = viewOriginY + (frameY - m_flow.cameraY) * viewScale;
    const float drawWidth = frameWidth * viewScale;
    const float drawHeight = frameHeight * viewScale;
    const int left = static_cast<int>(std::round(drawX));
    const int top = static_cast<int>(std::round(drawY));
    const int right = static_cast<int>(std::round(drawX + drawWidth));
    const int bottom = static_cast<int>(std::round(drawY + drawHeight));

    const float shutterT = Clamp01(m_flow.shutterFlashRemaining / gShutterFlashSeconds);
    const float frameInset = 10.0f * shutterT * viewScale;
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

    const int cornerLength = std::max(18, static_cast<int>(std::round(34.0f * viewScale)));
    const int cornerThickness = std::max(2, static_cast<int>(std::round(3.0f + shutterT * 2.0f)));
    const int guideInset = std::max(12, static_cast<int>(std::round(24.0f * viewScale)));

    if (m_flow.cameraMode)
    {
        const float vignetteAlpha = 0.15f + shutterT * 0.08f;
        const float edge0 = 34.0f * viewScale;
        const float edge1 = 72.0f * viewScale;
        const float edge2 = 118.0f * viewScale;
        const float edge3 = 164.0f * viewScale;
        const float topBottomBoost = 1.22f;
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
    drawFrameBand(drawX, drawY, drawWidth, std::max(4.0f, 8.0f * viewScale), 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX, drawY + drawHeight - std::max(4.0f, 8.0f * viewScale), drawWidth, std::max(4.0f, 8.0f * viewScale), 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX, drawY, std::max(4.0f, 8.0f * viewScale), drawHeight, 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX + drawWidth - std::max(4.0f, 8.0f * viewScale), drawY, std::max(4.0f, 8.0f * viewScale), drawHeight, 0.30f + shutterT * 0.16f);
    drawCornerFrame(left, top, right, bottom, cornerThickness, cornerLength, frameColor);

    const int centerX = (left + right) / 2;
    const int centerY = (top + bottom) / 2;
    DrawLine(centerX - guideInset, centerY, centerX + guideInset, centerY, guideColor);
    DrawLine(centerX, centerY - guideInset, centerX, centerY + guideInset, guideColor);
    DrawBox(left + guideInset, top + guideInset, right - guideInset, bottom - guideInset, guideColor, FALSE);

    if (Entity* target = FindCaptureTarget(*transform))
    {
        if (const auto* targetTransform = target->GetComponent<TransformComponent>())
        {
            const float targetDrawX = viewOriginX + (targetTransform->x - m_flow.cameraX) * viewScale;
            const float targetDrawY = viewOriginY + (targetTransform->y - m_flow.cameraY) * viewScale;
            const float targetDrawWidth = targetTransform->width * targetTransform->scale * viewScale;
            const float targetDrawHeight = targetTransform->height * targetTransform->scale * viewScale;
            Shader_SetOutline(0.34f, 1.0f, 0.48f, 1.0f, 1.8f);
            Shader_SetTint(0.10f, 0.30f, 0.14f, 0.12f);
            SpriteDraw(m_whiteTexture, targetDrawX, targetDrawY, targetDrawWidth, targetDrawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    if (m_flow.shutterFlashRemaining > 0.0f)
    {
        Shader_ResetStyle();
        Shader_SetTint(overlayR, overlayG, overlayB, 0.10f + shutterT * 0.55f);
        SpriteDraw(m_whiteTexture, overlayLeft, overlayTop, overlayWidth, overlayHeight, 0.0f, 0.0f, 1.0f, 1.0f);

        const int pulseInset = static_cast<int>(std::round(20.0f * shutterT * viewScale));
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
        "Game Tuning",
        GetColor(245, 248, 255));
    DrawString(
        static_cast<int>(kTuningPanelX + 16.0f),
        static_cast<int>(kTuningPanelY + 42.0f),
        "F1 close  Arrow keys adjust  Click +/- writes assets/tuning.json",
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

    drawSectionHeader(kTuningPanelY + 76.0f, "Camera");
    drawSectionHeader(kTuningPanelY + 76.0f + (2.0f * kTuningRowHeight) + kTuningSectionHeaderHeight + kTuningSectionGap, "Player");
    drawSectionHeader(kTuningPanelY + 76.0f + (12.0f * kTuningRowHeight) + (kTuningSectionHeaderHeight + kTuningSectionGap) * 2.0f, "Photo");

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

void GameScene::DrawDevelopedPhotoPreview() const
{
    if (m_flow.developedPhotoPreviewRemaining <= 0.0f || !m_photo.pendingStore.active || m_photo.pendingStore.capture.items.empty())
    {
        return;
    }

    const PhotoCaptureState& previewCapture = m_photo.pendingStore.capture;
    constexpr float kPreviewLifetime = 4.2f;
    const float progress = 1.0f - Clamp01(m_flow.developedPhotoPreviewRemaining / kPreviewLifetime);
    const float cardPhaseT = Clamp01(progress / 0.34f);
    const float orbPhaseT = Clamp01((progress - 0.14f) / 0.30f);
    const float orbArriveT = Clamp01((progress - 0.34f) / 0.10f);
    const float finalFade = Clamp01(m_flow.developedPhotoPreviewRemaining / 0.34f);

    float accentR = 0.32f;
    float accentG = 0.92f;
    float accentB = 1.0f;
    GetPhotoFilterThemeOverlayColor(previewCapture.capturedTheme, accentR, accentG, accentB);

    const float trayWidth = kPhotoTraySlotCount * kPhotoTraySlotWidth + (kPhotoTraySlotCount - 1) * kPhotoTraySlotGap;
    const float trayX = (static_cast<float>(SCREEN_WIDTH) - trayWidth) * 0.5f;
    const float hiddenOffset = (1.0f - m_flow.photoTrayReveal) * (kPhotoTraySlotHeight + 36.0f);
    const float trayY = static_cast<float>(SCREEN_HEIGHT) - kPhotoTraySlotHeight - 28.0f + hiddenOffset;
    const float targetSlotX = trayX + m_photo.pendingStore.slotIndex * (kPhotoTraySlotWidth + kPhotoTraySlotGap);
    const float targetCenterX = targetSlotX + 54.0f;
    const float targetCenterY = trayY + 53.0f;

    const float photoWidth = 220.0f;
    const float photoHeight = 248.0f;
    const float frameInset = 16.0f;
    const float imageWidth = photoWidth - frameInset * 2.0f;
    const float imageHeight = 150.0f;
    const float baseX = static_cast<float>(SCREEN_WIDTH) - photoWidth - 42.0f;
    const float startY = static_cast<float>(SCREEN_HEIGHT) + 30.0f;
    const float cardExitX = static_cast<float>(SCREEN_WIDTH) - 126.0f;
    const float cardCruiseY = 34.0f;

    const float cardRiseT = Clamp01(cardPhaseT / 0.56f);
    const float cardSlideT = Clamp01((cardPhaseT - 0.42f) / 0.58f);
    const float riseEase = cardRiseT * cardRiseT * (3.0f - 2.0f * cardRiseT);
    const float slideEase = cardSlideT * cardSlideT * (3.0f - 2.0f * cardSlideT);
    const float overshootT = std::sin(cardRiseT * 3.14159265f);
    const float settleScale = 0.95f + std::sin(cardRiseT * 1.57079632f) * 0.05f;
    const float pauseSlow = cardPhaseT >= 0.44f && cardPhaseT <= 0.68f
        ? std::sin(((cardPhaseT - 0.44f) / 0.24f) * 3.14159265f)
        : 0.0f;
    const float cardX = baseX + (1.0f - riseEase) * 4.0f;
    const float cardY = std::lerp(startY, cardCruiseY, riseEase) - overshootT * 14.0f + pauseSlow * 10.0f;
    const float cardAlpha = Clamp01(1.0f - Clamp01((progress - 0.22f) / 0.12f)) * finalFade;

    if (cardAlpha > 0.0f)
    {
        const float visibleHeight = photoHeight * settleScale;
        const float previewScaleOut = 1.0f;
        const float imageScaleOut = settleScale;

        DrawBox(
            static_cast<int>(std::round(cardX + 8.0f)),
            static_cast<int>(std::round(cardY + 10.0f)),
            static_cast<int>(std::round(cardX + photoWidth + 8.0f)),
            static_cast<int>(std::round(cardY + visibleHeight + 10.0f)),
            GetColor(16, 18, 24),
            TRUE);

        Shader_ResetStyle();
        Shader_SetTint(accentR, accentG, accentB, 0.14f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX - 8.0f, cardY - 8.0f, photoWidth + 16.0f, visibleHeight + 16.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(0.98f, 0.96f, 0.90f, 0.96f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY, photoWidth, visibleHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(accentR, accentG, accentB, 0.20f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY, photoWidth, 20.0f * imageScaleOut, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(0.92f, 0.88f, 0.74f, 0.12f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY + 22.0f * imageScaleOut, photoWidth, 10.0f * imageScaleOut, 0.0f, 0.0f, 1.0f, 1.0f);

        const float photoX = cardX + frameInset * previewScaleOut;
        const float photoY = cardY + frameInset * imageScaleOut;
        Shader_SetTint(0.12f, 0.14f, 0.18f, 0.88f * cardAlpha);
        SpriteDraw(m_whiteTexture, photoX - 3.0f, photoY - 3.0f, imageWidth + 6.0f, imageHeight * imageScaleOut + 6.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        const float previewScale = std::min(
            imageWidth / std::max(1.0f, previewCapture.width),
            imageHeight / std::max(1.0f, previewCapture.height));
        const float previewPop = settleScale + overshootT * 0.015f;
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

    const float launchX = cardX + photoWidth * 0.5f - 18.0f;
    const float launchY = cardCruiseY + photoHeight * 0.48f;
    const float control1X = launchX + 6.0f;
    const float control1Y = launchY - 172.0f;
    const float control2X = targetCenterX + 4.0f;
    const float control2Y = targetCenterY - 138.0f;
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
    if (m_flow.photoTrayReveal <= 0.05f)
    {
        return false;
    }

    const float trayWidth = kPhotoTraySlotCount * kPhotoTraySlotWidth + (kPhotoTraySlotCount - 1) * kPhotoTraySlotGap;
    const float trayX = (static_cast<float>(SCREEN_WIDTH) - trayWidth) * 0.5f;
    const float hiddenOffset = (1.0f - m_flow.photoTrayReveal) * (kPhotoTraySlotHeight + 36.0f);
    const float trayY = static_cast<float>(SCREEN_HEIGHT) - kPhotoTraySlotHeight - 28.0f + hiddenOffset;
    return
        screenX >= trayX &&
        screenX <= trayX + trayWidth &&
        screenY >= trayY &&
        screenY <= trayY + kPhotoTraySlotHeight;
}

void GameScene::DrawPhotoStorageTray() const
{
    if (m_flow.photoTrayReveal <= 0.01f)
    {
        return;
    }

    constexpr float kInnerPadding = 10.0f;
    const float trayWidth = kPhotoTraySlotCount * kPhotoTraySlotWidth + (kPhotoTraySlotCount - 1) * kPhotoTraySlotGap;
    const float trayX = (static_cast<float>(SCREEN_WIDTH) - trayWidth) * 0.5f;
    const float hiddenOffset = (1.0f - m_flow.photoTrayReveal) * (kPhotoTraySlotHeight + 36.0f);
    const float trayY = static_cast<float>(SCREEN_HEIGHT) - kPhotoTraySlotHeight - 28.0f + hiddenOffset;
    const int textBright = static_cast<int>(150.0f + m_flow.photoTrayReveal * 105.0f);
    const int textBrightCool = std::min(255, textBright + 10);

    for (int slotIndex = 0; slotIndex < kPhotoTraySlotCount; ++slotIndex)
    {
        const bool slotIsPending = m_photo.pendingStore.active && m_photo.pendingStore.slotIndex == slotIndex;
        const PhotoCaptureState& storedCapture = slotIsPending ? m_photo.pendingStore.capture : m_photo.savedCaptures[slotIndex];
        const bool selected = slotIndex == m_photo.selectedCaptureSlot;
        const float slotX = trayX + slotIndex * (kPhotoTraySlotWidth + kPhotoTraySlotGap);
        const float slotY = trayY;
        const unsigned int fillColor = selected ? GetColor(30, 42, 56) : GetColor(16, 22, 30);
        const unsigned int outlineColor = selected ? GetColor(255, 234, 156) : GetColor(188, 204, 224);

        DrawBox(
            static_cast<int>(std::round(slotX)),
            static_cast<int>(std::round(slotY)),
            static_cast<int>(std::round(slotX + kPhotoTraySlotWidth)),
            static_cast<int>(std::round(slotY + kPhotoTraySlotHeight)),
            fillColor,
            TRUE);
        DrawBox(
            static_cast<int>(std::round(slotX)),
            static_cast<int>(std::round(slotY)),
            static_cast<int>(std::round(slotX + kPhotoTraySlotWidth)),
            static_cast<int>(std::round(slotY + kPhotoTraySlotHeight)),
            outlineColor,
            FALSE);

        DrawFormatString(
            static_cast<int>(slotX + 10.0f),
            static_cast<int>(slotY + 8.0f),
            selected ? GetColor(255, textBright, 196) : GetColor(textBright, textBrightCool, 244),
            "PHOTO %d",
            slotIndex + 1);

        if (!storedCapture.hasPhoto || storedCapture.items.empty())
        {
            DrawString(
                static_cast<int>(slotX + 50.0f),
                static_cast<int>(slotY + 42.0f),
                slotIsPending ? "STORING" : "EMPTY",
                slotIsPending ? GetColor(214, 204, 156) : GetColor(122, 136, 156));
            continue;
        }

        const float previewX = slotX + kInnerPadding;
        const float previewY = slotY + 28.0f;
        const float previewWidth = 88.0f;
        const float previewHeight = 50.0f;
        const float scale = std::min(
            previewWidth / std::max(1.0f, storedCapture.width),
            previewHeight / std::max(1.0f, storedCapture.height));
        const float contentX = previewX + (previewWidth - storedCapture.width * scale) * 0.5f;
        const float contentY = previewY + (previewHeight - storedCapture.height * scale) * 0.5f;

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
                contentX + item.relativeX * scale,
                contentY + item.relativeY * scale,
                item.width * scale,
                item.height * scale,
                1.0f);
        }

        DrawBox(
            static_cast<int>(std::round(previewX)),
            static_cast<int>(std::round(previewY)),
            static_cast<int>(std::round(previewX + previewWidth)),
            static_cast<int>(std::round(previewY + previewHeight)),
            GetColor(215, 205, 180),
            FALSE);

        DrawFormatString(
            static_cast<int>(slotX + 108.0f),
            static_cast<int>(slotY + 34.0f),
            GetColor(230, 236, 242),
            "%s",
            GetPhotoFilterThemeLabel(storedCapture.capturedTheme));
        DrawFormatString(
            static_cast<int>(slotX + 108.0f),
            static_cast<int>(slotY + 56.0f),
            GetColor(170, 186, 204),
            "%.0fx%.0f",
            storedCapture.width,
            storedCapture.height);
        DrawFormatString(
            static_cast<int>(slotX + 108.0f),
            static_cast<int>(slotY + 74.0f),
            slotIsPending ? GetColor(230, 214, 158) : GetColor(150, 170, 190),
            "%s",
            slotIsPending ? "Storing..." : (selected ? "Selected" : "Click to select"));
    }
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

    DrawBackdropGridInView(viewOriginX, viewOriginY, viewWidth, viewHeight, viewScale);

    DrawBackdropFrameInView(viewOriginX, viewOriginY, viewWidth, viewHeight);

    DrawCameraWorldInView(viewOriginX, viewOriginY, viewScale);

    DrawPhotoFilterPanelInView();

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void GameScene::DrawBackdropBaseInView(
    float viewOriginX,
    float viewOriginY,
    float viewWidth,
    float viewHeight,
    float viewScale) const
{
    const int bgTexture = m_assets.GetTexture("sinrin10");
    const int bg1Texture = m_assets.GetTexture("sinrin11");

	if (bgTexture < 0 || bg1Texture < 0)
    {
        return;
    }

    // �e�N�X�`���T�C�Y sinrin10
    const int texW = TextureGetWidth(bgTexture);
    const int texH = TextureGetHeight(bgTexture);
    if (texW <= 0 || texH <= 0)
    {
        return;
    }

    // �e�N�X�`���T�C�Y sinrin11
    const int texW1 = TextureGetWidth(bg1Texture);
    const int texH1 = TextureGetHeight(bg1Texture);
    if (texW1 <= 0 || texH1 <= 0)
    {
        return;
    }

    // �`��T�C�Y�i��ʗ̈�j
    const float drawW = viewWidth;
    const float drawH = viewHeight;

    // �`��T�C�Y�i��ʗ̈�jbg1
    const float drawW1 = viewWidth;
    const float drawH1 = viewHeight;

    // �p�����b�N�X�W���i0.0 = �Œ�w�i�A1.0 = �J�����Ɠ����j
    // �K�v�ɉ����Ă�����ύX�i��: 0.2f, 0.5f, 1.0f�j
    const float parallaxX = 0.45f;
    const float parallaxY = 0.45f;

    // �p�����b�N�X�W���i0.0 = �Œ�w�i�A1.0 = �J�����Ɠ����j
    // �K�v�ɉ����Ă�����ύX�i��: 0.2f, 0.5f, 1.0f�jbg1
    const float parallaxX1 = 0.45f;
    const float parallaxY1 = 0.45f;

    // �J�����̃��[���h�ʒu���g����UV�X�N���[���ʂ��v�Z�i0..1�j
    float scrollU = std::fmod((m_flow.cameraX * parallaxX) / static_cast<float>(texW), 1.0f);
    if (scrollU < 0.0f) scrollU += 1.0f;
    float scrollV = std::fmod((m_flow.cameraY * parallaxY) / static_cast<float>(texH), 1.0f);
    if (scrollV < 0.0f) scrollV += 1.0f;

    // �J�����̃��[���h�ʒu���g����UV�X�N���[���ʂ��v�Z�i0..1�jbg1
    float scrollU1 = std::fmod((m_flow.cameraX * parallaxX1) / static_cast<float>(texW1), 1.0f);
    if (scrollU1 < 0.0f) scrollU1 += 1.0f;
    float scrollV1 = std::fmod((m_flow.cameraY * parallaxY1) / static_cast<float>(texH1), 1.0f);
    if (scrollV1 < 0.0f) scrollV1 += 1.0f;

    // UV�䂩���ʏ�̕��������v�Z�i��/�E, ��/���j
    const float leftUVWidth = 1.0f - scrollU;
    const float rightUVWidth = scrollU;
    const float topUVHeight = 1.0f - scrollV;
    const float bottomUVHeight = scrollV;

    // UV�䂩���ʏ�̕��������v�Z�i��/�E, ��/���jbg1
    const float leftUVWidth1 = 1.0f - scrollU1;
    const float rightUVWidth1 = scrollU1;
    const float topUVHeight1 = 1.0f - scrollV1;
    const float bottomUVHeight1 = scrollV1;

    const float leftDrawW = drawW * leftUVWidth;
    const float rightDrawW = drawW - leftDrawW; // = drawW * rightUVWidth
    const float topDrawH = drawH * topUVHeight;
    const float bottomDrawH = drawH - topDrawH; // = drawH * bottomUVHeight

	// UV�䂩���ʏ�̕��������v�Z�i��/�E, ��/���jbg1
    const float leftDrawW1 = drawW * leftUVWidth1;
    const float rightDrawW1 = drawW - leftDrawW1; // = drawW * rightUVWidth1
    const float topDrawH1 = drawH * topUVHeight1;
    const float bottomDrawH1 = drawH - topDrawH1; // = drawH * bottomUVHeight1

    // �ŏ��`�敝/������臒l�i���܂�ɏ�������Ε`���Ȃ��j
    constexpr float kMinDrawSize = 0.5f;

    // 4�����ŕ`��F����A�E��A�����A�E��
    // ����
    if (leftDrawW > kMinDrawSize && topDrawH > kMinDrawSize)
    {
        SpriteDraw(
            bgTexture,
            viewOriginX,
            viewOriginY,
            leftDrawW,
            topDrawH,
            scrollU,
            scrollV,
            leftUVWidth,
            topUVHeight,
            false,
            0.0f);
    }
	//����bg1
    if (leftDrawW1 > kMinDrawSize && topDrawH1 > kMinDrawSize)
    {
        SpriteDraw(
            bg1Texture,
            viewOriginX,
            viewOriginY,
            leftDrawW1,
            topDrawH1,
            scrollU1,
            scrollV1,
            leftUVWidth1,
            topUVHeight1,
            false,
            0.0f);
    }

    // �E��
    if (rightDrawW > kMinDrawSize && topDrawH > kMinDrawSize)
    {
        SpriteDraw(
            bgTexture,
            viewOriginX + leftDrawW,
            viewOriginY,
            rightDrawW,
            topDrawH,
            0.0f,
            scrollV,
            rightUVWidth,
            topUVHeight,
            false,
            0.0f);
    }

    // �E��bg1
    if (rightDrawW1 > kMinDrawSize && topDrawH1 > kMinDrawSize)
    {
        SpriteDraw(
            bg1Texture,
            viewOriginX + leftDrawW1,
            viewOriginY,
            rightDrawW1,
            topDrawH1,
            0.0f,
            scrollV1,
            rightUVWidth1,
            topUVHeight1,
            false,
            0.0f);
    }

    // ����
    if (leftDrawW > kMinDrawSize && bottomDrawH > kMinDrawSize)
    {
        SpriteDraw(
            bgTexture,
            viewOriginX,
            viewOriginY + topDrawH,
            leftDrawW,
            bottomDrawH,
            scrollU,
            0.0f,
            leftUVWidth,
            bottomUVHeight,
            false,
            0.0f);
    }

    // ����bg1
    if (leftDrawW1 > kMinDrawSize && bottomDrawH1 > kMinDrawSize)
    {
        SpriteDraw(
            bg1Texture,
            viewOriginX,
            viewOriginY + topDrawH1,
            leftDrawW1,
            bottomDrawH1,
            scrollU1,
            0.0f,
            leftUVWidth1,
            bottomUVHeight1,
            false,
            0.0f);
    }

    // �E��
    if (rightDrawW > kMinDrawSize && bottomDrawH > kMinDrawSize)
    {
        SpriteDraw(
            bgTexture,
            viewOriginX + leftDrawW,
            viewOriginY + topDrawH,
            rightDrawW,
            bottomDrawH,
            0.0f,
            0.0f,
            rightUVWidth,
            bottomUVHeight,
            false,
            0.0f);
    }
    // �E��bg1
    if (rightDrawW1 > kMinDrawSize && bottomDrawH1 > kMinDrawSize)
    {
        SpriteDraw(
            bg1Texture,
            viewOriginX + leftDrawW1,
            viewOriginY + topDrawH1,
            rightDrawW1,
            bottomDrawH1,
            0.0f,
            0.0f,
            rightUVWidth1,
            bottomUVHeight1,
            false,
            0.0f);
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
                const bool sourceMatches = link.sourceMapCsv == "*" || link.sourceMapCsv == gCurrentMapCsvPath;
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

            DrawBox(left, top, right, bottom, GetColor(255, 210, 90), FALSE);

            const std::string destName = GetMapDisplayName(transition->destinationMapCsv);
            DrawFormatString(left, top - 16, GetColor(180, 240, 255), "-> %s", destName.c_str());
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

void GameScene::DrawCameraWorldInView(float viewOriginX, float viewOriginY, float viewScale) const
{
    m_tileMap.Draw(m_tileTexture, viewOriginX - m_flow.cameraX * viewScale, viewOriginY - m_flow.cameraY * viewScale, viewScale);
    DrawStageTransitionMarkersInView(viewOriginX, viewOriginY, viewScale);
    DrawMapEditorMarkersInView(viewOriginX, viewOriginY, viewScale);
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

void GameScene::DrawStageGuideInView() const
{
    if (const Entity* player = FindEntityByTag(kTagPlayer))
    {
        if (const auto* transform = player->GetComponent<TransformComponent>())
        {
            DrawString(
                24,
                SCREEN_HEIGHT - 42,
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

    const int panelWidth = 308;
    const int panelX = SCREEN_WIDTH - panelWidth - 22;
    const int panelY = 18;
    const int panelHeight = 78;
    DrawBox(panelX, panelY, panelX + panelWidth, panelY + panelHeight, GetColor(14, 18, 24), TRUE);
    DrawBox(panelX, panelY, panelX + panelWidth, panelY + panelHeight, GetColor(220, 228, 236), FALSE);
    DrawBox(
        panelX + 10,
        panelY + 10,
        panelX + 44,
        panelY + 44,
        GetColor(
            static_cast<int>(filterR * 255.0f),
            static_cast<int>(filterG * 255.0f),
            static_cast<int>(filterB * 255.0f)),
        TRUE);
    DrawFormatString(
        panelX + 56,
        panelY + 10,
        GetColor(245, 248, 255),
        "Filter: %s",
        GetPhotoFilterThemeLabel(m_photo.capture.selectedTheme));
    DrawFormatString(
        panelX + 56,
        panelY + 32,
        GetColor(180, 210, 235),
        "%s",
        GetPhotoFilterThemeEffectText(m_photo.capture.selectedTheme));
    DrawFormatString(
        panelX + 12,
        panelY + 54,
        GetColor(205, 220, 235),
        "C cycle  1 None  2 Hot  3 Cold  4 Invert  5 Sepia");
}

void GameScene::GetCaptureFrameRect(const TransformComponent& playerTransform, float& x, float& y, float& width, float& height) const
{
    static_cast<void>(playerTransform);
    width = m_tileMap.GetTileSize() * gCaptureWidthTiles * m_flow.captureFinderScale;
    height = m_tileMap.GetTileSize() * gCaptureHeightTiles * m_flow.captureFinderScale;

    const float cursorStartWorldX = m_flow.cameraX + gCameraViewWidth * 0.5f;
    const float cursorStartWorldY = m_flow.cameraY + gCameraViewHeight * 0.5f;

    static float padCursorWorldX = cursorStartWorldX;
    static float padCursorWorldY = cursorStartWorldY;
    static float padCursorVelocityX = 0.0f;
    static float padCursorVelocityY = 0.0f;
    static unsigned int lastTimeMs = 0;
    static bool initialized = false;
    static int lastSessionId = -1;
    static float lastPadInputSeconds = -1000.0f;
    static int lastMouseX = Input_GetMouseX();
    static int lastMouseY = Input_GetMouseY();

    if (!initialized)
    {
        padCursorWorldX = cursorStartWorldX;
        padCursorWorldY = cursorStartWorldY;
        initialized = true;
    }

    if (lastSessionId != m_flow.cameraModeSessionId)
    {
        padCursorWorldX = cursorStartWorldX;
        padCursorWorldY = cursorStartWorldY;
        padCursorVelocityX = 0.0f;
        padCursorVelocityY = 0.0f;
        lastPadInputSeconds = -1000.0f;
        lastSessionId = m_flow.cameraModeSessionId;
    }

    const unsigned int nowMs = static_cast<unsigned int>(GetNowCount());
    const float dt = lastTimeMs ? (static_cast<float>(nowMs - lastTimeMs) / 1000.0f) : (1.0f / 60.0f);
    lastTimeMs = nowMs;

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const bool mouseMoved = mouseX != lastMouseX || mouseY != lastMouseY;
    lastMouseX = mouseX;
    lastMouseY = mouseY;

    const auto computeCaptureViewTransform = [this](float& outScale, float& outOriginX, float& outOriginY)
    {
        const float marginX = std::clamp(static_cast<float>(SCREEN_WIDTH) * 0.04f, 48.0f, 96.0f);
        const float marginY = std::clamp(static_cast<float>(SCREEN_HEIGHT) * 0.04f, 36.0f, 72.0f);
        const float maxWidth = static_cast<float>(SCREEN_WIDTH) - marginX * 2.0f;
        const float maxHeight = static_cast<float>(SCREEN_HEIGHT) - marginY * 2.0f;
        const float containScale = std::max(1.0f, std::min(maxWidth / gCameraViewWidth, maxHeight / gCameraViewHeight));

        float baseCameraZoomMultiplier = 1.0f;
        const float tileSize = m_tileMap.GetTileSize();
        if (tileSize > 0.0f)
        {
            const float targetWorldWidth = tileSize * 23.0f;
            if (targetWorldWidth > 0.0f)
            {
                baseCameraZoomMultiplier = std::max(1.0f, static_cast<float>(SCREEN_WIDTH) / targetWorldWidth);
            }
        }

        const float preMultiplier = m_mapEditor.active ? 1.0f : baseCameraZoomMultiplier;
        const float preScale = containScale * preMultiplier;
        const float preWidth = gCameraViewWidth * preScale;
        const float preHeight = gCameraViewHeight * preScale;
        const float preOriginX = preWidth >= static_cast<float>(SCREEN_WIDTH)
            ? 0.0f
            : std::round((static_cast<float>(SCREEN_WIDTH) - preWidth) * 0.5f);
        const float preOriginY = preHeight >= static_cast<float>(SCREEN_HEIGHT)
            ? 0.0f
            : std::round((static_cast<float>(SCREEN_HEIGHT) - preHeight) * 0.5f);
        const float anchorX = preOriginX + preWidth * 0.5f;
        const float anchorY = preOriginY + preHeight * 0.5f;

        const float zoomBlend = m_flow.captureModeZoomBlend * m_flow.captureModeZoomBlend * (3.0f - 2.0f * m_flow.captureModeZoomBlend);
        const float finalMultiplier = m_mapEditor.active ? 1.0f : (baseCameraZoomMultiplier + zoomBlend * 0.08f);
        outScale = containScale * finalMultiplier;
        const float finalWidth = gCameraViewWidth * outScale;
        const float finalHeight = gCameraViewHeight * outScale;

        if (finalWidth >= static_cast<float>(SCREEN_WIDTH))
        {
            outOriginX = (m_flow.cameraMode && !m_mapEditor.active)
                ? std::round(anchorX - finalWidth * 0.5f)
                : 0.0f;
        }
        else
        {
            outOriginX = std::round((static_cast<float>(SCREEN_WIDTH) - finalWidth) * 0.5f);
        }

        if (finalHeight >= static_cast<float>(SCREEN_HEIGHT))
        {
            outOriginY = (m_flow.cameraMode && !m_mapEditor.active)
                ? std::round(anchorY - finalHeight * 0.5f)
                : 0.0f;
        }
        else
        {
            outOriginY = std::round((static_cast<float>(SCREEN_HEIGHT) - finalHeight) * 0.5f);
        }
    };

    float viewScale = 1.0f;
    float viewOriginX = 0.0f;
    float viewOriginY = 0.0f;
    computeCaptureViewTransform(viewScale, viewOriginX, viewOriginY);
    const float mouseWorldX = ((static_cast<float>(mouseX) - viewOriginX) / viewScale) + m_flow.cameraX;
    const float mouseWorldY = ((static_cast<float>(mouseY) - viewOriginY) / viewScale) + m_flow.cameraY;

    const float rightX = Input_GetRightStickX();
    const float rightY = Input_GetRightStickY();
    UpdatePadCursor(
        mouseWorldX,
        mouseWorldY,
        mouseMoved,
        rightX,
        rightY,
        dt,
        padCursorWorldX,
        padCursorWorldY,
        padCursorVelocityX,
        padCursorVelocityY,
        lastPadInputSeconds,
        static_cast<float>(nowMs) / 1000.0f);

    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    padCursorWorldX = std::clamp(padCursorWorldX, 0.0f, std::max(0.0f, mapWidth));
    padCursorWorldY = std::clamp(padCursorWorldY, 0.0f, std::max(0.0f, mapHeight));

    x = padCursorWorldX - width * 0.5f;
    y = std::clamp(padCursorWorldY - height * 0.5f, 0.0f, std::max(0.0f, mapHeight - height));
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
    for (const auto& entity : m_entities)
    {
        if (HasTag(*entity, kTagPlayer) ||
            HasTag(*entity, kTagEnemy) ||
            HasTag(*entity, kTagBatterySwitch) ||
            HasTag(*entity, kTagElevator) ||
            HasTag(*entity, kTagLaserSwitch) ||
            HasTag(*entity, kTagShutter) ||
            HasTag(*entity, kTagLaserBeam) ||
            HasTag(*entity, kTagStageLight))
        {
            continue;
        }

        if (HasTag(*entity, kTagPhotoBox))
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

        const auto* transform = entity->GetComponent<TransformComponent>();
        const auto* sprite = entity->GetComponent<SpriteRenderComponent>();
        if (!transform || !sprite || !IntersectsRect(captureFrame, *transform))
        {
            continue;
        }

        const float targetCenterX = transform->x + transform->width * transform->scale * 0.5f;
        const float playerCenterX = playerTransform.x + playerTransform.width * playerTransform.scale * 0.5f;
        const float distance = std::fabs(targetCenterX - playerCenterX);
        if (!bestTarget || distance < bestDistance)
        {
            bestTarget = entity.get();
            bestDistance = distance;
        }
    }

    return bestTarget;
}

void GameScene::DrawBatterySwitchCounters() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float tileOffsetY = m_tileMap.GetTileSize() * 2.0f * viewScale;

    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagBatterySwitch))
        {
            continue;
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        const auto* batterySwitch = entity->GetComponent<BatterySwitchComponent>();
        if (!transform || !batterySwitch)
        {
            continue;
        }

        const float drawX = viewOriginX + (transform->x - m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + (transform->y - m_flow.cameraY) * viewScale;
        const float drawWidth = transform->width * transform->scale * viewScale;
        const float panelWidth = 58.0f;
        const float panelHeight = 22.0f;
        const float panelX = drawX + drawWidth * 0.5f - panelWidth * 0.5f;
        const float panelY = drawY - tileOffsetY - 26.0f;

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
            batterySwitch->isPressed ? GetColor(180, 255, 196) : GetColor(242, 226, 190),
            FALSE);

        DrawFormatString(
            static_cast<int>(std::round(panelX + 10.0f)),
            static_cast<int>(std::round(panelY + 4.0f)),
            batterySwitch->isPressed ? GetColor(220, 255, 228) : GetColor(255, 244, 220),
            "%d/%d",
            batterySwitch->insertedBatteryCount,
            batterySwitch->requiredBatteryCount);
    }
}
