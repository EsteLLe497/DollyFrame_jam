#include "pch.h"

#include "photo_geometry.h"

#include <cfloat>

#include <algorithm>
#include <cmath>

#include "game_scene_internal.h"

using namespace game_scene_detail;

namespace photo_geometry
{
    float GetPrintedPhotoWidth(float contentWidth)
    {
        return std::max(gPrintedPhotoMinWidth, contentWidth + gPrintedPhotoPaddingX * 2.0f);
    }

    float GetPrintedPhotoHeight(float contentHeight)
    {
        return std::max(gPrintedPhotoMinHeight, contentHeight + gPrintedPhotoPaddingTop + gPrintedPhotoFooterHeight);
    }

    float GetRotatedBoundsWidth(float width, float height, float rotation)
    {
        const float cosTheta = std::fabs(std::cos(rotation));
        const float sinTheta = std::fabs(std::sin(rotation));
        return width * cosTheta + height * sinTheta;
    }

    float GetRotatedBoundsHeight(float width, float height, float rotation)
    {
        const float cosTheta = std::fabs(std::cos(rotation));
        const float sinTheta = std::fabs(std::sin(rotation));
        return width * sinTheta + height * cosTheta;
    }

    namespace
    {
        template <typename IsInsideFn, typename IntersectFn>
        void ClipPolygonAgainstEdge(
            const std::vector<DamagePlatformPoint>& input,
            std::vector<DamagePlatformPoint>& output,
            IsInsideFn&& isInside,
            IntersectFn&& intersect)
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
            ClipPolygonAgainstEdge(
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
            ClipPolygonAgainstEdge(
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

    void RotatePrintedPhotoItems(std::vector<CapturedPhotoItem>& items, float& width, float& height, float rotation)
    {
        if (std::fabs(rotation) <= 0.0001f)
        {
            return;
        }

        const float baseWidth = width;
        const float baseHeight = height;
        const float centerX = baseWidth * 0.5f;
        const float centerY = baseHeight * 0.5f;
        const float cosTheta = std::cos(rotation);
        const float sinTheta = std::sin(rotation);
        float minX = FLT_MAX;
        float minY = FLT_MAX;
        float maxX = -FLT_MAX;
        float maxY = -FLT_MAX;

        for (auto& item : items)
        {
            const float itemCenterX = item.relativeX + item.width * 0.5f;
            const float itemCenterY = item.relativeY + item.height * 0.5f;
            const float localX = itemCenterX - centerX;
            const float localY = itemCenterY - centerY;
            const float rotatedCenterX = centerX + (localX * cosTheta - localY * sinTheta);
            const float rotatedCenterY = centerY + (localX * sinTheta + localY * cosTheta);

            item.relativeX = rotatedCenterX - item.width * 0.5f;
            item.relativeY = rotatedCenterY - item.height * 0.5f;
            item.rotation += rotation;
            const float rotatedVelocityX = item.projectileVelocityX * cosTheta - item.projectileVelocityY * sinTheta;
            const float rotatedVelocityY = item.projectileVelocityX * sinTheta + item.projectileVelocityY * cosTheta;
            item.projectileVelocityX = rotatedVelocityX;
            item.projectileVelocityY = rotatedVelocityY;
            const float rotatedSpearDirectionX = item.spearDirectionX * cosTheta - item.spearDirectionY * sinTheta;
            const float rotatedSpearDirectionY = item.spearDirectionX * sinTheta + item.spearDirectionY * cosTheta;
            item.spearDirectionX = rotatedSpearDirectionX;
            item.spearDirectionY = rotatedSpearDirectionY;

            minX = (std::min)(minX, item.relativeX);
            minY = (std::min)(minY, item.relativeY);
            maxX = (std::max)(maxX, item.relativeX + item.width);
            maxY = (std::max)(maxY, item.relativeY + item.height);
        }

        for (auto& item : items)
        {
            item.relativeX -= minX;
            item.relativeY -= minY;
        }

        width = maxX - minX;
        height = maxY - minY;
    }
}
