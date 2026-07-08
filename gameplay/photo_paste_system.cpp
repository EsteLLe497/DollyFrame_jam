#include "pch.h"

#include "photo_paste_system.h"

#include "game_scene_internal.h"
#include "game_scene_photo_storage_layout.h"
#include "imgui_layer.h"
#include "photo_filter_rules.h"
#include "photo_shared.h"
#include "DxLib.h"
#include <cmath>

using namespace game_scene_detail;

namespace
{
    constexpr int kMaxPhotoGroups = 3;
    constexpr float kArchetypePhotoFrameLifetimeSeconds = 0.45f;
    constexpr float kPadDeadZone = 0.18f;
    constexpr float kPadCursorMaxSpeed = 920.0f;
    constexpr float kPadCursorResponse = 14.0f;
    constexpr float kPadCursorDamping = 10.0f;
    constexpr float kPadCursorMouseReturnDelay = 0.28f;
    constexpr float kPlacementInvalidFlashSeconds = 0.22f;
    constexpr float kPlacementConfirmFlashSeconds = 0.14f;
    constexpr float kValidPreviewPulseHz = 2.2f;
    constexpr float kValidPreviewOutlineMin = 1.5f;
    constexpr float kValidPreviewOutlineMax = 2.2f;
    constexpr float kValidPreviewTintAlphaMin = 0.46f;
    constexpr float kValidPreviewTintAlphaMax = 0.62f;

    void BuildRotatedRect(
        float left,
        float top,
        float width,
        float height,
        float rotation,
        float& ax,
        float& ay,
        float& bx,
        float& by,
        float& cx,
        float& cy,
        float& dx,
        float& dy)
    {
        const float centerX = left + width * 0.5f;
        const float centerY = top + height * 0.5f;
        ax = left;
        ay = top;
        bx = left + width;
        by = top;
        cx = left + width;
        cy = top + height;
        dx = left;
        dy = top + height;
        RotatePoint(centerX, centerY, rotation, ax, ay);
        RotatePoint(centerX, centerY, rotation, bx, by);
        RotatePoint(centerX, centerY, rotation, cx, cy);
        RotatePoint(centerX, centerY, rotation, dx, dy);
    }

    void DrawRotatedPlacementRect(
        float left,
        float top,
        float width,
        float height,
        float rotation,
        unsigned int fillColor,
        unsigned int outlineColor,
        bool filled)
    {
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        float cx = 0.0f;
        float cy = 0.0f;
        float dx = 0.0f;
        float dy = 0.0f;
        BuildRotatedRect(left, top, width, height, rotation, ax, ay, bx, by, cx, cy, dx, dy);
        if (filled)
        {
            DrawQuadrangleAA(ax, ay, bx, by, cx, cy, dx, dy, fillColor, TRUE);
        }
        DrawQuadrangleAA(ax, ay, bx, by, cx, cy, dx, dy, outlineColor, FALSE);
    }

    bool IsPrintedPolaroidPreview(const std::vector<CapturedPhotoItem>& items)
    {
        return items.size() >= 2 &&
            items[0].layer == PhotoCopyLayer::Background &&
            items[1].layer == PhotoCopyLayer::Background &&
            items[0].origin == PhotoCopyOrigin::Generic &&
            items[1].origin == PhotoCopyOrigin::Tile;
    }

    void DrawPlacementPhotoFrameTexture(
        int frameTexture,
        float left,
        float top,
        float width,
        float height,
        bool valid,
        float alpha,
        float rotation)
    {
        if (frameTexture < 0)
        {
            DrawRotatedPlacementRect(
                left,
                top,
                width,
                height,
                rotation,
                valid ? GetColor(244, 242, 234) : GetColor(236, 120, 120),
                valid ? GetColor(222, 214, 196) : GetColor(255, 72, 72),
                true);
            return;
        }

        Shader_ResetStyle();
        Shader_SetTint(1.0f, valid ? 1.0f : 0.28f, valid ? 1.0f : 0.28f, alpha);
        SpriteDraw(
            frameTexture,
            left,
            top,
            width,
            height,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            false,
            rotation);
        Shader_ResetStyle();
    }

    void DrawPlacementPhotoFilmTexture(
        int filmTexture,
        float left,
        float top,
        float width,
        float height,
        bool valid,
        float alpha,
        float rotation)
    {
        if (filmTexture < 0)
        {
            return;
        }

        Shader_ResetStyle();
        Shader_SetTint(1.0f, valid ? 1.0f : 0.16f, valid ? 1.0f : 0.16f, alpha);
        SpriteDraw(
            filmTexture,
            left,
            top,
            width,
            height,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            false,
            rotation);
        Shader_ResetStyle();
    }

    int GetPlacementPreviewRenderTarget(int width, int height)
    {
        struct RenderTargetState
        {
            int handle = -1;
            int width = 0;
            int height = 0;
        };

        static RenderTargetState s_state;
        if (width <= 0 || height <= 0)
        {
            return -1;
        }

        if (s_state.handle >= 0 && (s_state.width != width || s_state.height != height))
        {
            DeleteGraph(s_state.handle);
            s_state.handle = -1;
            s_state.width = 0;
            s_state.height = 0;
        }

        if (s_state.handle < 0)
        {
            s_state.handle = MakeScreen(width, height, TRUE);
            s_state.width = width;
            s_state.height = height;
        }

        return s_state.handle;
    }

    constexpr float kPlacementQuarterTurn = 1.5707963268f;
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

    LaserTurretFireDirection GetLaserTurretFireDirectionFromRotation(float rotation)
    {
        constexpr float kQuarterTurn = 1.5707963268f;
        const float normalized = NormalizeAngleRadians(rotation);
        const int quadrant = static_cast<int>(std::floor((normalized + kQuarterTurn * 0.5f) / kQuarterTurn)) & 3;
        switch (quadrant)
        {
        case 0:
            return LaserTurretFireDirection::Right;
        case 1:
            return LaserTurretFireDirection::Down;
        case 2:
            return LaserTurretFireDirection::Left;
        default:
            return LaserTurretFireDirection::Up;
        }
    }

    int GetTileTextureForPaste(int tileValue, int defaultTexture, int tileTexture2, int tileTexture3, int tileTexture4)
    {
        // Keep pasted tile visuals in sync with the TileMap renderer's special tile textures.
        if (tileValue == 2 && tileTexture2 >= 0)
        {
            return tileTexture2;
        }
        if (tileValue == 3 && tileTexture3 >= 0)
        {
            return tileTexture3;
        }
        if (tileValue == 4 && tileTexture4 >= 0)
        {
            return tileTexture4;
        }
        return defaultTexture;
    }

    int GetPhotoItemTextureForPaste(
        const CapturedPhotoItem& item,
        int defaultTexture,
        int tileTexture2,
        int tileTexture3,
        int tileTexture4,
        int sepiaGroundTexture)
    {
        if (item.sourceTileValue > 0)
        {
            return GetTileTextureForPaste(item.sourceTileValue, defaultTexture, tileTexture2, tileTexture3, tileTexture4);
        }
        if (item.spawnArchetype == CapturedSpawnArchetype::SepiaGround &&
            !item.sepiaRestoredMarkerObject &&
            sepiaGroundTexture >= 0)
        {
            return sepiaGroundTexture;
        }
        if (item.sepiaRestoredTileValue > 0)
        {
            return GetTileTextureForPaste(item.sepiaRestoredTileValue, defaultTexture, tileTexture2, tileTexture3, tileTexture4);
        }
        return item.textureId >= 0 ? item.textureId : defaultTexture;
    }

    bool IsHazardTileValue(int tileValue)
    {
        return tileValue == 4 || tileValue == TileMap::kPitTileValue;
    }

    bool ShouldPasteAsSolidEnvironment(const CapturedPhotoItem& item)
    {
        // H marker spikes and hazard tiles must keep their damage role after paste.
        if (item.spikeStripTileSpan > 0)
        {
            return false;
        }
        if (item.sourceTileValue > 0)
        {
            return !IsHazardTileValue(item.sourceTileValue);
        }
        if (item.sepiaRestoredTileValue > 0)
        {
            return !IsHazardTileValue(item.sepiaRestoredTileValue);
        }
        return item.damagePlatformTileSpan > 0 || item.vanishOnCapture;
    }

    bool ShouldSplitSepiaGroundIntoCells(const CapturedPhotoItem& item, float tileSize)
    {
        if (tileSize <= 0.0f ||
            item.spawnArchetype != CapturedSpawnArchetype::SepiaGround ||
            (item.width <= tileSize + 0.01f && item.height <= tileSize + 0.01f))
        {
            return false;
        }

        // In the current capture spec, restored marker '+' is the only marker2 object
        // that reaches this SepiaGround path.
        return true;
    }

    std::vector<CapturedPhotoItem> BuildSepiaGroundPlacementPreviewCells(
        const std::vector<CapturedPhotoItem>& items,
        float tileSize)
    {
        if (tileSize <= 0.0f)
        {
            return items;
        }

        std::vector<CapturedPhotoItem> result;
        result.reserve(items.size());
        for (const auto& item : items)
        {
            if (!ShouldSplitSepiaGroundIntoCells(item, tileSize))
            {
                result.push_back(item);
                continue;
            }

            for (float offsetY = 0.0f; offsetY < item.height - 0.001f; offsetY += tileSize)
            {
                const float pieceHeight = (std::min)(tileSize, item.height - offsetY);
                if (pieceHeight <= 0.0f)
                {
                    continue;
                }

                for (float offsetX = 0.0f; offsetX < item.width - 0.001f; offsetX += tileSize)
                {
                    const float pieceWidth = (std::min)(tileSize, item.width - offsetX);
                    if (pieceWidth <= 0.0f)
                    {
                        continue;
                    }

                    CapturedPhotoItem piece = item;
                    piece.relativeX = item.relativeX + offsetX;
                    piece.relativeY = item.relativeY + offsetY;
                    piece.width = pieceWidth;
                    piece.height = pieceHeight;
                    piece.sourceX = 0.0f;
                    piece.sourceY = 0.0f;
                    piece.sourceWidth = pieceWidth / tileSize;
                    piece.sourceHeight = pieceHeight / tileSize;
                    result.push_back(piece);
                }
            }
        }

        return result;
    }

    std::unique_ptr<Entity> CreateSepiaGroundPhotoBox(
        const CapturedPhotoItem& item,
        int groupId,
        int pasteOrder,
        int textureId,
        float x,
        float y,
        float width,
        float height,
        float sourceX,
        float sourceY,
        float sourceWidth,
        float sourceHeight)
    {
        auto groundEntity = std::make_unique<Entity>();
        Entity* spawnedGround = groundEntity.get();

        spawnedGround->AddComponent<TagComponent>(kTagPhotoBox);
        spawnedGround->AddComponent<PhotoCopyGroupComponent>(groupId);
        spawnedGround->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
        spawnedGround->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
        const PhotoCopyRole pastedRole = ShouldPasteAsSolidEnvironment(item)
            ? PhotoCopyRole::Solid
            : item.role;
        spawnedGround->AddComponent<PhotoCopyRoleComponent>(pastedRole);
        spawnedGround->AddComponent<PhotoCopyLayerComponent>(item.layer);
        spawnedGround->AddComponent<PhotoCopyOriginComponent>(PhotoCopyOrigin::Generic);
        spawnedGround->AddComponent<PhotoCopyEffectComponent>(item.appliedTheme);
        spawnedGround->AddComponent<PhotoCopyLifetimeComponent>(gPastedObjectLifetimeSeconds);

        spawnedGround->AddComponent<TransformComponent>(x, y, width, height);
        spawnedGround->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
        spawnedGround->AddComponent<SpriteRenderComponent>(textureId);
        if (auto* sprite = spawnedGround->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetSourceRect(sourceX, sourceY, sourceWidth, sourceHeight);
            sprite->SetFlipX(item.flipX);
        }
        if (item.sepiaRestoredTileValue > 0)
        {
            spawnedGround->AddComponent<PhotoCopyTileValueComponent>(item.sepiaRestoredTileValue);
        }
        if (auto* transform = spawnedGround->GetComponent<TransformComponent>())
        {
            transform->rotation = item.rotation;
        }

        return groundEntity;
    }

    void UpdatePlacementPadCursor(
        float mouseWorldX,
        float mouseWorldY,
        bool mouseMoved,
        float rightX,
        float rightY,
        float dt,
        float& cursorWorldX,
        float& cursorWorldY,
        float& velocityX,
        float& velocityY,
        float& lastPadInputSeconds,
        float nowSeconds)
    {
        if (mouseMoved)
        {
            cursorWorldX = mouseWorldX;
            cursorWorldY = mouseWorldY;
            velocityX = 0.0f;
            velocityY = 0.0f;
            lastPadInputSeconds = -1000.0f;
            return;
        }

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
            velocityX += (desiredVelocityX - velocityX) * response;
            velocityY += (desiredVelocityY - velocityY) * response;
            lastPadInputSeconds = nowSeconds;
        }
        else
        {
            const float damping = std::max(0.0f, 1.0f - dt * kPadCursorDamping);
            velocityX *= damping;
            velocityY *= damping;

            if (nowSeconds - lastPadInputSeconds >= kPadCursorMouseReturnDelay)
            {
                const float returnFactor = std::min(1.0f, dt * 6.0f);
                cursorWorldX += (mouseWorldX - cursorWorldX) * returnFactor;
                cursorWorldY += (mouseWorldY - cursorWorldY) * returnFactor;
            }
        }

        cursorWorldX += velocityX * dt;
        cursorWorldY += velocityY * dt;
    }

}

int PhotoPasteSystem::GetPhotoTraySlotAt(const GameScene& scene, float screenX, float screenY)
{
    if (scene.m_ui.photoTrayReveal <= scene.m_ui.tuning.photoTray.revealThreshold)
    {
        return -1;
    }

    const int unlockedSlotCount = std::clamp(
        GameSession_Get().photoStorageSlots,
        0,
        3);
    for (int slotIndex = 0; slotIndex < unlockedSlotCount; ++slotIndex)
    {
        const UiLayoutRect slot = MakePhotoTraySlotRect(scene.m_ui.tuning, slotIndex);
        if (IsPointInRect(screenX, screenY, slot))
        {
            return slotIndex;
        }
    }

    return -1;
}

void PhotoPasteSystem::BeginPhotoPlacement(GameScene& scene, bool draggingFromTray)
{
    scene.m_flow.cameraMode = false;
    scene.m_player.captureAnimationActive = false;
    scene.m_player.captureAnimationReleased = false;
    scene.m_player.pasteAnimationActive = true;
    scene.m_player.pasteAnimationReleased = false;
    scene.m_player.pasteAnimationEnemyAttack = false;
    scene.m_player.afterimages.clear();
    scene.m_photo.placement.active = true;
    scene.m_photo.placement.draggingFromTray = draggingFromTray;
    scene.m_photo.placement.valid = false;
    scene.m_photo.placement.blockedByUi = false;
    ++scene.m_photo.placement.sessionId;
}

void PhotoPasteSystem::CancelPhotoPlacement(GameScene& scene)
{
    scene.m_player.pasteAnimationActive = false;
    scene.m_player.pasteAnimationReleased = false;
    scene.m_player.pasteAnimationEnemyAttack = false;
    scene.m_photo.placement.active = false;
    scene.m_photo.placement.valid = false;
    scene.m_photo.placement.blockedByUi = false;
    scene.m_photo.placement.draggingFromTray = false;
}

void PhotoPasteSystem::HandleSpawn(GameScene& scene)
{
    scene.m_photo.placement.valid = false;
    if (ImGuiLayer_WantsCaptureMouse())
    {
        return;
    }

    static bool previousRightDown = false;
    const bool rightDown = Input_IsKeyDown(VK_RBUTTON);
    const bool rightPressed = rightDown && !previousRightDown;
    const bool rightReleased = !rightDown && previousRightDown;
    previousRightDown = rightDown;

    const bool pasteReleasePlaying = scene.m_player.pasteAnimationActive && scene.m_player.pasteAnimationReleased;
    if (pasteReleasePlaying)
    {
        scene.m_photo.placement.active = false;
        scene.m_photo.placement.blockedByUi = false;
        scene.m_photo.placement.draggingFromTray = false;
        return;
    }

    if (!scene.m_photo.placement.active && rightPressed)
    {
        if (scene.m_photo.capture.containsEnemyAttackPaste)
        {
            return;
        }

        if (!scene.m_photo.capture.hasPhoto &&
            scene.m_photo.selectedCaptureSlot >= 0 &&
            scene.m_photo.selectedCaptureSlot < static_cast<int>(scene.m_photo.savedCaptures.size()) &&
            scene.m_photo.savedCaptures[scene.m_photo.selectedCaptureSlot].hasPhoto)
        {
            scene.SetSelectedPhotoSlot(scene.m_photo.selectedCaptureSlot);
        }

        if (scene.m_photo.capture.hasPhoto)
        {
            BeginPhotoPlacement(scene, false);
        }
    }

    if (!scene.m_photo.capture.hasPhoto || !scene.m_photo.placement.active)
    {
        scene.m_photo.placement.blockedByUi = false;
        scene.m_photo.placement.draggingFromTray = false;
        return;
    }

    if (Input_IsActionPressed(InputAction::Cancel) ||
        (!rightDown && !rightReleased))
    {
        CancelPhotoPlacement(scene);
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
    if (rightDown && Input_IsMouseLeftPressed())
    {
        scene.m_photo.placement.rotation += kPlacementQuarterTurn;
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
    const bool confirmDrop = rightReleased;
    if (!UpdatePlacementPreview(scene, confirmDrop, spawnX, spawnY, spawnWidth, spawnHeight))
    {
        if (confirmDrop)
        {
            CancelPhotoPlacement(scene);
        }
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

    const float viewScale = scene.GetViewScale();
    const float viewOriginX = scene.GetViewOriginX();
    const float viewOriginY = scene.GetViewOriginY();
    float previewWidth = 0.0f;
    float previewHeight = 0.0f;
    std::vector<CapturedPhotoItem> previewItems = photo_shared::BuildPlacementItems(
        scene.m_photo.capture,
        scene.m_photo.placement,
        scene.m_whiteTexture,
        previewWidth,
        previewHeight);
    previewItems = BuildSepiaGroundPlacementPreviewCells(previewItems, scene.m_tileMap.GetTileSize());
    PhotoPlacementState basePlacement = scene.m_photo.placement;
    basePlacement.rotation = 0.0f;
    float basePreviewWidth = 0.0f;
    float basePreviewHeight = 0.0f;
    std::vector<CapturedPhotoItem> basePreviewItems = photo_shared::BuildPlacementItems(
        scene.m_photo.capture,
        basePlacement,
        scene.m_whiteTexture,
        basePreviewWidth,
        basePreviewHeight);
    basePreviewItems = BuildSepiaGroundPlacementPreviewCells(basePreviewItems, scene.m_tileMap.GetTileSize());
    const bool pulseEnabled = scene.m_debug.effectPlacementPulseEnabled;
    const float timeSeconds = static_cast<float>(GetNowCount()) / 1000.0f;
    const float pulse01 = pulseEnabled ? (0.5f + 0.5f * std::sin(timeSeconds * 6.2831853072f * kValidPreviewPulseHz)) : 0.5f;
    const float validOutlineThickness = pulseEnabled
        ? (kValidPreviewOutlineMin + (kValidPreviewOutlineMax - kValidPreviewOutlineMin) * pulse01)
        : 1.8f;
    const float validTintAlpha = pulseEnabled
        ? (kValidPreviewTintAlphaMin + (kValidPreviewTintAlphaMax - kValidPreviewTintAlphaMin) * pulse01)
        : 0.55f;
    const float contentWidth = basePreviewWidth * viewScale;
    const float contentHeight = basePreviewHeight * viewScale;
    const float outerX = viewOriginX + (scene.m_photo.placement.x - scene.m_flow.cameraX) * viewScale;
    const float outerY = viewOriginY + (scene.m_photo.placement.y - scene.m_flow.cameraY) * viewScale;
    const float frameCenterX = outerX + previewWidth * viewScale * 0.5f;
    const float frameCenterY = outerY + previewHeight * viewScale * 0.5f;
    const float framePad = std::max(8.0f, 10.0f * viewScale);
    const float filmPad = std::max(6.0f, 7.0f * viewScale);
    const float paperWidth = contentWidth + framePad * 2.0f;
    const float paperHeight = contentHeight + framePad * 2.0f;
    const float paperLeft = frameCenterX - paperWidth * 0.5f;
    const float paperTop = frameCenterY - paperHeight * 0.5f;
    const float filmWidth = contentWidth + filmPad * 2.0f;
    const float filmHeight = contentHeight + filmPad * 2.0f;
    const float filmLeft = frameCenterX - filmWidth * 0.5f;
    const float filmTop = frameCenterY - filmHeight * 0.5f;
    const int placementFrameTexture = scene.m_assets.GetTexture("ui_photo_frame");
    const int placementFilmTexture = scene.m_assets.GetTexture("ui_photo_frame_film_brown");
    const bool usePolaroidComposite = IsPrintedPolaroidPreview(basePreviewItems);
    RECT previousDrawArea{};
    GetDrawArea(&previousDrawArea);

    const int canvasWidth = std::max(1, static_cast<int>(std::ceil(paperWidth)));
    const int canvasHeight = std::max(1, static_cast<int>(std::ceil(paperHeight)));
    const int renderTarget = GetPlacementPreviewRenderTarget(canvasWidth, canvasHeight);
    const int previousDrawScreen = GetDrawScreen();

    if (renderTarget >= 0)
    {
            const float contentOffsetX = (paperWidth - contentWidth) * 0.5f;
            const float contentOffsetY = (paperHeight - contentHeight) * 0.5f;
            const float filmOffsetX = (paperWidth - filmWidth) * 0.5f;
            const float filmOffsetY = (paperHeight - filmHeight) * 0.5f;
            const float filmRight = filmOffsetX + filmWidth;
            const float filmBottom = filmOffsetY + filmHeight;
            const size_t firstItemIndex = usePolaroidComposite ? 2u : 0u;

            SetDrawScreen(renderTarget);
            SetDrawArea(0, 0, canvasWidth, canvasHeight);
            ClearDrawScreen();

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, scene.m_photo.placement.valid ? 188 : 170);
            DrawPlacementPhotoFrameTexture(
                placementFrameTexture,
                0.0f,
                0.0f,
                static_cast<float>(canvasWidth),
                static_cast<float>(canvasHeight),
                scene.m_photo.placement.valid,
                scene.m_photo.placement.valid ? 0.88f : 0.94f,
                0.0f);

            SetDrawArea(
                static_cast<int>(std::floor(filmOffsetX)),
                static_cast<int>(std::floor(filmOffsetY)),
                static_cast<int>(std::ceil(filmRight)),
                static_cast<int>(std::ceil(filmBottom)));

            for (size_t index = firstItemIndex; index < basePreviewItems.size(); ++index)
            {
                const auto& item = basePreviewItems[index];
                CapturedPhotoItem previewItem = item;
                photo_shared::ApplyPreviewFilterTheme(previewItem);
                if (previewItem.spawnArchetype == CapturedSpawnArchetype::SepiaGround &&
                    !previewItem.sepiaRestoredMarkerObject)
                {
                    const int sepiaGroundTexture = scene.m_assets.GetTexture("sepia_rubble_stage");
                    if (sepiaGroundTexture >= 0)
                    {
                        previewItem.textureId = sepiaGroundTexture;
                        previewItem.tintR = 1.0f;
                        previewItem.tintG = 1.0f;
                        previewItem.tintB = 1.0f;
                        previewItem.tintA = 1.0f;
                    }
                }

                const float drawX = contentOffsetX + item.relativeX * viewScale;
                const float drawY = contentOffsetY + item.relativeY * viewScale;
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

                const float previewAlpha = item.spawnArchetype == CapturedSpawnArchetype::Log
                    ? 1.0f
                    : (scene.m_photo.placement.valid ? 0.55f : 0.42f);
                photo_shared::DrawCapturedPhotoItem(
                    scene.m_tileTexture,
                    previewItem,
                    drawX,
                    drawY,
                    drawWidth,
                    drawHeight,
                    previewAlpha);

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

            SetDrawArea(0, 0, canvasWidth, canvasHeight);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, scene.m_photo.placement.valid ? 255 : 235);
            DrawPlacementPhotoFilmTexture(
                placementFilmTexture,
                0.0f,
                0.0f,
                static_cast<float>(canvasWidth),
                static_cast<float>(canvasHeight),
                scene.m_photo.placement.valid,
                1.0f,
                0.0f);

            SetDrawArea(
                previousDrawArea.left,
                previousDrawArea.top,
                previousDrawArea.right,
                previousDrawArea.bottom);
            SetDrawScreen(previousDrawScreen);

            DrawRotaGraph3F(
                frameCenterX,
                frameCenterY,
                canvasWidth * 0.5f,
                canvasHeight * 0.5f,
                1.0,
                1.0,
                static_cast<double>(scene.m_photo.placement.rotation),
                renderTarget,
                TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    else
    {
        SetDrawScreen(previousDrawScreen);
        SetDrawArea(
            previousDrawArea.left,
            previousDrawArea.top,
            previousDrawArea.right,
            previousDrawArea.bottom);

        // Fallback path if the offscreen preview target cannot be created.
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, scene.m_photo.placement.valid ? 188 : 170);
        DrawPlacementPhotoFrameTexture(
            placementFrameTexture,
            paperLeft,
            paperTop,
            paperWidth,
            paperHeight,
            scene.m_photo.placement.valid,
            scene.m_photo.placement.valid ? 0.88f : 0.94f,
            scene.m_photo.placement.rotation);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

            SetDrawArea(
                static_cast<int>(std::floor(filmLeft)),
                static_cast<int>(std::floor(filmTop)),
                static_cast<int>(std::ceil(filmLeft + filmWidth)),
                static_cast<int>(std::ceil(filmTop + filmHeight)));

        for (const auto& item : previewItems)
        {
            CapturedPhotoItem previewItem = item;
            photo_shared::ApplyPreviewFilterTheme(previewItem);
            if (previewItem.spawnArchetype == CapturedSpawnArchetype::SepiaGround &&
                !previewItem.sepiaRestoredMarkerObject)
            {
                const int sepiaGroundTexture = scene.m_assets.GetTexture("sepia_rubble_stage");
                if (sepiaGroundTexture >= 0)
                {
                    previewItem.textureId = sepiaGroundTexture;
                    previewItem.tintR = 1.0f;
                    previewItem.tintG = 1.0f;
                    previewItem.tintB = 1.0f;
                    previewItem.tintA = 1.0f;
                }
            }
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

            const float previewAlpha = item.spawnArchetype == CapturedSpawnArchetype::Log
                ? 1.0f
                : (scene.m_photo.placement.valid ? 0.55f : 0.42f);
            photo_shared::DrawCapturedPhotoItem(
                scene.m_tileTexture,
                previewItem,
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                previewAlpha);

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

        SetDrawArea(
            previousDrawArea.left,
            previousDrawArea.top,
            previousDrawArea.right,
            previousDrawArea.bottom);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, scene.m_photo.placement.valid ? 255 : 235);
        DrawPlacementPhotoFilmTexture(
            placementFilmTexture,
            paperLeft,
            paperTop,
            paperWidth,
            paperHeight,
            scene.m_photo.placement.valid,
            1.0f,
            scene.m_photo.placement.rotation);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        SetDrawArea(
            previousDrawArea.left,
            previousDrawArea.top,
            previousDrawArea.right,
            previousDrawArea.bottom);
    }

    if (scene.m_photo.placement.valid && pulseEnabled)
    {
        const int pad = std::max(2, static_cast<int>(std::round(2.0f + pulse01 * 3.0f)));
        const int alpha = static_cast<int>(std::round(120.0f + pulse01 * 95.0f));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawBox(
            static_cast<int>(std::round(frameCenterX - paperWidth * 0.5f)) - pad,
            static_cast<int>(std::round(frameCenterY - paperHeight * 0.5f)) - pad,
            static_cast<int>(std::round(frameCenterX + paperWidth * 0.5f)) + pad,
            static_cast<int>(std::round(frameCenterY + paperHeight * 0.5f)) + pad,
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
        "Solid in world  Groups:%d/3  Rot:%.0f  Keys:F/B RMB+LMB Esc:Cancel",
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
    const char* statusText = scene.m_photo.placement.draggingFromTray ? "Drop: release LMB" : "Place: release RMB";
    if (!scene.m_photo.placement.draggingFromTray)
    {
        statusText = "Rotate: RMB + LMB / Place: release RMB";
    }
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

    if (scene.m_photo.placement.confirmFlashRemaining > 0.0f)
    {
        const float flashT = Clamp01(scene.m_photo.placement.confirmFlashRemaining / kPlacementConfirmFlashSeconds);
        const float ease = flashT * flashT * (3.0f - 2.0f * flashT);
        const int flashAlpha = static_cast<int>(std::round(220.0f * ease));
        const int glowAlpha = static_cast<int>(std::round(150.0f * ease));
        const float expand = (1.0f - ease) * std::max(8.0f, 12.0f * viewScale);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, glowAlpha);
        DrawRotatedPlacementRect(
            paperLeft - expand,
            paperTop - expand,
            paperWidth + expand * 2.0f,
            paperHeight + expand * 2.0f,
            scene.m_photo.placement.rotation,
            GetColor(255, 248, 228),
            GetColor(255, 248, 228),
            true);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, flashAlpha);
        DrawRotatedPlacementRect(
            paperLeft,
            paperTop,
            paperWidth,
            paperHeight,
            scene.m_photo.placement.rotation,
            GetColor(255, 255, 242),
            GetColor(255, 255, 242),
            false);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    Shader_ResetStyle();
}

bool PhotoPasteSystem::UpdatePlacementPreview(
    GameScene& scene,
    bool confirmDrop,
    float& spawnX,
    float& spawnY,
    float& spawnWidth,
    float& spawnHeight)
{
    float placementWidth = 0.0f;
    float placementHeight = 0.0f;
    static_cast<void>(photo_shared::BuildPlacementItems(
        scene.m_photo.capture,
        scene.m_photo.placement,
        scene.m_whiteTexture,
        placementWidth,
        placementHeight));
    spawnWidth = placementWidth;
    spawnHeight = placementHeight;
    // 入力と描画で同じ確定済みカメラを使い、動的ズーム中の配置ずれを防ぐ。
    scene.PrepareFrameRendering();
    const float viewScale = scene.GetViewScale();
    const float viewOriginX = scene.GetViewOriginX();
    const float viewOriginY = scene.GetViewOriginY();

    const float mapWidth = scene.GetMapPixelWidth();
    const float mapHeight = scene.GetMapPixelHeight();
    static float padCursorWorldX = 0.0f;
    static float padCursorWorldY = 0.0f;
    static float padCursorVelocityX = 0.0f;
    static float padCursorVelocityY = 0.0f;
    static unsigned int lastTimeMs = 0;
    static bool initialized = false;
    static int lastSessionId = -1;
    static float lastPadInputSeconds = -1000.0f;
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
        lastPadInputSeconds = -1000.0f;
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
    const float mouseWorldX = ((static_cast<float>(mouseX) - viewOriginX) / viewScale) + scene.m_flow.cameraX;
    const float mouseWorldY = ((static_cast<float>(mouseY) - viewOriginY) / viewScale) + scene.m_flow.cameraY;
    const float rightX = Input_GetRightStickX();
    const float rightY = Input_GetRightStickY();
    UpdatePlacementPadCursor(
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
    const bool blockedByTray = scene.IsPhotoTrayHit(cursorScreenX, cursorScreenY);
    scene.m_photo.placement.blockedByUi = blockedByTray;

    if (confirmDrop && (!scene.m_photo.placement.valid || blockedByTray))
    {
        scene.m_photo.placement.invalidFlashRemaining = kPlacementInvalidFlashSeconds;
        scene.m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "cant_paste", 0.0f, 0.0f });
        return false;
    }

    if (confirmDrop && scene.m_photo.placement.valid)
    {
        scene.m_player.captureAnimationActive = false;
        scene.m_player.captureAnimationReleased = false;
        scene.m_player.pasteAnimationEnemyAttack = scene.m_photo.capture.containsEnemyAttackPaste;
        scene.m_player.pasteAnimationReleased = true;
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
    std::vector<CapturedPhotoItem> spawnedItems = photo_shared::BuildPlacementItems(
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
        scene.m_world.EraseIf(
            [&](const std::unique_ptr<Entity>& entity)
            {
                if (!entity || !HasTag(*entity, "PhotoBox"))
                {
                    return false;
                }
                const auto* group = entity->GetComponent<PhotoCopyGroupComponent>();
                return group && group->groupId == groupToRemove;
            });
        scene.m_photo.groups.activeGroupCount = std::max(0, scene.m_photo.groups.activeGroupCount - 1);
    }

    const int groupId = scene.m_photo.groups.nextGroupId++;
    const int pasteOrder = scene.m_photo.groups.nextPasteOrder++;
    Entity* lastSpawnedEntity = nullptr;
    int spawnedPhotoBoxCount = 0;
    for (const auto& item : spawnedItems)
    {
        if (item.spawnArchetype == CapturedSpawnArchetype::Log)
        {
            auto logEntity = std::make_unique<Entity>();
            Entity* spawnedLog = logEntity.get();
            lastSpawnedEntity = spawnedLog;
            spawnedLog->AddComponent<TagComponent>(kTagLog);
            spawnedLog->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedLog->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            spawnedLog->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
            spawnedLog->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            spawnedLog->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
            if (!item.collisionOutline.empty())
            {
                std::vector<b2Vec2> normalizedOutline;
                normalizedOutline.reserve(item.collisionOutline.size());
                for (const auto& point : item.collisionOutline)
                {
                    normalizedOutline.push_back({ point.x, point.y });
                }
                spawnedLog->AddComponent<ImageOutlineColliderComponent>(std::move(normalizedOutline), 0.5f);
            }
            else
            {
                spawnedLog->AddComponent<ImageOutlineColliderComponent>(
                    std::vector<b2Vec2>{
                        { 0.0f, 0.0f },
                        { 1.0f, 0.0f },
                        { 1.0f, 1.0f },
                        { 0.0f, 1.0f }},
                    0.5f);
            }
            spawnedLog->AddComponent<BarrelComponent>(
                gBarrelGravity,
                gBarrelMaxFallSpeed,
                0.0f,
                0.0f,
                1,
                99999.0f,
                99999.0f);
            spawnedLog->AddComponent<PhotoCopyLifetimeComponent>(gPastedObjectLifetimeSeconds);
            spawnedLog->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
            if (auto* sprite = spawnedLog->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedLog->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            if (auto* barrel = spawnedLog->GetComponent<BarrelComponent>())
            {
                barrel->spawnX = spawnX + item.relativeX;
                barrel->spawnY = spawnY + item.relativeY;
                barrel->respawnEnabled = false;
                barrel->respawnWhenOffscreen = false;
                barrel->active = true;
            }
            scene.m_world.Spawn(std::move(logEntity));
            continue;
        }

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
            scene.m_world.Spawn(std::move(barrelEntity));
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::FallingRock)
        {
            auto fallingRockEntity = std::make_unique<Entity>();
            Entity* spawnedFallingRock = fallingRockEntity.get();
            lastSpawnedEntity = spawnedFallingRock;
            spawnedFallingRock->AddComponent<TagComponent>(kTagFallingRock);
            spawnedFallingRock->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedFallingRock->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            spawnedFallingRock->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
            spawnedFallingRock->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            spawnedFallingRock->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
            if (!item.collisionOutline.empty())
            {
                std::vector<b2Vec2> normalizedOutline;
                normalizedOutline.reserve(item.collisionOutline.size());
                for (const auto& point : item.collisionOutline)
                {
                    normalizedOutline.push_back({ point.x, point.y });
                }
                spawnedFallingRock->AddComponent<ImageOutlineColliderComponent>(
                    std::move(normalizedOutline),
                    0.5f);
            }
            else
            {
                spawnedFallingRock->AddComponent<ImageOutlineColliderComponent>(
                    std::vector<b2Vec2>{
                        { 0.0f, 0.0f },
                        { 1.0f, 0.0f },
                        { 1.0f, 1.0f },
                        { 0.0f, 1.0f }},
                    0.5f);
            }

            spawnedFallingRock->AddComponent<FallingRockComponent>(
                gBarrelGravity,
                gBarrelMaxFallSpeed,
                gBarrelRollSpeed,
                gBarrelGroundFriction,
                gBarrelContactDamage,
                gBarrelBreakMinFallDistance,
                gBarrelBreakMinImpactSpeed);
            spawnedFallingRock->AddComponent<PhotoCopyLifetimeComponent>(gPastedObjectLifetimeSeconds);
            spawnedFallingRock->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
            if (auto* sprite = spawnedFallingRock->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedFallingRock->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            if (auto* rock = spawnedFallingRock->GetComponent<FallingRockComponent>())
            {
                rock->spawnX = spawnX + item.relativeX;
                rock->spawnY = spawnY + item.relativeY;
                rock->respawnEnabled = false;
                rock->active = true;
            }
            scene.m_world.Spawn(std::move(fallingRockEntity));
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::Battery)
        {
            auto batteryEntity = std::make_unique<Entity>();
            Entity* spawnedBattery = batteryEntity.get();
            lastSpawnedEntity = spawnedBattery;
            spawnedBattery->AddComponent<TagComponent>(kTagBattery);
            spawnedBattery->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedBattery->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            spawnedBattery->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
            spawnedBattery->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            spawnedBattery->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
            spawnedBattery->AddComponent<BatteryComponent>(
                1900.0f,
                980.0f,
                260.0f,
                320.0f,
                1);
            spawnedBattery->AddComponent<PhotoCopyLifetimeComponent>(gPastedObjectLifetimeSeconds);
            spawnedBattery->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
            if (auto* sprite = spawnedBattery->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedBattery->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            scene.m_world.Spawn(std::move(batteryEntity));
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::Gear)
        {
            auto gearEntity = std::make_unique<Entity>();
            Entity* spawnedGear = gearEntity.get();
            lastSpawnedEntity = spawnedGear;
            spawnedGear->AddComponent<TagComponent>(kTagGear);
            spawnedGear->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedGear->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            spawnedGear->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
            spawnedGear->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            const int gearTexture = scene.m_assets.GetTexture("star");
            spawnedGear->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : (gearTexture >= 0 ? gearTexture : scene.m_whiteTexture));
            spawnedGear->AddComponent<GearComponent>(item.gearNo > 0 ? item.gearNo : 1, true);
            spawnedGear->AddComponent<PhotoCopyLifetimeComponent>(gPastedObjectLifetimeSeconds);
            spawnedGear->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
            if (auto* sprite = spawnedGear->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedGear->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            scene.m_world.Spawn(std::move(gearEntity));
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
            auto& projectile = spawnedBullet->AddComponent<ProjectileComponent>(item.projectileVelocityX, item.projectileVelocityY, item.projectileDamage, ProjectileComponent::Owner::Photo);
            if (item.spearProjectile)
            {
                auto& spear = spawnedBullet->AddComponent<MidBoss2SpearComponent>();
                spear.launched = true;
                spear.stuck = item.spearStuck;
                spear.directionX = item.spearDirectionX;
                spear.directionY = item.spearDirectionY;
                spear.targetDirectionX = item.spearDirectionX;
                spear.targetDirectionY = item.spearDirectionY;
                spear.launchDelay = 0.0f;
                spear.launchTimer = 0.0f;
                spear.fadeDuration = 1.0f;
                spear.fadeRemaining = spear.fadeDuration;
                spear.travelDistance = item.spearTravelDistance;
                if (spear.stuck)
                {
                    projectile.SetVelocityX(0.0f);
                    projectile.SetVelocityY(0.0f);
                }
                if (auto* transform = spawnedBullet->GetComponent<TransformComponent>())
                {
                    transform->rotation = std::atan2(spear.directionY, spear.directionX);
                }
            }
            if (auto* sprite = spawnedBullet->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedBullet->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            scene.m_world.Spawn(std::move(bulletEntity));
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::LaserTurret)
        {
            constexpr float kPastedBeamLifetimeSeconds = 3.0f;
            constexpr float kPastedBeamWarmupSeconds = 0.45f;
            constexpr float kPastedBeamKnockbackSpeed = 120.0f;
            const auto* playerTransform = player.GetComponent<TransformComponent>();
            const bool attackPaste = item.enemyAttackPaste && playerTransform != nullptr;
            const bool playerFacingRight = scene.m_player.facingRight;
            const auto fireDirection = attackPaste
                ? (playerFacingRight
                    ? LaserTurretFireDirection::Right
                    : LaserTurretFireDirection::Left)
                : GetLaserTurretFireDirectionFromRotation(item.rotation);
            const bool firesVertically = fireDirection == LaserTurretFireDirection::Up ||
                fireDirection == LaserTurretFireDirection::Down;

            auto turretEntity = std::make_unique<Entity>();
            Entity* spawnedTurret = turretEntity.get();
            lastSpawnedEntity = spawnedTurret;
            spawnedTurret->AddComponent<TagComponent>(kTagLaserTurret);
            spawnedTurret->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedTurret->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            float turretX = spawnX + item.relativeX;
            float turretY = spawnY + item.relativeY;
            if (attackPaste)
            {
                const float playerWidth = playerTransform->width * playerTransform->scale;
                const float playerHeight = playerTransform->height * playerTransform->scale;
                const float playerFrontX = playerFacingRight
                    ? (playerTransform->x + playerWidth)
                    : playerTransform->x;
                const float playerCenterY = playerTransform->y + playerHeight * 0.5f;
                turretY = playerCenterY - item.height * 0.5f;
                turretX = playerFacingRight
                    ? playerFrontX
                    : playerFrontX - item.width;
            }
            spawnedTurret->AddComponent<TransformComponent>(turretX, turretY, item.width, item.height);
            spawnedTurret->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            spawnedTurret->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
            const float pastedBeamThickness = item.laserBeamThickness > 0.0f ? item.laserBeamThickness : (item.height * 0.2f);
            auto& pastedTurret = spawnedTurret->AddComponent<LaserTurretComponent>(
                pastedBeamThickness,
                item.laserDamagePerSecond,
                firesVertically,
                fireDirection == LaserTurretFireDirection::Left,
                false);
            pastedTurret.fireDirection = fireDirection;
            pastedTurret.vertical = firesVertically;
            pastedTurret.shootsLeft = fireDirection == LaserTurretFireDirection::Left;
            pastedTurret.fireToLeft = fireDirection == LaserTurretFireDirection::Left;
            pastedTurret.warmupRemaining = kPastedBeamWarmupSeconds;
            pastedTurret.enemyKnockbackSpeed = item.laserEnemyKnockbackSpeed > 0.0f
                ? item.laserEnemyKnockbackSpeed
                : kPastedBeamKnockbackSpeed;
            spawnedTurret->AddComponent<PhotoCopyLifetimeComponent>(kPastedBeamLifetimeSeconds);
            spawnedTurret->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
            if (auto* sprite = spawnedTurret->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedTurret->GetComponent<TransformComponent>())
            {
                transform->rotation = item.rotation;
            }
            scene.m_world.Spawn(std::move(turretEntity));

            auto beamEntity = std::make_unique<Entity>();
            Entity* spawnedBeam = beamEntity.get();
            spawnedBeam->AddComponent<TagComponent>(kTagLaserBeam);
            spawnedBeam->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedBeam->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            float beamX = turretX;
            float beamY = turretY;
            float beamWidth = 0.0f;
            float beamHeight = 0.0f;
            if (fireDirection == LaserTurretFireDirection::Up)
            {
                beamX += std::max(0.0f, item.width * 0.5f - pastedBeamThickness * 0.5f);
                beamWidth = pastedBeamThickness;
            }
            else if (fireDirection == LaserTurretFireDirection::Down)
            {
                beamX += std::max(0.0f, item.width * 0.5f - pastedBeamThickness * 0.5f);
                beamY += item.height;
                beamWidth = pastedBeamThickness;
            }
            else if (fireDirection == LaserTurretFireDirection::Left)
            {
                beamY += std::max(0.0f, item.height * 0.5f - pastedBeamThickness * 0.5f);
                beamHeight = pastedBeamThickness;
            }
            else
            {
                beamX += item.width;
                beamY += std::max(0.0f, item.height * 0.5f - pastedBeamThickness * 0.5f);
                beamHeight = pastedBeamThickness;
            }
            spawnedBeam->AddComponent<TransformComponent>(beamX, beamY, beamWidth, beamHeight);
            spawnedBeam->AddComponent<TintComponent>(0.48f, 0.78f, 1.0f, 0.86f);
            spawnedBeam->AddComponent<SpriteRenderComponent>(scene.m_whiteTexture);
            spawnedBeam->AddComponent<LaserBeamComponent>(
                item.laserDamagePerSecond,
                item.laserEnemyKnockbackSpeed);
            auto& beamCapture = spawnedBeam->AddComponent<BossBeamCaptureComponent>();
            beamCapture.captureEnabled = false;
            beamCapture.sourceOnLeft = fireDirection != LaserTurretFireDirection::Left;
            beamCapture.visualLeakLength = 12.0f;
            spawnedBeam->AddComponent<PhotoCopyLifetimeComponent>(kPastedBeamLifetimeSeconds);
            pastedTurret.beamEntity = spawnedBeam;
            if (firesVertically)
            {
                pastedTurret.beamOriginOffsetX = std::max(0.0f, (item.width - pastedBeamThickness) * 0.5f);
                pastedTurret.beamOriginOffsetY = fireDirection == LaserTurretFireDirection::Up ? 0.0f : item.height;
            }
            else
            {
                pastedTurret.beamOriginOffsetX = fireDirection == LaserTurretFireDirection::Left ? 0.0f : item.width;
                pastedTurret.beamOriginOffsetY = item.height * 0.5f;
            }
            scene.m_world.Spawn(std::move(beamEntity));
            lastSpawnedEntity = spawnedBeam;
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::WalkerMelee)
        {
            constexpr float kWalkerMeleeWidth = 48.0f;
            constexpr float kWalkerMeleeHeight = 60.0f;
            constexpr float kWalkerMeleeDuration = 0.4f;

            auto meleeEntity = std::make_unique<Entity>();
            Entity* spawnedMelee = meleeEntity.get();
            lastSpawnedEntity = spawnedMelee;
            spawnedMelee->AddComponent<TagComponent>("WalkerMeleeAttack");
            spawnedMelee->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedMelee->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
            spawnedMelee->AddComponent<TransformComponent>(
                spawnX + item.relativeX,
                spawnY + item.relativeY,
                kWalkerMeleeWidth,
                kWalkerMeleeHeight);
            spawnedMelee->AddComponent<TintComponent>(1.0f, 0.55f, 0.15f, 0.72f);
            spawnedMelee->AddComponent<SpriteRenderComponent>(scene.m_whiteTexture);
            spawnedMelee->AddComponent<PhotoCopyLifetimeComponent>(kWalkerMeleeDuration);
            scene.m_world.Spawn(std::move(meleeEntity));
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::ShieldNormal ||
            item.spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst ||
            item.spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst)
        {
            constexpr float kTileSize = 48.0f;
            constexpr float kShieldRaiseOffsetY = kTileSize * 0.5f;
            constexpr float kBossRushSpeed = 520.0f;
            constexpr float kBossJumpDescendSpeed = 1200.0f;

            auto shieldEntity = std::make_unique<Entity>();
            Entity* spawnedShield = shieldEntity.get();
            lastSpawnedEntity = spawnedShield;
            spawnedShield->AddComponent<TagComponent>("CapturedShield");
            spawnedShield->AddComponent<PhotoCopyGroupComponent>(groupId);
            spawnedShield->AddComponent<PhotoPasteOrderComponent>(pasteOrder);

            float shieldX = spawnX + item.relativeX;
            float shieldY = spawnY + item.relativeY;
            float shieldW = item.width;
            float shieldH = item.height;
            const auto* playerTransform = player.GetComponent<TransformComponent>();
            const bool facingRight = scene.m_player.facingRight;

            if (item.spawnArchetype == CapturedSpawnArchetype::ShieldNormal)
            {
                const float centerX = shieldX + shieldW * 0.5f;
                const float centerY = shieldY + shieldH * 0.5f;
                shieldW = kTileSize;
                shieldH = kTileSize * 4.0f;
                shieldX = centerX - shieldW * 0.5f;
                shieldY = centerY - shieldH * 0.5f - kShieldRaiseOffsetY;
            }
            else if (item.spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst)
            {
                const float centerX = shieldX + shieldW * 0.5f;
                const float centerY = shieldY + shieldH * 0.5f;
                shieldW = kTileSize * 2.0f;
                shieldH = kTileSize * 4.0f;
                shieldX = centerX - shieldW * 0.5f;
                shieldY = centerY - shieldH * 0.5f - kShieldRaiseOffsetY;
            }
            else if (item.spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst)
            {
                const float centerX = shieldX + shieldW * 0.5f;
                const float centerY = shieldY + shieldH * 0.5f;
                shieldW = kTileSize * 3.0f;
                shieldH = kTileSize * 1.0f;
                shieldX = centerX - shieldW * 0.5f;
                shieldY = centerY - shieldH * 0.5f;
            }

            spawnedShield->AddComponent<TransformComponent>(shieldX, shieldY, shieldW, shieldH);
            spawnedShield->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
            spawnedShield->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : scene.m_tileTexture);
            auto& shieldComp = spawnedShield->AddComponent<ShieldComponent>();
            shieldComp.attached = false;
            shieldComp.photoSpawned = true;
            shieldComp.rotationSpeed = 0.0f;
            shieldComp.velocityX = 0.0f;
            shieldComp.velocityY = 0.0f;
            shieldComp.contactDamage = 1;
            shieldComp.knockbackGrids = 3.0f;
            shieldComp.grounded = false;
            shieldComp.shockwaveSpawned = false;
            if (auto* sprite = spawnedShield->GetComponent<SpriteRenderComponent>())
            {
                sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
                sprite->SetFlipX(item.flipX);
            }
            if (auto* transform = spawnedShield->GetComponent<TransformComponent>())
            {
                transform->rotation =
                    (item.spawnArchetype == CapturedSpawnArchetype::ShieldRushBurst ||
                        item.spawnArchetype == CapturedSpawnArchetype::ShieldJumpBurst)
                    ? 0.0f
                    : item.rotation;
            }

            switch (item.spawnArchetype)
            {
            case CapturedSpawnArchetype::ShieldNormal:
                shieldComp.capturedMode = CapturedShieldMode::Normal;
                shieldComp.gravityEnabled = true;
                shieldComp.contactDamage = 1;
                break;
            case CapturedSpawnArchetype::ShieldRushBurst:
                shieldComp.capturedMode = CapturedShieldMode::RushBurst;
                shieldComp.gravityEnabled = false;
                shieldComp.contactDamage = 2;
                shieldComp.velocityX = facingRight ? kBossRushSpeed : -kBossRushSpeed;
                spawnedShield->AddComponent<PhotoCopyLifetimeComponent>(0.5f);
                break;
            case CapturedSpawnArchetype::ShieldJumpBurst:
                shieldComp.capturedMode = CapturedShieldMode::JumpBurst;
                shieldComp.gravityEnabled = false;
                shieldComp.contactDamage = 0;
                shieldComp.followPlayer = false;
                shieldComp.hoverDuration = 0.0f;
                shieldComp.descendSpeed = kBossJumpDescendSpeed;
                if (playerTransform)
                {
                    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                    const float playerFootY = playerTransform->y + playerTransform->height * playerTransform->scale;
                    const float shieldCenterX = shieldX + shieldW * 0.5f;
                    shieldComp.followOffsetX = shieldCenterX - playerCenterX;
                    shieldComp.followOffsetY = shieldY - playerFootY;
                }
                spawnedShield->AddComponent<PhotoCopyLifetimeComponent>(2.0f);
                break;
            default:
                break;
            }

            scene.m_world.Spawn(std::move(shieldEntity));
            continue;
        }

        if (item.spawnArchetype == CapturedSpawnArchetype::SepiaGround)
        {
            const int sepiaGroundTexture = GetPhotoItemTextureForPaste(
                item,
                scene.m_tileTexture,
                scene.m_tileTexture2,
                scene.m_tileTexture3,
                scene.m_tileTexture4,
                scene.m_assets.GetTexture("sepia_rubble_stage"));
            const float tileSize = scene.m_tileMap.GetTileSize();
            if (!ShouldSplitSepiaGroundIntoCells(item, tileSize))
            {
                auto groundEntity = CreateSepiaGroundPhotoBox(
                    item,
                    groupId,
                    pasteOrder,
                    sepiaGroundTexture,
                    spawnX + item.relativeX,
                    spawnY + item.relativeY,
                    item.width,
                    item.height,
                    item.sourceX,
                    item.sourceY,
                    item.sourceWidth,
                    item.sourceHeight);
                lastSpawnedEntity = groundEntity.get();
                scene.m_world.Spawn(std::move(groundEntity));
                ++spawnedPhotoBoxCount;
                continue;
            }

            for (float offsetY = 0.0f; offsetY < item.height - 0.001f; offsetY += tileSize)
            {
                const float pieceHeight = (std::min)(tileSize, item.height - offsetY);
                if (pieceHeight <= 0.0f)
                {
                    continue;
                }

                for (float offsetX = 0.0f; offsetX < item.width - 0.001f; offsetX += tileSize)
                {
                    const float pieceWidth = (std::min)(tileSize, item.width - offsetX);
                    if (pieceWidth <= 0.0f)
                    {
                        continue;
                    }

                    auto groundEntity = CreateSepiaGroundPhotoBox(
                        item,
                        groupId,
                        pasteOrder,
                        sepiaGroundTexture,
                        spawnX + item.relativeX + offsetX,
                        spawnY + item.relativeY + offsetY,
                        pieceWidth,
                        pieceHeight,
                        0.0f,
                        0.0f,
                        pieceWidth / tileSize,
                        pieceHeight / tileSize);
                    lastSpawnedEntity = groundEntity.get();
                    scene.m_world.Spawn(std::move(groundEntity));
                    ++spawnedPhotoBoxCount;
                }
            }
            continue;
        }

        auto entity = std::make_unique<Entity>();
        lastSpawnedEntity = entity.get();
        ++spawnedPhotoBoxCount;
        lastSpawnedEntity->AddComponent<TagComponent>("PhotoBox");
        lastSpawnedEntity->AddComponent<PhotoCopyGroupComponent>(groupId);
        lastSpawnedEntity->AddComponent<PhotoPasteOrderComponent>(pasteOrder);
        lastSpawnedEntity->AddComponent<PhotoPasteAnimationComponent>(gPastedObjectPasteAnimationSeconds);
        const PhotoCopyRole pastedRole = ShouldPasteAsSolidEnvironment(item)
            ? PhotoCopyRole::Solid
            : item.role;
        lastSpawnedEntity->AddComponent<PhotoCopyRoleComponent>(pastedRole);
        lastSpawnedEntity->AddComponent<PhotoCopyOriginComponent>(item.origin);
        if (item.vanishOnCapture)
        {
            lastSpawnedEntity->AddComponent<VanishOnCaptureComponent>(true);
        }
        if (item.sourceTileValue > 0)
        {
            lastSpawnedEntity->AddComponent<PhotoCopyTileValueComponent>(item.sourceTileValue);
        }
        if (item.sepiaShutterObject)
        {
            lastSpawnedEntity->AddComponent<SepiaShutterVisualComponent>();
        }
        if (item.damagePlatformTileSpan > 0)
        {
            lastSpawnedEntity->AddComponent<DamagePlatformComponent>(
                item.damagePlatformTileSpan,
                item.damagePlatformPhotoCapturable);
        }
        if (item.spikeStripTileSpan > 0)
        {
            lastSpawnedEntity->AddComponent<SpikeStripComponent>(
                item.spikeStripTileSpan,
                item.damagePlatformPhotoCapturable);
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
        const bool persistPastedVanishObject =
            item.vanishOnCapture ||
            item.damagePlatformTileSpan > 0 ||
            item.spikeStripTileSpan > 0;
        if (!persistPastedVanishObject)
        {
            lastSpawnedEntity->AddComponent<PhotoCopyLifetimeComponent>(lifetimeSeconds);
        }
        lastSpawnedEntity->AddComponent<TransformComponent>(spawnX + item.relativeX, spawnY + item.relativeY, item.width, item.height);
        lastSpawnedEntity->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
        lastSpawnedEntity->AddComponent<SpriteRenderComponent>(GetPhotoItemTextureForPaste(
            item,
            scene.m_tileTexture,
            scene.m_tileTexture2,
            scene.m_tileTexture3,
            scene.m_tileTexture4,
            scene.m_assets.GetTexture("sepia_rubble_stage")));
        if (auto* sprite = lastSpawnedEntity->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
            sprite->SetFlipX(item.flipX);
        }
        if (auto* transform = lastSpawnedEntity->GetComponent<TransformComponent>())
        {
            transform->rotation = item.rotation;
        }
        if (item.lightRadius > 0.0f)
        {
            lastSpawnedEntity->AddComponent<FlickerLightComponent>(
                item.lightRadius,
                item.lightIntensity > 0.0f ? item.lightIntensity : 1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.90f,
                0.24f,
                false,
                1.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f);
        }
        // Generic pasted PhotoBoxes use their visible transform bounds so pasted floors and walls do not feel too thin.
        ApplyPhotoFilterToPhotoBox(*lastSpawnedEntity, item.appliedTheme);
        if (ShouldPasteAsSolidEnvironment(item))
        {
            if (auto* role = lastSpawnedEntity->GetComponent<PhotoCopyRoleComponent>())
            {
                role->role = PhotoCopyRole::Solid;
            }
        }
        scene.m_world.Spawn(std::move(entity));
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
    scene.m_photo.placement.draggingFromTray = false;
    scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, lastSpawnedEntity, "test_tone", 0.0f, 0.0f });
    scene.m_eventBus.Publish({ EventType::LogMessage, &player, lastSpawnedEntity, "Spawned filtered reconstruction", 0.0f, 0.0f });
}

