#include "photo_system.h"

#include <cfloat>

#include "game_scene_internal.h"
#include "photo_filter_rules.h"
#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr int kMaxPhotoGroups = 3;
    constexpr float kPhotoCopyLifetimeSeconds = 30.0f;
    constexpr float kPhotoPasteAnimationSeconds = 0.24f;
    constexpr float kPlacementRotateSpeed = 2.4f;

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

    void DrawTriangleItem(
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        bool risesRight,
        bool flipX,
        float rotation,
        int color)
    {
        const bool finalRisesRight = flipX ? !risesRight : risesRight;
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        float cx = 0.0f;
        float cy = 0.0f;

        if (finalRisesRight)
        {
            ax = drawX;
            ay = drawY + drawHeight;
            bx = drawX + drawWidth;
            by = drawY + drawHeight;
            cx = drawX + drawWidth;
            cy = drawY;
        }
        else
        {
            ax = drawX;
            ay = drawY;
            bx = drawX;
            by = drawY + drawHeight;
            cx = drawX + drawWidth;
            cy = drawY + drawHeight;
        }

        const float centerX = drawX + drawWidth * 0.5f;
        const float centerY = drawY + drawHeight * 0.5f;
        RotatePoint(centerX, centerY, rotation, ax, ay);
        RotatePoint(centerX, centerY, rotation, bx, by);
        RotatePoint(centerX, centerY, rotation, cx, cy);
        DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
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
        Shader_SetTint(item.tintR, item.tintG, item.tintB, alpha);
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

    void ApplyPreviewFilterTheme(CapturedPhotoItem& item)
    {
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

    std::vector<CapturedPhotoItem> BuildPrintedPhotoItems(
        const std::vector<CapturedPhotoItem>& sourceItems,
        int paperTextureId,
        PhotoFilterTheme capturedTheme,
        float contentWidth,
        float contentHeight,
        bool flipX,
        bool bridgeEnabled)
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
            }
        }

        CapturedPhotoItem paper;
        paper.textureId = paperTextureId;
        paper.role = PhotoCopyRole::Solid;
        paper.layer = PhotoCopyLayer::Background;
        paper.origin = PhotoCopyOrigin::Generic;
        paper.appliedTheme = PhotoFilterTheme::None;
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

    std::vector<CapturedPhotoItem> BuildPlacementItems(
        const PhotoCaptureState& capture,
        const PhotoPlacementState& placement,
        int whiteTexture,
        float& outWidth,
        float& outHeight)
    {
        const bool containsArchetype = ContainsSpawnArchetypeItem(capture.items);
        if (containsArchetype)
        {
            std::vector<CapturedPhotoItem> items = capture.items;
            outWidth = (std::max)(1.0f, capture.width);
            outHeight = (std::max)(1.0f, capture.height);

            if (placement.flipX)
            {
                for (auto& item : items)
                {
                    item.relativeX = outWidth - item.relativeX - item.width;
                    item.flipX = !item.flipX;
                }
            }

            RotatePrintedPhotoItems(items, outWidth, outHeight, placement.rotation);
            return items;
        }

        outWidth = GetPrintedPhotoWidth(capture.width);
        outHeight = GetPrintedPhotoHeight(capture.height);
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
}

class PhotoSystem
{
public:
    static void HandleCapture(GameScene& scene)
    {
            if (!scene.m_flow.cameraMode || !Input_IsActionPressed(InputAction::CapturePhoto))
            {
                return;
            }
            if (scene.IsPhotoTrayHit(static_cast<float>(Input_GetMouseX()), static_cast<float>(Input_GetMouseY())))
            {
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
            scene.GetCaptureFrameRect(*playerTransform, frameX, frameY, frameWidth, frameHeight);

            scene.m_photo.capture.items.clear();
            float capturedMaxRight = 0.0f;
            float capturedMaxBottom = 0.0f;
            CaptureEntitiesInFrame(scene, frameX, frameY, frameWidth, frameHeight, capturedMaxRight, capturedMaxBottom);
            CaptureTilesInFrame(scene, frameX, frameY, frameWidth, frameHeight, capturedMaxRight, capturedMaxBottom);
            if (scene.m_photo.capture.items.empty())
            {
                return;
            }

            FinalizeCapturedPhoto(scene, *player, frameWidth, frameHeight);
    }

    static void HandleSpawn(GameScene& scene)
    {
            scene.m_photo.placement.valid = false;

            if (scene.m_photo.capture.hasPhoto &&
                (Input_IsActionPressed(InputAction::HoldPlacement) || Input_IsNorthButtonPressed()))
            {
                scene.m_photo.placement.active = !scene.m_photo.placement.active;
            }

            if (!scene.m_photo.capture.hasPhoto || !scene.m_photo.placement.active)
            {
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

    static void DrawPlacementPreview(const GameScene& scene)
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
        std::vector<CapturedPhotoItem> previewItems = BuildPlacementItems(
            scene.m_photo.capture,
            scene.m_photo.placement,
            scene.m_whiteTexture,
            previewWidth,
            previewHeight);

        for (const auto& item : previewItems)
        {
            CapturedPhotoItem previewItem = item;
            ApplyPreviewFilterTheme(previewItem);
            const float drawX = viewOriginX + ((scene.m_photo.placement.x + item.relativeX) - scene.m_flow.cameraX) * viewScale;
            const float drawY = viewOriginY + (scene.m_photo.placement.y + item.relativeY) * viewScale;
            const float drawWidth = item.width * viewScale;
            const float drawHeight = item.height * viewScale;

            Shader_ResetStyle();
            if (scene.m_photo.placement.valid)
            {
                float outlineR = 0.32f;
                float outlineG = 0.92f;
                float outlineB = 1.0f;
                GetPhotoFilterThemePreviewOutlineColor(previewItem.appliedTheme, outlineR, outlineG, outlineB);
                Shader_SetOutline(outlineR, outlineG, outlineB, 1.0f, previewItem.appliedTheme == PhotoFilterTheme::None ? 1.6f : 1.8f);
                Shader_SetTint(previewItem.tintR, previewItem.tintG, previewItem.tintB, 0.55f);
            }
            else
            {
                Shader_SetOutline(1.0f, 0.24f, 0.24f, 1.0f, 1.6f);
                Shader_SetTint(1.0f, 0.24f, 0.24f, 0.42f);
            }

            DrawCapturedPhotoItem(
                scene.m_tileTexture,
                item,
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                scene.m_photo.placement.valid ? 0.55f : 0.42f);
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
            "Solid in world  Groups:%d/3  Rot:%.0f  Keys:F/B/Z/X",
            scene.m_photo.groups.activeGroupCount,
            scene.m_photo.placement.rotation * 57.2957795f);

        Shader_ResetStyle();
    }

private:
    static void AddBridgeSegments(std::vector<CapturedPhotoItem>& items, int textureId, bool flipX, bool enabled, PhotoFilterTheme theme)
    {
            if (!enabled || items.size() < 2)
            {
                return;
            }

            const std::vector<CapturedPhotoItem> baseItems = items;
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
                    bridge.textureId = textureId;
                    bridge.role = PhotoCopyRole::Solid;
                    bridge.layer = PhotoCopyLayer::Foreground;
                    bridge.appliedTheme = theme;
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
                    items.push_back(bridge);
                }
            }
    }

    static void CaptureEntitiesInFrame(GameScene& scene, float frameX, float frameY, float frameWidth, float frameHeight, float& capturedMaxRight, float& capturedMaxBottom)
    {
            for (const auto& entity : scene.m_entities)
            {
                if (!entity || HasTag(*entity, "Player") || HasTag(*entity, "PhotoBox"))
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
                if (overlapWidth <= 1.0f || overlapHeight <= 1.0f)
                {
                    continue;
                }

                const float localLeft = (overlapLeft - targetX) / targetWidth;
                const float localTop = (overlapTop - targetY) / targetHeight;
                const float localWidth = overlapWidth / targetWidth;
                const float localHeight = overlapHeight / targetHeight;

                CapturedPhotoItem item;
                const bool capturedBarrel = entity->GetComponent<BarrelComponent>() != nullptr;
                item.textureId = sprite->GetTextureId();
                item.role = GetEntityCopyRole(*entity);
                item.layer = PhotoCopyLayer::Foreground;
                item.origin = GetEntityCopyOrigin(*entity);
                item.appliedTheme = scene.m_photo.capture.selectedTheme;
                item.spawnArchetype = capturedBarrel ? CapturedSpawnArchetype::Barrel : CapturedSpawnArchetype::None;
                item.relativeX = overlapLeft - frameX;
                item.relativeY = overlapTop - frameY;
                item.width = overlapWidth;
                item.height = overlapHeight;
                item.sourceX = sprite->GetSourceX() + sprite->GetSourceWidth() * localLeft;
                item.sourceY = sprite->GetSourceY() + sprite->GetSourceHeight() * localTop;
                item.sourceWidth = sprite->GetSourceWidth() * localWidth;
                item.sourceHeight = sprite->GetSourceHeight() * localHeight;
                if (auto* tint = entity->GetComponent<TintComponent>())
                {
                    item.tintR = tint->r;
                    item.tintG = tint->g;
                    item.tintB = tint->b;
                    item.tintA = tint->a;
                }
                if (!capturedBarrel)
                {
                    item.role = GetRoleFromTint(item.tintR, item.tintG, item.tintB);
                    item.layer = GetLayerFromTint(item.tintR, item.tintG, item.tintB);
                }
                else
                {
                    item.role = PhotoCopyRole::Solid;
                    item.layer = PhotoCopyLayer::Foreground;
                }

                // �����ŕ␳�Frole �� Solid �̏ꍇ�͕K�� Foreground �ɂ���
                if (item.role == PhotoCopyRole::Solid)
                {
                    item.layer = PhotoCopyLayer::Foreground;
                }

                if (!capturedBarrel)
                {
                    ApplyPhotoFilterToCapturedTarget(*entity, scene.m_photo.capture.selectedTheme);
                }
                scene.m_photo.capture.items.push_back(item);
                capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
                capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
            }
    }

    static void CaptureTilesInFrame(GameScene& scene, float frameX, float frameY, float frameWidth, float frameHeight, float& capturedMaxRight, float& capturedMaxBottom)
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

                    // �ύX�_: CSV �� 1 �Ƃ��Ă���u�n�ʁv�^�C���͎B�e���Ȃ�
                    if (tileValue == 1)
                    {
                        continue;
                    }

                    if (tileValue == 8)
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
                    item.textureId = scene.m_tileTexture;
                    item.role = GetTileCopyRole(tileValue);
                    item.layer = PhotoCopyLayer::Foreground;
                    item.origin = GetTileCopyOrigin(tileValue);
                    item.appliedTheme = scene.m_photo.capture.selectedTheme;
                    item.relativeX = overlapLeft - frameX;
                    item.relativeY = overlapTop - frameY;
                    item.width = overlapWidth;
                    item.height = overlapHeight;
                    item.sourceX = 0.0f;
                    item.sourceY = 0.0f;
                    item.sourceWidth = 1.0f;
                    item.sourceHeight = 1.0f;
                    GetTileCaptureTint(tileValue, item.tintR, item.tintG, item.tintB, item.tintA);
                    item.sourceTileValue = tileValue;
                    scene.m_photo.capture.items.push_back(item);
                    capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
                    capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
                }
            }
    }

    static void FinalizeCapturedPhoto(GameScene& scene, Entity& player, float frameWidth, float frameHeight)
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
            scene.StoreCapturedPhoto();

            scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "shutter", 0.0f, 0.0f });
            scene.m_eventBus.Publish({ EventType::LogMessage, &player, nullptr, GetPhotoCaptureLogMessage(scene.m_photo.capture.capturedTheme), 0.0f, 0.0f });
            scene.m_flow.shutterFlashRemaining = gShutterFlashSeconds;
            scene.m_flow.developedPhotoPreviewRemaining = 3.2f;
    }

    static bool UpdatePlacementPreview(GameScene& scene, float& spawnX, float& spawnY, float& spawnWidth, float& spawnHeight)
    {
        float placementWidth = 0.0f;
        float placementHeight = 0.0f;
        static_cast<void>(BuildPlacementItems(
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

        // �}�E�X�i�X�N���[���j����[���h���W�֕ϊ��i�J����X�������j
        const float mouseWorldX = ((static_cast<float>(Input_GetMouseX()) - viewOriginX) / viewScale) + scene.m_flow.cameraX;
        const float mouseWorldY = ((static_cast<float>(Input_GetMouseY()) - viewOriginY) / viewScale);

        // �E�X�e�B�b�N�p�̉��z�J�[�\���i���[���h���W�j
        static float padCursorWorldX = mouseWorldX;
        static float padCursorWorldY = mouseWorldY;
        static int lastMouseX = Input_GetMouseX();
        static int lastMouseY = Input_GetMouseY();
        static unsigned int lastTimeMs = 0;

        // �}�E�X���������牼�z�J�[�\���𓯊�
        const int curMouseX = Input_GetMouseX();
        const int curMouseY = Input_GetMouseY();
        if (curMouseX != lastMouseX || curMouseY != lastMouseY)
        {
            padCursorWorldX = mouseWorldX;
            padCursorWorldY = mouseWorldY;
        }
        lastMouseX = curMouseX;
        lastMouseY = curMouseY;

        // �o�ߎ���
        const unsigned int nowMs = static_cast<unsigned int>(GetNowCount());
        const float dt = lastTimeMs ? (static_cast<float>(nowMs - lastTimeMs) / 1000.0f) : (1.0f / 60.0f);
        lastTimeMs = nowMs;

        // �E�X�e�B�b�N����
        const float rightX = Input_GetRightStickX();
        const float rightY = Input_GetRightStickY();

        // �p�b�h�ړ��E�߂�̋����i�v�����\�j
        constexpr float kPadDead = 0.08f;
        constexpr float kPadCursorSpeed = 800.0f; // ���[���h�P�� / �b
        constexpr float kPadReturnLerpSpeed = 8.0f; // �X�e�B�b�N���E��̖߂葬��

        const bool padActive = Input_IsGamepadConnected() && (std::fabs(rightX) > kPadDead || std::fabs(rightY) > kPadDead);
        if (padActive)
        {
            padCursorWorldX += rightX * kPadCursorSpeed * dt;
            padCursorWorldY += rightY * kPadCursorSpeed * dt;
        }
        else
        {
            // ���炩�Ƀ}�E�X�ʒu�֕�Ԃ��Ė߂�
            const float lerpFactor = std::min(1.0f, dt * kPadReturnLerpSpeed);
            padCursorWorldX += (mouseWorldX - padCursorWorldX) * lerpFactor;
            padCursorWorldY += (mouseWorldY - padCursorWorldY) * lerpFactor;
        }

        // ���͐��������A�c�̂݃}�b�v��ɃN�����v
        const float mapWidth = scene.GetMapPixelWidth();
        const float mapHeight = scene.GetMapPixelHeight();
        (void)mapWidth; // ��������s��Ȃ��̂Ŗ��g�p�i�x���}���j
        padCursorWorldY = std::clamp(padCursorWorldY, 0.0f, std::max(0.0f, mapHeight));

        // �ŏI�I�ȃJ�[�\���i���[���h���W�j
        const float cursorWorldX = padCursorWorldX;
        const float cursorWorldY = padCursorWorldY;

        // ��������O���ăv���r���[�\���i�z�u�m��� IsPhotoPlacementValid �ɂ�锻��j
        spawnX = cursorWorldX - spawnWidth * 0.5f;
        spawnY = std::clamp(cursorWorldY - spawnHeight * 0.5f, 0.0f, std::max(0.0f, mapHeight - spawnHeight));

        scene.m_photo.placement.active = true;
        scene.m_photo.placement.x = spawnX;
        scene.m_photo.placement.y = spawnY;
        scene.m_photo.placement.width = spawnWidth;
        scene.m_photo.placement.height = spawnHeight;
        scene.m_photo.placement.valid = scene.IsPhotoPlacementValid(spawnX, spawnY, spawnWidth, spawnHeight);

        // ���z�J�[�\���̃X�N���[�����W��쐬���ăg���C����Ɏg��
        const float cursorScreenX = viewOriginX + (cursorWorldX - scene.m_flow.cameraX) * viewScale;
        const float cursorScreenY = viewOriginY + cursorWorldY * viewScale;

        return
            scene.m_photo.placement.valid &&
            !scene.IsPhotoTrayHit(cursorScreenX, cursorScreenY) &&
            Input_IsActionPressed(InputAction::ConfirmPlacement);
    }

    static void SpawnPhotoGroup(GameScene& scene, Entity& player, float spawnX, float spawnY, float spawnWidth)
    {
            static_cast<void>(spawnWidth);
            float rotatedSpawnWidth = 0.0f;
            float rotatedSpawnHeight = 0.0f;
            std::vector<CapturedPhotoItem> spawnedItems = BuildPlacementItems(
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
                    spawnedBarrel->AddComponent<TransformComponent>(
                        spawnX + item.relativeX,
                        spawnY + item.relativeY,
                        item.width,
                        item.height);
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
                    if (auto* sprite = spawnedBarrel->GetComponent<SpriteRenderComponent>())
                    {
                        sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                        sprite->SetFlipX(item.flipX);
                    }
                    if (auto* transform = spawnedBarrel->GetComponent<TransformComponent>())
                    {
                        transform->rotation = item.rotation;
                    }
                    scene.m_entities.push_back(std::move(barrelEntity));
                    continue;
                }

                auto entity = std::make_unique<Entity>();
                lastSpawnedEntity = entity.get();
                ++spawnedPhotoBoxCount;
                lastSpawnedEntity->AddComponent<TagComponent>("PhotoBox");
                lastSpawnedEntity->AddComponent<PhotoCopyGroupComponent>(groupId);
                lastSpawnedEntity->AddComponent<PhotoCopyLifetimeComponent>(kPhotoCopyLifetimeSeconds);
                lastSpawnedEntity->AddComponent<PhotoPasteAnimationComponent>(kPhotoPasteAnimationSeconds);
                lastSpawnedEntity->AddComponent<PhotoCopyRoleComponent>(item.role);
                lastSpawnedEntity->AddComponent<PhotoCopyOriginComponent>(item.origin);
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
                lastSpawnedEntity->AddComponent<TransformComponent>(
                    spawnX + item.relativeX,
                    spawnY + item.relativeY,
                    item.width,
                    item.height);
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
                ApplyPhotoFilterToPhotoBox(*lastSpawnedEntity, item.appliedTheme);
                scene.m_entities.push_back(std::move(entity));
            }

            if (spawnedPhotoBoxCount > 0)
            {
                scene.m_photo.groups.activeGroupCount = std::min(kMaxPhotoGroups, scene.m_photo.groups.activeGroupCount + 1);
                scene.m_photo.groups.hasSpawnedCopy = true;
            }
            scene.ConsumeSelectedPhotoSlot();
            scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, lastSpawnedEntity, "test_tone", 0.0f, 0.0f });
            scene.m_eventBus.Publish({ EventType::LogMessage, &player, lastSpawnedEntity, "Spawned filtered reconstruction", 0.0f, 0.0f });
    }
};

namespace photo_system
{
void HandleCapture(GameScene& scene)
{
    PhotoSystem::HandleCapture(scene);
}

void HandleSpawn(GameScene& scene)
{
    PhotoSystem::HandleSpawn(scene);
}

void DrawPlacementPreview(const GameScene& scene)
{
    PhotoSystem::DrawPlacementPreview(scene);
}
}
