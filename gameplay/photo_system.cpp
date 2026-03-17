#include "photo_system.h"

#include "game_scene_internal.h"
#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr int kMaxPhotoGroups = 3;
    constexpr float kPhotoCopyLifetimeSeconds = 10.0f;
    constexpr float kPrintedPhotoPaddingX = 16.0f;
    constexpr float kPrintedPhotoPaddingTop = 16.0f;
    constexpr float kPrintedPhotoFooterHeight = 52.0f;
    constexpr float kPrintedPhotoMinWidth = 120.0f;
    constexpr float kPrintedPhotoMinHeight = 144.0f;
    constexpr float kPrintedPhotoImageMatteInset = 3.0f;

    float GetPrintedPhotoWidth(float contentWidth)
    {
        return std::max(kPrintedPhotoMinWidth, contentWidth + kPrintedPhotoPaddingX * 2.0f);
    }

    float GetPrintedPhotoHeight(float contentHeight)
    {
        return std::max(kPrintedPhotoMinHeight, contentHeight + kPrintedPhotoPaddingTop + kPrintedPhotoFooterHeight);
    }

    const char* GetPreviewLayerLabel(PhotoCopyLayer layer)
    {
        switch (layer)
        {
        case PhotoCopyLayer::Background:
            return "Background";
        case PhotoCopyLayer::Shadow:
            return "Shadow";
        case PhotoCopyLayer::Foreground:
        default:
            return "Foreground";
        }
    }

    const char* GetPreviewLayerEffectText(PhotoCopyLayer layer)
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

    const char* GetPreviewFilterThemeLabel(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            return "Hot";
        case PhotoFilterTheme::Cold:
            return "Cold";
        case PhotoFilterTheme::Invert:
            return "Invert";
        case PhotoFilterTheme::Sepia:
            return "Sepia";
        case PhotoFilterTheme::None:
        default:
            return "None";
        }
    }

    void ApplyPreviewFilterTheme(CapturedPhotoItem& item)
    {
        switch (item.appliedTheme)
        {
        case PhotoFilterTheme::Hot:
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 1.0f;
            item.tintG = 0.34f;
            item.tintB = 0.12f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::Cold:
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 0.76f;
            item.tintG = 0.90f;
            item.tintB = 1.0f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::Invert:
            item.role = item.origin == PhotoCopyOrigin::Enemy ? PhotoCopyRole::Ally : PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 0.62f;
            item.tintG = 0.62f;
            item.tintB = 0.64f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::Sepia:
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 0.76f;
            item.tintG = 0.58f;
            item.tintB = 0.34f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::None:
        default:
            break;
        }
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
            item.relativeX += kPrintedPhotoPaddingX;
            item.relativeY += kPrintedPhotoPaddingTop;
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
        matte.relativeX = kPrintedPhotoPaddingX - kPrintedPhotoImageMatteInset;
        matte.relativeY = kPrintedPhotoPaddingTop - kPrintedPhotoImageMatteInset;
        matte.width = contentWidth + kPrintedPhotoImageMatteInset * 2.0f;
        matte.height = contentHeight + kPrintedPhotoImageMatteInset * 2.0f;
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
}

class PhotoSystem
{
public:
    static void HandleCapture(GameScene& scene)
    {
            if (!scene.m_cameraMode || !Input_IsMouseLeftPressed())
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

            FinalizeCapturedPhoto(scene, *player, capturedMaxRight, capturedMaxBottom);
    }

    static void HandleSpawn(GameScene& scene)
    {
            scene.m_photo.placement.active = false;
            scene.m_photo.placement.valid = false;

            if (!scene.m_photo.capture.hasPhoto || !Input_IsKeyDown('E'))
            {
                return;
            }

            if (Input_IsKeyPressed('Q'))
            {
                scene.m_photo.placement.layer = CyclePlacementLayer(scene.m_photo.placement.layer);
            }
            if (Input_IsKeyPressed('F'))
            {
                scene.m_photo.placement.flipX = !scene.m_photo.placement.flipX;
            }
            if (Input_IsKeyPressed('B'))
            {
                scene.m_photo.placement.bridgeEnabled = !scene.m_photo.placement.bridgeEnabled;
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
        std::vector<CapturedPhotoItem> previewItems = BuildPrintedPhotoItems(
            scene.m_photo.capture.items,
            scene.m_whiteTexture,
            scene.m_photo.capture.capturedTheme,
            scene.m_photo.capture.width,
            scene.m_photo.capture.height,
            scene.m_photo.placement.flipX,
            scene.m_photo.placement.bridgeEnabled);

        for (const auto& item : previewItems)
        {
            CapturedPhotoItem previewItem = item;
            ApplyPreviewFilterTheme(previewItem);
            const float drawX = viewOriginX + ((scene.m_photo.placement.x + item.relativeX) - scene.m_cameraX) * viewScale;
            const float drawY = viewOriginY + (scene.m_photo.placement.y + item.relativeY) * viewScale;
            const float drawWidth = item.width * viewScale;
            const float drawHeight = item.height * viewScale;

            Shader_ResetStyle();
            if (scene.m_photo.placement.valid)
            {
                switch (previewItem.appliedTheme)
                {
                case PhotoFilterTheme::Hot:
                    Shader_SetOutline(1.0f, 0.42f, 0.18f, 1.0f, 1.8f);
                    break;
                case PhotoFilterTheme::Cold:
                    Shader_SetOutline(0.76f, 0.94f, 1.0f, 1.0f, 1.8f);
                    break;
                case PhotoFilterTheme::Invert:
                    Shader_SetOutline(0.86f, 0.86f, 0.92f, 1.0f, 1.8f);
                    break;
                case PhotoFilterTheme::Sepia:
                    Shader_SetOutline(0.90f, 0.72f, 0.42f, 1.0f, 1.8f);
                    break;
                case PhotoFilterTheme::None:
                default:
                    Shader_SetOutline(0.32f, 0.92f, 1.0f, 1.0f, 1.6f);
                    break;
                }
                Shader_SetTint(previewItem.tintR, previewItem.tintG, previewItem.tintB, 0.55f);
            }
            else
            {
                Shader_SetOutline(1.0f, 0.24f, 0.24f, 1.0f, 1.6f);
                Shader_SetTint(1.0f, 0.24f, 0.24f, 0.42f);
            }

            SpriteDraw(
                item.textureId >= 0 ? item.textureId : scene.m_tileTexture,
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                item.sourceX,
                item.sourceY,
                item.sourceWidth,
                item.sourceHeight,
                item.flipX,
                0.0f);
        }

        DrawFormatString(
            static_cast<int>(viewOriginX + 24.0f),
            static_cast<int>(viewOriginY + 24.0f),
            GetColor(230, 240, 255),
            "Filter:%s  Layer:%s  Flip:%s  Bridge:%s",
            GetPreviewFilterThemeLabel(scene.m_photo.capture.capturedTheme),
            GetPreviewLayerLabel(scene.m_photo.placement.layer),
            scene.m_photo.placement.flipX ? "On" : "Off",
            scene.m_photo.placement.bridgeEnabled ? "On" : "Off");
        DrawFormatString(
            static_cast<int>(viewOriginX + 24.0f),
            static_cast<int>(viewOriginY + 48.0f),
            GetColor(190, 220, 255),
            "%s  Groups:%d/3  Keys:Q/F/B",
            GetPreviewLayerEffectText(scene.m_photo.placement.layer),
            scene.m_photo.groups.activeGroupCount);

        Shader_ResetStyle();
    }

private:
    static PhotoCopyLayer CyclePlacementLayer(PhotoCopyLayer current)
    {
            switch (current)
            {
            case PhotoCopyLayer::Foreground:
                return PhotoCopyLayer::Background;
            case PhotoCopyLayer::Background:
                return PhotoCopyLayer::Shadow;
            case PhotoCopyLayer::Shadow:
            default:
                return PhotoCopyLayer::Foreground;
            }
    }

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
                item.textureId = sprite->GetTextureId();
                item.role = GetEntityCopyRole(*entity);
                item.layer = PhotoCopyLayer::Foreground;
                item.origin = GetEntityCopyOrigin(*entity);
                item.appliedTheme = scene.m_photo.capture.selectedTheme;
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
                    item.role = GetRoleFromTint(item.tintR, item.tintG, item.tintB);
                    item.layer = GetLayerFromTint(item.tintR, item.tintG, item.tintB);
                }
                ApplyPhotoFilterToCapturedTarget(*entity, scene.m_photo.capture.selectedTheme);
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

                    const float tileX = static_cast<float>(column) * tileSize;
                    const float tileY = static_cast<float>(row) * tileSize;
                    const float overlapLeft = (std::max)(frameX, tileX);
                    const float overlapTop = (std::max)(frameY, tileY);
                    const float overlapRight = (std::min)(frameX + frameWidth, tileX + tileSize);
                    const float overlapBottom = (std::min)(frameY + frameHeight, tileY + tileSize);
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
                    item.role = GetRoleFromTint(item.tintR, item.tintG, item.tintB);
                    item.layer = GetLayerFromTint(item.tintR, item.tintG, item.tintB);
                    scene.m_photo.capture.items.push_back(item);
                    capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
                    capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
                }
            }
    }

    static void FinalizeCapturedPhoto(GameScene& scene, Entity& player, float capturedMaxRight, float capturedMaxBottom)
    {
            scene.m_photo.capture.hasPhoto = true;
            scene.m_photo.capture.capturedTheme = scene.m_photo.capture.selectedTheme;
            scene.m_photo.placement.layer = PhotoCopyLayer::Foreground;
            scene.m_photo.placement.flipX = false;
            scene.m_photo.capture.width = (std::max)(1.0f, capturedMaxRight);
            scene.m_photo.capture.height = (std::max)(1.0f, capturedMaxBottom);
            scene.m_photo.capture.textureId = scene.m_photo.capture.items.front().textureId;
            scene.m_photo.capture.sourceX = scene.m_photo.capture.items.front().sourceX;
            scene.m_photo.capture.sourceY = scene.m_photo.capture.items.front().sourceY;
            scene.m_photo.capture.sourceWidth = scene.m_photo.capture.items.front().sourceWidth;
            scene.m_photo.capture.sourceHeight = scene.m_photo.capture.items.front().sourceHeight;
            scene.m_photo.capture.tintR = scene.m_photo.capture.items.front().tintR;
            scene.m_photo.capture.tintG = scene.m_photo.capture.items.front().tintG;
            scene.m_photo.capture.tintB = scene.m_photo.capture.items.front().tintB;
            scene.m_photo.capture.tintA = scene.m_photo.capture.items.front().tintA;

            scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "scene_change", 0.0f, 0.0f });
            scene.m_eventBus.Publish({ EventType::LogMessage, &player, nullptr, GetPhotoCaptureLogMessage(scene.m_photo.capture.capturedTheme), 0.0f, 0.0f });
            scene.m_shutterFlashRemaining = gShutterFlashSeconds;
            scene.m_developedPhotoPreviewRemaining = 3.2f;
    }

    static bool UpdatePlacementPreview(GameScene& scene, float& spawnX, float& spawnY, float& spawnWidth, float& spawnHeight)
    {
            spawnWidth = GetPrintedPhotoWidth(scene.m_photo.capture.width);
            spawnHeight = GetPrintedPhotoHeight(scene.m_photo.capture.height);
            const float viewScale = GetViewScale();
            const float viewOriginX = GetViewOriginX();
            const float viewOriginY = GetViewOriginY();
            const float cursorWorldX =
                ((static_cast<float>(Input_GetMouseX()) - viewOriginX) / viewScale) + scene.m_cameraX;
            const float cursorWorldY =
                (static_cast<float>(Input_GetMouseY()) - viewOriginY) / viewScale;

            const float mapWidth = scene.GetMapPixelWidth();
            const float mapHeight = scene.GetMapPixelHeight();
            spawnX = std::clamp(cursorWorldX - spawnWidth * 0.5f, 0.0f, std::max(0.0f, mapWidth - spawnWidth));
            spawnY = std::clamp(cursorWorldY - spawnHeight * 0.5f, 0.0f, std::max(0.0f, mapHeight - spawnHeight));

            scene.m_photo.placement.active = true;
            scene.m_photo.placement.x = spawnX;
            scene.m_photo.placement.y = spawnY;
            scene.m_photo.placement.width = spawnWidth;
            scene.m_photo.placement.height = spawnHeight;
            scene.m_photo.placement.valid = scene.IsPhotoPlacementValid(spawnX, spawnY, spawnWidth, spawnHeight);
            return scene.m_photo.placement.valid && Input_IsMouseLeftPressed();
    }

    static void SpawnPhotoGroup(GameScene& scene, Entity& player, float spawnX, float spawnY, float spawnWidth)
    {
            std::vector<CapturedPhotoItem> spawnedItems = BuildPrintedPhotoItems(
                scene.m_photo.capture.items,
                scene.m_whiteTexture,
                scene.m_photo.capture.capturedTheme,
                scene.m_photo.capture.width,
                scene.m_photo.capture.height,
                scene.m_photo.placement.flipX,
                scene.m_photo.placement.bridgeEnabled);

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
            Entity* lastSpawnedBox = nullptr;
            for (const auto& item : spawnedItems)
            {
                auto entity = std::make_unique<Entity>();
                lastSpawnedBox = entity.get();
                lastSpawnedBox->AddComponent<TagComponent>("PhotoBox");
                lastSpawnedBox->AddComponent<PhotoCopyGroupComponent>(groupId);
                lastSpawnedBox->AddComponent<PhotoCopyLifetimeComponent>(kPhotoCopyLifetimeSeconds);
                lastSpawnedBox->AddComponent<PhotoCopyRoleComponent>(item.role);
                lastSpawnedBox->AddComponent<PhotoCopyOriginComponent>(item.origin);
                lastSpawnedBox->AddComponent<PhotoCopyEffectComponent>(item.appliedTheme);
                const PhotoCopyLayer spawnedLayer =
                    item.layer == PhotoCopyLayer::Shadow ? PhotoCopyLayer::Shadow :
                    item.layer == PhotoCopyLayer::Background ? PhotoCopyLayer::Background :
                    scene.m_photo.placement.layer;
                lastSpawnedBox->AddComponent<PhotoCopyLayerComponent>(spawnedLayer);
                lastSpawnedBox->AddComponent<TransformComponent>(
                    spawnX + item.relativeX,
                    spawnY + item.relativeY,
                    item.width,
                    item.height);
                lastSpawnedBox->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
                lastSpawnedBox->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
                if (auto* sprite = lastSpawnedBox->GetComponent<SpriteRenderComponent>())
                {
                    sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                    sprite->SetFlipX(item.flipX);
                }
                ApplyPhotoFilterToPhotoBox(*lastSpawnedBox, item.appliedTheme);
                scene.m_entities.push_back(std::move(entity));
            }

            scene.m_photo.groups.activeGroupCount = std::min(kMaxPhotoGroups, scene.m_photo.groups.activeGroupCount + 1);
            scene.m_photo.groups.hasSpawnedCopy = true;
            scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, lastSpawnedBox, "test_tone", 0.0f, 0.0f });
            scene.m_eventBus.Publish({ EventType::LogMessage, &player, lastSpawnedBox, "Spawned filtered reconstruction", 0.0f, 0.0f });
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
