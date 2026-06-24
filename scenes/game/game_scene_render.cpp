#include "pch.h"

#include "game_scene_internal.h"
#include "photo_filter_rules.h"
#include "texture.h"

#include "DxLib.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

    void DrawWorldRectOutline(const GameScene& scene, float worldX, float worldY, float worldWidth, float worldHeight, float cameraX, float cameraY, unsigned int color)
    {
        const float viewScale = scene.GetViewScale();
        const float viewOriginX = scene.GetViewOriginX();
        const float viewOriginY = scene.GetViewOriginY();
        const int left = static_cast<int>(std::round(viewOriginX + (worldX - cameraX) * viewScale));
        const int top = static_cast<int>(std::round(viewOriginY + (worldY - cameraY) * viewScale));
        const int right = static_cast<int>(std::round(viewOriginX + (worldX + worldWidth - cameraX) * viewScale));
        const int bottom = static_cast<int>(std::round(viewOriginY + (worldY + worldHeight - cameraY) * viewScale));
        DrawBox(left, top, right, bottom, color, FALSE);
    }

    void DrawWorldPolygonOutline(
        const GameScene& scene,
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

        const float viewScale = scene.GetViewScale();
        const float viewOriginX = scene.GetViewOriginX();
        const float viewOriginY = scene.GetViewOriginY();
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

    void DrawProjectileGlowTriangle(
        float centerX,
        float centerY,
        float width,
        float height,
        float angle,
        unsigned int color,
        int alpha)
    {
        float ax = centerX - width * 0.5f;
        float ay = centerY - height * 0.5f;
        float bx = centerX - width * 0.5f;
        float by = centerY + height * 0.5f;
        float cx = centerX + width * 0.5f;
        float cy = centerY;
        RotatePoint(centerX, centerY, angle, ax, ay);
        RotatePoint(centerX, centerY, angle, bx, by);
        RotatePoint(centerX, centerY, angle, cx, cy);
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(alpha, 0, 255));
        DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void DrawAttackPredictionLine(
        float startX,
        float startY,
        float directionX,
        float directionY,
        float length,
        float viewScale,
        float pulseSeed)
    {
        const float directionLength = std::max(0.001f, std::hypot(directionX, directionY));
        const float dx = directionX / directionLength;
        const float dy = directionY / directionLength;
        const float endX = startX + dx * length;
        const float endY = startY + dy * length;
        const float pulse = 0.72f + 0.28f * std::sin(static_cast<float>(GetNowCount()) * 0.018f + pulseSeed);
        const int wideAlpha = std::clamp(static_cast<int>(std::round(88.0f * pulse)), 0, 150);
        const int coreAlpha = std::clamp(static_cast<int>(std::round(176.0f * pulse)), 0, 230);
        const float wideThickness = std::max(5.0f, 7.0f * viewScale);
        const float coreThickness = std::max(1.8f, 2.5f * viewScale);

        SetDrawBlendMode(DX_BLENDMODE_ADD, wideAlpha);
        DrawLineAA(startX, startY, endX, endY, GetColor(255, 24, 18), wideThickness);
        SetDrawBlendMode(DX_BLENDMODE_ADD, coreAlpha);
        DrawLineAA(startX, startY, endX, endY, GetColor(255, 90, 70), coreThickness);
        DrawCircleAA(startX, startY, std::max(3.0f, 4.0f * viewScale), 24, GetColor(255, 30, 20), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
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

    void DrawPlayerSwitchDomeFace(
        int left,
        int right,
        int height,
        int bevel,
        int faceTop,
        int faceBottom,
        int faceColor,
        int highlightColor,
        int rimColor,
        float pressRate,
        float alphaMultiplier)
    {
        constexpr int kDomeSegments = 120;
        constexpr float kPi = 3.14159265f;
        constexpr float kHighlightStartT = 0.18f;
        constexpr float kHighlightEndT = 0.82f;

        const int shadowColor = GetColor(58, 42, 36);
        const float domeLeft = static_cast<float>(left + bevel);
        const float domeRight = static_cast<float>(right - bevel);
        const float domeBaseY = static_cast<float>(faceBottom);
        const float centerX = (domeLeft + domeRight) * 0.5f;
        const float radiusX = std::max(1.0f, (domeRight - domeLeft) * 0.5f);
        const float normalRadiusY = std::max(1.0f, static_cast<float>(faceBottom - faceTop));
        const float flatRadiusY = std::max(1.0f, static_cast<float>(height) * 0.08f);
        const float radiusY = std::lerp(normalRadiusY, flatRadiusY, std::clamp(pressRate, 0.0f, 1.0f));

        struct Point
        {
            float x;
            float y;
        };

        const auto pointOnDome = [&](float t)
        {
            const float theta = kPi * (1.0f - t);
            return Point{
                centerX + std::cos(theta) * radiusX,
                domeBaseY - std::sin(theta) * radiusY
            };
        };

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(245.0f * alphaMultiplier)), 0, 255));
        for (int index = 0; index < kDomeSegments; ++index)
        {
            const float t0 = static_cast<float>(index) / static_cast<float>(kDomeSegments);
            const float t1 = static_cast<float>(index + 1) / static_cast<float>(kDomeSegments);
            const auto p0 = pointOnDome(t0);
            const auto p1 = pointOnDome(t1);
            DrawQuadrangleAA(
                p0.x, p0.y,
                p1.x, p1.y,
                p1.x, domeBaseY,
                p0.x, domeBaseY,
                faceColor,
                TRUE);
        }

        const float highlightBottomOffset = std::max(2.0f, static_cast<float>(height) * 0.12f);
        for (int index = 0; index < kDomeSegments; ++index)
        {
            const float segmentStart = static_cast<float>(index) / static_cast<float>(kDomeSegments);
            const float segmentEnd = static_cast<float>(index + 1) / static_cast<float>(kDomeSegments);
            const float t0 = std::lerp(kHighlightStartT, kHighlightEndT, segmentStart);
            const float t1 = std::lerp(kHighlightStartT, kHighlightEndT, segmentEnd);
            const auto p0 = pointOnDome(t0);
            const auto p1 = pointOnDome(t1);
            const float y0 = std::min(domeBaseY, p0.y + highlightBottomOffset);
            const float y1 = std::min(domeBaseY, p1.y + highlightBottomOffset);
            DrawQuadrangleAA(
                p0.x, p0.y,
                p1.x, p1.y,
                p1.x, y1,
                p0.x, y0,
                highlightColor,
                TRUE);
        }

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(230.0f * alphaMultiplier)), 0, 255));
        auto previous = pointOnDome(0.0f);
        for (int index = 1; index <= kDomeSegments; ++index)
        {
            const float t = static_cast<float>(index) / static_cast<float>(kDomeSegments);
            const auto current = pointOnDome(t);
            DrawLineAA(previous.x, previous.y, current.x, current.y, rimColor, std::max(1.2f, static_cast<float>(height) * 0.08f));
            previous = current;
        }
        DrawLineAA(domeLeft, domeBaseY, domeRight, domeBaseY, shadowColor, std::max(1.0f, static_cast<float>(height) * 0.06f));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void DrawMidBoss3DrillShape(
        float screenX,
        float screenY,
        float screenW,
        float screenH,
        float drillAngle,
        int direction,
        float grooveTime,
        float viewScale,
        float alphaMultiplier)
    {
        const float centerX = screenX + screenW * 0.5f;
        const float centerY = screenY + screenH * 0.5f;
        const float tipW = std::max(12.0f, screenH * 0.62f);
        const float bodyInsetY = screenH * 0.18f;
        const float bodyLeft = screenX + screenH * 0.18f;
        const float bodyRight = screenX + screenW - screenH * 0.18f;
        const float bodyTop = screenY + bodyInsetY;
        const float bodyBottom = screenY + screenH - bodyInsetY;
        const int shadowColor = GetColor(88, 44, 20);
        const int bodyColor = GetColor(228, 116, 42);
        const int coreColor = GetColor(255, 184, 86);
        const int grooveColor = GetColor(92, 44, 22);
        const int tipColor = GetColor(255, 220, 118);
        const auto rotatePoint = [&](float& x, float& y)
        {
            RotatePoint(centerX, centerY, drillAngle, x, y);
        };
        const auto drawRotatedBox = [&](float left, float top, float right, float bottom, int color)
        {
            float ax = left;
            float ay = top;
            float bx = right;
            float by = top;
            float cx = right;
            float cy = bottom;
            float dx = left;
            float dy = bottom;
            rotatePoint(ax, ay);
            rotatePoint(bx, by);
            rotatePoint(cx, cy);
            rotatePoint(dx, dy);
            DrawFilledQuad(ax, ay, bx, by, cx, cy, dx, dy, color);
        };
        const auto drawRotatedLine = [&](float ax, float ay, float bx, float by, int color, float thickness)
        {
            rotatePoint(ax, ay);
            rotatePoint(bx, by);
            DrawLineAA(ax, ay, bx, by, color, thickness);
        };

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(210.0f * alphaMultiplier)), 0, 255));
        drawRotatedBox(bodyLeft, bodyTop, bodyRight, bodyBottom, bodyColor);
        drawRotatedLine(bodyLeft, bodyTop, bodyRight, bodyTop, shadowColor, std::max(2.0f, 3.0f * viewScale));
        drawRotatedLine(bodyLeft, bodyBottom, bodyRight, bodyBottom, shadowColor, std::max(2.0f, 3.0f * viewScale));
        drawRotatedLine(bodyLeft, centerY, bodyRight, centerY, coreColor, std::max(3.0f, screenH * 0.12f));

        const float grooveSpacing = std::max(16.0f, screenH * 0.55f);
        const float groovePhase = std::fmod(grooveTime * 220.0f, grooveSpacing);
        const float directionSign = direction >= 0 ? 1.0f : -1.0f;
        for (float x = bodyLeft - grooveSpacing + groovePhase; x < bodyRight + grooveSpacing; x += grooveSpacing)
        {
            drawRotatedLine(
                x,
                bodyBottom,
                x + screenH * 0.36f * directionSign,
                bodyTop,
                grooveColor,
                std::max(2.0f, 3.0f * viewScale));
        }

        float tipAx = bodyRight;
        float tipAy = screenY + screenH * 0.08f;
        float tipBx = bodyRight;
        float tipBy = screenY + screenH * 0.92f;
        float tipCx = bodyRight + tipW;
        float tipCy = centerY;
        rotatePoint(tipAx, tipAy);
        rotatePoint(tipBx, tipBy);
        rotatePoint(tipCx, tipCy);
        DrawTriangleAA(tipAx, tipAy, tipBx, tipBy, tipCx, tipCy, tipColor, TRUE);
        drawRotatedLine(bodyRight + tipW, centerY, bodyRight, bodyTop, shadowColor, std::max(2.0f, 2.0f * viewScale));
        drawRotatedLine(bodyRight + tipW, centerY, bodyRight, bodyBottom, shadowColor, std::max(2.0f, 2.0f * viewScale));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void DrawStageLightFixtureShape(
        float x,
        float y,
        float width,
        float height,
        float rotation,
        const StageLightComponent& light,
        const TintComponent* tint,
        float alpha)
    {
        const float centerX = x + width * 0.5f;
        const float centerY = y + height * 0.5f;
        const float topHalfWidth = width * 0.5f * std::clamp(light.fixtureTopWidthRatio, 0.05f, 1.0f);
        const float bottomHalfWidth = width * 0.5f;

        float topLeftX = centerX - topHalfWidth;
        float topLeftY = y;
        float topRightX = centerX + topHalfWidth;
        float topRightY = y;
        float bottomRightX = centerX + bottomHalfWidth;
        float bottomRightY = y + height;
        float bottomLeftX = centerX - bottomHalfWidth;
        float bottomLeftY = y + height;
        RotatePoint(centerX, centerY, rotation, topLeftX, topLeftY);
        RotatePoint(centerX, centerY, rotation, topRightX, topRightY);
        RotatePoint(centerX, centerY, rotation, bottomRightX, bottomRightY);
        RotatePoint(centerX, centerY, rotation, bottomLeftX, bottomLeftY);

        const float tintR = tint ? tint->r : light.r;
        const float tintG = tint ? tint->g : light.g;
        const float tintB = tint ? tint->b : light.b;
        const int bodyColor = light.enabled
            ? GetColor(
                static_cast<int>(std::round(std::clamp(tintR * 0.86f, 0.0f, 1.0f) * 255.0f)),
                static_cast<int>(std::round(std::clamp(tintG * 0.82f, 0.0f, 1.0f) * 255.0f)),
                static_cast<int>(std::round(std::clamp(tintB * 0.56f, 0.0f, 1.0f) * 255.0f)))
            : GetColor(88, 82, 70);
        const int rimColor = light.enabled ? GetColor(72, 54, 30) : GetColor(42, 40, 38);
        const int lensColor = light.enabled ? GetColor(255, 238, 172) : GetColor(112, 104, 88);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(alpha * 245.0f)), 0, 255));
        DrawFilledQuad(topLeftX, topLeftY, topRightX, topRightY, bottomRightX, bottomRightY, bottomLeftX, bottomLeftY, bodyColor);
        DrawLine(static_cast<int>(std::round(topLeftX)), static_cast<int>(std::round(topLeftY)), static_cast<int>(std::round(topRightX)), static_cast<int>(std::round(topRightY)), rimColor);
        DrawLine(static_cast<int>(std::round(topRightX)), static_cast<int>(std::round(topRightY)), static_cast<int>(std::round(bottomRightX)), static_cast<int>(std::round(bottomRightY)), rimColor);
        DrawLine(static_cast<int>(std::round(bottomRightX)), static_cast<int>(std::round(bottomRightY)), static_cast<int>(std::round(bottomLeftX)), static_cast<int>(std::round(bottomLeftY)), rimColor);
        DrawLine(static_cast<int>(std::round(bottomLeftX)), static_cast<int>(std::round(bottomLeftY)), static_cast<int>(std::round(topLeftX)), static_cast<int>(std::round(topLeftY)), rimColor);

        const float lensWidth = width * 0.42f;
        const float lensHeight = std::max(2.0f, height * 0.16f);
        const int lensAlpha = std::clamp(static_cast<int>(std::round(alpha * (light.enabled ? 255.0f : 170.0f))), 0, 255);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, lensAlpha);
        DrawBoxAA(
            centerX - lensWidth * 0.5f,
            y + height * 0.72f,
            centerX + lensWidth * 0.5f,
            y + height * 0.72f + lensHeight,
            lensColor,
            TRUE);

        if (light.enabled)
        {
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(alpha * 92.0f * light.intensity)), 0, 255));
            DrawCircle(
                static_cast<int>(std::round(centerX)),
                static_cast<int>(std::round(y + height * 0.78f)),
                static_cast<int>(std::round(std::max(width, height) * 0.24f)),
                lensColor,
                TRUE);
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
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
        const GameScene& scene,
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

        const float viewScale = scene.GetViewScale();
        const float viewOriginX = scene.GetViewOriginX();
        const float viewOriginY = scene.GetViewOriginY();
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

    void DrawCompactFlickerLight(
        const GameScene& scene,
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

        const float viewScale = scene.GetViewScale();
        const float viewOriginX = scene.GetViewOriginX();
        const float viewOriginY = scene.GetViewOriginY();
        const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
        const float flicker = ComputeLightFlicker(timeSeconds, transform, light);
        const float radius = light.radius * viewScale * flicker * 0.18f;
        if (radius <= 1.0f)
        {
            return;
        }

        const float centerX = viewOriginX + ((transform.x + transform.width * 0.5f + light.offsetX) - cameraX) * viewScale;
        const float centerY = viewOriginY + ((transform.y + transform.height * 0.5f + light.offsetY) - cameraY) * viewScale;
        const int color = GetColor(
            static_cast<int>(std::round(std::clamp(light.r, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(light.g, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(light.b, 0.0f, 1.0f) * 255.0f)));
        const int alpha = std::clamp(static_cast<int>(std::round(82.0f * light.intensity * intensityScale)), 0, 180);
        if (alpha <= 0)
        {
            return;
        }

        SetDrawBlendMode(DX_BLENDMODE_ADD, alpha);
        DrawCircle(
            static_cast<int>(std::round(centerX)),
            static_cast<int>(std::round(centerY)),
            static_cast<int>(std::round(radius)),
            color,
            TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    }

    void DrawGodRay(
        const GameScene& scene,
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

        const float viewScale = scene.GetViewScale();
        const float viewOriginX = scene.GetViewOriginX();
        const float viewOriginY = scene.GetViewOriginY();
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

    void DrawStageLightBeam(
        const GameScene& scene,
        const TransformComponent& transform,
        const StageLightComponent& light,
        float beamLengthWorld,
        float cameraX,
        float cameraY)
    {
        if (!light.enabled || light.intensity <= 0.001f)
        {
            return;
        }

        const float viewScale = scene.GetViewScale();
        const float viewOriginX = scene.GetViewOriginX();
        const float viewOriginY = scene.GetViewOriginY();
        const float beamLength = beamLengthWorld * viewScale;
        const float topWidth = (light.beamTopWidth > 0.0f ? light.beamTopWidth : transform.width) * transform.scale * viewScale;
        const float bottomWidth = (light.beamBottomWidth > 0.0f ? light.beamBottomWidth : transform.width * 3.0f) * transform.scale * viewScale;
        if (beamLength <= 1.0f || topWidth <= 1.0f || bottomWidth <= 1.0f)
        {
            return;
        }

        const float centerX = viewOriginX + ((transform.x + transform.width * transform.scale * 0.5f) - cameraX) * viewScale;
        const float sourceY = viewOriginY + ((transform.y + transform.height * transform.scale) - cameraY) * viewScale;
        const float bottomY = sourceY + beamLength;
        const float topHalfWidth = topWidth * 0.5f;
        const float bottomHalfWidth = bottomWidth * 0.5f;
        const int beamColor = GetColor(
            static_cast<int>(std::round(std::clamp(light.r, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(light.g, 0.0f, 1.0f) * 255.0f)),
            static_cast<int>(std::round(std::clamp(light.b, 0.0f, 1.0f) * 255.0f)));
        const int innerColor = GetColor(255, 248, 216);

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(42.0f * light.intensity)), 0, 255));
        DrawFilledQuad(
            centerX - topHalfWidth,
            sourceY,
            centerX + topHalfWidth,
            sourceY,
            centerX + bottomHalfWidth,
            bottomY,
            centerX - bottomHalfWidth,
            bottomY,
            beamColor);

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(30.0f * light.intensity)), 0, 255));
        DrawFilledQuad(
            centerX - topHalfWidth * 0.38f,
            sourceY,
            centerX + topHalfWidth * 0.38f,
            sourceY,
            centerX + bottomHalfWidth * 0.42f,
            bottomY,
            centerX - bottomHalfWidth * 0.42f,
            bottomY,
            innerColor);

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(88.0f * light.intensity)), 0, 255));
        DrawCircle(
            static_cast<int>(std::round(centerX)),
            static_cast<int>(std::round(sourceY)),
            static_cast<int>(std::round(std::max(2.0f, topHalfWidth * 0.42f))),
            innerColor,
            TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
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

    void DrawWalkerAttackCaptureCue(
        const TransformComponent& transform,
        const EnemyComponent& enemy,
        float viewOriginX,
        float viewOriginY,
        float viewScale,
        float cameraX,
        float cameraY)
    {
        if (enemy.GetArchetype() != EnemyArchetype::Walker ||
            enemy.GetAIState() != EnemyComponent::AIState::Attack)
        {
            return;
        }

        const float charge = Clamp01(enemy.attackWarningProgress);
        const float flash = Clamp01(enemy.attackFlashRemaining / 0.18f);
        if (charge <= 0.001f && flash <= 0.001f)
        {
            return;
        }

        const float worldWidth = transform.width * transform.scale;
        const float worldHeight = transform.height * transform.scale;
        const float centerX = viewOriginX + (transform.x + worldWidth * 0.5f - cameraX) * viewScale;
        const float centerY = viewOriginY + (transform.y + worldHeight * 0.50f - cameraY) * viewScale;
        const float chargeEase = charge * charge * (3.0f - 2.0f * charge);
        const float postHitFade = enemy.attackFrameTriggered ? flash : 1.0f;
        const float ringFade = enemy.attackFrameTriggered ? std::min(1.0f, flash * 1.35f) : 1.0f;
        const float innerRadius = std::max(worldWidth, worldHeight) * (0.46f + flash * 0.04f) * viewScale;
        const float targetRadius = std::max(8.0f * viewScale, innerRadius * 0.18f);
        const float outerStartRadius = innerRadius * 1.72f;
        const float outerRadius = std::lerp(outerStartRadius, targetRadius, chargeEase);
        const float warningRadius = targetRadius * (1.08f + flash * 0.12f);
        const float pulse = 0.5f + 0.5f * std::sin(charge * 31.415926f);
        const int deepRed = GetColor(255, 24, 26);
        const int red = GetColor(255, 58, 42);
        const int orangeRed = GetColor(255, 122, 70);
        const int hotWhite = GetColor(255, 246, 224);
        const int posnum = 96;

        auto drawRingDash = [&](float angle, float length, float lineRadius, float alphaScale, float thicknessScale)
        {
            const float ax = centerX + std::cos(angle - length) * lineRadius;
            const float ay = centerY + std::sin(angle - length) * lineRadius;
            const float bx = centerX + std::cos(angle + length) * lineRadius;
            const float by = centerY + std::sin(angle + length) * lineRadius;
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(alphaScale)), 0, 255));
            DrawLineAA(ax, ay, bx, by, orangeRed, std::max(1.5f, thicknessScale * viewScale));
        };

        // Enemy1 charge warning: layered additive rings and short rim strokes like Endfield's charge cue.
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((12.0f + chargeEase * 24.0f) * ringFade)), 0, 80));
        DrawCircleAA(centerX, centerY, outerRadius * 1.08f, posnum, deepRed, TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((34.0f + pulse * 12.0f) * ringFade)), 0, 120));
        DrawCircleAA(centerX, centerY, warningRadius, posnum, deepRed, FALSE, std::max(3.0f, 5.0f * viewScale));
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((58.0f + chargeEase * 48.0f) * ringFade)), 0, 170));
        DrawCircleAA(centerX, centerY, targetRadius, posnum, red, FALSE, std::max(1.5f, 2.2f * viewScale));

        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((52.0f + chargeEase * 90.0f + pulse * 24.0f) * ringFade)), 0, 220));
        DrawCircleAA(centerX, centerY, outerRadius, posnum, red, FALSE, std::max(4.0f, (6.2f + chargeEase * 3.0f) * viewScale));
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((36.0f + chargeEase * 96.0f + flash * 70.0f) * ringFade)), 0, 255));
        DrawCircleAA(centerX, centerY, outerRadius * 0.985f, posnum, orangeRed, FALSE, std::max(2.0f, 2.6f * viewScale));

        const float orbit = charge * 6.2831853f;
        for (int index = 0; index < 6; ++index)
        {
            const float angle = orbit + static_cast<float>(index) * 1.0471976f;
            const float dashAlpha = (42.0f + chargeEase * 82.0f + (index % 2 == 0 ? pulse * 42.0f : 0.0f)) * ringFade;
            drawRingDash(angle, 0.09f + 0.03f * chargeEase, outerRadius * (1.0f + 0.018f * (index % 2)), dashAlpha, 2.0f + chargeEase * 2.0f);
        }

        if (enemy.attackCaptureWindowActive && postHitFade > 0.05f)
        {
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((112.0f + flash * 104.0f) * postHitFade)), 0, 255));
            DrawCircleAA(centerX, centerY, targetRadius * (1.08f + flash * 0.35f), posnum, orangeRed, FALSE, std::max(3.0f, (4.0f + flash * 3.0f) * viewScale));
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((62.0f + flash * 96.0f) * postHitFade)), 0, 255));
            DrawCircleAA(centerX, centerY, targetRadius * (1.38f + flash * 0.55f), posnum, red, FALSE, std::max(4.0f, (7.0f + flash * 4.0f) * viewScale));
        }

        if (flash > 0.001f)
        {
            const float beamExpand = Clamp01(1.0f - flash);
            const float beamExpandEase = 1.0f - std::pow(1.0f - beamExpand, 3.0f);
            const float beamMinHalfLength = std::max(52.0f * viewScale, innerRadius * 0.72f);
            const float beamMaxHalfLength = std::max(420.0f * viewScale, innerRadius * 4.8f);
            const float beamHalfLength = std::lerp(beamMinHalfLength, beamMaxHalfLength, beamExpandEase);
            const float beamThickness = std::max(2.0f, (3.0f + flash * 5.0f) * viewScale);
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(88.0f * flash)), 0, 255));
            DrawLineAA(centerX - beamHalfLength, centerY, centerX + beamHalfLength, centerY, deepRed, beamThickness * 6.2f);
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(174.0f * flash)), 0, 255));
            DrawLineAA(centerX - beamHalfLength, centerY, centerX + beamHalfLength, centerY, red, beamThickness * 3.4f);
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(245.0f * flash)), 0, 255));
            DrawLineAA(centerX - beamHalfLength, centerY, centerX + beamHalfLength, centerY, hotWhite, beamThickness);
            DrawLineAA(centerX, centerY - innerRadius * 0.34f, centerX, centerY + innerRadius * 0.34f, hotWhite, beamThickness * 0.72f);
            DrawLineAA(centerX - innerRadius * 0.24f, centerY - innerRadius * 0.24f, centerX + innerRadius * 0.24f, centerY + innerRadius * 0.24f, hotWhite, beamThickness * 0.55f);
            DrawLineAA(centerX - innerRadius * 0.24f, centerY + innerRadius * 0.24f, centerX + innerRadius * 0.24f, centerY - innerRadius * 0.24f, hotWhite, beamThickness * 0.55f);
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(214.0f * flash)), 0, 255));
            DrawCircleAA(centerX, centerY, targetRadius * 0.70f, posnum / 2, hotWhite, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(126.0f * flash)), 0, 255));
            DrawCircleAA(centerX, centerY, targetRadius * 1.55f, posnum / 2, orangeRed, TRUE);
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void DrawCapturedWalkerMeleeEffect(
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        float rotation,
        float alphaMultiplier,
        float lifeProgress)
    {
        const float alpha = Clamp01(alphaMultiplier);
        if (alpha <= 0.001f)
        {
            return;
        }

        const float progress = Clamp01(lifeProgress);
        const float appear = Clamp01(progress / 0.12f);
        const float fade = Clamp01((1.0f - progress) / 0.34f);
        const float flash = appear * fade;
        const float centerX = drawX + drawWidth * 0.5f;
        const float centerY = drawY + drawHeight * 0.48f;
        const float directionX = std::cos(rotation);
        const float directionY = std::sin(rotation);
        const float normalX = -directionY;
        const float normalY = directionX;
        const float effectSize = std::max(drawWidth, drawHeight);
        const float coreRadius = effectSize * (0.30f + 0.12f * (1.0f - fade));
        const float outerRadius = effectSize * (0.62f + 0.20f * progress);
        const float backLength = drawWidth * 0.78f;
        const float frontLength = drawWidth * (1.34f + 0.22f * appear);
        const float beamThickness = std::max(2.0f, drawHeight * 0.055f);
        const int deepRed = GetColor(255, 24, 26);
        const int red = GetColor(255, 58, 42);
        const int orangeRed = GetColor(255, 122, 70);
        const int hotWhite = GetColor(255, 246, 224);

        auto blendAlpha = [&](float value)
        {
            return std::clamp(static_cast<int>(std::round(value * flash * alpha)), 0, 255);
        };

        auto drawOrientedLine = [&](float fromForward, float fromSide, float toForward, float toSide, int color, float thickness)
        {
            DrawLineAA(
                centerX + directionX * fromForward + normalX * fromSide,
                centerY + directionY * fromForward + normalY * fromSide,
                centerX + directionX * toForward + normalX * toSide,
                centerY + directionY * toForward + normalY * toSide,
                color,
                thickness);
        };

        // Captured walker attack: use the same red/white flash language as the enemy charge cue.
        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(56.0f));
        DrawCircleAA(centerX, centerY, outerRadius, 72, deepRed, FALSE, std::max(3.0f, beamThickness * 1.7f));
        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(92.0f));
        DrawCircleAA(centerX, centerY, coreRadius, 72, red, FALSE, std::max(2.0f, beamThickness * 1.1f));

        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(88.0f));
        drawOrientedLine(-backLength, 0.0f, frontLength, 0.0f, deepRed, beamThickness * 6.5f);
        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(174.0f));
        drawOrientedLine(-backLength, 0.0f, frontLength, 0.0f, red, beamThickness * 3.2f);
        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(245.0f));
        drawOrientedLine(-backLength * 0.86f, 0.0f, frontLength * 0.94f, 0.0f, hotWhite, beamThickness);

        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(118.0f));
        drawOrientedLine(-backLength * 0.22f, -coreRadius * 0.50f, frontLength * 0.42f, -coreRadius * 0.14f, orangeRed, beamThickness * 0.9f);
        drawOrientedLine(-backLength * 0.22f, coreRadius * 0.50f, frontLength * 0.42f, coreRadius * 0.14f, orangeRed, beamThickness * 0.9f);
        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(230.0f));
        drawOrientedLine(-coreRadius * 0.38f, -coreRadius * 0.38f, coreRadius * 0.38f, coreRadius * 0.38f, hotWhite, beamThickness * 0.65f);
        drawOrientedLine(-coreRadius * 0.38f, coreRadius * 0.38f, coreRadius * 0.38f, -coreRadius * 0.38f, hotWhite, beamThickness * 0.65f);

        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(214.0f));
        DrawCircleAA(centerX, centerY, coreRadius * 0.26f, 48, hotWhite, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ADD, blendAlpha(126.0f));
        DrawCircleAA(centerX, centerY, coreRadius * 0.58f, 48, orangeRed, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    bool IsShieldBossAttackCaptureEffectActive(const Entity& entity, const ShieldBossComponent& boss)
    {
        constexpr int kAttack01BoostStartFrame = 60;
        if (boss.state == ShieldBossState::Rush)
        {
            const auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>();
            return animation &&
                animation->GetCurrentClipName() == "attack01" &&
                animation->GetCurrentLocalFrameIndex() >= kAttack01BoostStartFrame;
        }

        return boss.state == ShieldBossState::JumpAscend ||
            boss.state == ShieldBossState::AirHover ||
            boss.state == ShieldBossState::JumpDescend;
    }

    void DrawFallingShieldTrail(
        int textureId,
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        float sourceX,
        float sourceY,
        float sourceWidth,
        float sourceHeight,
        bool flipX,
        float rotation,
        float viewScale,
        float alphaMultiplier)
    {
        const float alpha = std::clamp(alphaMultiplier, 0.0f, 1.0f);
        const float centerX = drawX + drawWidth * 0.5f;

        // Falling blur is made from delayed shield copies, so the silhouette stays readable.
        constexpr int kBlurCopyCount = 5;
        for (int index = kBlurCopyCount; index >= 1; --index)
        {
            const float step = static_cast<float>(index);
            const float copyAlpha = (0.05f + 0.045f * step) * alpha;
            const float offsetY = (6.0f + step * 10.0f) * viewScale;
            const float scaleX = 1.0f - step * 0.018f;
            const float scaleY = 1.0f + step * 0.012f;
            const float copyX = centerX - drawWidth * scaleX * 0.5f;
            const float copyY = drawY - offsetY;
            const float copyWidth = drawWidth * scaleX;
            const float copyHeight = drawHeight * scaleY;

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(255.0f * copyAlpha)), 0, 255));
            Shader_SetTint(1.0f, 1.0f, 1.0f, copyAlpha);
            SpriteDraw(
                textureId,
                copyX,
                copyY,
                copyWidth,
                copyHeight,
                sourceX,
                sourceY,
                sourceWidth,
                sourceHeight,
                flipX,
                rotation);

            Shader_ResetStyle();
            const int lineAlpha = std::clamp(static_cast<int>(std::round((44.0f + step * 20.0f) * alpha)), 0, 170);
            const float lineThickness = std::max(1.0f, (0.75f + step * 0.12f) * viewScale);
            const int lineColor = GetColor(255, 250, 235);
            SetDrawBlendMode(DX_BLENDMODE_ADD, lineAlpha);
            for (int lineIndex = 0; lineIndex < 3; ++lineIndex)
            {
                const float side = static_cast<float>(lineIndex - 1);
                const float startX = copyX + copyWidth * (0.5f + side * 0.22f);
                const float startY = copyY + copyHeight * 0.24f;
                const float endX = startX + side * (3.0f + step * 1.5f) * viewScale;
                const float endY = startY - (18.0f + step * 8.0f) * viewScale;
                DrawLineAA(startX, startY, endX, endY, lineColor, lineThickness);
            }
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        Shader_ResetStyle();
    }

    void DrawShieldBossAttackCaptureFrame(
        const TransformComponent& transform,
        float viewOriginX,
        float viewOriginY,
        float viewScale,
        float cameraX,
        float cameraY)
    {
        const float worldWidth = transform.width * transform.scale;
        const float worldHeight = transform.height * transform.scale;
        const float centerX = viewOriginX + (transform.x + worldWidth * 0.5f - cameraX) * viewScale;
        const float centerY = viewOriginY + (transform.y + worldHeight * 0.50f - cameraY) * viewScale;
        const float time = static_cast<float>(GetNowCount()) * 0.001f;
        const float charge = std::fmod(time * 1.75f, 1.0f);
        const float chargeEase = charge * charge * (3.0f - 2.0f * charge);
        const float pulse = 0.5f + 0.5f * std::sin(charge * 31.415926f);
        const float innerRadius = std::max(worldWidth, worldHeight) * 0.66f * viewScale;
        const float outerRadius = innerRadius * std::lerp(1.68f, 1.0f, chargeEase);
        const float warningRadius = innerRadius * 1.02f;
        const int deepRed = GetColor(255, 24, 26);
        const int red = GetColor(255, 58, 42);
        const int orangeRed = GetColor(255, 122, 70);
        const int hotWhite = GetColor(255, 246, 224);
        const int posnum = 96;

        auto drawRingDash = [&](float angle, float length, float lineRadius, float alphaScale, float thicknessScale)
        {
            const float ax = centerX + std::cos(angle - length) * lineRadius;
            const float ay = centerY + std::sin(angle - length) * lineRadius;
            const float bx = centerX + std::cos(angle + length) * lineRadius;
            const float by = centerY + std::sin(angle + length) * lineRadius;
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(alphaScale)), 0, 255));
            DrawLineAA(ax, ay, bx, by, orangeRed, std::max(1.5f, thicknessScale * viewScale));
        };

        // Mid-boss1 rush cue reuses Enemy1's red charge-ring language at a boss-sized radius.
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(12.0f + chargeEase * 24.0f)), 0, 80));
        DrawCircleAA(centerX, centerY, outerRadius * 1.08f, posnum, deepRed, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(34.0f + pulse * 12.0f)), 0, 120));
        DrawCircleAA(centerX, centerY, warningRadius, posnum, deepRed, FALSE, std::max(3.0f, 5.0f * viewScale));
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(58.0f + chargeEase * 48.0f)), 0, 170));
        DrawCircleAA(centerX, centerY, innerRadius, posnum, red, FALSE, std::max(1.5f, 2.2f * viewScale));
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(52.0f + chargeEase * 90.0f + pulse * 24.0f)), 0, 220));
        DrawCircleAA(centerX, centerY, outerRadius, posnum, red, FALSE, std::max(3.0f, (4.2f + chargeEase * 2.4f) * viewScale));
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(36.0f + chargeEase * 96.0f)), 0, 255));
        DrawCircleAA(centerX, centerY, outerRadius * 0.985f, posnum, orangeRed, FALSE, std::max(1.0f, 1.8f * viewScale));

        const float orbit = charge * 6.2831853f;
        for (int index = 0; index < 6; ++index)
        {
            const float angle = orbit + static_cast<float>(index) * 1.0471976f;
            const float dashAlpha = 42.0f + chargeEase * 82.0f + (index % 2 == 0 ? pulse * 42.0f : 0.0f);
            drawRingDash(angle, 0.09f + 0.03f * chargeEase, outerRadius * (1.0f + 0.018f * (index % 2)), dashAlpha, 2.0f + chargeEase * 2.0f);
        }

        const float beamHalfLength = std::max(420.0f * viewScale, innerRadius * 4.4f);
        const float beamThickness = std::max(2.0f, 4.2f * viewScale);
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(72.0f + pulse * 46.0f)), 0, 180));
        DrawLineAA(centerX - beamHalfLength, centerY, centerX + beamHalfLength, centerY, deepRed, beamThickness * 5.2f);
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(126.0f + pulse * 60.0f)), 0, 230));
        DrawLineAA(centerX - beamHalfLength, centerY, centerX + beamHalfLength, centerY, red, beamThickness * 2.7f);
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(186.0f + pulse * 42.0f)), 0, 255));
        DrawLineAA(centerX - beamHalfLength * 0.92f, centerY, centerX + beamHalfLength * 0.92f, centerY, hotWhite, beamThickness * 0.82f);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

}

void GameScene::DrawPhotoBoxesByLayer(PhotoCopyLayer layer) const
{
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::PhotoBox))
    {
        if (!entity)
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

void GameScene::DrawBossShockwavesUnderlay() const
{
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::BossShockwave))
    {
        if (!entity)
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

    const auto& photoBoxes = m_world.EntitiesByTag(EntityTag::PhotoBox);
    const auto& barrels = m_world.EntitiesByTag(EntityTag::Barrel);
    const auto& logs = m_world.EntitiesByTag(EntityTag::Log);
    const auto& fallingRocks = m_world.EntitiesByTag(EntityTag::FallingRock);
    const auto& batteries = m_world.EntitiesByTag(EntityTag::Battery);
    const auto& bullets = m_world.EntitiesByTag(EntityTag::Bullet);
    const auto& laserTurrets = m_world.EntitiesByTag(EntityTag::LaserTurret);
    const auto& laserBeams = m_world.EntitiesByTag(EntityTag::LaserBeam);
    const auto& meleeAttacks = m_world.EntitiesByTag(EntityTag::WalkerMeleeAttack);
    const auto& capturedShields = m_world.EntitiesByTag(EntityTag::CapturedShield);

    std::vector<DrawTarget> drawTargets;
    drawTargets.reserve(
        photoBoxes.size() +
        barrels.size() +
        logs.size() +
        fallingRocks.size() +
        batteries.size() +
        bullets.size() +
        laserTurrets.size() +
        laserBeams.size() +
        meleeAttacks.size() +
        capturedShields.size());

    auto appendDrawTargets = [&](EntityTag tag)
    {
        for (Entity* entity : m_world.EntitiesByTag(tag))
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

            drawTargets.push_back({ entity, pasteOrder->order, layerPriority });
        }
    };

    appendDrawTargets(EntityTag::PhotoBox);
    appendDrawTargets(EntityTag::Barrel);
    appendDrawTargets(EntityTag::Log);
    appendDrawTargets(EntityTag::FallingRock);
    appendDrawTargets(EntityTag::Battery);
    appendDrawTargets(EntityTag::Bullet);
    appendDrawTargets(EntityTag::LaserTurret);
    appendDrawTargets(EntityTag::LaserBeam);
    appendDrawTargets(EntityTag::WalkerMeleeAttack);
    appendDrawTargets(EntityTag::CapturedShield);

    std::sort(
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

    struct LightDrawTarget
    {
        const TransformComponent* transform = nullptr;
        const FlickerLightComponent* light = nullptr;
        float intensityScale = 1.0f;
        float priority = 0.0f;
    };

    const auto& goalEntities = m_world.EntitiesByTag(EntityTag::Goal);
    const auto& checkpointEntities = m_world.EntitiesByTag(EntityTag::Checkpoint);
    const auto& photoSourceEntities = m_world.EntitiesByTag(EntityTag::PhotoSource);
    const auto& hazardEntities = m_world.EntitiesByTag(EntityTag::Hazard);
    const auto& batteryEntities = m_world.EntitiesByTag(EntityTag::Battery);
    const auto& stageLightEntities = m_world.EntitiesByTag(EntityTag::StageLight);
    const auto& enemyEntities = m_world.EntitiesByTag(EntityTag::Enemy);

    std::vector<LightDrawTarget> lightTargets;
    lightTargets.reserve(
        goalEntities.size() +
        checkpointEntities.size() +
        photoSourceEntities.size() +
        hazardEntities.size() +
        batteryEntities.size());
    const float viewRight = viewOriginX + GetViewWidth();
    const float viewBottom = viewOriginY + GetViewHeight();
    const float viewCenterWorldX = m_flow.cameraX + GetViewWidth() / std::max(0.001f, viewScale) * 0.5f;
    const float viewCenterWorldY = m_flow.cameraY + GetViewHeight() / std::max(0.001f, viewScale) * 0.5f;

    auto gatherLightTargets = [&](EntityTag tag)
    {
        for (Entity* entity : m_world.EntitiesByTag(tag))
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

            const float centerX = transform->x + transform->width * transform->scale * 0.5f + light->offsetX;
            const float centerY = transform->y + transform->height * transform->scale * 0.5f + light->offsetY;
            const float maxRadius = light->radius + std::abs(light->offsetX) + std::abs(light->offsetY) +
                (std::max)(transform->width, transform->height) * transform->scale * 0.5f;
            const float drawLeft = viewOriginX + (centerX - maxRadius - m_flow.cameraX) * viewScale;
            const float drawRight = viewOriginX + (centerX + maxRadius - m_flow.cameraX) * viewScale;
            const float drawTop = viewOriginY + (centerY - maxRadius - m_flow.cameraY) * viewScale;
            const float drawBottom = viewOriginY + (centerY + maxRadius - m_flow.cameraY) * viewScale;
            if (drawRight < viewOriginX || drawLeft > viewRight || drawBottom < viewOriginY || drawTop > viewBottom)
            {
                continue;
            }

            float intensityScale = 1.0f;
            if (HasTag(*entity, EntityTag::Goal) && !m_flow.goalUnlocked)
            {
                intensityScale = 0.45f;
            }
            if (const auto* checkpoint = entity->GetComponent<CheckpointComponent>())
            {
                intensityScale *= checkpoint->activated ? 1.15f : 0.85f;
            }

            const float dx = centerX - viewCenterWorldX;
            const float dy = centerY - viewCenterWorldY;
            const float distancePenalty = std::sqrt(dx * dx + dy * dy) * 0.02f;
            lightTargets.push_back({
                transform,
                light,
                intensityScale,
                light->radius * light->intensity * intensityScale - distancePenalty
                });
        }
    };

    gatherLightTargets(EntityTag::Goal);
    gatherLightTargets(EntityTag::Checkpoint);
    gatherLightTargets(EntityTag::PhotoSource);
    gatherLightTargets(EntityTag::Hazard);
    gatherLightTargets(EntityTag::Battery);

    const int maxLightEffects = m_lifecycle.darknessStageEnabled ? 6 : 16;
    if (lightTargets.size() > static_cast<size_t>(maxLightEffects))
    {
        const auto priorityCompare = [](const LightDrawTarget& a, const LightDrawTarget& b)
        {
            return a.priority > b.priority;
        };
        std::nth_element(
            lightTargets.begin(),
            lightTargets.begin() + maxLightEffects,
            lightTargets.end(),
            priorityCompare);
        lightTargets.resize(maxLightEffects);
        std::sort(lightTargets.begin(), lightTargets.end(), priorityCompare);
    }

    int godRayCount = 0;
    const int maxGodRays = m_lifecycle.darknessStageEnabled ? 0 : 8;
    for (const LightDrawTarget& target : lightTargets)
    {
        if (godRayCount < maxGodRays)
        {
            DrawGodRay(*this, *target.transform, *target.light, m_flow.cameraX, m_flow.cameraY, target.intensityScale);
            ++godRayCount;
        }
        if (m_lifecycle.darknessStageEnabled)
        {
            DrawCompactFlickerLight(*this, *target.transform, *target.light, m_flow.cameraX, m_flow.cameraY, target.intensityScale);
        }
        else
        {
            DrawFlickerLight(*this, *target.transform, *target.light, m_flow.cameraX, m_flow.cameraY, target.intensityScale);
        }
    }

    for (Entity* entity : stageLightEntities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* stageLight = entity->GetComponent<StageLightComponent>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!stageLight || !transform || !stageLight->enabled)
        {
            continue;
        }

        const float centerX = transform->x + transform->width * transform->scale * 0.5f;
        const float maxHalfWidth = std::max(stageLight->beamTopWidth, stageLight->beamBottomWidth) * transform->scale * 0.5f;
        const float drawLeft = viewOriginX + (centerX - maxHalfWidth - m_flow.cameraX) * viewScale;
        const float drawRight = viewOriginX + (centerX + maxHalfWidth - m_flow.cameraX) * viewScale;
        if (drawRight < viewOriginX || drawLeft > viewOriginX + GetViewWidth())
        {
            continue;
        }

        const float beamLengthWorld = (stageLight->beamLength > 0.0f ? stageLight->beamLength : transform->height * 3.0f) * transform->scale;
        DrawStageLightBeam(*this, *transform, *stageLight, beamLengthWorld, m_flow.cameraX, m_flow.cameraY);
    }

    for (Entity* entity : enemyEntities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!enemy || !transform || !enemy->IsEnabled())
        {
            continue;
        }

        DrawWalkerAttackCaptureCue(
            *transform,
            *enemy,
            viewOriginX,
            viewOriginY,
            viewScale,
            m_flow.cameraX,
            m_flow.cameraY);
    }

    for (Entity* entity : enemyEntities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        const auto* boss = entity->GetComponent<ShieldBossComponent>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!enemy ||
            enemy->GetArchetype() != EnemyArchetype::ShieldBoss ||
            !enemy->IsEnabled() ||
            !boss ||
            boss->deathAnimationActive ||
            boss->deathAnimationFinished ||
            !transform ||
            !IsShieldBossAttackCaptureEffectActive(*entity, *boss))
        {
            continue;
        }

        DrawShieldBossAttackCaptureFrame(
            *transform,
            viewOriginX,
            viewOriginY,
            viewScale,
            m_flow.cameraX,
            m_flow.cameraY);
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

    for (const auto& particle : m_effects.slamDust)
    {
        const float lifeT = Clamp01(particle.life / std::max(0.001f, particle.maxLife));
        const float fade = lifeT * lifeT;
        Shader_ResetStyle();
        Shader_SetTint(particle.r, particle.g, particle.b, fade * particle.alphaScale);
        SpriteDraw(
            m_whiteTexture,
            viewOriginX + (particle.x - m_flow.cameraX) * viewScale,
            viewOriginY + (particle.y - m_flow.cameraY) * viewScale,
            particle.width * viewScale,
            particle.height * viewScale,
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
        const float size = (2.0f + 3.0f * lifeT) * std::max(0.1f, spark.sizeScale) * viewScale;
        const float drawX = viewOriginX + (spark.x - m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + (spark.y - m_flow.cameraY) * viewScale;
        if (spark.drawCircle)
        {
            const int alpha = std::clamp(static_cast<int>(std::round(255.0f * lifeT)), 0, 255);
            const int color = GetColor(
                std::clamp(static_cast<int>(std::round(255.0f * spark.r)), 0, 255),
                std::clamp(static_cast<int>(std::round(255.0f * spark.g)), 0, 255),
                std::clamp(static_cast<int>(std::round(255.0f * spark.b)), 0, 255));

            Shader_ResetStyle();
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            DrawCircleAA(drawX, drawY, std::max(0.75f, size * 0.5f), 24, color, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            continue;
        }

        Shader_ResetStyle();
        Shader_SetTint(spark.r, spark.g, spark.b, lifeT);
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

    for (const auto& shockwave : m_effects.beamShockwaves)
    {
        const float lifeT = Clamp01(shockwave.life / std::max(0.001f, shockwave.maxLife));
        const float progress = 1.0f - lifeT;
        const float eased = EaseOutCubic(progress);
        const float radius = std::lerp(shockwave.startRadius, shockwave.endRadius, eased) * viewScale;
        const float thickness = std::max(1.5f, shockwave.thickness * std::lerp(1.0f, 0.45f, progress) * viewScale);
        const float drawX = viewOriginX + (shockwave.x - m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + (shockwave.y - m_flow.cameraY) * viewScale;
        const int alpha = static_cast<int>(std::round(150.0f * lifeT));
        const int shockwaveColor = GetColor(
            static_cast<int>(std::round(255.0f * shockwave.r)),
            static_cast<int>(std::round(255.0f * shockwave.g)),
            static_cast<int>(std::round(255.0f * shockwave.b)));

        SetDrawBlendMode(DX_BLENDMODE_ADD, alpha);
        if (std::fabs(shockwave.directionX) > 0.001f)
        {
            const float directionX = shockwave.directionX >= 0.0f ? 1.0f : -1.0f;
            const float frontX = drawX + directionX * radius;
            const float flareHeight = radius * 0.34f;
            DrawLineAA(
                drawX,
                drawY,
                frontX,
                drawY,
                shockwaveColor,
                std::max(1.0f, thickness * 1.35f));
            DrawLineAA(
                drawX,
                drawY - flareHeight * 0.32f,
                frontX,
                drawY,
                GetColor(188, 240, 255),
                std::max(1.0f, thickness * 0.58f));
            DrawLineAA(
                drawX,
                drawY + flareHeight * 0.32f,
                frontX,
                drawY,
                GetColor(188, 240, 255),
                std::max(1.0f, thickness * 0.58f));
            DrawCircleAA(
                drawX,
                drawY,
                std::max(2.0f, radius * 0.13f),
                48,
                shockwaveColor,
                FALSE,
                std::max(1.0f, thickness * 0.72f));
            DrawCircleAA(
                frontX,
                drawY,
                std::max(2.0f, radius * 0.08f),
                32,
                GetColor(255, 255, 255),
                TRUE,
                std::max(1.0f, thickness * 0.24f));
        }
        else
        {
            DrawCircleAA(
                drawX,
                drawY,
                radius,
                64,
                shockwaveColor,
                FALSE,
                thickness);
            DrawCircleAA(
                drawX,
                drawY,
                radius * 0.78f,
                64,
                GetColor(255, 255, 255),
                FALSE,
                std::max(1.0f, thickness * 0.42f));
            DrawLineAA(
                drawX - radius * 0.9f,
                drawY,
                drawX + radius * 0.9f,
                drawY,
                GetColor(188, 240, 255),
                std::max(1.0f, thickness * 0.28f));
            DrawLineAA(
                drawX,
                drawY - radius * 0.62f,
                drawX,
                drawY + radius * 0.62f,
                GetColor(188, 240, 255),
                std::max(1.0f, thickness * 0.22f));
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
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
    float drawX = viewOriginX + (transform->x + sprite->GetRenderOffsetX() - m_flow.cameraX) * viewScale;
    float drawY = viewOriginY + (transform->y + sprite->GetRenderOffsetY() - m_flow.cameraY) * viewScale;
    float drawWidth = transform->width * transform->scale * sprite->GetRenderScaleX() * viewScale;
    float drawHeight = transform->height * transform->scale * sprite->GetRenderScaleY() * viewScale;
    const auto* tag = entity.GetComponent<TagComponent>();
    bool fallingShieldTrailActive = false;
    if (tag && (HasTag(tag, EntityTag::BossShield) || HasTag(tag, EntityTag::CapturedShield)))
    {
        const auto* shield = entity.GetComponent<ShieldComponent>();
        if (shield && sprite->GetTextureId() == m_whiteTexture)
        {
            // 攻撃・盾の判定用白テクスチャは見た目だけ隠し、当たり判定は残す。
            return;
        }
        if (HasTag(tag, EntityTag::CapturedShield) &&
            shield &&
            shield->photoSpawned &&
            shield->capturedMode != CapturedShieldMode::Normal &&
            sprite->GetTextureId() == m_whiteTexture)
        {
            // Attack-captured shields sometimes keep a white hitbox texture for collision only.
            // Hide that placeholder so pasted/captured attacks do not show debug boxes.
            return;
        }
        const auto* ownerBoss = shield && shield->ownerBoss
            ? shield->ownerBoss->GetComponent<ShieldBossComponent>()
            : nullptr;
        const bool bossSlamFalling =
            HasTag(tag, EntityTag::BossShield) &&
            shield &&
            ownerBoss &&
            ownerBoss->state == ShieldBossState::JumpDescend &&
            !shield->attached;
        const bool capturedSlamFalling =
            HasTag(tag, EntityTag::CapturedShield) &&
            shield &&
            shield->photoSpawned &&
            shield->capturedMode == CapturedShieldMode::JumpBurst &&
            !shield->grounded &&
            shield->descendSpeed > 0.0f;
        fallingShieldTrailActive = bossSlamFalling || capturedSlamFalling;
        const bool bossShieldVisual =
            shield && ownerBoss &&
            (shield->attached || ownerBoss->knockbackActive || ownerBoss->state == ShieldBossState::Rush || ownerBoss->state == ShieldBossState::RushCooldown);
        const bool capturedShieldVisual =
            shield && shield->photoSpawned && shield->capturedMode == CapturedShieldMode::Normal;
        if (bossShieldVisual || capturedShieldVisual)
        {
            const int textureId = sprite->GetTextureId();
            const int textureWidth = TextureGetWidth(textureId);
            const int textureHeight = TextureGetHeight(textureId);
            const float sourceWidth = sprite->GetSourceWidth();
            const float sourceHeight = sprite->GetSourceHeight();
            if (textureWidth > 0 &&
                textureHeight > 0 &&
                sourceWidth > 0.0f &&
                sourceHeight > 0.0f)
            {
                const float frameWidth = static_cast<float>(textureWidth) * sourceWidth;
                const float frameHeight = static_cast<float>(textureHeight) * sourceHeight;
                if (frameWidth > 0.0f && frameHeight > 0.0f)
                {
                    // Boss shield sheets are authored with a wider frame than the collision box.
                    // Preserve the source aspect ratio here so the shield does not look squashed,
                    // but keep the art anchored to the shield's actual collision box.
                    drawWidth = drawHeight * (frameWidth / frameHeight);
                }
            }
            const float shieldWorldW = transform->width * transform->scale;
            const float shieldWorldH = transform->height * transform->scale;
            const float visualWorldW = drawWidth / viewScale;
            const float visualWorldH = drawHeight / viewScale;
            const float shieldWorldX = transform->x + (shieldWorldW - visualWorldW) * 0.5f;
            const float shieldWorldY = transform->y + (shieldWorldH - visualWorldH) * 0.5f;
            drawX = viewOriginX + (shieldWorldX - m_flow.cameraX) * viewScale;
            drawY = viewOriginY + (shieldWorldY - m_flow.cameraY) * viewScale;
        }
    }
    if (tag && HasTag(tag, EntityTag::Player))
    {
        const auto* animation = entity.GetComponent<SpriteSheetAnimationComponent>();
        if (animation)
        {
            const int textureId = sprite->GetTextureId();
            const int textureWidth = TextureGetWidth(textureId);
            const int textureHeight = TextureGetHeight(textureId);
            const float sourceWidth = sprite->GetSourceWidth();
            const float sourceHeight = sprite->GetSourceHeight();
            if (textureWidth > 0 &&
                textureHeight > 0 &&
                sourceWidth > 0.0f &&
                sourceHeight > 0.0f)
            {
                const float frameWidth = static_cast<float>(textureWidth) * sourceWidth;
                const float frameHeight = static_cast<float>(textureHeight) * sourceHeight;
                if (frameHeight > 0.0f)
                {
                    const float centerX = drawX + drawWidth * 0.5f;
                    drawWidth = drawHeight * (frameWidth / frameHeight);
                    drawX = centerX - drawWidth * 0.5f;
                }
            }
        }
    }
    const auto* tint = entity.GetComponent<TintComponent>();
    const auto* enemyComponent = entity.GetComponent<EnemyComponent>();
    const auto* photoFilter = entity.GetComponent<PhotoFilterComponent>();
    const auto* damagePlatform = entity.GetComponent<DamagePlatformComponent>();
    const auto* spikeStrip = entity.GetComponent<SpikeStripComponent>();
    const auto* photoCopyTile = entity.GetComponent<PhotoCopyTileValueComponent>();
    const auto* midBoss2Spear = entity.GetComponent<MidBoss2SpearComponent>();
    const auto* midBoss2 = entity.GetComponent<MidBoss2Component>();
    const auto* bossBeamCapture = entity.GetComponent<BossBeamCaptureComponent>();
    const auto* capturedBoss2BeamCharge = entity.GetComponent<CapturedBoss2BeamChargeComponent>();
    const bool midBoss2TeleportFlashActive =
        enemyComponent &&
        enemyComponent->GetArchetype() == EnemyArchetype::MidBoss2 &&
        midBoss2 &&
        midBoss2->teleportFlashRemaining > 0.0f;
    const float midBoss2TeleportFlashT = midBoss2TeleportFlashActive
        ? Clamp01(midBoss2->teleportFlashRemaining / 0.24f)
        : 0.0f;

    if (midBoss2TeleportFlashActive)
    {
        const float centerX = drawX + drawWidth * 0.5f;
        const float centerY = drawY + drawHeight * 0.5f;
        drawWidth *= std::lerp(1.0f, 0.84f, midBoss2TeleportFlashT);
        drawHeight *= std::lerp(1.0f, 1.72f, midBoss2TeleportFlashT);
        drawX = centerX - drawWidth * 0.5f;
        drawY = centerY - drawHeight * 0.5f - drawHeight * 0.03f * midBoss2TeleportFlashT;

        const float ghostOffsetX = std::max(4.0f, drawWidth * 0.11f) * std::lerp(0.55f, 1.0f, midBoss2TeleportFlashT);
        const float ghostOffsetY = std::max(3.0f, drawHeight * 0.06f) * std::lerp(0.35f, 0.12f, midBoss2TeleportFlashT);
        if (sprite)
        {
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(72.0f + midBoss2TeleportFlashT * 108.0f)), 0, 255));
            Shader_SetTint(0.70f, 0.94f, 1.0f, std::lerp(0.20f, 0.38f, midBoss2TeleportFlashT));
            SpriteDraw(
                sprite->GetTextureId(),
                drawX - ghostOffsetX,
                drawY + ghostOffsetY,
                drawWidth,
                drawHeight,
                sprite->GetSourceX(),
                sprite->GetSourceY(),
                sprite->GetSourceWidth(),
                sprite->GetSourceHeight(),
                sprite->GetFlipX(),
                transform->rotation);
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(58.0f + midBoss2TeleportFlashT * 96.0f)), 0, 255));
            Shader_SetTint(0.92f, 0.98f, 1.0f, std::lerp(0.12f, 0.26f, midBoss2TeleportFlashT));
            SpriteDraw(
                sprite->GetTextureId(),
                drawX + ghostOffsetX * 0.65f,
                drawY - ghostOffsetY * 0.75f,
                drawWidth,
                drawHeight,
                sprite->GetSourceX(),
                sprite->GetSourceY(),
                sprite->GetSourceWidth(),
                sprite->GetSourceHeight(),
                sprite->GetFlipX(),
                transform->rotation);
            Shader_ResetStyle();
        }

        const float ringRadius = std::max(drawWidth, drawHeight) * std::lerp(0.72f, 1.18f, midBoss2TeleportFlashT);
        const float ringThickness = std::max(2.5f, viewScale * std::lerp(2.8f, 5.0f, midBoss2TeleportFlashT));
        const float ringPulse = 0.92f + 0.08f * std::sin(static_cast<float>(GetNowCount()) * 0.014f);
        const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(124.0f * midBoss2TeleportFlashT * ringPulse)), 0, 255));
        DrawCircleAA(centerX, centerY, ringRadius, 64, GetColor(124, 220, 255), FALSE, ringThickness);
        SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(208.0f * midBoss2TeleportFlashT)), 0, 255));
        DrawCircleAA(centerX, centerY, ringRadius * 0.72f, 64, GetColor(255, 255, 255), FALSE, std::max(1.5f, ringThickness * 0.52f));
        const float flashLine = ringRadius * std::lerp(0.42f, 1.1f, midBoss2TeleportFlashT);
        DrawLineAA(centerX - flashLine, centerY, centerX + flashLine, centerY, GetColor(168, 232, 255), std::max(1.5f, ringThickness * 0.28f));
        DrawLineAA(centerX, centerY - flashLine * 0.72f, centerX, centerY + flashLine * 0.72f, GetColor(168, 232, 255), std::max(1.5f, ringThickness * 0.24f));
        const float burstRadius = ringRadius * std::lerp(0.82f, 1.08f, midBoss2TeleportFlashT);
        const float burstThickness = std::max(1.2f, ringThickness * 0.18f);
        const float burstSpin = timeSeconds * std::lerp(4.0f, 6.5f, midBoss2TeleportFlashT);
        for (int burstIndex = 0; burstIndex < 4; ++burstIndex)
        {
            const float angle = burstSpin + static_cast<float>(burstIndex) * 1.5707963f;
            const float rayX = centerX + std::cos(angle) * burstRadius;
            const float rayY = centerY + std::sin(angle) * burstRadius * 0.62f;
            DrawLineAA(centerX, centerY, rayX, rayY, GetColor(196, 240, 255), burstThickness);
            DrawCircleAA(rayX, rayY, std::max(1.2f, ringThickness * 0.16f), 24, GetColor(255, 255, 255), TRUE, std::max(1.0f, burstThickness * 0.7f));
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        Shader_ResetStyle();
    }
    const auto drawMidBoss3AttackPrediction = [&]()
    {
        if (const auto* capturedMidBoss3Attack = entity.GetComponent<CapturedMidBoss3AttackComponent>())
        {
            if (capturedMidBoss3Attack->kind == CapturedMidBoss3AttackKind::Drill &&
                capturedMidBoss3Attack->waitRemaining > 0.0f &&
                !capturedMidBoss3Attack->launched &&
                !capturedMidBoss3Attack->attachedToBoss)
            {
                const float drillCenterX = drawX + drawWidth * 0.5f;
                const float drillCenterY = drawY + drawHeight * 0.5f;
                const float dirX = std::cos(transform->rotation);
                const float dirY = std::sin(transform->rotation);
                DrawAttackPredictionLine(
                    drillCenterX + dirX * drawWidth * 0.5f,
                    drillCenterY + dirY * drawWidth * 0.5f,
                    dirX,
                    dirY,
                    std::max(560.0f, 1040.0f * viewScale),
                    viewScale,
                    6.2f);
            }
        }

        if (const auto* midBoss3Fist = entity.GetComponent<MidBoss3FistComponent>())
        {
            const auto* ownerBoss = midBoss3Fist->ownerBoss ? midBoss3Fist->ownerBoss->GetComponent<MidBoss3Component>() : nullptr;
            const float fistWorldW = transform->width * transform->scale;
            const float fistWorldH = transform->height * transform->scale;
            const float fistCenterWorldX = transform->x + fistWorldW * 0.5f;
            const float fistCenterWorldY = transform->y + fistWorldH * 0.5f;
            bool drawPrediction = false;
            float predictionStartWorldX = fistCenterWorldX;
            float predictionStartWorldY = fistCenterWorldY;
            float predictionDirX = 0.0f;
            float predictionDirY = 0.0f;
            if (midBoss3Fist->state == MidBoss3FistState::LauncherReady && midBoss3Fist->atAttackStart)
            {
                const float direction = ownerBoss && ownerBoss->launcherDirection < 0 ? -1.0f : 1.0f;
                predictionDirX = direction;
                predictionDirY = 0.0f;
                predictionStartWorldX = direction >= 0.0f ? transform->x + fistWorldW : transform->x;
                predictionStartWorldY = fistCenterWorldY;
                drawPrediction = true;
            }
            else if (midBoss3Fist->state == MidBoss3FistState::MeteorReady && midBoss3Fist->atAttackStart)
            {
                predictionDirX = 0.0f;
                predictionDirY = 1.0f;
                predictionStartWorldX = fistCenterWorldX;
                predictionStartWorldY = transform->y + fistWorldH;
                drawPrediction = true;
            }
            if (drawPrediction)
            {
                DrawAttackPredictionLine(
                    viewOriginX + (predictionStartWorldX - m_flow.cameraX) * viewScale,
                    viewOriginY + (predictionStartWorldY - m_flow.cameraY) * viewScale,
                    predictionDirX,
                    predictionDirY,
                    std::max(480.0f, 900.0f * viewScale),
                    viewScale,
                    static_cast<float>(midBoss3Fist->fistIndex) * 1.37f);
            }
        }

        if (const auto* midBoss3 = entity.GetComponent<MidBoss3Component>())
        {
            if (midBoss3->state == MidBoss3State::DrillFist &&
                midBoss3->drillActive &&
                std::fabs(midBoss3->drillVelocityX) < 0.001f &&
                std::fabs(midBoss3->drillVelocityY) < 0.001f)
            {
                const float screenX = viewOriginX + (midBoss3->drillX - m_flow.cameraX) * viewScale;
                const float screenY = viewOriginY + (midBoss3->drillY - m_flow.cameraY) * viewScale;
                const float screenW = midBoss3->drillWidth * viewScale;
                const float screenH = midBoss3->drillHeight * viewScale;
                const float drillAngle = midBoss3->drillGroundRush
                    ? (midBoss3->drillDirection >= 0 ? 0.0f : 3.14159265f)
                    : std::atan2(midBoss3->drillAimY, midBoss3->drillAimX);
                const float drillCenterX = screenX + screenW * 0.5f;
                const float drillCenterY = screenY + screenH * 0.5f;
                DrawAttackPredictionLine(
                    drillCenterX + std::cos(drillAngle) * screenW * 0.5f,
                    drillCenterY + std::sin(drillAngle) * screenW * 0.5f,
                    std::cos(drillAngle),
                    std::sin(drillAngle),
                    std::max(560.0f, 1040.0f * viewScale),
                    viewScale,
                    4.8f);
            }
        }
    };
    bool hasVisibleMidBoss3Drill = false;
    if (const auto* midBoss3 = entity.GetComponent<MidBoss3Component>())
    {
        if (midBoss3->drillActive)
        {
            const float drillDrawX = viewOriginX + (midBoss3->drillX - m_flow.cameraX) * viewScale;
            const float drillDrawW = midBoss3->drillWidth * viewScale;
            hasVisibleMidBoss3Drill = drillDrawX + drillDrawW >= viewOriginX &&
                drillDrawX <= viewOriginX + viewWidth;
        }
    }
    const bool horizontallyOutside = drawX + drawWidth < viewOriginX || drawX > viewOriginX + viewWidth;
    if (!hasVisibleMidBoss3Drill && horizontallyOutside)
    {
        drawMidBoss3AttackPrediction();
        return;
    }

    Shader_ResetStyle();
    float alphaMultiplier = 1.0f;
    float lifeProgress = 0.0f;
    if (const auto* lifetime = entity.GetComponent<PhotoCopyLifetimeComponent>())
    {
        const float totalLifetime = std::max(0.001f, lifetime->GetLifetimeSeconds());
        const float remainingRatio = Clamp01(lifetime->GetRemainingSeconds() / totalLifetime);
        alphaMultiplier = remainingRatio;
        lifeProgress = 1.0f - remainingRatio;
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

    if (tag && HasTag(tag, "WalkerMeleeAttack"))
    {
        DrawCapturedWalkerMeleeEffect(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            transform->rotation,
            alphaMultiplier,
            lifeProgress);
        Shader_ResetStyle();
        return;
    }

    if (tag && HasTag(tag, EntityTag::ConveyorBelt))
    {
        const auto* beltConveyor = entity.GetComponent<BeltConveyorComponent>();
        if (beltConveyor)
        {
            const float directionX = beltConveyor->directionX < 0 ? -1.0f : 1.0f;
            const int widthTiles = std::max(1, beltConveyor->widthTiles);
            const float tileWidth = drawWidth / static_cast<float>(widthTiles);
            const float baseR = tint ? tint->r : 0.50f;
            const float baseG = tint ? tint->g : 0.46f;
            const float baseB = tint ? tint->b : 0.56f;
            const float baseA = tint ? tint->a : 1.0f;

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(230.0f * alphaMultiplier)));
            Shader_SetTint(baseR, baseG, baseB, baseA * alphaMultiplier);
            SpriteDraw(
                m_whiteTexture,
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
            Shader_ResetStyle();
            DrawBoxAA(
                drawX,
                drawY,
                drawX + drawWidth,
                drawY + drawHeight,
                GetColor(58, 48, 68),
                FALSE,
                std::max(1.0f, 2.0f * viewScale));

            const int arrowTexture = m_assets.GetTexture("arrow");
            if (arrowTexture >= 0)
            {
                const float arrowPaddingX = tileWidth * 0.10f;
                const float arrowPaddingY = drawHeight * 0.16f;
                Shader_SetTint(1.0f, 0.90f, 1.0f, 0.82f * alphaMultiplier);
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(210.0f * alphaMultiplier)));
                for (int tileIndex = 0; tileIndex < widthTiles; ++tileIndex)
                {
                    SpriteDraw(
                        arrowTexture,
                        drawX + static_cast<float>(tileIndex) * tileWidth + arrowPaddingX,
                        drawY + arrowPaddingY,
                        std::max(1.0f, tileWidth - arrowPaddingX * 2.0f),
                        std::max(1.0f, drawHeight - arrowPaddingY * 2.0f),
                        0.0f,
                        0.0f,
                        1.0f,
                        1.0f,
                        directionX < 0.0f,
                        0.0f);
                }
                Shader_ResetStyle();
            }

            const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
            const float lineSpacing = std::max(10.0f, tileWidth * 0.48f);
            const float lineLength = std::max(6.0f, tileWidth * 0.30f);
            const float lineOffset = std::fmod(timeSeconds * lineSpacing * 1.8f, lineSpacing);
            const float animatedOffset = directionX >= 0.0f ? lineOffset : -lineOffset;
            const float lineTopY = drawY + std::max(2.0f, drawHeight * 0.10f);
            const float lineBottomY = drawY + drawHeight - std::max(2.0f, drawHeight * 0.10f);
            const float wideThickness = std::max(2.0f, 3.2f * viewScale);
            const float coreThickness = std::max(1.0f, 1.4f * viewScale);
            const int dashCount = std::max(4, widthTiles * 3 + 4);
            const auto drawFlowLine = [&](float y)
            {
                for (int lineIndex = 0; lineIndex < dashCount; ++lineIndex)
                {
                    const float rawStartX =
                        drawX - lineSpacing +
                        static_cast<float>(lineIndex) * lineSpacing +
                        animatedOffset;
                    const float startX = std::clamp(rawStartX, drawX, drawX + drawWidth);
                    const float endX = std::clamp(rawStartX + lineLength, drawX, drawX + drawWidth);
                    if (endX <= startX)
                    {
                        continue;
                    }

                    SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(84.0f * alphaMultiplier)));
                    DrawLineAA(startX, y, endX, y, GetColor(255, 76, 214), wideThickness);
                    SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(188.0f * alphaMultiplier)));
                    DrawLineAA(startX, y, endX, y, GetColor(255, 210, 250), coreThickness);
                }
            };

            drawFlowLine(lineTopY);
            drawFlowLine(lineBottomY);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            Shader_ResetStyle();
            return;
        }
    }

    if (tag && HasTag(tag, kTagBatterySwitch))
    {
        const auto* batterySwitch = entity.GetComponent<BatterySwitchComponent>();
        const bool laserSwitch = batterySwitch && batterySwitch->controlsLaserPower;
        const bool pressed = batterySwitch && batterySwitch->isPressed;
        const int left = static_cast<int>(std::round(drawX));
        const int top = static_cast<int>(std::round(drawY));
        const int right = static_cast<int>(std::round(drawX + drawWidth));
        const int bottom = static_cast<int>(std::round(drawY + drawHeight));
        const int width = std::max(1, right - left);
        const int height = std::max(1, bottom - top);
        const int bevel = std::max(2, height / 4);
        const int faceTop = top - std::max(2, height / 2);
        const int faceBottom = top + std::max(3, height / 3);
        const int rimColor = laserSwitch ? GetColor(112, 38, 28) : GetColor(96, 44, 36);
        const int sideColor = laserSwitch ? GetColor(116, 52, 42) : GetColor(112, 58, 46);
        const int baseColor = laserSwitch ? GetColor(60, 62, 72) : GetColor(68, 62, 56);
        const int faceColor = pressed
            ? (laserSwitch ? GetColor(255, 92, 56) : GetColor(78, 228, 112))
            : (laserSwitch ? GetColor(214, 72, 48) : GetColor(224, 54, 42));
        const int highlightColor = pressed
            ? (laserSwitch ? GetColor(255, 174, 112) : GetColor(182, 255, 190))
            : (laserSwitch ? GetColor(255, 138, 84) : GetColor(255, 116, 96));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(220.0f * alphaMultiplier)));
        DrawBox(left, top, right, bottom, baseColor, TRUE);
        DrawBox(left, top, right, bottom, rimColor, FALSE);
        DrawBox(left + bevel, faceBottom, right - bevel, bottom, sideColor, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(245.0f * alphaMultiplier)));
        if (batterySwitch && batterySwitch->pressMode == SwitchPressMode::Player)
        {
            const float pressRate = batterySwitch->pressDepth > 0.0f
                ? std::clamp(batterySwitch->currentPress / batterySwitch->pressDepth, 0.0f, 1.0f)
                : (batterySwitch->isPressed ? 1.0f : 0.0f);
            DrawPlayerSwitchDomeFace(
                left,
                right,
                height,
                bevel,
                faceTop,
                faceBottom,
                faceColor,
                highlightColor,
                rimColor,
                pressRate,
                alphaMultiplier);
            Shader_ResetStyle();
            return;
        }

        DrawBox(left + bevel, faceTop, right - bevel, faceBottom, faceColor, TRUE);
        DrawBox(left + bevel, faceTop, right - bevel, faceBottom, rimColor, FALSE);
        DrawBox(left + bevel * 2, faceTop + 2, right - bevel * 2, faceTop + std::max(3, height / 5), highlightColor, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        Shader_ResetStyle();
        return;
    }

    if (tag && HasTag(tag, EntityTag::StageLight))
    {
        if (const auto* stageLight = entity.GetComponent<StageLightComponent>())
        {
            DrawStageLightFixtureShape(
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                transform->rotation,
                *stageLight,
                tint,
                alphaMultiplier);
            Shader_ResetStyle();
            return;
        }
    }

    if (tag && HasTag(tag, EntityTag::JumpPad))
    {
        const auto* jumpPad = entity.GetComponent<JumpPadComponent>();
        if (jumpPad)
        {
            const float worldWidth = transform->width * transform->scale;
            const float worldHeight = transform->height * transform->scale;
            const float boardHeight = worldHeight * 0.5f;
            const float halfBoardWidth = worldWidth * 0.5f;
            const float halfBoardHeight = boardHeight * 0.5f;
            const float centerWorldX = transform->x + halfBoardWidth;
            const float centerWorldY = transform->y + halfBoardHeight;
            const float cosTilt = std::cos(jumpPad->tilt);
            const float sinTilt = std::sin(jumpPad->tilt);
            const float normalX = -sinTilt;
            const float normalY = cosTilt;
            const auto toScreenX = [&](float worldX) -> float
            {
                return viewOriginX + (worldX - m_flow.cameraX) * viewScale;
            };
            const auto toScreenY = [&](float worldY) -> float
            {
                return viewOriginY + (worldY - m_flow.cameraY) * viewScale;
            };
            const auto cornerX = [&](float localX, float localY) -> float
            {
                return toScreenX(centerWorldX + localX * cosTilt + localY * normalX);
            };
            const auto cornerY = [&](float localX, float localY) -> float
            {
                return toScreenY(centerWorldY + localX * sinTilt + localY * normalY);
            };
            const int blue = GetColor(0, 0, 255);
            const int outline = GetColor(0, 0, 142);
            const float leftTopX = cornerX(-halfBoardWidth, -halfBoardHeight);
            const float leftTopY = cornerY(-halfBoardWidth, -halfBoardHeight);
            const float rightTopX = cornerX(halfBoardWidth, -halfBoardHeight);
            const float rightTopY = cornerY(halfBoardWidth, -halfBoardHeight);
            const float rightBottomX = cornerX(halfBoardWidth, halfBoardHeight);
            const float rightBottomY = cornerY(halfBoardWidth, halfBoardHeight);
            const float leftBottomX = cornerX(-halfBoardWidth, halfBoardHeight);
            const float leftBottomY = cornerY(-halfBoardWidth, halfBoardHeight);

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(255.0f * alphaMultiplier)));
            DrawQuadrangleAA(
                leftTopX, leftTopY,
                rightTopX, rightTopY,
                rightBottomX, rightBottomY,
                leftBottomX, leftBottomY,
                blue,
                TRUE);
            DrawLineAA(leftTopX, leftTopY, rightTopX, rightTopY, outline, std::max(1.0f, 2.0f * viewScale));
            DrawLineAA(rightTopX, rightTopY, rightBottomX, rightBottomY, outline, std::max(1.0f, 2.0f * viewScale));
            DrawLineAA(rightBottomX, rightBottomY, leftBottomX, leftBottomY, outline, std::max(1.0f, 2.0f * viewScale));
            DrawLineAA(leftBottomX, leftBottomY, leftTopX, leftTopY, outline, std::max(1.0f, 2.0f * viewScale));

            const float tileSize = m_tileMap.GetTileSize();
            const float baseSide = tileSize > 0.0f ? tileSize : boardHeight;
            const float baseHeight = baseSide * 0.8660254f;
            const float baseTopX = toScreenX(centerWorldX);
            const float baseTopY = toScreenY(transform->y + boardHeight);
            const float baseLeftX = toScreenX(centerWorldX - baseSide * 0.5f);
            const float baseRightX = toScreenX(centerWorldX + baseSide * 0.5f);
            const float baseBottomY = toScreenY(transform->y + boardHeight + baseHeight);
            DrawTriangleAA(baseTopX, baseTopY, baseLeftX, baseBottomY, baseRightX, baseBottomY, blue, TRUE);
            DrawLineAA(baseTopX, baseTopY, baseLeftX, baseBottomY, outline, std::max(1.0f, 2.0f * viewScale));
            DrawLineAA(baseLeftX, baseBottomY, baseRightX, baseBottomY, outline, std::max(1.0f, 2.0f * viewScale));
            DrawLineAA(baseRightX, baseBottomY, baseTopX, baseTopY, outline, std::max(1.0f, 2.0f * viewScale));
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            Shader_ResetStyle();
            return;
        }
    }

    if (tag && HasTag(tag, EntityTag::SepiaRubble))
    {
        if (const auto* group = entity.GetComponent<SepiaRubbleGroupComponent>())
        {
            if (group->markerType == '<')
            {
                const float tileSize = m_tileMap.GetTileSize();
                if (tileSize > 0.0f)
                {
                    const float groupX = static_cast<float>(group->minColumn) * tileSize;
                    const float groupY = static_cast<float>(group->minRow) * tileSize;
                    const float groupW = static_cast<float>(group->maxColumn - group->minColumn + 1) * tileSize;
                    const float groupH = static_cast<float>(group->maxRow - group->minRow + 1) * tileSize;

                    const bool isRepresentative =
                        std::fabs(transform->x - groupX) <= 0.01f &&
                        std::fabs(transform->y - groupY) <= 0.01f;

                    if (!isRepresentative)
                    {
                        Shader_ResetStyle();
                        return;
                    }

                    drawX = GetViewOriginX() + (groupX + sprite->GetRenderOffsetX() - m_flow.cameraX) * viewScale;
                    drawY = GetViewOriginY() + (groupY + sprite->GetRenderOffsetY() - m_flow.cameraY) * viewScale;
                    drawWidth = groupW * transform->scale * sprite->GetRenderScaleX() * viewScale;
                    drawHeight = groupH * transform->scale * sprite->GetRenderScaleY() * viewScale;
                }
            }
        }

        if (tint)
        {
            Shader_SetTint(tint->r, tint->g, tint->b, tint->a);
        }

        SpriteDraw(
            sprite->GetTextureId(),
            drawX, drawY, drawWidth, drawHeight,
            sprite->GetSourceX(), sprite->GetSourceY(),
            sprite->GetSourceWidth(), sprite->GetSourceHeight(),
            sprite->GetFlipX(),
            transform->rotation);

        Shader_ResetStyle();
        return;
    }

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
    else if (photoFilter)
    {
        switch (photoFilter->GetTheme())
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
    else if (tag && HasTag(tag, kTagPhotoBox))
    {
        const auto* photoLayer = entity.GetComponent<PhotoCopyLayerComponent>();
        const auto* photoOrigin = entity.GetComponent<PhotoCopyOriginComponent>();
        ApplyPhotoBoxRoleStyle(entity.GetComponent<PhotoCopyRoleComponent>());
        ApplyPhotoBoxLayerStyle(photoLayer, photoOrigin, tint);
        ApplyPhotoBoxThemeStyle(entity.GetComponent<PhotoCopyEffectComponent>());
    }
    else if (tag && HasTag(tag, "BossShockwave"))
    {
        // 叩きつけ後の衝撃波は判定だけ残し、たたき台の青い可視判定は描かない。
        Shader_ResetStyle();
        return;
    }

    if (fallingShieldTrailActive)
    {
        DrawFallingShieldTrail(
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
            transform->rotation,
            viewScale,
            alphaMultiplier);
    }

    else if (tag && HasTag(tag, kTagBullet))
    {
        const auto* projectile = entity.GetComponent<ProjectileComponent>();
        if (projectile)
        {
            if (const auto* capturedMidBoss3Attack = entity.GetComponent<CapturedMidBoss3AttackComponent>())
            {
                if (capturedMidBoss3Attack->kind == CapturedMidBoss3AttackKind::Drill)
                {
                    if (capturedMidBoss3Attack->waitRemaining > 0.0f &&
                        !capturedMidBoss3Attack->launched &&
                        !capturedMidBoss3Attack->attachedToBoss)
                    {
                        const float drillCenterX = drawX + drawWidth * 0.5f;
                        const float drillCenterY = drawY + drawHeight * 0.5f;
                        const float dirX = std::cos(transform->rotation);
                        const float dirY = std::sin(transform->rotation);
                        DrawAttackPredictionLine(
                            drillCenterX + dirX * drawWidth * 0.5f,
                            drillCenterY + dirY * drawWidth * 0.5f,
                            dirX,
                            dirY,
                            std::max(560.0f, 1040.0f * viewScale),
                            viewScale,
                            6.2f);
                    }
                    DrawMidBoss3DrillShape(
                        drawX,
                        drawY,
                        drawWidth,
                        drawHeight,
                        transform->rotation,
                        capturedMidBoss3Attack->direction,
                        transform->x * 0.015f,
                        viewScale,
                        alphaMultiplier);
                    Shader_ResetStyle();
                    return;
                }

                const int color = GetColor(246, 132, 46);
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(235.0f * alphaMultiplier)));
                const float centerX = drawX + drawWidth * 0.5f;
                const float centerY = drawY + drawHeight * 0.5f;
                const float halfW = drawWidth * 0.5f;
                const float halfH = drawHeight * 0.5f;
                const float c = std::cos(transform->rotation);
                const float s = std::sin(transform->rotation);
                const auto cornerX = [&](float localX, float localY) -> float
                {
                    return centerX + localX * c - localY * s;
                };
                const auto cornerY = [&](float localX, float localY) -> float
                {
                    return centerY + localX * s + localY * c;
                };
                DrawQuadrangleAA(
                    cornerX(-halfW, -halfH),
                    cornerY(-halfW, -halfH),
                    cornerX(halfW, -halfH),
                    cornerY(halfW, -halfH),
                    cornerX(halfW, halfH),
                    cornerY(halfW, halfH),
                    cornerX(-halfW, halfH),
                    cornerY(-halfW, halfH),
                    color,
                    TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                Shader_ResetStyle();
                return;
            }

            const float angle = std::atan2(projectile->GetVelocityY(), projectile->GetVelocityX());
            if (midBoss2Spear)
            {
                const auto* spear = midBoss2Spear;
                const float spearAngle = transform->rotation;
                const float centerX = drawX + drawWidth * 0.5f;
                const float centerY = drawY + drawHeight * 0.5f;
                const float progress = spear
                    ? std::clamp(spear->launchDelay > 0.0f ? spear->launchTimer / spear->launchDelay : 1.0f, 0.0f, 1.0f)
                    : 1.0f;
                const bool isForming = spear && !spear->launched;
                const float spearFade = spear
                    ? std::clamp(spear->fadeDuration > 0.0f ? spear->fadeRemaining / spear->fadeDuration : 1.0f, 0.0f, 1.0f)
                    : 1.0f;
                const float formationStretch = isForming ? std::lerp(0.18f, 1.0f, progress) : 1.0f;
                const float shaftLength = drawWidth * std::lerp(0.24f, 0.88f, formationStretch);
                const float shaftHalfThickness = std::max(1.5f, drawHeight * std::lerp(0.08f, 0.14f, formationStretch));
                const float headLength = std::max(drawHeight * std::lerp(0.26f, 1.02f, formationStretch), drawWidth * 0.18f);
                const float tailLength = std::max(drawHeight * std::lerp(0.10f, 0.28f, formationStretch), drawWidth * 0.05f);
                const float haloScale = 1.0f;

                float tipX = centerX + shaftLength * 0.5f + headLength;
                float tipY = centerY;
                float leftHeadBaseX = centerX + shaftLength * 0.5f;
                float leftHeadBaseY = centerY - drawHeight * 0.42f;
                float rightHeadBaseX = centerX + shaftLength * 0.5f;
                float rightHeadBaseY = centerY + drawHeight * 0.42f;
                float bodyTopLeftX = centerX - shaftLength * 0.5f;
                float bodyTopLeftY = centerY - shaftHalfThickness;
                float bodyTopRightX = centerX + shaftLength * 0.5f;
                float bodyTopRightY = centerY - shaftHalfThickness;
                float bodyBottomLeftX = centerX - shaftLength * 0.5f;
                float bodyBottomLeftY = centerY + shaftHalfThickness;
                float bodyBottomRightX = centerX + shaftLength * 0.5f;
                float bodyBottomRightY = centerY + shaftHalfThickness;
                float tailTopX = centerX - shaftLength * 0.5f;
                float tailTopY = centerY - drawHeight * 0.34f;
                float tailBottomX = centerX - shaftLength * 0.5f;
                float tailBottomY = centerY + drawHeight * 0.34f;
                float tailBackX = centerX - shaftLength * 0.5f - tailLength;
                float tailBackY = centerY;

                RotatePoint(centerX, centerY, spearAngle, tipX, tipY);
                RotatePoint(centerX, centerY, spearAngle, leftHeadBaseX, leftHeadBaseY);
                RotatePoint(centerX, centerY, spearAngle, rightHeadBaseX, rightHeadBaseY);
                RotatePoint(centerX, centerY, spearAngle, bodyTopLeftX, bodyTopLeftY);
                RotatePoint(centerX, centerY, spearAngle, bodyTopRightX, bodyTopRightY);
                RotatePoint(centerX, centerY, spearAngle, bodyBottomLeftX, bodyBottomLeftY);
                RotatePoint(centerX, centerY, spearAngle, bodyBottomRightX, bodyBottomRightY);
                RotatePoint(centerX, centerY, spearAngle, tailTopX, tailTopY);
                RotatePoint(centerX, centerY, spearAngle, tailBottomX, tailBottomY);
                RotatePoint(centerX, centerY, spearAngle, tailBackX, tailBackY);

                const float spearAlphaScale = spear ? spearFade : 1.0f;
                const int auraAlpha = std::clamp(static_cast<int>(std::round((isForming ? 128.0f + progress * 80.0f : 164.0f) * alphaMultiplier * spearAlphaScale)), 0, 255);
                const int coreAlpha = std::clamp(static_cast<int>(std::round((isForming ? 180.0f + progress * 60.0f : 224.0f) * alphaMultiplier * spearAlphaScale)), 0, 255);

                SetDrawBlendMode(DX_BLENDMODE_ADD, auraAlpha);
                DrawLine(
                    static_cast<int>(std::round(bodyTopLeftX)),
                    static_cast<int>(std::round(bodyTopLeftY)),
                    static_cast<int>(std::round(tipX)),
                    static_cast<int>(std::round(tipY)),
                    GetColor(110, 228, 255),
                    std::max(3, static_cast<int>(std::round(6.0f * haloScale))));
                DrawLine(
                    static_cast<int>(std::round(bodyBottomLeftX)),
                    static_cast<int>(std::round(bodyBottomLeftY)),
                    static_cast<int>(std::round(tipX)),
                    static_cast<int>(std::round(tipY)),
                    GetColor(110, 228, 255),
                    std::max(3, static_cast<int>(std::round(6.0f * haloScale))));

                if (isForming)
                {
                    const float glowHeight = drawHeight * std::lerp(0.18f, 0.92f, progress);
                    const float glowHalfWidth = std::max(2.0f, drawHeight * 0.16f);
                    float glowTopX = centerX;
                    float glowTopY = centerY - glowHeight * 0.5f;
                    float glowBottomX = centerX;
                    float glowBottomY = centerY + glowHeight * 0.5f;
                    float glowLeftX = centerX - glowHalfWidth;
                    float glowLeftY = centerY;
                    float glowRightX = centerX + glowHalfWidth;
                    float glowRightY = centerY;
                    RotatePoint(centerX, centerY, spearAngle, glowTopX, glowTopY);
                    RotatePoint(centerX, centerY, spearAngle, glowBottomX, glowBottomY);
                    RotatePoint(centerX, centerY, spearAngle, glowLeftX, glowLeftY);
                    RotatePoint(centerX, centerY, spearAngle, glowRightX, glowRightY);
                    DrawQuadrangleAA(
                        glowTopX, glowTopY,
                        glowRightX, glowRightY,
                        glowBottomX, glowBottomY,
                        glowLeftX, glowLeftY,
                        GetColor(138, 232, 255),
                        TRUE);

                    const float telegraphRadius = std::max(drawWidth, drawHeight) * std::lerp(0.38f, 0.82f, progress);
                    const float telegraphThickness = std::max(1.5f, drawHeight * std::lerp(0.06f, 0.12f, progress));
                    float telegraphLineStartX = centerX - telegraphRadius;
                    float telegraphLineStartY = centerY;
                    float telegraphLineEndX = centerX + telegraphRadius * 1.12f;
                    float telegraphLineEndY = centerY;
                    RotatePoint(centerX, centerY, spearAngle, telegraphLineStartX, telegraphLineStartY);
                    RotatePoint(centerX, centerY, spearAngle, telegraphLineEndX, telegraphLineEndY);
                    SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((90.0f + progress * 110.0f) * alphaMultiplier * spearAlphaScale)), 0, 255));
                    DrawCircleAA(
                        centerX,
                        centerY,
                        telegraphRadius * 0.98f,
                        48,
                        GetColor(122, 224, 255),
                        FALSE,
                        telegraphThickness * 0.72f);
                    DrawLineAA(
                        telegraphLineStartX,
                        telegraphLineStartY,
                        telegraphLineEndX,
                        telegraphLineEndY,
                        GetColor(242, 255, 255),
                        telegraphThickness);
                }

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, coreAlpha);
                DrawQuadrangleAA(
                    bodyTopLeftX, bodyTopLeftY,
                    bodyTopRightX, bodyTopRightY,
                    bodyBottomRightX, bodyBottomRightY,
                    bodyBottomLeftX, bodyBottomLeftY,
                    GetColor(188, 246, 255),
                    TRUE);
                DrawTriangleAA(
                    tipX, tipY,
                    leftHeadBaseX, leftHeadBaseY,
                    rightHeadBaseX, rightHeadBaseY,
                    GetColor(255, 255, 255),
                    TRUE);
                DrawTriangleAA(
                    tailTopX, tailTopY,
                    tailBottomX, tailBottomY,
                    tailBackX, tailBackY,
                    GetColor(118, 220, 255),
                    TRUE);

                const float runProgress = isForming ? progress : 1.0f;
                const float runX = std::lerp(tipX, tailBackX, runProgress);
                const float runY = std::lerp(tipY, tailBackY, runProgress);
                SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(168.0f * alphaMultiplier * spearAlphaScale)), 0, 255));
                DrawCircle(
                    static_cast<int>(std::round(runX)),
                    static_cast<int>(std::round(runY)),
                    std::max(2, static_cast<int>(std::round((drawHeight * 0.22f) * haloScale))),
                    GetColor(255, 255, 255),
                    TRUE);

                const float particleRadius = std::max(drawWidth, drawHeight) * std::lerp(0.85f, 0.42f, progress);
                const float spearTimeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
                constexpr int kParticleCount = 8;
                for (int index = 0; index < kParticleCount; ++index)
                {
                    const float seed = static_cast<float>(index) / static_cast<float>(kParticleCount);
                    const float cycle = std::fmod(spearTimeSeconds * 1.8f + seed * 1.37f, 1.0f);
                    const float travelT = isForming ? (1.0f - std::pow(cycle, 1.6f)) : (0.16f + 0.18f * std::sin(seed * 19.0f + spearTimeSeconds * 6.0f));
                    const float particleAngle = seed * 6.28318530718f + std::sin(seed * 29.0f) * 0.45f;
                    float orbitX = centerX + std::cos(particleAngle) * particleRadius;
                    float orbitY = centerY + std::sin(particleAngle) * particleRadius * 0.55f;
                    RotatePoint(centerX, centerY, spearAngle, orbitX, orbitY);
                    const float particleX = std::lerp(centerX, orbitX, travelT);
                    const float particleY = std::lerp(centerY, orbitY, travelT);
                    SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((104.0f + progress * 92.0f) * alphaMultiplier * spearAlphaScale)), 0, 255));
                    DrawCircle(
                        static_cast<int>(std::round(particleX)),
                        static_cast<int>(std::round(particleY)),
                        std::max(1, static_cast<int>(std::round(drawHeight * 0.12f))),
                        GetColor(194, 246, 255),
                        TRUE);
                }

                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                Shader_ResetStyle();
                return;
            }
            int color = GetColor(255, 230, 50);
            if (projectile->GetOwner() == ProjectileComponent::Owner::BlasterRobot)
            {
                const float centerX = drawX + drawWidth * 0.5f;
                const float centerY = drawY + drawHeight * 0.5f;
                const unsigned int outerGlowColor = GetColor(60, 255, 150);
                const unsigned int innerGlowColor = GetColor(180, 255, 220);
                DrawProjectileGlowTriangle(centerX, centerY, drawWidth * 2.4f, drawHeight * 2.4f, angle, outerGlowColor, 72);
                DrawProjectileGlowTriangle(centerX, centerY, drawWidth * 1.8f, drawHeight * 1.8f, angle, innerGlowColor, 116);
                DrawProjectileGlowTriangle(centerX, centerY, drawWidth * 1.3f, drawHeight * 1.3f, angle, GetColor(235, 255, 245), 148);
                color = GetColor(110, 255, 170);
            }
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
        if ((entity.GetComponent<PhotoCopyGroupComponent>() || bossBeamCapture) &&
            drawWidth >= drawHeight)
        {
            float renderDrawX = drawX;
            float renderDrawY = drawY;
            float renderDrawWidth = drawWidth;
            float renderDrawHeight = drawHeight;
            if (bossBeamCapture && bossBeamCapture->visualLeakLength > 0.0f)
            {
                const float leakLength = bossBeamCapture->visualLeakLength * viewScale;
                if (bossBeamCapture->sourceOnLeft)
                {
                    renderDrawWidth += leakLength;
                }
                else
                {
                    renderDrawX -= leakLength;
                    renderDrawWidth += leakLength;
                }
            }
            const int outerColor = GetColor(124, 206, 255);
            const int coreColor = GetColor(236, 248, 255);
            const bool isBossBeam = bossBeamCapture != nullptr;
            const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
            const float beamCenterY = renderDrawY + renderDrawHeight * 0.5f;
            const float beamHalfHeight = renderDrawHeight * 0.5f;
            const float basePulse = isBossBeam
                ? 0.94f + 0.06f * std::sin(timeSeconds * 7.0f)
                : 0.97f + 0.03f * std::sin(timeSeconds * 6.0f);
            const float beamEnergy = isBossBeam ? 1.25f : 1.0f;
            const float coreThickness = std::max(2.0f, renderDrawHeight * (isBossBeam ? 0.22f : 0.18f));
            const float innerGlowThickness = std::max(4.0f, renderDrawHeight * (isBossBeam ? 0.40f : 0.34f));
            const float outerGlowThickness = std::max(6.0f, renderDrawHeight * (isBossBeam ? 0.68f : 0.58f));
            const int outerGlowAlpha = std::clamp(static_cast<int>(std::round(60.0f * alphaMultiplier * basePulse * beamEnergy)), 0, 255);
            const int innerGlowAlpha = std::clamp(static_cast<int>(std::round(108.0f * alphaMultiplier * basePulse * beamEnergy)), 0, 255);
            const int coreAlpha = std::clamp(static_cast<int>(std::round(240.0f * alphaMultiplier * basePulse)), 0, 255);
            constexpr int kWrapSegments = 96;
            const float wrapAmplitude = beamHalfHeight * (isBossBeam ? 0.42f : 0.34f);
            const float wrapFrequency = isBossBeam ? 2.15f : 1.8f;
            const float wrapSpeed = isBossBeam ? 4.8f : 3.8f;
            const float wrapThickness = std::max(1.0f, renderDrawHeight * (isBossBeam ? 0.10f : 0.08f));
            const int wrapGlowAlpha = std::clamp(static_cast<int>(std::round((isBossBeam ? 104.0f : 84.0f) * alphaMultiplier * beamEnergy)), 0, 255);
            const int wrapCoreAlpha = std::clamp(static_cast<int>(std::round((isBossBeam ? 172.0f : 136.0f) * alphaMultiplier * beamEnergy)), 0, 255);
            const bool beamSourceOnLeft = !bossBeamCapture || bossBeamCapture->sourceOnLeft;
            const float sourceX = beamSourceOnLeft ? renderDrawX : renderDrawX + renderDrawWidth;
            const float targetX = beamSourceOnLeft ? renderDrawX + renderDrawWidth : renderDrawX;
            const float muzzlePulse = 0.88f + 0.12f * std::sin(timeSeconds * 18.0f);
            const float impactPulse = 0.84f + 0.16f * std::sin(timeSeconds * 11.0f + 1.7f);

            SetDrawBlendMode(DX_BLENDMODE_ADD, outerGlowAlpha);
            DrawLineAA(renderDrawX, beamCenterY, renderDrawX + renderDrawWidth, beamCenterY, outerColor, outerGlowThickness);
            SetDrawBlendMode(DX_BLENDMODE_ADD, innerGlowAlpha);
            DrawLineAA(renderDrawX, beamCenterY, renderDrawX + renderDrawWidth, beamCenterY, GetColor(176, 226, 255), innerGlowThickness);
            if (isBossBeam)
            {
                DrawLineAA(
                    renderDrawX,
                    beamCenterY - renderDrawHeight * 0.14f,
                    renderDrawX + renderDrawWidth,
                    beamCenterY - renderDrawHeight * 0.14f,
                    GetColor(150, 224, 255),
                    std::max(2.0f, outerGlowThickness * 0.42f));
                DrawLineAA(
                    renderDrawX,
                    beamCenterY + renderDrawHeight * 0.14f,
                    renderDrawX + renderDrawWidth,
                    beamCenterY + renderDrawHeight * 0.14f,
                    GetColor(150, 224, 255),
                    std::max(2.0f, outerGlowThickness * 0.42f));
            }

            auto drawWrappedLine = [&](int alpha, int color, float thickness, float pointRadius)
            {
                SetDrawBlendMode(DX_BLENDMODE_ADD, alpha);
                auto sampleX = [&](float t)
                {
                    return std::lerp(sourceX, targetX, t);
                };
                auto sampleY = [&](float t)
                {
                    return beamCenterY + std::sin(t * 6.28318530718f * wrapFrequency - timeSeconds * wrapSpeed) * wrapAmplitude;
                };
                float previousX = sampleX(0.0f);
                float previousY = sampleY(0.0f);
                for (int segment = 1; segment <= kWrapSegments; ++segment)
                {
                    const float t = static_cast<float>(segment) / static_cast<float>(kWrapSegments);
                    const float x = sampleX(t);
                    const float y = sampleY(t);
                    DrawLineAA(previousX, previousY, x, y, color, thickness);
                    DrawCircle(
                        static_cast<int>(std::round(x)),
                        static_cast<int>(std::round(y)),
                        static_cast<int>(std::round(pointRadius)),
                        color,
                        TRUE);
                    previousX = x;
                    previousY = y;
                }
            };

            drawWrappedLine(wrapGlowAlpha, GetColor(140, 214, 255), wrapThickness * 2.2f, wrapThickness * 1.2f);
            drawWrappedLine(wrapCoreAlpha, GetColor(224, 244, 255), wrapThickness, wrapThickness * 0.72f);

            SetDrawBlendMode(DX_BLENDMODE_ADD, coreAlpha);
            DrawLineAA(renderDrawX, beamCenterY, renderDrawX + renderDrawWidth, beamCenterY, coreColor, innerGlowThickness * 0.52f);
            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round(255.0f * alphaMultiplier)), 0, 255));
            DrawLineAA(renderDrawX, beamCenterY, renderDrawX + renderDrawWidth, beamCenterY, GetColor(255, 255, 255), coreThickness);
            if (isBossBeam)
            {
                const int muzzleAlpha = std::clamp(static_cast<int>(std::round(132.0f * alphaMultiplier * muzzlePulse)), 0, 255);
                const int impactAlpha = std::clamp(static_cast<int>(std::round(150.0f * alphaMultiplier * impactPulse)), 0, 255);
                const float capRadius = std::max(3.0f, renderDrawHeight * 0.52f);
                SetDrawBlendMode(DX_BLENDMODE_ADD, muzzleAlpha);
                DrawCircleAA(
                    sourceX,
                    beamCenterY,
                    capRadius * 0.98f,
                    32,
                    GetColor(150, 224, 255),
                    TRUE,
                    std::max(1.6f, capRadius * 0.16f));
                DrawCircleAA(
                    sourceX,
                    beamCenterY,
                    capRadius * 0.42f,
                    32,
                    GetColor(255, 255, 255),
                    TRUE,
                    std::max(1.0f, capRadius * 0.10f));
                SetDrawBlendMode(DX_BLENDMODE_ADD, impactAlpha);
                DrawCircleAA(
                    targetX,
                    beamCenterY,
                    capRadius * 1.30f,
                    32,
                    GetColor(138, 214, 255),
                    TRUE,
                    std::max(1.8f, capRadius * 0.20f));
                DrawLineAA(
                    targetX - capRadius * 0.9f,
                    beamCenterY,
                    targetX + capRadius * 0.9f,
                    beamCenterY,
                    GetColor(255, 255, 255),
                    std::max(1.6f, capRadius * 0.18f));
            }
        }
        else
        {
            const int outerColor = GetColor(255, 86, 86);
            const int coreColor = GetColor(255, 224, 196);
            const int glowColor = GetColor(255, 48, 42);
            const int outerLeft = static_cast<int>(std::round(drawX));
            const int outerTop = static_cast<int>(std::round(drawY));
            const int outerRight = static_cast<int>(std::round(drawX + drawWidth));
            const int outerBottom = static_cast<int>(std::round(drawY + drawHeight));
            const bool verticalBeam = drawHeight > drawWidth;
            const float coreInset = (verticalBeam ? drawWidth : drawHeight) * 0.28f;
            const float shortSize = std::max(1.0f, verticalBeam ? drawWidth : drawHeight);
            const float broadGlow = std::max(6.0f, shortSize * 2.8f);
            const float tightGlow = std::max(3.0f, shortSize * 1.35f);
            const float broadPadX = verticalBeam ? broadGlow : tightGlow;
            const float broadPadY = verticalBeam ? tightGlow : broadGlow;
            const float tightPadX = verticalBeam ? tightGlow : shortSize * 0.55f;
            const float tightPadY = verticalBeam ? shortSize * 0.55f : tightGlow;
            const int coreLeft = static_cast<int>(std::round(drawX + (verticalBeam ? coreInset : 0.0f)));
            const int coreTop = static_cast<int>(std::round(drawY + (verticalBeam ? 0.0f : coreInset)));
            const int coreRight = static_cast<int>(std::round(drawX + drawWidth - (verticalBeam ? coreInset : 0.0f)));
            const int coreBottom = static_cast<int>(std::round(drawY + drawHeight - (verticalBeam ? 0.0f : coreInset)));

            SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(48.0f * alphaMultiplier)));
            DrawBoxAA(
                drawX - broadPadX,
                drawY - broadPadY,
                drawX + drawWidth + broadPadX,
                drawY + drawHeight + broadPadY,
                glowColor,
                TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(86.0f * alphaMultiplier)));
            DrawBoxAA(
                drawX - tightPadX,
                drawY - tightPadY,
                drawX + drawWidth + tightPadX,
                drawY + drawHeight + tightPadY,
                outerColor,
                TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(196.0f * alphaMultiplier)));
            DrawBox(outerLeft, outerTop, outerRight, outerBottom, outerColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(238.0f * alphaMultiplier)));
            DrawBox(coreLeft, coreTop, coreRight, coreBottom, coreColor, TRUE);
        }
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
    }

    if (const auto* cooldown = entity.GetComponent<DamageCooldownComponent>())
    {
        if (cooldown->GetRemainingSeconds() > 0.0f)
        {
            const bool isPlayer = tag && HasTag(tag, kTagPlayer);
            const bool isEnemy = enemyComponent != nullptr;
            const float cooldownSeconds = std::max(0.001f, cooldown->GetCooldownSeconds());
            const float flashT = Clamp01(cooldown->GetRemainingSeconds() / cooldownSeconds);
            if (isPlayer)
            {
                Shader_SetFlash(1.0f, 0.30f, 0.22f, 1.0f, flashT);
            }
            else if (isEnemy)
            {
                const float blinkPhase = flashT * 7.0f * 3.14159265f;
                const float blinkWave = 0.45f + 0.55f * std::sin(blinkPhase);
                alphaMultiplier *= std::lerp(0.20f, 1.0f, blinkWave);
                Shader_SetOutline(1.0f, 0.18f, 0.16f, 1.0f, 2.0f);
                Shader_SetFlash(1.0f, 0.08f, 0.08f, 1.0f, std::max(flashT, blinkWave * 0.75f));
            }
            else
            {
                Shader_SetFlash(1.0f, 0.88f, 0.28f, 1.0f, flashT);
            }
        }
    }

    if (tint)
    {
        Shader_SetTint(tint->r, tint->g, tint->b, tint->a * alphaMultiplier);
    }
    else
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, alphaMultiplier);
    }

    if (const auto* midBoss3Fist = entity.GetComponent<MidBoss3FistComponent>())
    {
        const bool meteorTrail = midBoss3Fist->state == MidBoss3FistState::MeteorFalling &&
            std::fabs(midBoss3Fist->velocityY) > 0.001f;
        if (meteorTrail)
        {
            const float directionY = meteorTrail ? (midBoss3Fist->velocityY >= 0.0f ? 1.0f : -1.0f) : 0.0f;
            const int lineColor = GetColor(255, 238, 202);

            if (meteorTrail)
            {
                for (int trailIndex = 3; trailIndex >= 1; --trailIndex)
                {
                    const float t = static_cast<float>(trailIndex);
                    const float alpha = 0.18f * (4.0f - t) / 3.0f;
                    const float offset = drawHeight * 0.42f * t;
                    const float blurW = drawWidth;
                    const float blurH = drawHeight * (1.0f + 0.16f * t);
                    const float blurX = drawX - (blurW - drawWidth) * 0.5f;
                    const float blurY = drawY - directionY * offset - (blurH - drawHeight) * 0.5f;

                    SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(static_cast<int>(std::round(alpha * 255.0f * alphaMultiplier)), 0, 255));
                    Shader_SetTint(1.0f, 0.66f, 0.28f, alpha);
                    SpriteDraw(
                        sprite->GetTextureId(),
                        blurX,
                        blurY,
                        blurW,
                        blurH,
                        sprite->GetSourceX(),
                        sprite->GetSourceY(),
                        sprite->GetSourceWidth(),
                        sprite->GetSourceHeight(),
                        sprite->GetFlipX(),
                        transform->rotation);
                }
            }

            if (meteorTrail)
            {
                SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(74.0f * alphaMultiplier)));
                const float tailY = directionY >= 0.0f ? drawY : drawY + drawHeight;
                const float lineLength = drawHeight * 1.2f;
                for (int lineIndex = 0; lineIndex < 6; ++lineIndex)
                {
                    const float ratio = (static_cast<float>(lineIndex) + 0.5f) / 6.0f;
                    const float x = drawX + drawWidth * std::lerp(0.10f, 0.90f, ratio);
                    const float jitter = std::sin(static_cast<float>(GetNowCount()) * 0.024f + static_cast<float>(lineIndex) * 1.31f) * drawWidth * 0.025f;
                    DrawLineAA(
                        x + jitter,
                        tailY - directionY * lineLength,
                        x + jitter * 0.35f,
                        tailY - directionY * drawHeight * 0.08f,
                        lineColor,
                        std::max(1.0f, 1.8f * viewScale));
                }
            }
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

            if (tint)
            {
                Shader_SetTint(tint->r, tint->g, tint->b, tint->a * alphaMultiplier);
            }
            else
            {
                Shader_SetTint(1.0f, 1.0f, 1.0f, alphaMultiplier);
            }
        }
    }

    if (!DrawDamagePlatformShape(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            damagePlatform,
            tint,
            sprite->GetSourceX(),
            sprite->GetSourceY(),
            sprite->GetSourceWidth(),
            sprite->GetSourceHeight(),
            transform->rotation,
            tint ? tint->a * alphaMultiplier : alphaMultiplier) &&
        !DrawSpikeStripShape(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            spikeStrip,
            tint,
            sprite->GetSourceX(),
            sprite->GetSourceY(),
            sprite->GetSourceWidth(),
            sprite->GetSourceHeight(),
            transform->rotation,
            tint ? tint->a * alphaMultiplier : alphaMultiplier) &&
        !DrawSlopeTriangle(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            photoCopyTile ? photoCopyTile->tileValue : 0,
            tint,
            sprite->GetFlipX(),
            transform->rotation,
            tint ? tint->a * alphaMultiplier : alphaMultiplier))
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
    if (midBoss2TeleportFlashActive)
    {
        const int glowAlpha = std::clamp(static_cast<int>(std::round(180.0f * midBoss2TeleportFlashT)), 0, 255);
        if (glowAlpha > 0)
        {
            const float glowPadX = std::max(8.0f, drawWidth * 0.14f);
            const float glowPadY = std::max(12.0f, drawHeight * 0.20f);
            SetDrawBlendMode(DX_BLENDMODE_ADD, glowAlpha);
            Shader_SetTint(1.0f, 1.0f, 1.0f, std::lerp(0.24f, 0.78f, midBoss2TeleportFlashT));
            SpriteDraw(
                m_whiteTexture,
                drawX - glowPadX,
                drawY - glowPadY,
                drawWidth + glowPadX * 2.0f,
                drawHeight + glowPadY * 2.0f,
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                false,
                0.0f);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            Shader_ResetStyle();
        }
    }

    if (const auto* midBoss3Fist = entity.GetComponent<MidBoss3FistComponent>())
    {
        const auto* ownerBoss = midBoss3Fist->ownerBoss ? midBoss3Fist->ownerBoss->GetComponent<MidBoss3Component>() : nullptr;
        const float fistWorldW = transform->width * transform->scale;
        const float fistWorldH = transform->height * transform->scale;
        const float fistCenterWorldX = transform->x + fistWorldW * 0.5f;
        const float fistCenterWorldY = transform->y + fistWorldH * 0.5f;
        bool drawPrediction = false;
        float predictionStartWorldX = fistCenterWorldX;
        float predictionStartWorldY = fistCenterWorldY;
        float predictionDirX = 0.0f;
        float predictionDirY = 0.0f;
        if (midBoss3Fist->state == MidBoss3FistState::LauncherReady && midBoss3Fist->atAttackStart)
        {
            const float direction = ownerBoss && ownerBoss->launcherDirection < 0 ? -1.0f : 1.0f;
            predictionDirX = direction;
            predictionDirY = 0.0f;
            predictionStartWorldX = direction >= 0.0f ? transform->x + fistWorldW : transform->x;
            predictionStartWorldY = fistCenterWorldY;
            drawPrediction = true;
        }
        else if (midBoss3Fist->state == MidBoss3FistState::MeteorReady && midBoss3Fist->atAttackStart)
        {
            predictionDirX = 0.0f;
            predictionDirY = 1.0f;
            predictionStartWorldX = fistCenterWorldX;
            predictionStartWorldY = transform->y + fistWorldH;
            drawPrediction = true;
        }
        if (drawPrediction)
        {
            DrawAttackPredictionLine(
                viewOriginX + (predictionStartWorldX - m_flow.cameraX) * viewScale,
                viewOriginY + (predictionStartWorldY - m_flow.cameraY) * viewScale,
                predictionDirX,
                predictionDirY,
                std::max(480.0f, 900.0f * viewScale),
                viewScale,
                static_cast<float>(midBoss3Fist->fistIndex) * 1.37f);
        }
        if (midBoss3Fist->captureJammerActive)
        {
            constexpr float kTileSize = 48.0f;
            const float jammerSize = kTileSize * 3.0f;
            const float fistWidth = transform->width * transform->scale;
            const float fistHeight = transform->height * transform->scale;
            const float jammerWorldX = transform->x + fistWidth * 0.5f - jammerSize * 0.5f;
            const float jammerWorldY = transform->y + fistHeight * 0.5f - jammerSize * 0.5f;
            const float screenX = GetViewOriginX() + (jammerWorldX - m_flow.cameraX) * viewScale;
            const float screenY = GetViewOriginY() + (jammerWorldY - m_flow.cameraY) * viewScale;
            const float screenSize = jammerSize * viewScale;
            const float cellSize = kTileSize * viewScale;

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 96);
            DrawBoxAA(screenX, screenY, screenX + screenSize, screenY + screenSize, GetColor(90, 225, 238), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 144);
            DrawBoxAA(screenX, screenY, screenX + screenSize, screenY + screenSize, GetColor(42, 180, 212), FALSE);
            DrawLineAA(screenX, screenY + cellSize, screenX + screenSize, screenY + cellSize, GetColor(60, 190, 210), 1.5f);
            DrawLineAA(screenX, screenY + cellSize * 2.0f, screenX + screenSize, screenY + cellSize * 2.0f, GetColor(60, 190, 210), 1.5f);
            DrawLineAA(screenX + cellSize, screenY, screenX + cellSize, screenY + screenSize, GetColor(60, 190, 210), 1.5f);
            DrawLineAA(screenX + cellSize * 2.0f, screenY, screenX + cellSize * 2.0f, screenY + screenSize, GetColor(60, 190, 210), 1.5f);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
        if (midBoss3Fist->impactAttackActive)
        {
            const float screenX = GetViewOriginX() + (midBoss3Fist->impactAttackX - m_flow.cameraX) * viewScale;
            const float screenY = GetViewOriginY() + (midBoss3Fist->impactAttackY - m_flow.cameraY) * viewScale;
            const float screenW = midBoss3Fist->impactAttackWidth * viewScale;
            const float screenH = midBoss3Fist->impactAttackHeight * viewScale;
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 96);
            DrawBoxAA(screenX, screenY, screenX + screenW, screenY + screenH, GetColor(255, 96, 48), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    if (const auto* midBoss3 = entity.GetComponent<MidBoss3Component>())
    {
        if (midBoss3->drillActive)
        {
            const float screenX = GetViewOriginX() + (midBoss3->drillX - m_flow.cameraX) * viewScale;
            const float screenY = GetViewOriginY() + (midBoss3->drillY - m_flow.cameraY) * viewScale;
            const float screenW = midBoss3->drillWidth * viewScale;
            const float screenH = midBoss3->drillHeight * viewScale;
            const float drillAngle = midBoss3->drillGroundRush
                ? (midBoss3->drillDirection >= 0 ? 0.0f : 3.14159265f)
                : std::atan2(midBoss3->drillAimY, midBoss3->drillAimX);
            if (midBoss3->state == MidBoss3State::DrillFist &&
                midBoss3->drillActive &&
                std::fabs(midBoss3->drillVelocityX) < 0.001f &&
                std::fabs(midBoss3->drillVelocityY) < 0.001f)
            {
                const float drillCenterX = screenX + screenW * 0.5f;
                const float drillCenterY = screenY + screenH * 0.5f;
                DrawAttackPredictionLine(
                    drillCenterX + std::cos(drillAngle) * screenW * 0.5f,
                    drillCenterY + std::sin(drillAngle) * screenW * 0.5f,
                    std::cos(drillAngle),
                    std::sin(drillAngle),
                    std::max(560.0f, 1040.0f * viewScale),
                    viewScale,
                    4.8f);
            }
            DrawMidBoss3DrillShape(
                screenX,
                screenY,
                screenW,
                screenH,
                drillAngle,
                midBoss3->drillDirection,
                midBoss3->stateTimer,
                viewScale,
                alphaMultiplier);
        }
    }

    if (const auto* enemy = enemyComponent)
    {
        const auto* boss = entity.GetComponent<MidBoss2Component>();
        if (boss &&
            enemy->GetArchetype() == EnemyArchetype::MidBoss2 &&
            boss->state == MidBoss2State::BeamCharge &&
            boss->params.beamChargeTime > 0.0f)
        {
            const float chargeT = Clamp01(boss->stateTimer / boss->params.beamChargeTime);
            const float bossWidth = transform->width * transform->scale;
            const float bossHeight = transform->height * transform->scale;
            const float tileSize = boss->params.boss2WidthGrid > 0
                ? bossWidth / static_cast<float>(boss->params.boss2WidthGrid)
                : 48.0f;
            const float effectCenterWorldX = transform->x + (boss->beamFacingRight ? bossWidth + tileSize * 0.8f : -tileSize * 0.8f);
            const float effectCenterWorldY = transform->y + bossHeight * 0.5f;
            const float effectCenterX = viewOriginX + (effectCenterWorldX - m_flow.cameraX) * viewScale;
            const float effectCenterY = viewOriginY + (effectCenterWorldY - m_flow.cameraY) * viewScale;
            const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
            const float pulse = 0.92f + 0.08f * std::sin(timeSeconds * 10.0f);
            const float outerRadius = std::max(9.0f, tileSize * viewScale * std::lerp(0.42f, 0.20f, chargeT) * pulse);
            const float innerRadius = std::max(3.0f, outerRadius * 0.40f);
            const float haloRadius = outerRadius * std::lerp(3.2f, 1.8f, chargeT);
            const float streamRadius = tileSize * viewScale * std::lerp(2.2f, 0.75f, chargeT);
            const float beamDirectionX = boss->beamFacingRight ? 1.0f : -1.0f;
            const float flashT = Clamp01((chargeT - 0.84f) / 0.16f);
            const float flashBoost = 1.0f + flashT * flashT * 1.8f;
            const int haloAlpha = std::clamp(static_cast<int>(std::round(std::lerp(28.0f, 108.0f, chargeT) * flashBoost)), 0, 255);
            const int outerAlpha = std::clamp(static_cast<int>(std::round(std::lerp(56.0f, 180.0f, chargeT) * flashBoost)), 0, 255);
            const int coreAlpha = std::clamp(static_cast<int>(std::round(std::lerp(120.0f, 255.0f, chargeT) * flashBoost)), 0, 255);
            constexpr int kSparkCount = 24;

            SetDrawBlendMode(DX_BLENDMODE_ADD, haloAlpha);
            DrawCircle(
                static_cast<int>(std::round(effectCenterX)),
                static_cast<int>(std::round(effectCenterY)),
                static_cast<int>(std::round(haloRadius)),
                GetColor(82, 168, 255),
                TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ADD, outerAlpha);
            DrawCircle(
                static_cast<int>(std::round(effectCenterX)),
                static_cast<int>(std::round(effectCenterY)),
                static_cast<int>(std::round(outerRadius * (1.0f + flashT * 0.25f))),
                GetColor(168, 220, 255),
                TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ADD, coreAlpha);
            DrawCircle(
                static_cast<int>(std::round(effectCenterX)),
                static_cast<int>(std::round(effectCenterY)),
                static_cast<int>(std::round(innerRadius * (1.0f + flashT * 0.55f))),
                GetColor(255, 255, 255),
                TRUE);

            for (int index = 0; index < kSparkCount; ++index)
            {
                const float seed = static_cast<float>(index) / static_cast<float>(kSparkCount);
                const float cycle = std::fmod(timeSeconds * (1.3f + flashT * 0.7f) + seed * 1.7f, 1.0f);
                const float travelT = std::pow(cycle, 1.35f);
                const float randomA = seed * 6.28318530718f + std::sin(seed * 34.0f) * 0.7f;
                const float randomR = streamRadius * (0.62f + 0.38f * std::fmod(seed * 13.0f, 1.0f));
                const float streamEndX = effectCenterX + beamDirectionX * randomR * (1.06f + flashT * 0.34f);
                const float streamEndY = effectCenterY + std::sin(randomA) * streamRadius * 0.44f;
                const float sparkX = std::lerp(effectCenterX, streamEndX, travelT);
                const float sparkY = std::lerp(effectCenterY, streamEndY, travelT);
                const float tailT = std::max(0.0f, travelT - 0.16f - flashT * 0.08f);
                const float tailX = std::lerp(effectCenterX, streamEndX, tailT);
                const float tailY = std::lerp(effectCenterY, streamEndY, tailT);
                const float sparkRadius = std::max(2.0f, tileSize * viewScale * std::lerp(0.10f, 0.028f, travelT) * (1.0f + flashT * 0.35f));
                const int sparkAlpha = std::clamp(static_cast<int>(std::round(std::lerp(232.0f, 72.0f, travelT) * (0.75f + 0.55f * chargeT))), 0, 255);
                const int tailAlpha = std::clamp(static_cast<int>(std::round(static_cast<float>(sparkAlpha) * (0.30f + flashT * 0.24f))), 0, 255);
                SetDrawBlendMode(DX_BLENDMODE_ADD, tailAlpha);
                DrawLine(
                    static_cast<int>(std::round(tailX)),
                    static_cast<int>(std::round(tailY)),
                    static_cast<int>(std::round(sparkX)),
                    static_cast<int>(std::round(sparkY)),
                    GetColor(170, 220, 255),
                    2);
                SetDrawBlendMode(DX_BLENDMODE_ADD, sparkAlpha);
                DrawCircle(
                    static_cast<int>(std::round(sparkX)),
                    static_cast<int>(std::round(sparkY)),
                    static_cast<int>(std::round(sparkRadius)),
                    GetColor(240, 252, 255),
                    TRUE);
            }

            const float flashCross = tileSize * viewScale * std::lerp(0.28f, 1.05f, flashT);
            const int flashAlpha = std::clamp(static_cast<int>(std::round(255.0f * flashT)), 0, 255);
            if (flashAlpha > 0)
            {
                SetDrawBlendMode(DX_BLENDMODE_ADD, flashAlpha);
                DrawLine(
                    static_cast<int>(std::round(effectCenterX)),
                    static_cast<int>(std::round(effectCenterY)),
                    static_cast<int>(std::round(effectCenterX + beamDirectionX * flashCross)),
                    static_cast<int>(std::round(effectCenterY)),
                    GetColor(255, 255, 255),
                    4);
                DrawLine(
                    static_cast<int>(std::round(effectCenterX)),
                    static_cast<int>(std::round(effectCenterY - flashCross)),
                    static_cast<int>(std::round(effectCenterX)),
                    static_cast<int>(std::round(effectCenterY + flashCross)),
                    GetColor(220, 242, 255),
                    3);
            }

            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            Shader_ResetStyle();
        }
    }

    if (enemyComponent &&
        enemyComponent->GetArchetype() == EnemyArchetype::MidBoss2 &&
        midBoss2)
    {
        if (midBoss2->state == MidBoss2State::BeamCharge)
        {
            const float bossWidth = transform->width * transform->scale;
            const float bossHeight = transform->height * transform->scale;
            const float bodyCenterX = viewOriginX + ((transform->x + bossWidth * 0.5f) - m_flow.cameraX) * viewScale;
            const float bodyCenterY = viewOriginY + ((transform->y + bossHeight * 0.5f) - m_flow.cameraY) * viewScale;
            const float beamCenterX = viewOriginX + ((transform->x + (midBoss2->beamFacingRight ? bossWidth : 0.0f)) - m_flow.cameraX) * viewScale;
            const float beamCenterY = viewOriginY + ((transform->y + bossHeight * 0.5f) - m_flow.cameraY) * viewScale;
            const float chargeT = Clamp01(midBoss2->stateTimer / midBoss2->params.beamChargeTime);
            const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
            const float pulse = 0.92f + 0.08f * std::sin(static_cast<float>(GetNowCount()) * 0.01f + chargeT * 4.0f);
            const float bodyRingRadius = std::max(12.0f, std::min(bossWidth, bossHeight) * viewScale * std::lerp(0.52f, 0.74f, chargeT));
            const float beamRingRadius = std::max(10.0f, std::min(bossWidth, bossHeight) * viewScale * std::lerp(0.30f, 0.58f, chargeT));
            const float bodyRingThickness = std::max(2.0f, viewScale * std::lerp(2.4f, 4.8f, chargeT));
            const float beamRingThickness = std::max(1.5f, viewScale * std::lerp(1.8f, 3.5f, chargeT));
            const float swirlSpin = timeSeconds * std::lerp(2.8f, 5.4f, chargeT);
            const float beamDirectionX = midBoss2->beamFacingRight ? 1.0f : -1.0f;

            SetDrawBlendMode(DX_BLENDMODE_ADD, std::clamp(static_cast<int>(std::round((64.0f + chargeT * 92.0f) * alphaMultiplier * pulse)), 0, 255));
            DrawLineAA(
                bodyCenterX,
                bodyCenterY,
                beamCenterX,
                beamCenterY,
                GetColor(130, 208, 255),
                std::max(1.8f, viewScale * std::lerp(2.0f, 3.6f, chargeT)));
            DrawCircleAA(
                bodyCenterX,
                bodyCenterY,
                bodyRingRadius,
                64,
                GetColor(150, 228, 255),
                FALSE,
                bodyRingThickness);
            DrawCircleAA(
                beamCenterX,
                beamCenterY,
                beamRingRadius,
                64,
                GetColor(255, 255, 255),
                FALSE,
                beamRingThickness);
            DrawLineAA(
                beamCenterX,
                beamCenterY,
                beamCenterX + beamDirectionX * beamRingRadius * 0.92f,
                beamCenterY,
                GetColor(176, 232, 255),
                std::max(1.5f, beamRingThickness * 0.36f));
            DrawLineAA(
                beamCenterX,
                beamCenterY - beamRingRadius * 0.7f,
                beamCenterX,
                beamCenterY + beamRingRadius * 0.7f,
                GetColor(176, 232, 255),
                std::max(1.5f, beamRingThickness * 0.32f));
            DrawLineAA(
                beamCenterX,
                beamCenterY,
                beamCenterX + beamDirectionX * beamRingRadius * 0.68f,
                beamCenterY + beamRingRadius * 0.44f,
                GetColor(140, 224, 255),
                std::max(1.4f, beamRingThickness * 0.22f));
            DrawLineAA(
                beamCenterX,
                beamCenterY,
                beamCenterX + beamDirectionX * beamRingRadius * 0.68f,
                beamCenterY - beamRingRadius * 0.44f,
                GetColor(140, 224, 255),
                std::max(1.4f, beamRingThickness * 0.22f));
            const int shardCount = 6;
            for (int shardIndex = 0; shardIndex < shardCount; ++shardIndex)
            {
                const float angle = swirlSpin + static_cast<float>(shardIndex) * 1.0471976f;
                const float shardDistance = beamRingRadius * std::lerp(0.76f, 1.14f, 1.0f - chargeT);
                const float shardX = beamCenterX + beamDirectionX * std::fabs(std::cos(angle)) * shardDistance;
                const float shardY = beamCenterY + std::sin(angle) * shardDistance * 0.74f;
                DrawLineAA(
                    beamCenterX,
                    beamCenterY,
                    shardX,
                    shardY,
                    GetColor(184, 236, 255),
                    std::max(1.2f, beamRingThickness * 0.16f));
                DrawCircleAA(
                    shardX,
                    shardY,
                    std::max(1.0f, beamRingThickness * 0.18f),
                    24,
                    GetColor(255, 255, 255),
                    TRUE,
                    std::max(1.0f, beamRingThickness * 0.12f));
            }
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            Shader_ResetStyle();
        }
    }

    if (tag &&
        HasTag(tag, kTagLaserTurret) &&
        capturedBoss2BeamCharge &&
        capturedBoss2BeamCharge->chargeDuration > 0.0f)
    {
        if (const auto* turret = entity.GetComponent<LaserTurretComponent>())
        {
            if (turret->warmupRemaining > 0.0f)
            {
                const float chargeT = Clamp01(1.0f - (turret->warmupRemaining / capturedBoss2BeamCharge->chargeDuration));
                const float tileSize = 48.0f;
                const float beamCenterX = viewOriginX + ((transform->x + turret->beamOriginOffsetX) - m_flow.cameraX) * viewScale;
                const float beamCenterY = viewOriginY + ((transform->y + turret->beamOriginOffsetY) - m_flow.cameraY) * viewScale;
                const float timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
                const float pulse = 0.92f + 0.08f * std::sin(static_cast<float>(GetNowCount()) * 0.01f + chargeT * 4.0f);
                const float ringRadius = std::max(10.0f, tileSize * viewScale * std::lerp(0.72f, 1.24f, chargeT));
                const float ringThickness = std::max(1.5f, viewScale * std::lerp(1.8f, 3.5f, chargeT));
                const float swirlSpin = timeSeconds * std::lerp(2.8f, 5.4f, chargeT);
                const float beamDirectionX = turret->fireDirection == LaserTurretFireDirection::Left ? -1.0f : 1.0f;
                const float flashT = Clamp01((chargeT - 0.84f) / 0.16f);
                const float flashBoost = 1.0f + flashT * flashT * 1.8f;
                const int ringAlpha = std::clamp(
                    static_cast<int>(std::round((72.0f + chargeT * 104.0f) * alphaMultiplier * pulse * flashBoost)),
                    0,
                    255);

                SetDrawBlendMode(DX_BLENDMODE_ADD, ringAlpha);
                DrawCircleAA(
                    beamCenterX,
                    beamCenterY,
                    ringRadius,
                    64,
                    GetColor(255, 255, 255),
                    FALSE,
                    ringThickness);
                DrawLineAA(
                    beamCenterX,
                    beamCenterY,
                    beamCenterX + beamDirectionX * ringRadius * 0.92f,
                    beamCenterY,
                    GetColor(176, 232, 255),
                    std::max(1.5f, ringThickness * 0.36f));
                DrawLineAA(
                    beamCenterX,
                    beamCenterY - ringRadius * 0.7f,
                    beamCenterX,
                    beamCenterY + ringRadius * 0.7f,
                    GetColor(176, 232, 255),
                    std::max(1.5f, ringThickness * 0.32f));
                DrawLineAA(
                    beamCenterX,
                    beamCenterY,
                    beamCenterX + beamDirectionX * ringRadius * 0.68f,
                    beamCenterY + ringRadius * 0.44f,
                    GetColor(140, 224, 255),
                    std::max(1.4f, ringThickness * 0.22f));
                DrawLineAA(
                    beamCenterX,
                    beamCenterY,
                    beamCenterX + beamDirectionX * ringRadius * 0.68f,
                    beamCenterY - ringRadius * 0.44f,
                    GetColor(140, 224, 255),
                    std::max(1.4f, ringThickness * 0.22f));

                constexpr int kShardCount = 6;
                for (int shardIndex = 0; shardIndex < kShardCount; ++shardIndex)
                {
                    const float angle = swirlSpin + static_cast<float>(shardIndex) * 1.0471976f;
                    const float shardDistance = ringRadius * std::lerp(0.76f, 1.14f, 1.0f - chargeT);
                    const float shardX = beamCenterX + beamDirectionX * std::fabs(std::cos(angle)) * shardDistance;
                    const float shardY = beamCenterY + std::sin(angle) * shardDistance * 0.74f;
                    DrawLineAA(
                        beamCenterX,
                        beamCenterY,
                        shardX,
                        shardY,
                        GetColor(184, 236, 255),
                        std::max(1.2f, ringThickness * 0.16f));
                    DrawCircleAA(
                        shardX,
                        shardY,
                        std::max(1.0f, ringThickness * 0.18f),
                        24,
                        GetColor(255, 255, 255),
                        TRUE,
                        std::max(1.0f, ringThickness * 0.12f));
                }

                if (flashT > 0.0f)
                {
                    const float flashCross = tileSize * viewScale * std::lerp(0.28f, 1.05f, flashT);
                    const int flashAlpha = std::clamp(
                        static_cast<int>(std::round(255.0f * flashT * alphaMultiplier)),
                        0,
                        255);
                    SetDrawBlendMode(DX_BLENDMODE_ADD, flashAlpha);
                    DrawLineAA(
                        beamCenterX,
                        beamCenterY,
                        beamCenterX + beamDirectionX * flashCross,
                        beamCenterY,
                        GetColor(255, 255, 255),
                        std::max(2.0f, 4.0f * viewScale));
                    DrawLineAA(
                        beamCenterX,
                        beamCenterY - flashCross,
                        beamCenterX,
                        beamCenterY + flashCross,
                        GetColor(220, 242, 255),
                        std::max(1.5f, 3.0f * viewScale));
                }

                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                Shader_ResetStyle();
            }
        }
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

    const bool shouldDrawCollisionDebug = m_debug.showCollisionDebug && (
        photoFilter ||
        enemyComponent ||
        midBoss2Spear ||
        (tag && (HasTag(tag, kTagPlayer) || HasTag(tag, kTagPhotoSource) || HasTag(tag, kTagPhotoBox) || HasTag(tag, kTagLaserBeam))));
    if (shouldDrawCollisionDebug)
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
        else if (tag && HasTag(tag, kTagLaserBeam))
        {
            color = GetColor(255, 72, 72);
        }
        else if (midBoss2Spear)
        {
            color = GetColor(255, 220, 120);
        }
        else if (const auto* enemy = enemyComponent)
        {
            color = enemy->GetArchetype() == EnemyArchetype::MidBoss2
                ? GetColor(255, 120, 255)
                : GetColor(120, 220, 255);
        }
        else if (photoFilter)
        {
            color = GetColor(96, 255, 220);
        }

        DrawWorldRectOutline(*this,
            transform->x,
            transform->y,
            transform->width * transform->scale,
            transform->height * transform->scale,
            m_flow.cameraX,
            m_flow.cameraY,
            color);

        if (const auto* imageCollider = entity.GetComponent<ImageOutlineColliderComponent>())
        {
            DrawWorldPolygonOutline(*this, *transform, *imageCollider, m_flow.cameraX, m_flow.cameraY, color);
        }

        if (const auto* spear = midBoss2Spear)
        {
            const float viewScaleLocal = GetViewScale();
            const float viewOriginXLocal = GetViewOriginX();
            const float viewOriginYLocal = GetViewOriginY();
            const float centerX = transform->x + transform->width * transform->scale * 0.5f;
            const float centerY = transform->y + transform->height * transform->scale * 0.5f;
            const float tipX = centerX + spear->directionX * 48.0f;
            const float tipY = centerY + spear->directionY * 48.0f;
            DrawLine(
                static_cast<int>(std::round(viewOriginXLocal + (centerX - m_flow.cameraX) * viewScaleLocal)),
                static_cast<int>(std::round(viewOriginYLocal + (centerY - m_flow.cameraY) * viewScaleLocal)),
                static_cast<int>(std::round(viewOriginXLocal + (tipX - m_flow.cameraX) * viewScaleLocal)),
                static_cast<int>(std::round(viewOriginYLocal + (tipY - m_flow.cameraY) * viewScaleLocal)),
                color,
                2);
        }
    }

        Shader_ResetStyle();
}

void GameScene::DrawEnemyAttackRects() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!entity) continue;

        // Walker
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

        // ・ｽ・ｽ・ｽ{・ｽX・ｽU・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
        const auto* boss = entity->GetComponent<ShieldBossComponent>();
        if (boss && boss->attackRectActive)
        {
            const float screenX = viewOriginX + (boss->attackRectX - m_flow.cameraX) * viewScale;
            const float screenY = viewOriginY + (boss->attackRectY - m_flow.cameraY) * viewScale;
            const float screenW = boss->attackRectWidth * viewScale;
            const float screenH = boss->attackRectHeight * viewScale;

            const unsigned int color =
                boss->state == ShieldBossState::Rush
                ? GetColor(255, 80, 80)    // ・ｽﾋ進・ｽﾍオ・ｽ・ｽ・ｽ・ｽ・ｽW
                : boss->state == ShieldBossState::SlamPhase1
                ? GetColor(255, 140, 0)    // ・ｽ・ｽ・ｽ・ｽ@・ｽﾍオ・ｽ・ｽ・ｽ・ｽ・ｽW
                : GetColor(180, 0, 255);   // ・ｽ・ｽ・ｽ・ｽA・ｽﾍ趣ｿｽ

            DrawBoxAA(screenX, screenY, screenX + screenW, screenY + screenH,
                color, TRUE);
        }
    }
}

