#include "game_scene_internal.h"
#include "photo_filter_rules.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    float EaseOutCubic(float t)
    {
        const float clamped = Clamp01(t);
        const float inv = 1.0f - clamped;
        return 1.0f - inv * inv * inv;
    }

    float EaseOutBack(float t)
    {
        const float clamped = Clamp01(t);
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        const float shifted = clamped - 1.0f;
        return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
    }

    void DrawWorldRectOutline(float worldX, float worldY, float worldWidth, float worldHeight, float cameraX, float cameraY, unsigned int color)
    {
        const float viewScale = GetViewScale();
        const float viewOriginX = GetViewOriginX();
        const float viewOriginY = GetViewOriginY();
        const int left = static_cast<int>(std::round(viewOriginX + (worldX - cameraX) * viewScale));
        const int top = static_cast<int>(std::round(viewOriginY + (worldY - cameraY) * viewScale));
        const int right = static_cast<int>(std::round(viewOriginX + (worldX + worldWidth - cameraX) * viewScale));
        const int bottom = static_cast<int>(std::round(viewOriginY + (worldY + worldHeight - cameraY) * viewScale));
        DrawBox(left, top, right, bottom, color, FALSE);
    }

    void DrawWorldPolygonOutline(
        const TransformComponent& transform,
        const ImageOutlineColliderComponent& collider,
        float cameraX,
        float cameraY,
        unsigned int color)
    {
        const auto& normalizedOutline = collider.GetNormalizedOutline();
        if (normalizedOutline.size() < 2)
        {
            return;
        }

        const float viewScale = GetViewScale();
        const float viewOriginX = GetViewOriginX();
        const float viewOriginY = GetViewOriginY();
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float centerX = transform.x + width * 0.5f;
        const float centerY = transform.y + height * 0.5f;

        std::vector<std::pair<int, int>> screenPoints;
        screenPoints.reserve(normalizedOutline.size());
        for (const b2Vec2& point : normalizedOutline)
        {
            float worldX = transform.x + point.x * width;
            float worldY = transform.y + point.y * height;
            if (std::fabs(transform.rotation) > 0.0001f)
            {
                const float localX = worldX - centerX;
                const float localY = worldY - centerY;
                const float cosTheta = std::cos(transform.rotation);
                const float sinTheta = std::sin(transform.rotation);
                worldX = centerX + (localX * cosTheta - localY * sinTheta);
                worldY = centerY + (localX * sinTheta + localY * cosTheta);
            }
            screenPoints.emplace_back(
                static_cast<int>(std::round(viewOriginX + (worldX - cameraX) * viewScale)),
                static_cast<int>(std::round(viewOriginY + (worldY - cameraY) * viewScale)));
        }

        for (size_t index = 0; index < screenPoints.size(); ++index)
        {
            const auto& a = screenPoints[index];
            const auto& b = screenPoints[(index + 1) % screenPoints.size()];
            DrawLine(a.first, a.second, b.first, b.second, color);
        }
    }

    const char* GetLayerEffectText(PhotoCopyLayer layer)
    {
        switch (layer)
        {
        case PhotoCopyLayer::Background:
            return "Visible only / pass through";
        case PhotoCopyLayer::Shadow:
            return "Black shadow / pass through";
        case PhotoCopyLayer::Foreground:
        default:
            return "Solid in world";
        }
    }

    void RotatePoint(float centerX, float centerY, float rotation, float& x, float& y)
    {
        if (std::fabs(rotation) <= 0.0001f)
        {
            return;
        }

        const float localX = x - centerX;
        const float localY = y - centerY;
        const float cosTheta = std::cos(rotation);
        const float sinTheta = std::sin(rotation);
        x = centerX + (localX * cosTheta - localY * sinTheta);
        y = centerY + (localX * sinTheta + localY * cosTheta);
    }

    bool DrawSlopeTriangle(float x, float y, float width, float height, int tileValue, const TintComponent* tint, bool flipX, float rotation, float alpha)
    {
        const TileTriangleShape triangle = TileMap::GetTriangleShape(tileValue);
        if (!tint || !triangle.isTriangle)
        {
            return false;
        }

        const int color = GetColor(
            static_cast<int>(std::round(tint->r * 255.0f)),
            static_cast<int>(std::round(tint->g * 255.0f)),
            static_cast<int>(std::round(tint->b * 255.0f)));
        const bool risesRight = flipX ? !triangle.risesRight : triangle.risesRight;
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        float cx = 0.0f;
        float cy = 0.0f;
        if (risesRight)
        {
            ax = x;
            ay = y + height;
            bx = x + width;
            by = y + height;
            cx = x + width;
            cy = y;
        }
        else
        {
            ax = x;
            ay = y;
            bx = x;
            by = y + height;
            cx = x + width;
            cy = y + height;
        }

        const float centerX = x + width * 0.5f;
        const float centerY = y + height * 0.5f;
        RotatePoint(centerX, centerY, rotation, ax, ay);
        RotatePoint(centerX, centerY, rotation, bx, by);
        RotatePoint(centerX, centerY, rotation, cx, cy);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(alpha * 255.0f)), 0, 255));
        DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        return true;
    }
    void DrawFilledQuad(
        float ax,
        float ay,
        float bx,
        float by,
        float cx,
        float cy,
        float dx,
        float dy,
        int color)
    {
        DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
        DrawTriangleAA(ax, ay, cx, cy, dx, dy, color, TRUE);
    }

    struct DamagePlatformPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    void ClipDamagePlatformPolygonAgainstEdge(
        const std::vector<DamagePlatformPoint>& input,
        std::vector<DamagePlatformPoint>& output,
        auto&& isInside,
        auto&& intersect)
    {
        output.clear();
        if (input.empty())
        {
            return;
        }

        DamagePlatformPoint previous = input.back();
        bool previousInside = isInside(previous);
        for (const DamagePlatformPoint& current : input)
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

    bool ClipDamagePlatformPolygonToCrop(
        std::vector<DamagePlatformPoint>& polygon,
        float cropLeft,
        float cropTop,
        float cropRight,
        float cropBottom)
    {
        std::vector<DamagePlatformPoint> scratch;
        auto clipVertical = [&](float edgeX, bool keepGreater)
        {
            ClipDamagePlatformPolygonAgainstEdge(
                polygon,
                scratch,
                [=](const DamagePlatformPoint& point)
                {
                    return keepGreater ? point.x >= edgeX : point.x <= edgeX;
                },
                [=](const DamagePlatformPoint& a, const DamagePlatformPoint& b)
                {
                    const float delta = b.x - a.x;
                    const float t = std::fabs(delta) <= 0.0001f ? 0.0f : (edgeX - a.x) / delta;
                    return DamagePlatformPoint{
                        a.x + (b.x - a.x) * std::clamp(t, 0.0f, 1.0f),
                        a.y + (b.y - a.y) * std::clamp(t, 0.0f, 1.0f)
                    };
                });
            polygon.swap(scratch);
        };
        auto clipHorizontal = [&](float edgeY, bool keepGreater)
        {
            ClipDamagePlatformPolygonAgainstEdge(
                polygon,
                scratch,
                [=](const DamagePlatformPoint& point)
                {
                    return keepGreater ? point.y >= edgeY : point.y <= edgeY;
                },
                [=](const DamagePlatformPoint& a, const DamagePlatformPoint& b)
                {
                    const float delta = b.y - a.y;
                    const float t = std::fabs(delta) <= 0.0001f ? 0.0f : (edgeY - a.y) / delta;
                    return DamagePlatformPoint{
                        a.x + (b.x - a.x) * std::clamp(t, 0.0f, 1.0f),
                        a.y + (b.y - a.y) * std::clamp(t, 0.0f, 1.0f)
                    };
                });
            polygon.swap(scratch);
        };

        clipVertical(cropLeft, true);
        clipVertical(cropRight, false);
        clipHorizontal(cropTop, true);
        clipHorizontal(cropBottom, false);
        return polygon.size() >= 3;
    }

    void DrawDamagePlatformPolygon(
        const std::vector<DamagePlatformPoint>& polygon,
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        float cropLeft,
        float cropTop,
        float cropWidth,
        float cropHeight,
        float rotation,
        int color)
    {
        if (polygon.size() < 3 || cropWidth <= 0.0001f || cropHeight <= 0.0001f)
        {
            return;
        }

        const float centerX = drawX + drawWidth * 0.5f;
        const float centerY = drawY + drawHeight * 0.5f;
        std::vector<DamagePlatformPoint> transformed;
        transformed.reserve(polygon.size());
        for (const DamagePlatformPoint& point : polygon)
        {
            float x = drawX + ((point.x - cropLeft) / cropWidth) * drawWidth;
            float y = drawY + ((point.y - cropTop) / cropHeight) * drawHeight;
            RotatePoint(centerX, centerY, rotation, x, y);
            transformed.push_back({ x, y });
        }

        for (size_t index = 1; index + 1 < transformed.size(); ++index)
        {
            DrawTriangleAA(
                transformed[0].x,
                transformed[0].y,
                transformed[index].x,
                transformed[index].y,
                transformed[index + 1].x,
                transformed[index + 1].y,
                color,
                TRUE);
        }
    }

    bool DrawDamagePlatformShape(
        float x,
        float y,
        float width,
        float height,
        const DamagePlatformComponent* damagePlatform,
        const TintComponent* tint,
        float sourceX,
        float sourceY,
        float sourceWidth,
        float sourceHeight,
        float rotation,
        float alpha)
    {
        if (!damagePlatform || !tint)
        {
            return false;
        }

        const int baseColor = GetColor(
            static_cast<int>(std::round(tint->r * 255.0f)),
            static_cast<int>(std::round(tint->g * 255.0f)),
            static_cast<int>(std::round(tint->b * 255.0f)));
        const int spikeColor = GetColor(235, 26, 26);
        const float cropLeft = std::clamp(sourceX, 0.0f, 1.0f);
        const float cropTop = std::clamp(sourceY, 0.0f, 1.0f);
        const float cropWidth = std::clamp(sourceWidth, 0.0001f, 1.0f);
        const float cropHeight = std::clamp(sourceHeight, 0.0001f, 1.0f);
        const float cropRight = std::clamp(cropLeft + cropWidth, 0.0f, 1.0f);
        const float cropBottom = std::clamp(cropTop + cropHeight, 0.0f, 1.0f);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(alpha * 255.0f)), 0, 255));

        std::vector<DamagePlatformPoint> basePolygon =
        {
            { 0.0f, 0.5f },
            { 1.0f, 0.5f },
            { 1.0f, 1.0f },
            { 0.0f, 1.0f }
        };
        if (ClipDamagePlatformPolygonToCrop(basePolygon, cropLeft, cropTop, cropRight, cropBottom))
        {
            DrawDamagePlatformPolygon(basePolygon, x, y, width, height, cropLeft, cropTop, cropRight - cropLeft, cropBottom - cropTop, rotation, baseColor);
        }

        const int spikeCount = (std::max)(1, damagePlatform->tileSpan);
        const float spikeWidth = 1.0f / static_cast<float>(spikeCount);
        for (int spikeIndex = 0; spikeIndex < spikeCount; ++spikeIndex)
        {
            std::vector<DamagePlatformPoint> spikePolygon =
            {
                { static_cast<float>(spikeIndex) * spikeWidth, 0.5f },
                { static_cast<float>(spikeIndex + 1) * spikeWidth, 0.5f },
                { (static_cast<float>(spikeIndex) + 0.5f) * spikeWidth, 0.0f }
            };
            if (!ClipDamagePlatformPolygonToCrop(spikePolygon, cropLeft, cropTop, cropRight, cropBottom))
            {
                continue;
            }

            DrawDamagePlatformPolygon(spikePolygon, x, y, width, height, cropLeft, cropTop, cropRight - cropLeft, cropBottom - cropTop, rotation, spikeColor);
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        return true;
    }

    bool DrawSpikeStripShape(
        float x,
        float y,
        float width,
        float height,
        const SpikeStripComponent* spikeStrip,
        const TintComponent* tint,
        float sourceX,
        float sourceY,
        float sourceWidth,
        float sourceHeight,
        float rotation,
        float alpha)
    {
        if (!spikeStrip || !tint)
        {
            return false;
        }

        const int spikeColor = GetColor(
            static_cast<int>(std::round(tint->r * 255.0f)),
            static_cast<int>(std::round(tint->g * 255.0f)),
            static_cast<int>(std::round(tint->b * 255.0f)));
        const float cropLeft = std::clamp(sourceX, 0.0f, 1.0f);
        const float cropTop = std::clamp(sourceY, 0.0f, 1.0f);
        const float cropWidth = std::clamp(sourceWidth, 0.0001f, 1.0f);
        const float cropHeight = std::clamp(sourceHeight, 0.0001f, 1.0f);
        const float cropRight = std::clamp(cropLeft + cropWidth, 0.0f, 1.0f);
        const float cropBottom = std::clamp(cropTop + cropHeight, 0.0f, 1.0f);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(alpha * 255.0f)), 0, 255));

        const int spikeCount = (std::max)(1, spikeStrip->tileSpan);
        const float spikeWidth = 1.0f / static_cast<float>(spikeCount);
        for (int spikeIndex = 0; spikeIndex < spikeCount; ++spikeIndex)
        {
            std::vector<DamagePlatformPoint> spikePolygon =
            {
                { static_cast<float>(spikeIndex) * spikeWidth, 1.0f },
                { static_cast<float>(spikeIndex + 1) * spikeWidth, 1.0f },
                { (static_cast<float>(spikeIndex) + 0.5f) * spikeWidth, 0.0f }
            };
            if (!ClipDamagePlatformPolygonToCrop(spikePolygon, cropLeft, cropTop, cropRight, cropBottom))
            {
                continue;
            }

            DrawDamagePlatformPolygon(spikePolygon, x, y, width, height, cropLeft, cropTop, cropRight - cropLeft, cropBottom - cropTop, rotation, spikeColor);
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        return true;
    }
        float ComputeLightFlicker(float timeSeconds, const TransformComponent& transform, const FlickerLightComponent& light)
    {
        const float seed = transform.x * 0.0137f + transform.y * 0.0091f + light.offsetY * 0.17f;
        const float waveA = std::sin(timeSeconds * light.flickerSpeed + seed);
        const float waveB = std::sin(timeSeconds * (light.flickerSpeed * 2.17f + 0.35f) + seed * 1.91f);
        const float waveC = std::cos(timeSeconds * (light.flickerSpeed * 1.37f + 0.22f) + seed * 0.73f);
        const float composite = waveA * 0.55f + waveB * 0.30f + waveC * 0.15f;
        return std::max(0.55f, 1.0f + composite * light.flickerAmplitude);
    }

    float ComputeGodRayPulse(float timeSeconds, const TransformComponent& transform, const FlickerLightComponent& light)
    {
        const float seed = transform.x * 0.0061f + transform.y * 0.0037f + light.godRayWidth * 0.011f;
        const float waveA = std::sin(timeSeconds * (0.8f + light.godRayDriftSpeed * 0.7f) + seed);
        const float waveB = std::cos(timeSeconds * (1.3f + light.godRayDriftSpeed) + seed * 1.7f);
        return 0.78f + (waveA * 0.12f + waveB * 0.10f);
    }

    float LerpFloat(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    float SmoothStep01(float t)
    {
        const float clamped = Clamp01(t);
        return clamped * clamped * (3.0f - 2.0f * clamped);
    }

    float Hash01(float value)
    {
        const float s = std::sin(value * 127.1f) * 43758.5453f;
        return s - std::floor(s);
    }

    float ValueNoise1D(float value)
    {
        const float base = std::floor(value);
        const float fraction = value - base;
        const float weight = SmoothStep01(fraction);
        return LerpFloat(Hash01(base), Hash01(base + 1.0f), weight);
    }

    float ComputeGodRayDensity(float normalizedY, float timeSeconds, const TransformComponent& transform, const FlickerLightComponent& light)
    {
        const float seed = transform.x * 0.0043f + transform.y * 0.0021f + light.godRayWidth * 0.017f;
        const float coarse = ValueNoise1D(normalizedY * 6.5f + timeSeconds * (0.22f + light.godRayDriftSpeed * 0.35f) + seed);
        const float detail = ValueNoise1D(normalizedY * 17.0f - timeSeconds * (0.55f + light.godRayDriftSpeed * 0.45f) + seed * 1.9f);
        const float streaks = ValueNoise1D(normalizedY * 29.0f + timeSeconds * 0.18f + seed * 3.7f);
        const float layeredNoise = coarse * 0.56f + detail * 0.29f + streaks * 0.15f;
        const float topFade = SmoothStep01(normalizedY * 1.35f);
        const float bottomFade = 1.0f - SmoothStep01(std::max(0.0f, (normalizedY - 0.72f) / 0.28f));
        return Clamp01((0.38f + layeredNoise * 0.82f) * topFade * bottomFade);
    }

    void DrawFlickerLight(
        const TransformComponent& transform,
        const FlickerLightComponent& light,
        float cameraX,
        float cameraY,
        float intensityScale)
    {
        if (intensityScale <= 0.001f)
        {
            return;
        }

        const float viewScale = GetViewScale();
        const float viewOriginX = GetViewOriginX();
        const float viewOriginY = GetViewOriginY();
        const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
        const float flicker = ComputeLightFlicker(timeSeconds, transform, light);
        const float radius = light.radius * viewScale * flicker * 0.42f;
        const float coreRadius = radius * 0.16f;
        const float emberRadius = radius * 0.08f;
        const float centerX = viewOriginX + ((transform.x + transform.width * 0.5f + light.offsetX) - cameraX) * viewScale;
        const float centerY = viewOriginY + ((transform.y + transform.height * 0.5f + light.offsetY) - cameraY) * viewScale;
        const float emberOffsetX = std::sin(timeSeconds * (light.flickerSpeed * 1.9f) + transform.x * 0.021f) * radius * 0.05f;
        const float emberOffsetY = std::cos(timeSeconds * (light.flickerSpeed * 1.6f) + transform.y * 0.018f) * radius * 0.04f;
        const int warmColor = GetColor(
            static_cast<int>(std::round(std::clamp(light.r, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(light.g, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(light.b, 0.0f, 1.0f) * 255.0f)));
        const int coreColor = GetColor(255, 242, 214);
        const float alphaScale = light.intensity * intensityScale;

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(56.0f * alphaScale)), 0, 255));
        DrawCircle(
            static_cast<int>(std::round(centerX)),
            static_cast<int>(std::round(centerY)),
            static_cast<int>(std::round(radius)),
            warmColor,
            TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(34.0f * alphaScale)), 0, 255));
        DrawCircle(
            static_cast<int>(std::round(centerX)),
            static_cast<int>(std::round(centerY - radius * 0.02f)),
            static_cast<int>(std::round(radius * 0.52f)),
            warmColor,
            TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(142.0f * alphaScale)), 0, 255));
        DrawCircle(
            static_cast<int>(std::round(centerX + emberOffsetX)),
            static_cast<int>(std::round(centerY - radius * 0.18f + emberOffsetY)),
            static_cast<int>(std::round(coreRadius)),
            coreColor,
            TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(96.0f * alphaScale)), 0, 255));
        DrawCircle(
            static_cast<int>(std::round(centerX + emberOffsetX * 0.7f)),
            static_cast<int>(std::round(centerY - radius * 0.28f + emberOffsetY * 1.4f)),
            static_cast<int>(std::round(emberRadius)),
            coreColor,
            TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    }

    void DrawGodRay(
        const TransformComponent& transform,
        const FlickerLightComponent& light,
        float cameraX,
        float cameraY,
        float intensityScale)
    {
        if (!light.godRayEnabled || light.godRayIntensity <= 0.001f || intensityScale <= 0.001f)
        {
            return;
        }

        const float viewScale = GetViewScale();
        const float viewOriginX = GetViewOriginX();
        const float viewOriginY = GetViewOriginY();
        const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
        const float pulse = ComputeGodRayPulse(timeSeconds, transform, light);
        const float beamLength = light.godRayLength * viewScale;
        const float beamWidth = light.godRayWidth * viewScale;
        const float softnessWidth = beamWidth * (0.42f + (0.82f - 0.42f) * light.godRaySoftness);
        const float driftX = std::sin(timeSeconds * (0.65f + light.godRayDriftSpeed) + transform.x * 0.014f) * beamWidth * 0.10f;
        const float sourceX = viewOriginX + ((transform.x + transform.width * 0.5f + light.offsetX) - cameraX) * viewScale + driftX;
        const float sourceY = viewOriginY + ((transform.y + transform.height * 0.5f + light.offsetY) - cameraY) * viewScale - beamLength * 0.05f;
        const float topY = sourceY - beamLength;
        const float bottomY = sourceY + beamLength * 0.08f;
        const float topHalfWidth = softnessWidth * 0.38f;
        const float bottomHalfWidth = beamWidth;
        const int beamColor = GetColor(
            static_cast<int>(std::round(std::clamp(light.r * 0.95f, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(light.g * 0.97f, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(std::min(1.0f, light.b + 0.10f), 0.0f, 1.0f) * 255.0f)));
        const int innerColor = GetColor(255, 248, 228);
        const float alphaScale = light.godRayIntensity * intensityScale * pulse;

        constexpr int kSlices = 14;
        for (int slice = 0; slice < kSlices; ++slice)
        {
            const float t0 = static_cast<float>(slice) / static_cast<float>(kSlices);
            const float t1 = static_cast<float>(slice + 1) / static_cast<float>(kSlices);
            const float density0 = ComputeGodRayDensity(t0, timeSeconds, transform, light);
            const float density1 = ComputeGodRayDensity(t1, timeSeconds, transform, light);
            const float sliceDensity = (density0 + density1) * 0.5f;
            if (sliceDensity <= 0.01f)
            {
                continue;
            }

            const float wave0 = std::sin(timeSeconds * 0.75f + t0 * 9.0f + transform.x * 0.009f) * beamWidth * 0.035f;
            const float wave1 = std::sin(timeSeconds * 0.75f + t1 * 9.0f + transform.x * 0.009f) * beamWidth * 0.035f;
            const float x0 = sourceX + wave0;
            const float x1 = sourceX + wave1;
            const float y0 = LerpFloat(topY, bottomY, t0);
            const float y1 = LerpFloat(topY, bottomY, t1);
            const float outerHalfWidth0 = LerpFloat(topHalfWidth, bottomHalfWidth, SmoothStep01(t0));
            const float outerHalfWidth1 = LerpFloat(topHalfWidth, bottomHalfWidth, SmoothStep01(t1));
            const float innerHalfWidth0 = outerHalfWidth0 * LerpFloat(0.26f, 0.38f, 1.0f - light.godRaySoftness);
            const float innerHalfWidth1 = outerHalfWidth1 * LerpFloat(0.26f, 0.38f, 1.0f - light.godRaySoftness);
            const int outerAlpha = std::clamp(static_cast<int>(std::round(32.0f * alphaScale * sliceDensity)), 0, 255);
            const int innerAlpha = std::clamp(static_cast<int>(std::round(20.0f * alphaScale * sliceDensity)), 0, 255);

            if (outerAlpha > 0)
            {
                SetDrawBlendMode(DX_BLENDMODE_ADD, outerAlpha);
                DrawTriangleAA(x0 - outerHalfWidth0, y0, x1 - outerHalfWidth1, y1, x1 + outerHalfWidth1, y1, beamColor, TRUE);
                DrawTriangleAA(x0 - outerHalfWidth0, y0, x1 + outerHalfWidth1, y1, x0 + outerHalfWidth0, y0, beamColor, TRUE);
            }

            if (innerAlpha > 0)
            {
                SetDrawBlendMode(DX_BLENDMODE_ADD, innerAlpha);
                DrawTriangleAA(x0 - innerHalfWidth0, y0, x1 - innerHalfWidth1, y1, x1 + innerHalfWidth1, y1, innerColor, TRUE);
                DrawTriangleAA(x0 - innerHalfWidth0, y0, x1 + innerHalfWidth1, y1, x0 + innerHalfWidth0, y0, innerColor, TRUE);
            }
        }

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    }

}

namespace
{
    void DrawPlayerAfterimages(
        const GameScenePlayerState& playerState,
        PhotoFilterTheme selectedTheme,
        const SpriteRenderComponent& sprite,
        const TransformComponent& transform,
        float viewOriginX,
        float viewOriginY,
        float viewScale,
        float cameraX,
        float cameraY)
    {
        float afterimageR = 0.42f;
        float afterimageG = 0.88f;
        float afterimageB = 1.0f;
        GetPhotoFilterThemeOverlayColor(selectedTheme, afterimageR, afterimageG, afterimageB);
        const float outlineBoost = selectedTheme == PhotoFilterTheme::None ? 0.0f : 0.08f;

        for (size_t index = playerState.afterimages.size(); index > 0; --index)
        {
            const auto& afterimage = playerState.afterimages[index - 1];
            const float afterimageDrawX = viewOriginX + (afterimage.x - cameraX) * viewScale;
            const float afterimageDrawY = viewOriginY + (afterimage.y - cameraY) * viewScale;
            const float afterimageDrawWidth = transform.width * afterimage.scale * viewScale;
            const float afterimageDrawHeight = transform.height * afterimage.scale * viewScale;
            const float alpha = Clamp01(afterimage.life / 0.18f) * 0.32f;
            Shader_ResetStyle();
            Shader_SetOutline(
                std::min(1.0f, afterimageR + outlineBoost),
                std::min(1.0f, afterimageG + outlineBoost),
                std::min(1.0f, afterimageB + outlineBoost),
                1.0f,
                1.4f);
            Shader_SetTint(afterimageR, afterimageG, afterimageB, alpha);
            SpriteDraw(
                sprite.GetTextureId(),
                afterimageDrawX,
                afterimageDrawY,
                afterimageDrawWidth,
                afterimageDrawHeight,
                sprite.GetSourceX(),
                sprite.GetSourceY(),
                sprite.GetSourceWidth(),
                sprite.GetSourceHeight(),
                afterimage.flipX,
                afterimage.rotation);
        }
    }

    void DrawPhotoPasteAnimationOutline(
        const Entity& entity,
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        float viewScale)
    {
        const auto* pasteAnimation = entity.GetComponent<PhotoPasteAnimationComponent>();
        if (!pasteAnimation || pasteAnimation->IsFinished())
        {
            return;
        }

        const float progress = pasteAnimation->GetNormalizedProgress();
        const float clamped = Clamp01(progress);
        const int alpha = static_cast<int>(std::round(std::lerp(220.0f, 80.0f, clamped)));
        const int outlinePad = std::max(2, static_cast<int>(std::round(2.0f * viewScale)));
        float outlineR = 1.0f;
        float outlineG = 1.0f;
        float outlineB = 1.0f;
        if (const auto* effect = entity.GetComponent<PhotoCopyEffectComponent>())
        {
            GetPhotoFilterThemeOverlayColor(effect->GetTheme(), outlineR, outlineG, outlineB);
        }
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawBox(
            static_cast<int>(std::round(drawX)) - outlinePad,
            static_cast<int>(std::round(drawY)) - outlinePad,
            static_cast<int>(std::round(drawX + drawWidth)) + outlinePad,
            static_cast<int>(std::round(drawY + drawHeight)) + outlinePad,
            GetColor(
                static_cast<int>(std::round(outlineR * 255.0f)),
                static_cast<int>(std::round(outlineG * 255.0f)),
                static_cast<int>(std::round(outlineB * 255.0f))),
            FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    void ApplyPhotoBoxRoleStyle(const PhotoCopyRoleComponent* photoRole)
    {
        if (!photoRole)
        {
            Shader_SetFlash(0.82f, 0.90f, 1.0f, 1.0f, 0.18f);
            return;
        }

        switch (photoRole->role)
        {
        case PhotoCopyRole::Hazard:
            Shader_SetFlash(1.0f, 0.28f, 0.22f, 1.0f, 0.24f);
            break;
        case PhotoCopyRole::GoalRelay:
            Shader_SetOutline(0.96f, 0.88f, 0.22f, 1.0f, 1.6f);
            break;
        case PhotoCopyRole::Pickup:
            Shader_SetOutline(0.18f, 0.90f, 1.0f, 1.0f, 1.6f);
            break;
        case PhotoCopyRole::Ally:
            Shader_SetOutline(0.78f, 0.94f, 0.82f, 1.0f, 1.8f);
            break;
        case PhotoCopyRole::Solid:
        default:
            Shader_SetFlash(0.82f, 0.90f, 1.0f, 1.0f, 0.18f);
            break;
        }
    }

    void ApplyPhotoBoxLayerStyle(
        const PhotoCopyLayerComponent* photoLayer,
        const PhotoCopyOriginComponent* photoOrigin,
        const TintComponent* tint)
    {
        if (!photoLayer)
        {
            return;
        }

        if (photoLayer->layer == PhotoCopyLayer::Background)
        {
            const bool looksLikePrintedPhotoPaper =
                photoOrigin &&
                photoOrigin->origin == PhotoCopyOrigin::Generic &&
                tint &&
                tint->r > 0.9f &&
                tint->g > 0.9f &&
                tint->b > 0.85f;
            if (looksLikePrintedPhotoPaper)
            {
                Shader_SetOutline(0.90f, 0.84f, 0.72f, 1.0f, 1.4f);
                Shader_SetTint(0.98f, 0.96f, 0.90f, 0.92f);
            }
            else if (tint && tint->r < 0.2f && tint->g < 0.2f && tint->b < 0.2f)
            {
                Shader_SetOutline(0.22f, 0.22f, 0.24f, 1.0f, 1.2f);
                Shader_SetTint(0.10f, 0.12f, 0.14f, 0.94f);
            }
            else
            {
                Shader_SetTint(0.64f, 0.72f, 0.84f, 0.44f);
            }
        }
        else if (photoLayer->layer == PhotoCopyLayer::Shadow)
        {
            Shader_SetOutline(0.04f, 0.04f, 0.06f, 1.0f, 1.6f);
            Shader_SetTint(0.02f, 0.02f, 0.03f, 0.72f);
        }
    }

    void ApplyPhotoBoxThemeStyle(const PhotoCopyEffectComponent* effect)
    {
        if (!effect)
        {
            return;
        }

        switch (effect->GetTheme())
        {
        case PhotoFilterTheme::Hot:
            Shader_SetOutline(1.0f, 0.52f, 0.20f, 1.0f, 2.1f);
            Shader_SetFlash(1.0f, 0.30f, 0.12f, 1.0f, 0.28f);
            break;
        case PhotoFilterTheme::Cold:
            Shader_SetOutline(0.74f, 0.92f, 1.0f, 1.0f, 2.2f);
            Shader_SetFlash(0.34f, 0.74f, 1.0f, 1.0f, 0.12f);
            break;
        case PhotoFilterTheme::Invert:
            Shader_SetOutline(0.90f, 0.94f, 0.92f, 1.0f, 2.0f);
            Shader_SetFlash(0.78f, 0.96f, 0.84f, 1.0f, 0.16f);
            break;
        case PhotoFilterTheme::Sepia:
            Shader_SetOutline(0.92f, 0.72f, 0.44f, 1.0f, 2.0f);
            Shader_SetFlash(0.82f, 0.64f, 0.34f, 1.0f, 0.14f);
            break;
        case PhotoFilterTheme::None:
        default:
            break;
        }
    }

}

void GameScene::DrawPhotoBoxesByLayer(PhotoCopyLayer layer) const
{
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagPhotoBox))
        {
            continue;
        }

        if (entity->GetComponent<PhotoPasteOrderComponent>())
        {
            continue;
        }

        const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>();
        const PhotoCopyLayer currentLayer = photoLayer ? photoLayer->layer : PhotoCopyLayer::Foreground;
        if (currentLayer != layer)
        {
            continue;
        }

        DrawEntity(*entity);
    }
}

void GameScene::DrawPastedEntitiesFront() const
{
    struct DrawTarget
    {
        const Entity* entity = nullptr;
        int pasteOrder = 0;
        int layerPriority = 0;
    };

    std::vector<DrawTarget> drawTargets;
    drawTargets.reserve(m_entities.size());

    // Only entities with PhotoPasteOrder are drawn in the pasted-front pass.
    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* pasteOrder = entity->GetComponent<PhotoPasteOrderComponent>();
        if (!pasteOrder)
        {
            continue;
        }

        int layerPriority = 2;
        if (const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>())
        {
            switch (photoLayer->layer)
            {
            case PhotoCopyLayer::Background:
                layerPriority = 0;
                break;
            case PhotoCopyLayer::Shadow:
                layerPriority = 1;
                break;
            case PhotoCopyLayer::Foreground:
            default:
                layerPriority = 3;
                break;
            }
        }

        drawTargets.push_back({ entity.get(), pasteOrder->order, layerPriority });
    }

    std::stable_sort(
        drawTargets.begin(),
        drawTargets.end(),
        [](const DrawTarget& a, const DrawTarget& b)
        {
            // Draw ascending so later pasted objects appear in front.
            if (a.pasteOrder != b.pasteOrder)
            {
                return a.pasteOrder < b.pasteOrder;
            }
            // If order is equal, stabilize by layer priority (BG -> Shadow -> FG).
            return a.layerPriority < b.layerPriority;
        });

    for (const DrawTarget& target : drawTargets)
    {
        DrawEntity(*target.entity);
    }
}

void GameScene::DrawEffects() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* light = entity->GetComponent<FlickerLightComponent>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!light || !transform)
        {
            continue;
        }

        const float centerX = transform->x + transform->width * 0.5f;
        const float maxRadius = light->radius + std::abs(light->offsetX) + transform->width * 0.5f;
        const float drawLeft = viewOriginX + (centerX - maxRadius - m_flow.cameraX) * viewScale;
        const float drawRight = viewOriginX + (centerX + maxRadius - m_flow.cameraX) * viewScale;
        if (drawRight < viewOriginX || drawLeft > viewOriginX + GetViewWidth())
        {
            continue;
        }

        float intensityScale = 1.0f;
        if (HasTag(*entity, kTagGoal) && !m_flow.goalUnlocked)
        {
            intensityScale = 0.45f;
        }
        if (const auto* checkpoint = entity->GetComponent<CheckpointComponent>())
        {
            intensityScale *= checkpoint->activated ? 1.15f : 0.85f;
        }

        DrawGodRay(*transform, *light, m_flow.cameraX, m_flow.cameraY, intensityScale);
        DrawFlickerLight(*transform, *light, m_flow.cameraX, m_flow.cameraY, intensityScale);
    }

    for (const auto& particle : m_effects.barrelDebris)
    {
        const float lifeT = Clamp01(particle.life / std::max(0.001f, particle.maxLife));
        Shader_ResetStyle();
        Shader_SetTint(particle.r, particle.g, particle.b, lifeT * 0.85f);
        SpriteDraw(
            m_whiteTexture,
            viewOriginX + (particle.x - m_flow.cameraX) * viewScale,
            viewOriginY + (particle.y - m_flow.cameraY) * viewScale,
            particle.size * viewScale,
            particle.size * viewScale,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            false,
            particle.rotation);
    }


    for (const auto& spark : m_effects.laserSparks)
    {
        const float lifeT = Clamp01(spark.life / std::max(0.001f, spark.maxLife));
        const float size = (2.0f + 3.0f * lifeT) * viewScale;
        const float drawX = viewOriginX + (spark.x - m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + (spark.y - m_flow.cameraY) * viewScale;
        Shader_ResetStyle();
        Shader_SetTint(1.0f, 0.76f, 0.28f, lifeT);
        SpriteDraw(
            m_whiteTexture,
            drawX - size * 0.5f,
            drawY - size * 0.5f,
            size,
            size,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            false,
            0.0f);
    }

    Shader_ResetStyle();
}

void GameScene::DrawEntity(const Entity& entity) const
{
    const auto* transform = entity.GetComponent<TransformComponent>();
    const auto* sprite = entity.GetComponent<SpriteRenderComponent>();
    if (!transform || !sprite)
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    float drawX = viewOriginX + (transform->x - m_flow.cameraX) * viewScale;
    float drawY = viewOriginY + (transform->y - m_flow.cameraY) * viewScale;
    float drawWidth = transform->width * transform->scale * viewScale;
    float drawHeight = transform->height * transform->scale * viewScale;
    if (drawX + drawWidth < viewOriginX || drawX > viewOriginX + viewWidth)
    {
        return;
    }

    Shader_ResetStyle();
    float alphaMultiplier = 1.0f;
    if (const auto* lifetime = entity.GetComponent<PhotoCopyLifetimeComponent>())
    {
        const float totalLifetime = std::max(0.001f, lifetime->GetLifetimeSeconds());
        alphaMultiplier = Clamp01(lifetime->GetRemainingSeconds() / totalLifetime);
    }

    const auto* pasteAnimation = entity.GetComponent<PhotoPasteAnimationComponent>();
    if (pasteAnimation && !pasteAnimation->IsFinished())
    {
        const float progress = pasteAnimation->GetNormalizedProgress();
        const float settleT = EaseOutBack(progress);
        const float slamT = EaseOutCubic(progress);
        const float stickT = m_debug.effectPasteStickEnabled ? Clamp01(1.0f - progress / 0.085f) : 0.0f;
        const float stickEase = stickT * stickT * (3.0f - 2.0f * stickT);
        const float animationScale = 0.82f + 0.18f * settleT;
        const float centerX = drawX + drawWidth * 0.5f;
        const float bottomY = drawY + drawHeight;
        const float animatedWidth = drawWidth * animationScale * (1.0f + 0.18f * stickEase);
        const float animatedHeight = drawHeight * (1.12f - 0.12f * slamT) * (1.0f - 0.14f * stickEase);
        drawX = centerX - animatedWidth * 0.5f;
        drawY = bottomY - animatedHeight - (1.0f - slamT) * 18.0f * viewScale + stickEase * 5.0f * viewScale;
        drawWidth = animatedWidth;
        drawHeight = animatedHeight;
        alphaMultiplier *= 0.45f + 0.55f * slamT;
        Shader_SetFlash(1.0f, 0.98f, 0.92f, 1.0f, (1.0f - progress) * 0.28f);

        float effectR = 0.32f;
        float effectG = 0.92f;
        float effectB = 1.0f;
        if (const auto* effect = entity.GetComponent<PhotoCopyEffectComponent>())
        {
            GetPhotoFilterThemeOverlayColor(effect->GetTheme(), effectR, effectG, effectB);
        }
        else
        {
            GetPhotoFilterThemeOverlayColor(PhotoFilterTheme::None, effectR, effectG, effectB);
        }

        const float stampAlpha = (1.0f - slamT) * 0.22f;
        if (stampAlpha > 0.001f)
        {
            Shader_ResetStyle();
            Shader_SetTint(effectR, effectG, effectB, stampAlpha);
            SpriteDraw(
                m_whiteTexture,
                drawX - 8.0f * viewScale,
                drawY - 8.0f * viewScale,
                drawWidth + 16.0f * viewScale,
                drawHeight + 16.0f * viewScale,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
        }
    }

    const auto* tag = entity.GetComponent<TagComponent>();
    if (tag && HasTag(tag, kTagGoal))
    {
        Shader_SetOutline(
            m_flow.goalUnlocked ? 0.28f : 0.92f,
            m_flow.goalUnlocked ? 1.0f : 0.22f,
            m_flow.goalUnlocked ? 0.42f : 0.18f,
            1.0f,
            1.5f);
    }
    else if (tag && HasTag(tag, kTagPhotoSource))
    {
        Shader_SetOutline(0.18f, 0.90f, 1.0f, 1.0f, 1.4f);
    }
    else if (entity.GetComponent<PhotoFilterComponent>())
    {
        if (const auto* filter = entity.GetComponent<PhotoFilterComponent>())
        {
            switch (filter->GetTheme())
            {
            case PhotoFilterTheme::Hot:
                Shader_SetOutline(1.0f, 0.40f, 0.18f, 1.0f, 1.9f);
                Shader_SetFlash(1.0f, 0.28f, 0.10f, 1.0f, 0.26f);
                break;
            case PhotoFilterTheme::Cold:
                Shader_SetOutline(0.70f, 0.92f, 1.0f, 1.0f, 1.9f);
                Shader_SetFlash(0.18f, 0.74f, 1.0f, 1.0f, 0.18f);
                break;
            case PhotoFilterTheme::Invert:
                Shader_SetOutline(0.92f, 0.92f, 0.96f, 1.0f, 1.8f);
                Shader_SetFlash(0.72f, 0.72f, 0.78f, 1.0f, 0.16f);
                break;
            case PhotoFilterTheme::Sepia:
                Shader_SetOutline(0.88f, 0.66f, 0.34f, 1.0f, 1.9f);
                Shader_SetFlash(0.74f, 0.56f, 0.28f, 1.0f, 0.16f);
                break;
            case PhotoFilterTheme::None:
            default:
                Shader_SetOutline(0.26f, 1.0f, 0.92f, 1.0f, 1.8f);
                Shader_SetFlash(0.18f, 0.92f, 0.88f, 1.0f, 0.22f);
                break;
            }
        }
    }
    else if (tag && HasTag(tag, kTagPhotoBox))
    {
        const auto* photoLayer = entity.GetComponent<PhotoCopyLayerComponent>();
        const auto* photoOrigin = entity.GetComponent<PhotoCopyOriginComponent>();
        const auto* tint = entity.GetComponent<TintComponent>();

        ApplyPhotoBoxRoleStyle(entity.GetComponent<PhotoCopyRoleComponent>());
        ApplyPhotoBoxLayerStyle(photoLayer, photoOrigin, tint);
        ApplyPhotoBoxThemeStyle(entity.GetComponent<PhotoCopyEffectComponent>());
    }
    else if (tag && HasTag(tag, kTagBullet))
    {
        const auto* projectile = entity.GetComponent<ProjectileComponent>();
        if (projectile)
        {
            const float angle = std::atan2(projectile->GetVelocityY(), projectile->GetVelocityX());
            const int color = GetColor(255, 230, 50);
            float ax = drawX;
            float ay = drawY;
            float bx = drawX;
            float by = drawY + drawHeight;
            float cx = drawX + drawWidth;
            float cy = drawY + drawHeight * 0.5f;
            const float centerX = drawX + drawWidth * 0.5f;
            const float centerY = drawY + drawHeight * 0.5f;
            RotatePoint(centerX, centerY, angle, ax, ay);
            RotatePoint(centerX, centerY, angle, bx, by);
            RotatePoint(centerX, centerY, angle, cx, cy);
            DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
            Shader_ResetStyle();
            return;
        }
    }


    else if (tag && HasTag(tag, kTagLaserSwitch))
    {
        const int baseColor = GetColor(236, 204, 46);
        const int borderColor = GetColor(168, 132, 24);
        const int textColor = GetColor(48, 42, 18);
        const int left = static_cast<int>(std::round(drawX));
        const int top = static_cast<int>(std::round(drawY));
        const int right = static_cast<int>(std::round(drawX + drawWidth));
        const int bottom = static_cast<int>(std::round(drawY + drawHeight));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(220.0f * alphaMultiplier)));
        DrawBox(left, top, right, bottom, baseColor, TRUE);
        DrawBox(left, top, right, bottom, borderColor, FALSE);

        const int centerX = static_cast<int>(std::round(drawX + drawWidth * 0.5f));
        const int topY = static_cast<int>(std::round(drawY + drawHeight * 0.24f));
        const int bottomY = static_cast<int>(std::round(drawY + drawHeight * 0.62f));
        DrawString(centerX - 4, topY, "L", textColor);
        DrawString(centerX - 4, bottomY, "S", textColor);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        Shader_ResetStyle();
        return;
    }


    else if (tag && HasTag(tag, kTagLaserBeam))
    {
        const int outerColor = GetColor(255, 86, 86);
        const int coreColor = GetColor(255, 224, 196);
        const int outerLeft = static_cast<int>(std::round(drawX));
        const int outerTop = static_cast<int>(std::round(drawY));
        const int outerRight = static_cast<int>(std::round(drawX + drawWidth));
        const int outerBottom = static_cast<int>(std::round(drawY + drawHeight));
        const float coreInsetY = drawHeight * 0.28f;
        const int coreTop = static_cast<int>(std::round(drawY + coreInsetY));
        const int coreBottom = static_cast<int>(std::round(drawY + drawHeight - coreInsetY));

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(196.0f * alphaMultiplier)));
        DrawBox(outerLeft, outerTop, outerRight, outerBottom, outerColor, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(238.0f * alphaMultiplier)));
        DrawBox(outerLeft, coreTop, outerRight, coreBottom, coreColor, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        Shader_ResetStyle();
        return;
    }


    else if (tag && HasTag(tag, kTagPlayer))
    {
        DrawPlayerAfterimages(
            m_player,
            m_photo.capture.selectedTheme,
            *sprite,
            *transform,
            viewOriginX,
            viewOriginY,
            viewScale,
            m_flow.cameraX,
            m_flow.cameraY);

        if (const auto* cooldown = entity.GetComponent<DamageCooldownComponent>())
        {
            if (cooldown->GetRemainingSeconds() > 0.0f)
            {
                const float cooldownSeconds = std::max(0.001f, cooldown->GetCooldownSeconds());
                const float blinkProgress = 1.0f - Clamp01(cooldown->GetRemainingSeconds() / cooldownSeconds);
                const float blinkPhase = blinkProgress * 6.0f * 3.14159265f;
                const float blinkWave = 0.5f + 0.5f * std::sin(blinkPhase);
                alphaMultiplier *= std::lerp(0.18f, 1.0f, blinkWave);

                const float flash = 0.40f + 0.60f * std::sin(cooldown->GetRemainingSeconds() * 28.0f);
                Shader_SetFlash(1.0f, 0.30f, 0.22f, 1.0f, Clamp01(flash));
            }
        }
    }

    if (const auto* tint = entity.GetComponent<TintComponent>())
    {
        Shader_SetTint(tint->r, tint->g, tint->b, tint->a * alphaMultiplier);
    }
    else
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, alphaMultiplier);
    }

    if (!DrawDamagePlatformShape(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            entity.GetComponent<DamagePlatformComponent>(),
            entity.GetComponent<TintComponent>(),
            sprite->GetSourceX(),
            sprite->GetSourceY(),
            sprite->GetSourceWidth(),
            sprite->GetSourceHeight(),
            transform->rotation,
            entity.GetComponent<TintComponent>() ? entity.GetComponent<TintComponent>()->a * alphaMultiplier : alphaMultiplier) &&
        !DrawSpikeStripShape(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            entity.GetComponent<SpikeStripComponent>(),
            entity.GetComponent<TintComponent>(),
            sprite->GetSourceX(),
            sprite->GetSourceY(),
            sprite->GetSourceWidth(),
            sprite->GetSourceHeight(),
            transform->rotation,
            entity.GetComponent<TintComponent>() ? entity.GetComponent<TintComponent>()->a * alphaMultiplier : alphaMultiplier) &&
        !DrawSlopeTriangle(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            entity.GetComponent<PhotoCopyTileValueComponent>() ? entity.GetComponent<PhotoCopyTileValueComponent>()->tileValue : 0,
            entity.GetComponent<TintComponent>(),
            sprite->GetFlipX(),
            transform->rotation,
            entity.GetComponent<TintComponent>() ? entity.GetComponent<TintComponent>()->a * alphaMultiplier : alphaMultiplier))
    {
        SpriteDraw(
            sprite->GetTextureId(),
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            sprite->GetSourceX(),
            sprite->GetSourceY(),
            sprite->GetSourceWidth(),
            sprite->GetSourceHeight(),
            sprite->GetFlipX(),
            transform->rotation);
    }

    if (m_debug.effectPasteRingEnabled)
    {
        DrawPhotoPasteAnimationOutline(
        entity,
        drawX,
        drawY,
        drawWidth,
        drawHeight,
        viewScale);
    }

    if (m_debug.showCollisionDebug && (entity.GetComponent<PhotoFilterComponent>() || (tag && (HasTag(tag, kTagPlayer) || HasTag(tag, kTagPhotoSource) || HasTag(tag, kTagPhotoBox)))))
    {
        unsigned int color = GetColor(255, 255, 255);
        if (tag && HasTag(tag, kTagPlayer))
        {
            color = GetColor(255, 96, 96);
        }
        else if (tag && HasTag(tag, kTagPhotoSource))
        {
            color = GetColor(96, 255, 255);
        }
        else if (tag && HasTag(tag, kTagPhotoBox))
        {
            color = GetColor(255, 220, 96);
        }
        else if (entity.GetComponent<PhotoFilterComponent>())
        {
            color = GetColor(96, 255, 220);
        }

        DrawWorldRectOutline(
            transform->x,
            transform->y,
            transform->width * transform->scale,
            transform->height * transform->scale,
            m_flow.cameraX,
            m_flow.cameraY,
            color);

        if (const auto* imageCollider = entity.GetComponent<ImageOutlineColliderComponent>())
        {
            DrawWorldPolygonOutline(*transform, *imageCollider, m_flow.cameraX, m_flow.cameraY, color);
        }
    }

        Shader_ResetStyle();
}

void GameScene::DrawEnemyAttackRects() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();

    for (const auto& entity : m_entities)
    {
        if (!entity) continue;

        // Walker�U������
        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (enemy && enemy->attackRectActive)
        {
            const float screenX = viewOriginX + (enemy->attackRectX - m_flow.cameraX) * viewScale;
            const float screenY = viewOriginY + (enemy->attackRectY - m_flow.cameraY) * viewScale;
            const float screenW = enemy->attackRectWidth * viewScale;
            const float screenH = enemy->attackRectHeight * viewScale;

            DrawBoxAA(screenX, screenY, screenX + screenW, screenY + screenH,
                GetColor(255, 80, 80), TRUE);
        }

        // ���{�X�U������
        const auto* boss = entity->GetComponent<ShieldBossComponent>();
        if (boss && boss->attackRectActive)
        {
            const float screenX = viewOriginX + (boss->attackRectX - m_flow.cameraX) * viewScale;
            const float screenY = viewOriginY + (boss->attackRectY - m_flow.cameraY) * viewScale;
            const float screenW = boss->attackRectWidth * viewScale;
            const float screenH = boss->attackRectHeight * viewScale;

            const unsigned int color =
                boss->state == ShieldBossState::Rush
                ? GetColor(255, 80, 80)    // �ːi�̓I�����W
                : boss->state == ShieldBossState::SlamPhase1
                ? GetColor(255, 140, 0)    // ����@�̓I�����W
                : GetColor(180, 0, 255);   // ����A�͎�

            DrawBoxAA(screenX, screenY, screenX + screenW, screenY + screenH,
                color, TRUE);
        }
    }
}
