#include "pch.h"

#include "photo_shared.h"

#include "game_scene_internal.h"
#include "game_scene_draw_helpers.h"
#include "photo_geometry.h"
#include "photo_filter_rules.h"
#include "DxLib.h"

using namespace game_scene_detail;
using namespace photo_geometry;

namespace
{
void DrawQuadItem(
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

void DrawDamagePlatformItem(
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    int tileSpan,
    float sourceX,
    float sourceY,
    float sourceWidth,
    float sourceHeight,
    float rotation,
    int baseColor,
    int spikeColor,
    float alpha)
{
    const int clampedAlpha = std::clamp(static_cast<int>(std::round(alpha * 255.0f)), 0, 255);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, clampedAlpha);

    const float cropLeft = std::clamp(sourceX, 0.0f, 1.0f);
    const float cropTop = std::clamp(sourceY, 0.0f, 1.0f);
    const float cropWidth = std::clamp(sourceWidth, 0.0001f, 1.0f);
    const float cropHeight = std::clamp(sourceHeight, 0.0001f, 1.0f);
    const float cropRight = std::clamp(cropLeft + cropWidth, 0.0f, 1.0f);
    const float cropBottom = std::clamp(cropTop + cropHeight, 0.0f, 1.0f);

    std::vector<DamagePlatformPoint> basePolygon =
    {
        { 0.0f, 0.5f },
        { 1.0f, 0.5f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };
    if (ClipDamagePlatformPolygonToCrop(basePolygon, cropLeft, cropTop, cropRight, cropBottom))
    {
        DrawDamagePlatformPolygon(
            basePolygon,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            cropLeft,
            cropTop,
            cropRight - cropLeft,
            cropBottom - cropTop,
            rotation,
            baseColor);
    }

    const int spikeCount = (std::max)(1, tileSpan);
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

        DrawDamagePlatformPolygon(
            spikePolygon,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            cropLeft,
            cropTop,
            cropRight - cropLeft,
            cropBottom - cropTop,
            rotation,
            spikeColor);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void DrawSpikeStripItem(
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    int tileSpan,
    float sourceX,
    float sourceY,
    float sourceWidth,
    float sourceHeight,
    float rotation,
    int spikeColor,
    float alpha)
{
    const int clampedAlpha = std::clamp(static_cast<int>(std::round(alpha * 255.0f)), 0, 255);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, clampedAlpha);

    const float cropLeft = std::clamp(sourceX, 0.0f, 1.0f);
    const float cropTop = std::clamp(sourceY, 0.0f, 1.0f);
    const float cropWidth = std::clamp(sourceWidth, 0.0001f, 1.0f);
    const float cropHeight = std::clamp(sourceHeight, 0.0001f, 1.0f);
    const float cropRight = std::clamp(cropLeft + cropWidth, 0.0f, 1.0f);
    const float cropBottom = std::clamp(cropTop + cropHeight, 0.0f, 1.0f);

    const int spikeCount = (std::max)(1, tileSpan);
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

        DrawDamagePlatformPolygon(
            spikePolygon,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            cropLeft,
            cropTop,
            cropRight - cropLeft,
            cropBottom - cropTop,
            rotation,
            spikeColor);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void DrawTexturedPhotoItemPreview(
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    int tileSpan,
    float alpha)
{
    if (item.textureId < 0)
    {
        return;
    }

    Shader_ResetStyle();
    Shader_SetTint(1.0f, 1.0f, 1.0f, std::clamp(item.tintA, 0.0f, 1.0f) * alpha);

    const int blockCount = (std::max)(1, tileSpan);
    const float blockWidth = drawWidth / static_cast<float>(blockCount);
    for (int blockIndex = 0; blockIndex < blockCount; ++blockIndex)
    {
        SpriteDraw(
            item.textureId,
            drawX + blockWidth * static_cast<float>(blockIndex),
            drawY,
            blockWidth,
            drawHeight,
            item.sourceX,
            item.sourceY,
            item.sourceWidth,
            item.sourceHeight,
            item.flipX,
            item.rotation);
    }
}

std::vector<CapturedPhotoItem> BuildPrintedPhotoItems(
    const std::vector<CapturedPhotoItem>& sourceItems,
    int paperTextureId,
    PhotoFilterTheme capturedTheme,
    float contentWidth,
    float contentHeight,
    bool flipX,
    bool bridgeEnabled,
    PhotoPlacementRuleGroup paperRuleGroup = PhotoPlacementRuleGroup::Group1)
{
    const float printedWidth = GetPrintedPhotoWidth(contentWidth);
    std::vector<CapturedPhotoItem> printedItems = sourceItems;
    for (auto& item : printedItems)
    {
        item.relativeX += gPrintedPhotoPaddingX;
        item.relativeY += gPrintedPhotoPaddingTop;
    }

    if (bridgeEnabled && printedItems.size() >= 2)
    {
        const std::vector<CapturedPhotoItem> baseItems = printedItems;
        constexpr float kSegmentSize = 18.0f;
        for (size_t index = 1; index < baseItems.size(); ++index)
        {
            const auto& a = baseItems[index - 1];
            const auto& b = baseItems[index];
            const float ax = a.relativeX + a.width * 0.5f;
            const float ay = a.relativeY + a.height * 0.5f;
            const float bx = b.relativeX + b.width * 0.5f;
            const float by = b.relativeY + b.height * 0.5f;
            const float length = std::max(std::fabs(bx - ax), std::fabs(by - ay));
            const int steps = std::max(1, static_cast<int>(length / kSegmentSize));
            for (int step = 1; step < steps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                CapturedPhotoItem bridge;
                bridge.textureId = paperTextureId;
                bridge.role = PhotoCopyRole::Solid;
                bridge.layer = PhotoCopyLayer::Foreground;
                bridge.appliedTheme = capturedTheme;
                bridge.placementRuleGroup = PhotoPlacementRuleGroup::Group1;
                bridge.relativeX = std::lerp(ax, bx, t) - kSegmentSize * 0.5f;
                bridge.relativeY = std::lerp(ay, by, t) - kSegmentSize * 0.5f;
                bridge.width = kSegmentSize;
                bridge.height = kSegmentSize;
                bridge.sourceX = 0.0f;
                bridge.sourceY = 0.0f;
                bridge.sourceWidth = 1.0f;
                bridge.sourceHeight = 1.0f;
                bridge.tintR = 0.90f;
                bridge.tintG = 0.96f;
                bridge.tintB = 1.0f;
                bridge.tintA = 0.92f;
                bridge.flipX = flipX;
                printedItems.push_back(bridge);
            }
        }
    }

    if (flipX)
    {
        for (auto& item : printedItems)
        {
            item.relativeX = printedWidth - item.relativeX - item.width;
            item.flipX = !item.flipX;
            item.projectileVelocityX = -item.projectileVelocityX;
            for (auto& point : item.collisionOutline)
            {
                point.x = 1.0f - point.x;
            }
        }
    }

    CapturedPhotoItem paper;
    paper.textureId = paperTextureId;
    paper.role = PhotoCopyRole::Solid;
    paper.layer = PhotoCopyLayer::Background;
    paper.origin = PhotoCopyOrigin::Generic;
    paper.appliedTheme = PhotoFilterTheme::None;
    paper.placementRuleGroup = paperRuleGroup;
    paper.relativeX = 0.0f;
    paper.relativeY = 0.0f;
    paper.width = printedWidth;
    paper.height = GetPrintedPhotoHeight(contentHeight);
    paper.sourceX = 0.0f;
    paper.sourceY = 0.0f;
    paper.sourceWidth = 1.0f;
    paper.sourceHeight = 1.0f;
    paper.tintR = 0.98f;
    paper.tintG = 0.96f;
    paper.tintB = 0.90f;
    paper.tintA = 0.94f;

    CapturedPhotoItem matte;
    matte.textureId = paperTextureId;
    matte.role = PhotoCopyRole::Solid;
    matte.layer = PhotoCopyLayer::Background;
    matte.origin = PhotoCopyOrigin::Tile;
    matte.appliedTheme = PhotoFilterTheme::None;
    matte.placementRuleGroup = paperRuleGroup;
    matte.relativeX = gPrintedPhotoPaddingX - gPrintedPhotoMatteInset;
    matte.relativeY = gPrintedPhotoPaddingTop - gPrintedPhotoMatteInset;
    matte.width = contentWidth + gPrintedPhotoMatteInset * 2.0f;
    matte.height = contentHeight + gPrintedPhotoMatteInset * 2.0f;
    matte.sourceX = 0.0f;
    matte.sourceY = 0.0f;
    matte.sourceWidth = 1.0f;
    matte.sourceHeight = 1.0f;
    matte.tintR = 0.10f;
    matte.tintG = 0.12f;
    matte.tintB = 0.14f;
    matte.tintA = 0.92f;

    std::vector<CapturedPhotoItem> result;
    result.reserve(printedItems.size() + 2);
    result.push_back(paper);
    result.push_back(matte);
    result.insert(result.end(), printedItems.begin(), printedItems.end());
    return result;
}

bool ContainsSpawnArchetypeItem(const std::vector<CapturedPhotoItem>& items)
{
    for (const auto& item : items)
    {
        if (item.spawnArchetype != CapturedSpawnArchetype::None)
        {
            return true;
        }
    }
    return false;
}

bool IsShieldArchetype(CapturedSpawnArchetype archetype)
{
    return archetype == CapturedSpawnArchetype::ShieldNormal ||
        archetype == CapturedSpawnArchetype::ShieldRushBurst ||
        archetype == CapturedSpawnArchetype::ShieldJumpBurst;
}

bool ContainsOnlyShieldArchetypeItems(const std::vector<CapturedPhotoItem>& items)
{
    bool foundShield = false;
    for (const auto& item : items)
    {
        if (IsShieldArchetype(item.spawnArchetype))
        {
            foundShield = true;
            continue;
        }

        if (item.spawnArchetype != CapturedSpawnArchetype::None ||
            item.layer == PhotoCopyLayer::Foreground)
        {
            return false;
        }
    }
    return foundShield;
}

bool IsPureVanishObjectItem(const CapturedPhotoItem& item)
{
    return item.vanishOnCapture &&
        item.spawnArchetype == CapturedSpawnArchetype::None &&
        item.damagePlatformTileSpan <= 0 &&
        item.spikeStripTileSpan <= 0;
}

bool ContainsOnlyPureVanishObjectItems(const std::vector<CapturedPhotoItem>& items)
{
    bool foundVanishObject = false;
    for (const auto& item : items)
    {
        if (IsPureVanishObjectItem(item))
        {
            foundVanishObject = true;
            continue;
        }
        return false;
    }
    return foundVanishObject;
}

void NormalizeItemsToBounds(std::vector<CapturedPhotoItem>& items, float& width, float& height)
{
    if (items.empty())
    {
        width = 1.0f;
        height = 1.0f;
        return;
    }

    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    for (const auto& item : items)
    {
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

    width = (std::max)(1.0f, maxX - minX);
    height = (std::max)(1.0f, maxY - minY);
}

float GetLSizePhotoContentHeight(float contentWidth)
{
    constexpr float kLSizePhotoAspect = 127.0f / 89.0f;
    const float printedWidth = GetPrintedPhotoWidth(contentWidth);
    const float printedHeight = printedWidth / kLSizePhotoAspect;
    return (std::max)(1.0f, printedHeight - gPrintedPhotoPaddingTop - gPrintedPhotoFooterHeight);
}

std::vector<CapturedPhotoItem> BuildRawPlacementItems(
    const std::vector<CapturedPhotoItem>& sourceItems,
    float captureWidth,
    float captureHeight,
    bool flipX);

std::vector<CapturedPhotoItem> BuildPureVanishObjectPrintedPhotoItems(
    const std::vector<CapturedPhotoItem>& sourceItems,
    int paperTextureId,
    PhotoFilterTheme capturedTheme,
    float captureWidth,
    float captureHeight,
    bool flipX,
    float& outWidth,
    float& outHeight)
{
    std::vector<CapturedPhotoItem> items = BuildRawPlacementItems(
        sourceItems,
        captureWidth,
        captureHeight,
        flipX);

    float boundsWidth = 1.0f;
    float boundsHeight = 1.0f;
    NormalizeItemsToBounds(items, boundsWidth, boundsHeight);

    const float contentWidth = (std::max)((std::max)(1.0f, captureWidth), boundsWidth);
    const float contentHeight = GetLSizePhotoContentHeight(contentWidth);
    const float fitScale = boundsHeight > contentHeight
        ? std::clamp(contentHeight / boundsHeight, 0.01f, 1.0f)
        : 1.0f;
    if (fitScale < 1.0f)
    {
        for (auto& item : items)
        {
            item.relativeX *= fitScale;
            item.relativeY *= fitScale;
            item.width *= fitScale;
            item.height *= fitScale;
        }
        boundsWidth *= fitScale;
        boundsHeight *= fitScale;
    }
    const float offsetX = (std::max)(0.0f, (contentWidth - boundsWidth) * 0.5f);
    const float offsetY = (std::max)(0.0f, (contentHeight - boundsHeight) * 0.5f);
    for (auto& item : items)
    {
        item.relativeX += offsetX;
        item.relativeY += offsetY;
    }

    outWidth = GetPrintedPhotoWidth(contentWidth);
    outHeight = GetPrintedPhotoHeight(contentHeight);
    return BuildPrintedPhotoItems(
        items,
        paperTextureId,
        capturedTheme,
        contentWidth,
        contentHeight,
        false,
        false);
}

bool ContainsShapePreservingItem(const std::vector<CapturedPhotoItem>& items)
{
    for (const auto& item : items)
    {
        if (IsPureVanishObjectItem(item))
        {
            continue;
        }

        if (item.sourceTileValue > 0 ||
            !item.collisionOutline.empty() ||
            item.lightRadius > 0.0f ||
            item.sepiaShutterObject ||
            item.sepiaPlainRubbleObject ||
            item.blasterRobotProjectile)
        {
            return true;
        }
    }
    return false;
}

std::vector<CapturedPhotoItem> BuildRawPlacementItems(
    const std::vector<CapturedPhotoItem>& sourceItems,
    float captureWidth,
    float captureHeight,
    bool flipX)
{
    std::vector<CapturedPhotoItem> items = sourceItems;
    if (!flipX)
    {
        return items;
    }

    for (auto& item : items)
    {
        item.relativeX = captureWidth - item.relativeX - item.width;
        item.flipX = !item.flipX;
        item.projectileVelocityX = -item.projectileVelocityX;
        item.spearDirectionX = -item.spearDirectionX;
        if (item.spawnArchetype == CapturedSpawnArchetype::Projectile &&
            (std::fabs(item.projectileVelocityX) > 0.0001f || std::fabs(item.projectileVelocityY) > 0.0001f))
        {
            item.rotation = std::atan2(item.projectileVelocityY, item.projectileVelocityX) -
                (item.blasterRobotProjectile ? 3.1415926535f : 0.0f);
        }
        for (auto& point : item.collisionOutline)
        {
            point.x = 1.0f - point.x;
        }
    }

    return items;
}
}

namespace photo_shared
{
void DrawSepiaShutterItem(
    int textureId,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    bool flipX,
    float rotation)
{
    if (textureId < 0 || drawWidth <= 0.0f || drawHeight <= 0.0f)
    {
        return;
    }

    constexpr float kSourceCapRatio = 0.15f;
    constexpr float kTextureAspectHeightOverWidth = 1536.0f / 480.0f;
    if (drawWidth > drawHeight)
    {
        const float capWidth = std::min(
            drawWidth * 0.34f,
            drawHeight * kSourceCapRatio * kTextureAspectHeightOverWidth);
        const float clampedCapWidth = std::max(0.0f, std::min(capWidth, drawWidth * 0.5f));
        const float middleWidth = std::max(0.0f, drawWidth - clampedCapWidth * 2.0f);

        SpriteDraw(
            textureId,
            drawX,
            drawY,
            clampedCapWidth,
            drawHeight,
            0.0f,
            0.0f,
            kSourceCapRatio,
            1.0f,
            flipX,
            rotation);

        if (middleWidth > 0.0f)
        {
            SpriteDraw(
                textureId,
                drawX + clampedCapWidth,
                drawY,
                middleWidth,
                drawHeight,
                kSourceCapRatio,
                0.0f,
                1.0f - kSourceCapRatio * 2.0f,
                1.0f,
                flipX,
                rotation);
        }

        SpriteDraw(
            textureId,
            drawX + clampedCapWidth + middleWidth,
            drawY,
            clampedCapWidth,
            drawHeight,
            1.0f - kSourceCapRatio,
            0.0f,
            kSourceCapRatio,
            1.0f,
            flipX,
            rotation);
        return;
    }

    const float capHeight = std::min(
        drawHeight * 0.34f,
        drawWidth * kSourceCapRatio * kTextureAspectHeightOverWidth);
    const float clampedCapHeight = std::max(0.0f, std::min(capHeight, drawHeight * 0.5f));
    const float middleHeight = std::max(0.0f, drawHeight - clampedCapHeight * 2.0f);

    SpriteDraw(
        textureId,
        drawX,
        drawY,
        drawWidth,
        clampedCapHeight,
        0.0f,
        0.0f,
        1.0f,
        kSourceCapRatio,
        flipX,
        rotation);

    if (middleHeight > 0.0f)
    {
        SpriteDraw(
            textureId,
            drawX,
            drawY + clampedCapHeight,
            drawWidth,
            middleHeight,
            0.0f,
            kSourceCapRatio,
            1.0f,
            1.0f - kSourceCapRatio * 2.0f,
            flipX,
            rotation);
    }

    SpriteDraw(
        textureId,
        drawX,
        drawY + clampedCapHeight + middleHeight,
        drawWidth,
        clampedCapHeight,
        0.0f,
        1.0f - kSourceCapRatio,
        1.0f,
        kSourceCapRatio,
        flipX,
        rotation);
}

bool DrawDamagePlatformItemPreview(
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha)
{
    if (item.damagePlatformTileSpan <= 0)
    {
        return false;
    }

    if (item.textureId >= 0)
    {
        // Preserve captured H-marker base textures in paste previews.
        DrawTexturedPhotoItemPreview(
            item,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            item.damagePlatformTileSpan,
            alpha);
        return true;
    }

    const int baseColor = GetColor(
        static_cast<int>(std::round(item.tintR * 255.0f)),
        static_cast<int>(std::round(item.tintG * 255.0f)),
        static_cast<int>(std::round(item.tintB * 255.0f)));
    const int spikeColor = GetColor(235, 26, 26);
    DrawDamagePlatformItem(
        drawX,
        drawY,
        drawWidth,
        drawHeight,
        item.damagePlatformTileSpan,
        item.sourceX,
        item.sourceY,
        item.sourceWidth,
        item.sourceHeight,
        item.rotation,
        baseColor,
        spikeColor,
        alpha);
    return true;
}

bool DrawSpikeStripItemPreview(
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha)
{
    if (item.spikeStripTileSpan <= 0)
    {
        return false;
    }

    if (item.textureId >= 0)
    {
        // Preserve captured H-marker spike textures in paste previews.
        DrawTexturedPhotoItemPreview(
            item,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            item.spikeStripTileSpan,
            alpha);
        return true;
    }

    const int spikeColor = GetColor(
        static_cast<int>(std::round(std::clamp(item.tintR, 0.0f, 1.0f) * 255.0f)),
        static_cast<int>(std::round(std::clamp(item.tintG, 0.0f, 1.0f) * 255.0f)),
        static_cast<int>(std::round(std::clamp(item.tintB, 0.0f, 1.0f) * 255.0f)));
    DrawSpikeStripItem(
        drawX,
        drawY,
        drawWidth,
        drawHeight,
        item.spikeStripTileSpan,
        item.sourceX,
        item.sourceY,
        item.sourceWidth,
        item.sourceHeight,
        item.rotation,
        spikeColor,
        alpha);
    return true;
}

std::vector<CapturedPhotoItem> BuildPlacementItems(
    const PhotoCaptureState& capture,
    const PhotoPlacementState& placement,
    int whiteTexture,
    float& outWidth,
    float& outHeight)
{
    const bool containsArchetype = ContainsSpawnArchetypeItem(capture.items);
    const bool preservesShape = ContainsShapePreservingItem(capture.items);
    if (ContainsOnlyPureVanishObjectItems(capture.items))
    {
        std::vector<CapturedPhotoItem> items = BuildPureVanishObjectPrintedPhotoItems(
            capture.items,
            whiteTexture,
            capture.capturedTheme,
            (std::max)(1.0f, capture.width),
            (std::max)(1.0f, capture.height),
            placement.flipX,
            outWidth,
            outHeight);
        RotatePrintedPhotoItems(items, outWidth, outHeight, placement.rotation);
        return items;
    }

    if (ContainsOnlyShieldArchetypeItems(capture.items))
    {
        outWidth = (std::max)(1.0f, capture.width);
        outHeight = (std::max)(1.0f, capture.height);
        std::vector<CapturedPhotoItem> items = BuildRawPlacementItems(
            capture.items,
            outWidth,
            outHeight,
            placement.flipX);
        NormalizeItemsToBounds(items, outWidth, outHeight);
        RotatePrintedPhotoItems(items, outWidth, outHeight, placement.rotation);
        return items;
    }

    if (preservesShape)
    {
        outWidth = (std::max)(1.0f, capture.width);
        outHeight = (std::max)(1.0f, capture.height);
        std::vector<CapturedPhotoItem> items = BuildRawPlacementItems(
            capture.items,
            outWidth,
            outHeight,
            placement.flipX);
        RotatePrintedPhotoItems(items, outWidth, outHeight, placement.rotation);
        return items;
    }

    if (containsArchetype)
    {
        outWidth = (std::max)(1.0f, capture.width);
        outHeight = (std::max)(1.0f, capture.height);

        PhotoPlacementRuleGroup paperRuleGroup = PhotoPlacementRuleGroup::Group1;
        for (const auto& item : capture.items)
        {
            if (item.spawnArchetype == CapturedSpawnArchetype::WalkerMelee ||
                item.spawnArchetype == CapturedSpawnArchetype::ShieldNormal ||
                item.spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst ||
                item.spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst)
            {
                paperRuleGroup = PhotoPlacementRuleGroup::Group3;
                break;
            }
        }

        std::vector<CapturedPhotoItem> items = BuildPrintedPhotoItems(
            capture.items,
            whiteTexture,
            capture.capturedTheme,
            outWidth,
            outHeight,
            placement.flipX,
            false,
            paperRuleGroup);
        if (placement.flipX)
        {
            for (auto& item : items)
            {
                item.relativeX = outWidth - item.relativeX - item.width;
                item.flipX = !item.flipX;
                item.projectileVelocityX = -item.projectileVelocityX;
                item.spearDirectionX = -item.spearDirectionX;
                if (item.spawnArchetype == CapturedSpawnArchetype::Projectile &&
                    (std::fabs(item.projectileVelocityX) > 0.0001f || std::fabs(item.projectileVelocityY) > 0.0001f))
                {
                    item.rotation = std::atan2(item.projectileVelocityY, item.projectileVelocityX) -
                        (item.blasterRobotProjectile ? 3.1415926535f : 0.0f);
                }
            }
        }
        RotatePrintedPhotoItems(items, outWidth, outHeight, placement.rotation);
        return items;
    }

    // 通常写真も中身サイズ基準で配置し、写真を再撮影した時の余白二重乗りで巨大化しないようにします。
    outWidth = (std::max)(1.0f, capture.width);
    outHeight = (std::max)(1.0f, capture.height);
    std::vector<CapturedPhotoItem> items = BuildPrintedPhotoItems(
        capture.items,
        whiteTexture,
        capture.capturedTheme,
        capture.width,
        capture.height,
        placement.flipX,
        placement.bridgeEnabled);
    RotatePrintedPhotoItems(items, outWidth, outHeight, placement.rotation);
    return items;
}

void ApplyPreviewFilterTheme(CapturedPhotoItem& item)
{
    if (item.sepiaRestoredMarkerObject ||
        item.sepiaShutterObject ||
        item.sepiaPlainRubbleObject ||
        item.spawnArchetype == CapturedSpawnArchetype::SepiaGround ||
        item.spawnArchetype == CapturedSpawnArchetype::FallingRock)
    {
        return;
    }

    ApplyPhotoFilterThemeToPreviewItem(
        item.appliedTheme,
        item.origin,
        item.role,
        item.layer,
        item.tintR,
        item.tintG,
        item.tintB,
        item.tintA);
}

void DrawCapturedPhotoItem(
    int fallbackTextureId,
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha)
{
    Shader_ResetStyle();
    const float tintScale = item.spriteProjectile ? 0.8f : 1.0f;
    Shader_SetTint(item.tintR * tintScale, item.tintG * tintScale, item.tintB * tintScale, alpha);
    if (item.sepiaShutterObject)
    {
        DrawSepiaShutterItem(
            item.textureId >= 0 ? item.textureId : fallbackTextureId,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            item.flipX,
            item.rotation);
        return;
    }

    if (item.spawnArchetype == CapturedSpawnArchetype::Projectile)
    {
        if (item.spriteProjectile)
        {
            SpriteDraw(
                item.textureId >= 0 ? item.textureId : fallbackTextureId,
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                item.sourceX,
                item.sourceY,
                item.sourceWidth,
                item.sourceHeight,
                item.flipX,
                item.rotation);
            return;
        }

        const int color = GetColor(
            static_cast<int>(std::round(item.tintR * 255.0f)),
            static_cast<int>(std::round(item.tintG * 255.0f)),
            static_cast<int>(std::round(item.tintB * 255.0f)));
        const float projectileAngle = item.spearProjectile &&
            (std::fabs(item.spearDirectionX) > 0.0001f || std::fabs(item.spearDirectionY) > 0.0001f)
            ? std::atan2(item.spearDirectionY, item.spearDirectionX)
            : std::atan2(item.projectileVelocityY, item.projectileVelocityX);
        DrawProjectileItem(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            false,
            projectileAngle,
            color);
        return;
    }

    if (DrawDamagePlatformItemPreview(item, drawX, drawY, drawWidth, drawHeight, alpha))
    {
        return;
    }

    if (DrawSpikeStripItemPreview(item, drawX, drawY, drawWidth, drawHeight, alpha))
    {
        return;
    }

    const TileTriangleShape triangle = TileMap::GetTriangleShape(item.sourceTileValue);
    if (triangle.isTriangle)
    {
        const int color = GetColor(
            static_cast<int>(std::round(item.tintR * 255.0f)),
            static_cast<int>(std::round(item.tintG * 255.0f)),
            static_cast<int>(std::round(item.tintB * 255.0f)));
        DrawTriangleItem(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            triangle.risesRight,
            item.flipX,
            item.rotation,
            color);
        return;
    }

    SpriteDraw(
        item.textureId >= 0 ? item.textureId : fallbackTextureId,
        drawX,
        drawY,
        drawWidth,
        drawHeight,
        item.sourceX,
        item.sourceY,
        item.sourceWidth,
        item.sourceHeight,
        item.flipX,
        item.rotation);
}
}
