#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_combat_common.h"
#include "game_scene_combat_enemy_system.h"
#include "game_scene_combat_bullet_system.h"
#include "game_scene_player_system.h"
#include "game_scene_player_movement_system.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_photo_tray_system.h"
#include "photo_system.h"
#include "game_scene_camerawork.h"

#include <unordered_map>

#include "DxLib.h"
#include <game_scene_player_visual_system.h>

using namespace game_scene_detail;

namespace
{
    constexpr float kFloorCameraWidth = 1920.0f;
    constexpr float kCameraFollowTargetTilesX = 23.0f;
    constexpr float kCameraOffsetTilesY = -2.0f;
    constexpr float kFixedCameraYDeadZone = 18.0f;
    constexpr float kFixedCameraYFollowSpeed = 7.5f;
    constexpr float kGroundedCameraYSmoothSpeed = 5.0f;
    constexpr float kAirborneCameraYSmoothSpeed = 14.0f;
    constexpr float kCameraYSnapDistance = 180.0f;
    constexpr float kFixedLockExitMargin = 24.0f;
    constexpr float kBarrelDebrisLifetime = 0.55f;
    constexpr float kBarrelRespawnOffscreenMargin = 64.0f;
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
    struct TuningRowLayout
    {
        float y;
        bool isSectionHeader;
    };

    TuningRowLayout GetTuningRowLayout(int index)
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
        y += static_cast<float>(index) * kTuningRowHeight;
        return { y, false };
    }

    bool IsPointInside(float pointX, float pointY, float x, float y, float width, float height)
    {
        return pointX >= x && pointX <= x + width && pointY >= y && pointY <= y + height;
    }

    struct JumpPadBoardGeometry
    {
        float centerX = 0.0f;
        float centerY = 0.0f;
        float halfLength = 0.0f;
        float halfThickness = 0.0f;
        float dirX = 1.0f;
        float dirY = 0.0f;
        float normalX = 0.0f;
        float normalY = 1.0f;
    };

    struct JumpPadContact
    {
        bool edgeContact = false;
        int side = 0;
    };

    JumpPadBoardGeometry BuildJumpPadBoardGeometry(const TransformComponent& transform, const JumpPadComponent& jumpPad)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float boardHeight = height * 0.5f;
        const float cosTilt = std::cos(jumpPad.tilt);
        const float sinTilt = std::sin(jumpPad.tilt);
        return JumpPadBoardGeometry{
            transform.x + width * 0.5f,
            transform.y + boardHeight * 0.5f,
            width * 0.5f,
            boardHeight * 0.5f,
            cosTilt,
            sinTilt,
            -sinTilt,
            cosTilt,
        };
    }

    void WorldToJumpPadLocal(const JumpPadBoardGeometry& board, float worldX, float worldY, float& outLocalX, float& outLocalY)
    {
        const float dx = worldX - board.centerX;
        const float dy = worldY - board.centerY;
        outLocalX = dx * board.dirX + dy * board.dirY;
        outLocalY = dx * board.normalX + dy * board.normalY;
    }

    float GetJumpPadLocalX(const JumpPadBoardGeometry& board, float worldX, float worldY)
    {
        return (worldX - board.centerX) * board.dirX + (worldY - board.centerY) * board.dirY;
    }

    bool TryGetJumpPadTopYAtX(const JumpPadBoardGeometry& board, float worldX, float& outY)
    {
        if (std::fabs(board.normalY) <= 0.0001f)
        {
            return false;
        }

        const float topLocalY = -board.halfThickness;
        outY = board.centerY + (topLocalY - (worldX - board.centerX) * board.normalX) / board.normalY;
        return true;
    }

    bool TryResolveJumpPadContact(
        TransformComponent& bounds,
        const TransformComponent& jumpPadTransform,
        const JumpPadComponent& jumpPad,
        float verticalTolerance,
        float edgeVerticalTolerance,
        float horizontalEdgeAllowance,
        float minHorizontalOverlap,
        float edgeZoneWidth,
        JumpPadContact* outContact = nullptr,
        float previousBoundsY = 0.0f,
        bool useSweptContact = false)
    {
        const float width = bounds.width * bounds.scale;
        const float height = bounds.height * bounds.scale;
        if (width <= 0.0f || height <= 0.0f)
        {
            return false;
        }

        const float bottom = bounds.y + height;
        const float previousBottom = previousBoundsY + height;
        const JumpPadBoardGeometry board = BuildJumpPadBoardGeometry(jumpPadTransform, jumpPad);
        const float topLocalY = -board.halfThickness;
        const float allowance = std::max(0.0f, horizontalEdgeAllowance);
        const float minLocalX = -board.halfLength - allowance;
        const float maxLocalX = board.halfLength + allowance;

        const float leftLocalX = GetJumpPadLocalX(board, bounds.x, bottom);
        const float rightLocalX = GetJumpPadLocalX(board, bounds.x + width, bottom);
        const float objectLocalLeft = std::min(leftLocalX, rightLocalX);
        const float objectLocalRight = std::max(leftLocalX, rightLocalX);
        const float overlapLeft = std::max(objectLocalLeft, minLocalX);
        const float overlapRight = std::min(objectLocalRight, maxLocalX);
        const float requiredOverlap = std::max(1.0f, std::min(minHorizontalOverlap, width * 0.35f));
        if (overlapRight - overlapLeft < requiredOverlap)
        {
            return false;
        }

        const float sampleXs[3] = { bounds.x, bounds.x + width * 0.5f, bounds.x + width };
        bool foundContact = false;
        bool bestEdgeContact = false;
        int bestSide = 0;
        float bestSurfaceY = 0.0f;
        float bestDistance = 0.0f;
        for (float sampleX : sampleXs)
        {
            float localX = 0.0f;
            float localY = 0.0f;
            WorldToJumpPadLocal(board, sampleX, bottom, localX, localY);
            if (localX < minLocalX || localX > maxLocalX)
            {
                continue;
            }

            float surfaceY = bottom;
            if (!TryGetJumpPadTopYAtX(board, sampleX, surfaceY))
            {
                continue;
            }

            const float surfaceLocalX = GetJumpPadLocalX(board, sampleX, surfaceY);
            if (surfaceLocalX < minLocalX || surfaceLocalX > maxLocalX)
            {
                continue;
            }

            const bool edgeContact =
                edgeZoneWidth > 0.0f &&
                std::fabs(surfaceLocalX) >= board.halfLength - edgeZoneWidth;
            const float tolerance = edgeContact ? edgeVerticalTolerance : verticalTolerance;
            const bool nearSurface = std::fabs(localY - topLocalY) <= tolerance;
            bool sweptSurface = false;
            if (useSweptContact)
            {
                float previousLocalX = 0.0f;
                float previousLocalY = 0.0f;
                WorldToJumpPadLocal(board, sampleX, previousBottom, previousLocalX, previousLocalY);
                sweptSurface =
                    previousLocalX >= minLocalX &&
                    previousLocalX <= maxLocalX &&
                    previousLocalY <= topLocalY + tolerance &&
                    localY >= topLocalY - tolerance;
            }
            if (!nearSurface && !sweptSurface)
            {
                continue;
            }

            const float distance = std::fabs(localY - topLocalY);
            if (!foundContact || surfaceY < bestSurfaceY || (std::fabs(surfaceY - bestSurfaceY) <= 0.001f && distance < bestDistance))
            {
                bestSurfaceY = surfaceY;
                bestDistance = distance;
                bestEdgeContact = edgeContact;
                bestSide = surfaceLocalX < 0.0f ? -1 : 1;
                foundContact = true;
            }
        }

        if (!foundContact)
        {
            return false;
        }

        bounds.y = bestSurfaceY - height;
        if (outContact)
        {
            outContact->edgeContact = bestEdgeContact;
            outContact->side = bestSide;
        }
        return true;
    }

    float EaseInOutCubic(float t)
    {
        const float clamped = Clamp01(t);
        if (clamped < 0.5f)
        {
            return 4.0f * clamped * clamped * clamped;
        }

        const float f = -2.0f * clamped + 2.0f;
        return 1.0f - (f * f * f) * 0.5f;
    }

    bool IsVerticalLaserDirection(LaserTurretFireDirection direction)
    {
        return direction == LaserTurretFireDirection::Up ||
            direction == LaserTurretFireDirection::Down;
    }

    float GetCameraFollowSpanX(const TileMap& tileMap)
    {
        const float tileSize = tileMap.GetTileSize();
        if (tileSize <= 0.0f)
        {
            return gCameraViewWidth;
        }

        return std::min(gCameraViewWidth, tileSize * kCameraFollowTargetTilesX);
    }

    float GetCameraFollowOffsetY(const TileMap& tileMap)
    {
        return std::max(0.0f, tileMap.GetTileSize()) * kCameraOffsetTilesY;
    }

    float GetCameraVisibleHeight(const TileMap& tileMap)
    {
        const float visibleWidth = GetCameraFollowSpanX(tileMap);
        const float safeViewWidth = std::max(1.0f, gCameraViewWidth);
        const float aspect = gCameraViewHeight / safeViewWidth;
        return std::max(1.0f, visibleWidth * aspect);
    }

    float GetNormalizedHorizontalCameraDistance(float deltaX, float visibleWidth)
    {
        // ボスとプレイヤーの横距離だけを、カメラの表示幅に対して正規化する。
        return std::clamp(
            std::fabs(deltaX) / std::max(1.0f, visibleWidth),
            0.0f,
            1.0f);
    }

    bool IsMidBoss3CameraStabilizeStage(const std::string& mapPath)
    {
        return GetMapDisplayName(mapPath) == "ruins_boss";
    }
}


void GameScene::StartFloorCameraTransition(int directionX, int directionY)
{
    static_cast<void>(directionY);
    if (directionX == 0)
    {
        return;
    }

    const float maxCameraX = std::max(0.0f, GetMapPixelWidth() - gCameraViewWidth);
    const float targetX = std::clamp(
        m_flow.cameraX + static_cast<float>(directionX) * kFloorCameraWidth,
        0.0f,
        maxCameraX);
    // T marker transition: keep current Y, move only one floor on X.
    const float targetY = m_flow.cameraY;
    if (targetX == m_flow.cameraX && targetY == m_flow.cameraY)
    {
        return;
    }

    m_camera.floorCameraTransitionActive = true;
    m_camera.floorCameraTransitionElapsed = 0.0f;
    m_camera.floorCameraTransitionStartX = m_flow.cameraX;
    m_camera.floorCameraTransitionStartY = m_flow.cameraY;
    m_camera.floorCameraTransitionTargetX = targetX;
    m_camera.floorCameraTransitionTargetY = targetY;
}



void GameScene::UpdateCameraByMarkers(const TransformComponent& playerTransform, float deltaTime, bool followY)
{
   /* int activeIndex = -1;
    int bestPriority = -1;

    for (int i = 0; i < m_camera.fixedRanges.size(); i++)
    {
        const auto& fixedCamera = m_camera.fixedRanges[i];

        if (fixedCamera.IsInRange(playerCenterX, playerCenterY))
        {
            if (fixedCamera.GetCameraNum() > bestPriority)
            {
                bestPriority = fixedCamera.GetCameraNum();
                activeIndex = i;
            }
        }
    }

    bool prevFollow = (m_camera.prevCameraIndex == -1);
    bool currFollow = (activeIndex == -1);

    bool easingNeeded = true;
    if (prevFollow && currFollow)
    {
        easingNeeded = false;
    }

    if (easingNeeded && m_camera.prevCameraIndex != activeIndex)
    {
        m_camera.easingActive = true;
        m_camera.easingElapsedTime = 0.0f;

        m_camera.easingStartX = m_flow.cameraX;
        m_camera.easingStartY = m_flow.cameraY;

        if (activeIndex == -1)
        {
            m_camera.easingTargetX = playerCenterX - (gCameraViewWidth * 0.25f);
            m_camera.easingTargetY = playerCenterY - (gCameraViewHeight * 0.5f);
        }
        else
        {
            const auto& cam = m_camera.fixedRanges[activeIndex];

            gCameraViewWidth = cam.GetZoomWidth();
            gCameraViewHeight = cam.GetZoomHeight();

            float centerX = (cam.GetStartX() + cam.GetEndX()) * 0.5f;

            m_camera.easingTargetX = centerX - (gCameraViewWidth * 0.25f) + cam.GetOffsetX();
            m_camera.easingTargetY = playerCenterY - (gCameraViewHeight * 0.5f) + cam.GetOffsetY();
        }
    }

    if (m_camera.easingActive)
    {
        m_camera.easingElapsedTime += deltaTime;
        float t = m_camera.easingElapsedTime / easingTime;

        if (t >= 1.0f)
        {
            t = 1.0f;
            m_camera.easingActive = false;
        }

        m_flow.cameraX = std::lerp(m_camera.easingStartX, m_camera.easingTargetX, t);
        m_flow.cameraY = std::lerp(m_camera.easingStartY, m_camera.easingTargetY, t);

        m_camera.prevCameraIndex = activeIndex;
        return;
    }

    // 追従カメラ
    if (activeIndex < 0)
    {
        if (m_camera.prevCameraIndex >= 0)
        {
            const auto& prevCam = m_camera.fixedRanges[m_camera.prevCameraIndex];

            gCameraViewWidth = prevCam.GetZoomWidth();
            gCameraViewHeight = prevCam.GetZoomHeight();

            float centerX = (prevCam.GetStartX() + prevCam.GetEndX()) * 0.5f;

            m_flow.cameraX = centerX - (gCameraViewWidth * 0.25f) + prevCam.GetOffsetX();
            m_flow.cameraY = playerCenterY - (gCameraViewHeight * 0.5f) + prevCam.GetOffsetY();

            return;
        }

        m_flow.cameraX = playerCenterX - (gCameraViewWidth * 0.25f);
        if (followY)
        {
            m_flow.cameraY = playerCenterY - (gCameraViewHeight * 0.5f);
        }

        m_camera.prevCameraIndex = activeIndex;
        return;
    }

    // 固定カメラ
    const auto& cameraRange = m_camera.fixedRanges[activeIndex];

    gCameraViewWidth = cameraRange.GetZoomWidth();
    gCameraViewHeight = cameraRange.GetZoomHeight();

    float centerX = (cameraRange.GetStartX() + cameraRange.GetEndX()) * 0.5f;

    m_flow.cameraX = centerX - (gCameraViewWidth * 0.25f) + cameraRange.GetOffsetX();
    m_flow.cameraY = playerCenterY - (gCameraViewHeight * 0.5f) + cameraRange.GetOffsetY();

    m_camera.prevCameraIndex = activeIndex;*/

    if (!followY)
    {
        m_camera.cameraYRecenteringStrength = 0.0f;
        return;
    }

    const float playerHeight = playerTransform.height * playerTransform.scale;
    const float playerCenterY = playerTransform.y + playerHeight * 0.5f;
    const float visibleHeight = GetCameraVisibleHeight(m_tileMap);
    const float maxCameraY = std::max(0.0f, GetMapPixelHeight() - visibleHeight);

    m_flow.cameraY = std::clamp(m_flow.cameraY, 0.0f, maxCameraY);

    const float cameraCenterY = m_flow.cameraY + visibleHeight * 0.5f;
    const float deadZoneY = std::max(1.0f, gCameraDeadZoneY);

    const float targetPlayerCenterY = playerCenterY + gCameraTargetOffsetY;
    const float centerDeltaY = targetPlayerCenterY - cameraCenterY;
    float targetCameraY = targetPlayerCenterY - visibleHeight * 0.5f;
    targetCameraY = std::clamp(targetCameraY, 0.0f, maxCameraY);
    const bool movingCameraDown = targetCameraY > m_flow.cameraY;

    const float normalizedDeadZoneDistance = std::clamp(
        std::fabs(centerDeltaY) / std::max(1.0f, deadZoneY),
        0.0f,
        1.0f);
    const float smoothedRecenteringStrength =
        normalizedDeadZoneDistance * normalizedDeadZoneDistance *
        (3.0f - 2.0f * normalizedDeadZoneDistance);
    const float targetRecenteringStrength =
        movingCameraDown ? normalizedDeadZoneDistance : smoothedRecenteringStrength;
    if (movingCameraDown)
    {
        if (gCameraDeadZoneDownStrengthResponse <= 0.001f)
        {
            m_camera.cameraYRecenteringStrength = targetRecenteringStrength;
        }
        else
        {
            const float strengthBlend = 1.0f - std::pow(
                0.001f,
                deltaTime * std::max(0.01f, gCameraDeadZoneDownStrengthResponse));
            m_camera.cameraYRecenteringStrength +=
                (targetRecenteringStrength - m_camera.cameraYRecenteringStrength) * strengthBlend;
        }
    }
    else
    {
        const float strengthResponseSpeed =
            targetRecenteringStrength > m_camera.cameraYRecenteringStrength
            ? std::max(0.01f, gCameraDeadZoneStrengthRiseResponse)
            : std::max(0.01f, gCameraDeadZoneStrengthFallResponse);
        const float strengthBlend = 1.0f - std::pow(0.001f, deltaTime * strengthResponseSpeed);
        m_camera.cameraYRecenteringStrength +=
            (targetRecenteringStrength - m_camera.cameraYRecenteringStrength) * strengthBlend;
    }

    if (m_camera.cameraYRecenteringStrength <= 0.001f)
    {
        m_camera.cameraYRecenteringStrength = 0.0f;
        return;
    }

    const float cameraDeltaY = targetCameraY - m_flow.cameraY;
    if (movingCameraDown)
    {
        const float maxStepY = std::max(
            0.0f,
            gCameraDeadZoneDownMaxSpeedY * m_camera.cameraYRecenteringStrength * deltaTime);
        m_flow.cameraY += std::min(cameraDeltaY, maxStepY);
    }
    else
    {
        const float followRate = std::clamp(
            std::max(0.01f, gCameraDeadZoneFollowSpeedY) * m_camera.cameraYRecenteringStrength * deltaTime,
            0.0f,
            1.0f);
        m_flow.cameraY += cameraDeltaY * followRate;
    }
    m_flow.cameraY = std::clamp(m_flow.cameraY, 0.0f, maxCameraY);

}

void GameScene::OffsetCameraX(bool facingRight, bool playerMovingHorizontally, float deltaTime)
{
    const float offsetAmount = std::max(0.0f, gCameraLookAheadOffsetX);
    const float targetOffsetX = playerMovingHorizontally ? 0.0f : (facingRight ? offsetAmount : -offsetAmount);

    const float response = playerMovingHorizontally
        ? std::max(0.01f, gCameraLookAheadReturnResponse)
        : std::max(0.01f, gCameraLookAheadResponse);
    const float blend = 1.0f - std::pow(0.001f, deltaTime * response);
    m_camera.cameraOffsetX = std::lerp(m_camera.cameraOffsetX, targetOffsetX, blend);
}

void GameScene::ApplyShieldBossSlamCameraWork(float deltaTime)
{
    m_render.slamCameraZoomBoost = 0.0f;
    if (m_mapEditor.active || deltaTime <= 0.0f)
    {
        m_render.bossIntroCameraZoomBoost = 0.0f;
        m_render.bossIntroCameraInfluence = 0.0f;
        m_render.bossIntroCameraAnchorActive = false;
        return;
    }

    const Entity* player = FindEntityByTag(kTagPlayer);
    const auto* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    if (!playerTransform)
    {
        m_render.bossIntroCameraZoomBoost = 0.0f;
        m_render.bossIntroCameraInfluence = 0.0f;
        m_render.bossIntroCameraAnchorActive = false;
        return;
    }

    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
    const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
    float bestWeight = 0.0f;
    float bestTargetCenterX = playerCenterX;
    float bestTargetCenterY = playerCenterY;
    float bestZoomBoost = 0.0f;
    float bestCameraSpeed = 4.0f;
    bool bestIntroCameraWork = false;

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        const auto* boss = entity->GetComponent<ShieldBossComponent>();
        const auto* bossTransform = entity->GetComponent<TransformComponent>();
        const auto* bossSprite = entity->GetComponent<SpriteRenderComponent>();
        if (!enemy ||
            enemy->GetArchetype() != EnemyArchetype::ShieldBoss ||
            !enemy->IsEnabled() ||
            !boss ||
            !bossTransform)
        {
            continue;
        }

        float weight = 0.0f;
        float verticalLookBias = 0.0f;
        float zoomBoost = 0.0f;
        float targetBossBlendX = 0.44f;
        float targetBossBlendY = 0.52f;
        float cameraSpeed = 4.0f;
        bool introCameraWork = false;
        if (boss->introDropActive)
        {
            // Point 1: wide upper camera that shows the boss dropping in.
            weight = 0.34f;
            verticalLookBias = -132.0f;
            zoomBoost = 0.0f;
            targetBossBlendX = 0.38f;
            targetBossBlendY = 0.34f;
            cameraSpeed = 9.5f;
            introCameraWork = true;
        }
        else if (boss->appearAnimationActive)
        {
            // Point 1 hold: keep the pullback until the roar actually starts.
            weight = 0.34f;
            verticalLookBias = -132.0f;
            zoomBoost = 0.0f;
            targetBossBlendX = 0.38f;
            targetBossBlendY = 0.34f;
            cameraSpeed = 9.5f;
            introCameraWork = true;
        }
        else if (boss->roarAnimationActive)
        {
            // Point 1 hold: keep the pullback through the full entrance.
            weight = 0.34f;
            verticalLookBias = -132.0f;
            zoomBoost = 0.0f;
            targetBossBlendX = 0.38f;
            targetBossBlendY = 0.34f;
            cameraSpeed = 9.5f;
            introCameraWork = true;
        }
        switch (boss->state)
        {
        case ShieldBossState::JumpAscend:
            if (!introCameraWork)
            {
                weight = 0.46f;
                verticalLookBias = -72.0f;
                zoomBoost = 0.020f;
            }
            break;
        case ShieldBossState::AirHover:
            if (!introCameraWork)
            {
                weight = 0.58f;
                verticalLookBias = -112.0f;
                zoomBoost = 0.030f;
            }
            break;
        case ShieldBossState::JumpDescend:
            if (!introCameraWork)
            {
                weight = 0.66f;
                verticalLookBias = -46.0f;
                zoomBoost = 0.045f;
            }
            break;
        case ShieldBossState::SlamPhase2:
            if (!introCameraWork)
            {
                weight = std::max(0.0f, 0.32f * (1.0f - std::min(1.0f, boss->stateTimer / 0.34f)));
                verticalLookBias = 38.0f;
                zoomBoost = 0.018f * (weight / 0.32f);
            }
            break;
        default:
            break;
        }

        if (weight <= bestWeight)
        {
            continue;
        }

        const float bossDrawWidth = bossTransform->width * bossTransform->scale * (bossSprite ? bossSprite->GetRenderScaleX() : 1.0f);
        const float bossDrawHeight = bossTransform->height * bossTransform->scale * (bossSprite ? bossSprite->GetRenderScaleY() : 1.0f);
        const float bossCenterX = bossTransform->x + (bossSprite ? bossSprite->GetRenderOffsetX() : 0.0f) + bossDrawWidth * 0.5f;
        const float bossCenterY = bossTransform->y + (bossSprite ? bossSprite->GetRenderOffsetY() : 0.0f) + bossDrawHeight * 0.5f;
        if (!introCameraWork)
        {
            const float normalizedDistance = GetNormalizedHorizontalCameraDistance(
                bossCenterX - playerCenterX,
                GetCameraFollowSpanX(m_tileMap));
            const float distanceBlend =
                normalizedDistance * normalizedDistance * (3.0f - 2.0f * normalizedDistance);
            // 遠距離では両者の中点を優先し、叩きつけ演出で片方が画面外へ出るのを防ぎます。
            targetBossBlendX = std::lerp(targetBossBlendX, 0.5f, distanceBlend);
            targetBossBlendY = std::lerp(targetBossBlendY, 0.5f, distanceBlend);
            verticalLookBias *= std::lerp(1.0f, 0.35f, distanceBlend);
            zoomBoost *= 1.0f - distanceBlend;
        }
        bestWeight = weight;
        bestTargetCenterX = std::lerp(playerCenterX, bossCenterX, targetBossBlendX);
        bestTargetCenterY = std::lerp(playerCenterY, bossCenterY, targetBossBlendY) + verticalLookBias;
        bestZoomBoost = zoomBoost;
        bestCameraSpeed = cameraSpeed;
        bestIntroCameraWork = introCameraWork;
    }

    auto applyCameraTarget = [&](float targetCenterX, float targetCenterY, float weight, float speedScale, bool clampToMap, bool useIntroPullView)
    {
        float cameraViewWidth = gCameraViewWidth;
        float cameraViewHeight = gCameraViewHeight;
        if (useIntroPullView)
        {
            const float tileSize = m_tileMap.GetTileSize();
            if (tileSize > 0.0f)
            {
                constexpr float kIntroPullTargetTilesX = 23.0f;
                const float targetWorldWidth = tileSize * kIntroPullTargetTilesX;
                if (targetWorldWidth > 0.0f)
                {
                    const float introViewScale = std::max(0.0001f, static_cast<float>(SCREEN_WIDTH) / targetWorldWidth);
                    cameraViewWidth = static_cast<float>(SCREEN_WIDTH) / introViewScale;
                    cameraViewHeight = static_cast<float>(SCREEN_HEIGHT) / introViewScale;
                }
            }
        }

        const float desiredCameraX = targetCenterX - cameraViewWidth * 0.5f;
        const float desiredCameraY = targetCenterY - cameraViewHeight * 0.5f;
        const float maxCameraX = std::max(0.0f, GetMapPixelWidth() - cameraViewWidth);
        const float maxCameraY = std::max(0.0f, GetMapPixelHeight() - cameraViewHeight);
        const float targetCameraX = clampToMap ? std::clamp(desiredCameraX, 0.0f, maxCameraX) : desiredCameraX;
        const float targetCameraY = clampToMap ? std::clamp(desiredCameraY, 0.0f, maxCameraY) : desiredCameraY;
        const float blend = 1.0f - std::pow(0.001f, deltaTime * speedScale);

        m_flow.cameraX = std::lerp(m_flow.cameraX, targetCameraX, blend * weight);
        m_flow.cameraY = std::lerp(m_flow.cameraY, targetCameraY, blend * weight);
    };

    const bool hasActiveCameraWork = bestWeight > 0.0f;
    if (hasActiveCameraWork && bestIntroCameraWork)
    {
        const float influenceBlend = 1.0f - std::pow(0.001f, deltaTime * 5.8f);
        m_render.bossIntroCameraInfluence = std::lerp(m_render.bossIntroCameraInfluence, 1.0f, influenceBlend);
        m_render.bossIntroCameraTargetX = bestTargetCenterX;
        m_render.bossIntroCameraTargetY = bestTargetCenterY;
        m_render.bossIntroCameraZoomBoost = 0.0f;
        m_render.bossIntroCameraAnchorActive = false;
        applyCameraTarget(
            bestTargetCenterX,
            bestTargetCenterY,
            bestWeight * m_render.bossIntroCameraInfluence,
            bestCameraSpeed,
            false,
            true);
    }
    else
    {
        m_render.bossIntroCameraAnchorActive = false;
        const float returnBlend = 1.0f - std::pow(0.001f, deltaTime * 0.58f);
        m_render.bossIntroCameraInfluence = std::lerp(m_render.bossIntroCameraInfluence, 0.0f, returnBlend);
        m_render.bossIntroCameraZoomBoost = std::lerp(m_render.bossIntroCameraZoomBoost, 0.0f, returnBlend);
        if (m_render.bossIntroCameraInfluence <= 0.01f)
        {
            m_render.bossIntroCameraInfluence = 0.0f;
            m_render.bossIntroCameraZoomBoost = 0.0f;
        }

        if (hasActiveCameraWork)
        {
            applyCameraTarget(
                bestTargetCenterX,
                bestTargetCenterY,
                bestWeight,
                5.5f + bestWeight * 5.0f,
                true,
                false);
            m_render.slamCameraZoomBoost = bestZoomBoost;
        }
    }
}

void GameScene::ApplyShieldBossFramingCameraWork(float deltaTime)
{
    constexpr float kCameraEaseSpeed = 0.9f;
    constexpr float kZoomOutEaseSpeed = 1.1f;
    constexpr float kZoomInEaseSpeed = 0.45f;
    constexpr float kMinimumDistanceZoomScale = 0.80f;
    constexpr float kMirroredPlayerAnchorX = 0.56f;
    constexpr float kSideChangeConfirmSeconds = 0.4f;
    constexpr float kNearZoomScale = 1.0f;
    constexpr float kMiddleZoomScale = 0.80f;
    const Entity* player = FindEntityByTag(kTagPlayer);
    const auto* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    const float tileSize = std::max(1.0f, m_tileMap.GetTileSize());
    const float sideChangeDeadZone = tileSize * 1.25f;
    const float framingMargin = tileSize * 1.25f;
    const float baseVisibleWidth = GetCameraFollowSpanX(m_tileMap);
    const float baseVisibleHeight = GetCameraVisibleHeight(m_tileMap);
    const float blend = 1.0f - std::pow(0.001f, deltaTime * kCameraEaseSpeed);
    bool shieldBossFound = false;
    float normalizedDistance = 0.0f;
    float playerLeft = 0.0f;
    float playerRight = 0.0f;
    float bossLeft = 0.0f;
    float bossRight = 0.0f;
    bool slamVerticalTrackingActive = false;
    float shieldBossTargetCameraX = m_flow.cameraX;

    if (playerTransform)
    {
        playerLeft = playerTransform->x;
        playerRight = playerLeft + playerTransform->width * playerTransform->scale;
        const float playerCenterX =
            (playerLeft + playerRight) * 0.5f;

        for (Entity* entity : m_world.EntitiesByTag(EntityTag::Enemy))
        {
            const auto* enemy = entity ? entity->GetComponent<EnemyComponent>() : nullptr;
            const auto* boss = entity ? entity->GetComponent<ShieldBossComponent>() : nullptr;
            const auto* bossTransform = entity ? entity->GetComponent<TransformComponent>() : nullptr;
            const auto* bossSprite = entity ? entity->GetComponent<SpriteRenderComponent>() : nullptr;
            if (!enemy ||
                enemy->GetArchetype() != EnemyArchetype::ShieldBoss ||
                !enemy->IsEnabled() ||
                !boss ||
                !bossTransform)
            {
                continue;
            }

            const bool introFinished =
                boss->combatStarted &&
                !boss->introDropActive &&
                !boss->appearAnimationActive &&
                !boss->roarAnimationActive;
            // 登場演出後から破壊モーション終了までだけ、ボス戦用カメラを有効にします。
            if (!introFinished || boss->deathAnimationFinished)
            {
                continue;
            }

            const float bossVisualWidth =
                bossTransform->width * bossTransform->scale *
                (bossSprite ? bossSprite->GetRenderScaleX() : 1.0f);
            bossLeft = bossTransform->x + (bossSprite ? bossSprite->GetRenderOffsetX() : 0.0f);
            bossRight = bossLeft + bossVisualWidth;
            const float bossCenterX = (bossLeft + bossRight) * 0.5f;
            const float bossOffsetX = bossCenterX - playerCenterX;
            // 接近中は直前の側を維持し、交差地点でカメラが細かく往復するのを防ぎます。
            int requestedCameraSide = m_camera.shieldBossCameraSide;
            if (bossOffsetX > sideChangeDeadZone)
            {
                requestedCameraSide = 1;
            }
            else if (bossOffsetX < -sideChangeDeadZone)
            {
                requestedCameraSide = -1;
            }

            if (requestedCameraSide == m_camera.shieldBossCameraSide)
            {
                m_camera.shieldBossPendingCameraSide = requestedCameraSide;
                m_camera.shieldBossSideChangeTimer = 0.0f;
            }
            else if (requestedCameraSide != m_camera.shieldBossPendingCameraSide)
            {
                m_camera.shieldBossPendingCameraSide = requestedCameraSide;
                m_camera.shieldBossSideChangeTimer = 0.0f;
            }
            else
            {
                m_camera.shieldBossSideChangeTimer += deltaTime;
                if (m_camera.shieldBossSideChangeTimer >= kSideChangeConfirmSeconds)
                {
                    m_camera.shieldBossCameraSide = requestedCameraSide;
                    m_camera.shieldBossSideChangeTimer = 0.0f;
                }
            }

            // 高低差は無視し、横方向の距離だけでズーム倍率を決める。
            normalizedDistance = GetNormalizedHorizontalCameraDistance(
                bossOffsetX,
                baseVisibleWidth);
            slamVerticalTrackingActive =
                boss->state == ShieldBossState::JumpAscend ||
                boss->state == ShieldBossState::AirHover ||
                boss->state == ShieldBossState::JumpDescend ||
                boss->state == ShieldBossState::SlamPhase1 ||
                boss->state == ShieldBossState::SlamPhase2;
            shieldBossFound = true;
            break;
        }

        float targetZoomScale = 1.0f;
        if (shieldBossFound)
        {
            // 境界に幅を持たせ、近・中の境目でズームが往復しないようにする。
            switch (m_camera.shieldBossZoomTier)
            {
            case 0:
                if (normalizedDistance > 0.34f) m_camera.shieldBossZoomTier = 1;
                break;
            default:
                if (normalizedDistance < 0.24f) m_camera.shieldBossZoomTier = 0;
                break;
            }

            const float requiredWidth =
                std::max(playerRight, bossRight) - std::min(playerLeft, bossLeft) +
                framingMargin * 2.0f;
            const float fitZoomScale = std::clamp(
                baseVisibleWidth / std::max(1.0f, requiredWidth),
                kMinimumDistanceZoomScale,
                1.0f);
            const int fitZoomTier =
                fitZoomScale >= kNearZoomScale ? 0 : 1;
            m_camera.shieldBossZoomTier =
                std::max(m_camera.shieldBossZoomTier, fitZoomTier);

            constexpr float kZoomScales[] =
            {
                kNearZoomScale,
                kMiddleZoomScale,
            };
            targetZoomScale = kZoomScales[
                std::clamp(m_camera.shieldBossZoomTier, 0, 1)];
        }
        else
        {
            m_camera.shieldBossZoomTier = 0;
            m_camera.shieldBossPendingCameraSide = m_camera.shieldBossCameraSide;
            m_camera.shieldBossSideChangeTimer = 0.0f;
        }

        const float zoomBlend =
            targetZoomScale < m_camera.shieldBossDistanceZoomScale
            ? 1.0f - std::pow(0.001f, deltaTime * kZoomOutEaseSpeed)
            : 1.0f - std::pow(0.001f, deltaTime * kZoomInEaseSpeed);
        m_camera.shieldBossDistanceZoomScale = std::lerp(
            m_camera.shieldBossDistanceZoomScale,
            targetZoomScale,
            zoomBlend);
        const float visibleWidth =
            baseVisibleWidth / std::max(0.01f, m_camera.shieldBossDistanceZoomScale);

        if (shieldBossFound)
        {
            float targetCameraX = m_flow.cameraX;
            if (m_camera.shieldBossCameraSide < 0)
            {
                targetCameraX = playerCenterX - visibleWidth * kMirroredPlayerAnchorX;
            }

            const float minimumCameraX =
                std::max(playerRight, bossRight) + framingMargin - visibleWidth;
            const float maximumCameraX =
                std::min(playerLeft, bossLeft) - framingMargin;
            targetCameraX = minimumCameraX <= maximumCameraX
                ? std::clamp(targetCameraX, minimumCameraX, maximumCameraX)
                : (std::min(playerLeft, bossLeft) + std::max(playerRight, bossRight)) * 0.5f -
                    visibleWidth * 0.5f;

            shieldBossTargetCameraX = targetCameraX;
        }

        // Horizontal framing follows an absolute target below instead of accumulating an offset.
        m_camera.shieldBossCameraOffsetX = 0.0f;

        if (shieldBossFound && !m_camera.shieldBossCameraBaseYInitialized)
        {
            // 最も近い通常カメラの高さを、ボス戦中の最低表示位置として固定する。
            m_camera.shieldBossCameraBaseY = m_flow.cameraY;
            m_camera.shieldBossCameraOffsetY = 0.0f;
            m_camera.shieldBossCameraBaseYInitialized = true;
        }

        if (m_camera.shieldBossCameraBaseYInitialized)
        {
            if (shieldBossFound && slamVerticalTrackingActive)
            {
                // 座標が小さくなる上方向だけを、たたきつけアクション中に許可する。
                m_camera.shieldBossCameraOffsetY =
                    std::min(0.0f, m_flow.cameraY - m_camera.shieldBossCameraBaseY);
            }
            else if (shieldBossFound)
            {
                m_camera.shieldBossCameraOffsetY = std::lerp(
                    m_camera.shieldBossCameraOffsetY,
                    0.0f,
                    blend);
            }
            else
            {
                // 討伐後は通常カメラへ滑らかに戻してから、基準Yの固定を解除する。
                const float lockedCameraY =
                    m_camera.shieldBossCameraBaseY + m_camera.shieldBossCameraOffsetY;
                m_camera.shieldBossCameraBaseY = std::lerp(
                    lockedCameraY,
                    m_flow.cameraY,
                    blend);
                m_camera.shieldBossCameraOffsetY = 0.0f;
                if (std::fabs(m_camera.shieldBossCameraBaseY - m_flow.cameraY) <= 0.5f &&
                    std::fabs(m_camera.shieldBossDistanceZoomScale - 1.0f) <= 0.005f)
                {
                    m_camera.shieldBossCameraBaseYInitialized = false;
                }
            }
        }
    }
    else
    {
        m_camera.shieldBossCameraOffsetX = 0.0f;
        m_camera.shieldBossCameraOffsetY = 0.0f;
        m_camera.shieldBossCameraBaseY = 0.0f;
        m_camera.shieldBossDistanceZoomScale = 1.0f;
        m_camera.shieldBossSideChangeTimer = 0.0f;
        m_camera.shieldBossCameraSide = 1;
        m_camera.shieldBossPendingCameraSide = 1;
        m_camera.shieldBossZoomTier = 0;
        m_camera.shieldBossCameraBaseYInitialized = false;
    }

    // ズーム後の実表示範囲でマップ内へ収めます。
    const float visibleWidth =
        baseVisibleWidth / std::max(0.01f, m_camera.shieldBossDistanceZoomScale);
    const float visibleHeight =
        baseVisibleHeight / std::max(0.01f, m_camera.shieldBossDistanceZoomScale);
    const float maxCameraX = std::max(0.0f, GetMapPixelWidth() - visibleWidth);
    const float maxCameraY = std::max(0.0f, GetMapPixelHeight() - visibleHeight);
    const float cameraXBeforeClamp = shieldBossFound
        ? std::lerp(m_flow.cameraX, shieldBossTargetCameraX, blend)
        : m_flow.cameraX;
    m_flow.cameraX = std::clamp(
        cameraXBeforeClamp,
        0.0f,
        maxCameraX);
    const float targetCameraY = m_camera.shieldBossCameraBaseYInitialized
        ? m_camera.shieldBossCameraBaseY + m_camera.shieldBossCameraOffsetY
        : m_flow.cameraY;
    m_flow.cameraY = std::clamp(targetCameraY, 0.0f, maxCameraY);
}



void GameScene::SpawnBarrelBreakEffect(float x, float y, float width, float height)
{
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    constexpr float velocities[][2] =
    {
        { -180.0f, -260.0f },
        { -120.0f, -210.0f },
        { -60.0f, -180.0f },
        {  60.0f, -190.0f },
        {  120.0f, -220.0f },
        {  180.0f, -250.0f },
        { -90.0f, -140.0f },
        {  90.0f, -150.0f },
    };
    constexpr float sizes[] = { 12.0f, 10.0f, 9.0f, 11.0f, 8.0f, 10.0f, 7.0f, 9.0f };
    constexpr float rotations[] = { -0.4f, 0.2f, -0.8f, 0.6f, -0.3f, 0.5f, -0.9f, 0.9f };
    constexpr float rotationSpeeds[] = { -4.0f, 3.2f, -5.4f, 4.6f, -3.8f, 5.0f, -6.2f, 6.0f };
    constexpr float colors[][3] =
    {
        { 0.52f, 0.31f, 0.16f },
        { 0.44f, 0.25f, 0.12f },
        { 0.60f, 0.38f, 0.20f },
        { 0.34f, 0.18f, 0.08f },
    };

    for (int index = 0; index < 8; ++index)
    {
        BarrelDebrisParticle particle;
        particle.x = centerX - sizes[index] * 0.5f;
        particle.y = centerY - sizes[index] * 0.5f;
        particle.velocityX = velocities[index][0];
        particle.velocityY = velocities[index][1];
        particle.size = sizes[index];
        particle.rotation = rotations[index];
        particle.rotationSpeed = rotationSpeeds[index];
        particle.life = kBarrelDebrisLifetime;
        particle.maxLife = kBarrelDebrisLifetime;
        particle.r = colors[index % 4][0];
        particle.g = colors[index % 4][1];
        particle.b = colors[index % 4][2];
        m_effects.barrelDebris.push_back(particle);
    }
}

void GameScene::SpawnSlamImpactEffect(float centerX, float groundY, float width)
{
    constexpr float kDustLifetime = 0.58f;
    constexpr int kFlashCount = 7;
    constexpr int kGroundStreakCount = 14;
    constexpr int kDustCount = 36;
    constexpr int kChipCount = 26;
    constexpr int kVerticalBurstCount = 18;
    const float clampedWidth = std::max(128.0f, width * 1.18f);
    const float halfWidth = clampedWidth * 0.5f;

    for (int index = 0; index < kFlashCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kFlashCount - 1);
        SlamDustParticle particle;
        particle.width = std::lerp(clampedWidth * 0.34f, clampedWidth * 1.05f, t);
        particle.height = std::lerp(16.0f, 46.0f, t);
        particle.x = centerX - particle.width * 0.5f;
        particle.y = groundY - particle.height * 0.70f;
        particle.velocityX = 0.0f;
        particle.velocityY = -28.0f - t * 38.0f;
        particle.rotation = (index % 2 == 0 ? -1.0f : 1.0f) * (0.03f + t * 0.05f);
        particle.rotationSpeed = particle.rotation * -3.0f;
        particle.life = std::lerp(0.13f, 0.25f, t);
        particle.maxLife = particle.life;
        particle.alphaScale = std::lerp(1.0f, 0.58f, t);
        particle.r = std::lerp(1.0f, 0.78f, t);
        particle.g = std::lerp(0.98f, 0.86f, t);
        particle.b = std::lerp(0.86f, 0.68f, t);
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kGroundStreakCount; ++index)
    {
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float spreadT = static_cast<float>(index / 2) / static_cast<float>((kGroundStreakCount / 2) - 1);
        const float distance = std::lerp(0.08f, 0.92f, spreadT) * halfWidth;
        SlamDustParticle particle;
        particle.width = std::lerp(84.0f, 154.0f, 1.0f - spreadT);
        particle.height = std::lerp(8.0f, 17.0f, 1.0f - spreadT);
        particle.x = centerX + side * distance - particle.width * 0.5f;
        particle.y = groundY - particle.height * 0.5f - static_cast<float>(GetRand(5));
        particle.velocityX = side * (460.0f + static_cast<float>(GetRand(220)));
        particle.velocityY = -20.0f - static_cast<float>(GetRand(34));
        particle.rotation = side * std::lerp(0.04f, 0.13f, spreadT);
        particle.rotationSpeed = side * 0.8f;
        particle.life = 0.26f + spreadT * 0.12f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.92f;
        particle.r = 0.92f;
        particle.g = 0.78f;
        particle.b = 0.48f;
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kVerticalBurstCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kVerticalBurstCount - 1);
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float offset = side * std::lerp(4.0f, halfWidth * 0.26f, t);
        SlamDustParticle particle;
        particle.width = 8.0f + static_cast<float>(GetRand(12));
        particle.height = 70.0f + static_cast<float>(GetRand(78));
        particle.x = centerX + offset - particle.width * 0.5f;
        particle.y = groundY - particle.height;
        particle.velocityX = side * (58.0f + t * 150.0f + static_cast<float>(GetRand(70)));
        particle.velocityY = -300.0f - static_cast<float>(GetRand(260));
        particle.rotation = side * std::lerp(0.02f, 0.30f, t);
        particle.rotationSpeed = side * (1.5f + t * 2.0f);
        particle.life = 0.28f + static_cast<float>(GetRand(14)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.84f;
        particle.r = 0.82f;
        particle.g = 0.90f;
        particle.b = 1.0f;
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kDustCount; ++index)
    {
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float spreadT = static_cast<float>(index / 2) / static_cast<float>((kDustCount / 2) - 1);
        const float outward = std::lerp(0.08f, 1.16f, spreadT);
        const float jitter = (static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f) * 38.0f;
        const float sizeT = 1.52f - spreadT * 0.46f;

        SlamDustParticle particle;
        particle.width = (58.0f + static_cast<float>(GetRand(46))) * sizeT;
        particle.height = (20.0f + static_cast<float>(GetRand(18))) * sizeT;
        particle.x = centerX + side * outward * halfWidth * 0.50f + jitter - particle.width * 0.5f;
        particle.y = groundY - 16.0f - static_cast<float>(GetRand(24)) - particle.height * 0.35f;
        particle.velocityX = side * (320.0f + outward * 330.0f + static_cast<float>(GetRand(150)));
        particle.velocityY = -72.0f - static_cast<float>(GetRand(120));
        particle.rotation = side * (0.08f + spreadT * 0.18f);
        particle.rotationSpeed = side * (0.7f + spreadT * 0.8f);
        particle.life = kDustLifetime + static_cast<float>(GetRand(12)) * 0.01f;
        particle.maxLife = kDustLifetime;
        particle.alphaScale = 0.86f;
        particle.r = 0.70f;
        particle.g = 0.63f;
        particle.b = 0.52f;
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kChipCount; ++index)
    {
        const float angle = -3.1415926f + (static_cast<float>(index) + 0.5f) / static_cast<float>(kChipCount) * 3.1415926f;
        const float speed = 240.0f + static_cast<float>(GetRand(310));
        SlamDustParticle particle;
        particle.width = 10.0f + static_cast<float>(GetRand(16));
        particle.height = 6.0f + static_cast<float>(GetRand(10));
        particle.x = centerX + (static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f) * clampedWidth * 0.30f - particle.width * 0.5f;
        particle.y = groundY - 10.0f - particle.height * 0.5f;
        particle.velocityX = std::cos(angle) * speed;
        particle.velocityY = -126.0f - std::abs(std::sin(angle)) * (220.0f + static_cast<float>(GetRand(150)));
        particle.rotation = static_cast<float>(GetRand(628)) * 0.01f;
        particle.rotationSpeed = (static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f) * 10.0f;
        particle.life = 0.56f + static_cast<float>(GetRand(16)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.92f;
        particle.r = 0.46f;
        particle.g = 0.40f;
        particle.b = 0.32f;
        m_effects.slamDust.push_back(particle);
    }
}

void GameScene::SpawnBossDefeatStartEffect(float centerX, float groundY, float width)
{
    constexpr int kCoreFlashCount = 6;
    constexpr int kShockStreakCount = 18;
    constexpr int kEmberCount = 28;
    constexpr int kSmokeCount = 34;
    const float clampedWidth = std::max(180.0f, width * 1.45f);
    const float halfWidth = clampedWidth * 0.5f;

    // ボス撃破開始専用。通常スラムより暗く、赤い破片と重い黒煙で差別化する。
    for (int index = 0; index < kCoreFlashCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kCoreFlashCount - 1);
        SlamDustParticle particle;
        particle.width = std::lerp(clampedWidth * 0.24f, clampedWidth * 1.18f, t);
        particle.height = std::lerp(28.0f, 82.0f, t);
        particle.x = centerX - particle.width * 0.5f;
        particle.y = groundY - particle.height * 0.74f;
        particle.velocityX = 0.0f;
        particle.velocityY = -18.0f - t * 34.0f;
        particle.rotation = (index % 2 == 0 ? -1.0f : 1.0f) * (0.04f + t * 0.08f);
        particle.rotationSpeed = particle.rotation * -2.4f;
        particle.life = std::lerp(0.16f, 0.30f, t);
        particle.maxLife = particle.life;
        particle.alphaScale = std::lerp(0.98f, 0.62f, t);
        particle.r = std::lerp(1.0f, 0.58f, t);
        particle.g = std::lerp(0.62f, 0.16f, t);
        particle.b = std::lerp(0.28f, 0.16f, t);
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kShockStreakCount; ++index)
    {
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float spreadT = static_cast<float>(index / 2) / static_cast<float>((kShockStreakCount / 2) - 1);
        const float distance = std::lerp(0.06f, 1.06f, spreadT) * halfWidth;
        SlamDustParticle particle;
        particle.width = std::lerp(126.0f, 58.0f, spreadT) + static_cast<float>(GetRand(28));
        particle.height = std::lerp(18.0f, 7.0f, spreadT);
        particle.x = centerX + side * distance - particle.width * 0.5f;
        particle.y = groundY - particle.height * 0.65f - static_cast<float>(GetRand(8));
        particle.velocityX = side * (520.0f + spreadT * 260.0f + static_cast<float>(GetRand(180)));
        particle.velocityY = -28.0f - static_cast<float>(GetRand(46));
        particle.rotation = side * std::lerp(0.05f, 0.18f, spreadT);
        particle.rotationSpeed = side * 1.25f;
        particle.life = 0.34f + spreadT * 0.14f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.78f;
        particle.r = 0.42f;
        particle.g = 0.12f;
        particle.b = 0.14f;
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kEmberCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kEmberCount - 1);
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float offset = side * std::lerp(8.0f, halfWidth * 0.38f, t);
        SlamDustParticle particle;
        particle.width = 6.0f + static_cast<float>(GetRand(12));
        particle.height = 34.0f + static_cast<float>(GetRand(74));
        particle.x = centerX + offset - particle.width * 0.5f;
        particle.y = groundY - particle.height - static_cast<float>(GetRand(24));
        particle.velocityX = side * (80.0f + t * 210.0f + static_cast<float>(GetRand(120)));
        particle.velocityY = -260.0f - static_cast<float>(GetRand(320));
        particle.rotation = side * std::lerp(0.08f, 0.42f, t);
        particle.rotationSpeed = side * (2.2f + t * 3.4f);
        particle.life = 0.36f + static_cast<float>(GetRand(18)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.86f;
        particle.r = 1.0f;
        particle.g = 0.28f;
        particle.b = 0.12f;
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kSmokeCount; ++index)
    {
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float spreadT = static_cast<float>(index / 2) / static_cast<float>((kSmokeCount / 2) - 1);
        const float jitter = (static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f) * 52.0f;
        SlamDustParticle particle;
        particle.width = std::lerp(92.0f, 148.0f, 1.0f - spreadT) + static_cast<float>(GetRand(48));
        particle.height = std::lerp(32.0f, 58.0f, 1.0f - spreadT) + static_cast<float>(GetRand(22));
        particle.x = centerX + side * std::lerp(0.04f, 0.72f, spreadT) * halfWidth + jitter - particle.width * 0.5f;
        particle.y = groundY - 28.0f - static_cast<float>(GetRand(36)) - particle.height * 0.42f;
        particle.velocityX = side * (170.0f + spreadT * 260.0f + static_cast<float>(GetRand(130)));
        particle.velocityY = -64.0f - static_cast<float>(GetRand(118));
        particle.rotation = side * (0.08f + spreadT * 0.22f);
        particle.rotationSpeed = side * (0.45f + spreadT * 0.9f);
        particle.life = 0.66f + static_cast<float>(GetRand(18)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.72f;
        particle.r = 0.22f;
        particle.g = 0.16f;
        particle.b = 0.18f;
        m_effects.slamDust.push_back(particle);
    }
}

void GameScene::SpawnRushSmokeEffect(float centerX, float groundY, float direction)
{
    const float dir = direction >= 0.0f ? 1.0f : -1.0f;
    constexpr int kSmokeCount = 6;

    for (int index = 0; index < kSmokeCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kSmokeCount - 1);
        SlamDustParticle particle;
        particle.width = std::lerp(36.0f, 76.0f, t) + static_cast<float>(GetRand(18));
        particle.height = std::lerp(22.0f, 48.0f, t) + static_cast<float>(GetRand(12));
        particle.x = centerX - dir * (8.0f + static_cast<float>(GetRand(24)) + t * 22.0f) - particle.width * 0.5f;
        particle.y = groundY - particle.height * 1.18f - 16.0f - static_cast<float>(GetRand(18)) - t * 16.0f;
        particle.velocityX = -dir * (170.0f + static_cast<float>(GetRand(170)) + t * 105.0f);
        particle.velocityY = -132.0f - static_cast<float>(GetRand(110)) - t * 96.0f;
        particle.rotation = -dir * (0.22f + t * 0.32f);
        particle.rotationSpeed = -dir * (1.8f + t * 2.7f);
        particle.life = 0.42f + static_cast<float>(GetRand(14)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = std::lerp(0.88f, 0.52f, t);
        particle.r = 0.64f;
        particle.g = 0.58f;
        particle.b = 0.50f;
        m_effects.slamDust.push_back(particle);
    }
}

void GameScene::SpawnLightLandingEffect(float centerX, float groundY, float width)
{
    constexpr int kDustCount = 10;
    const float clampedWidth = std::max(72.0f, width);
    const float halfWidth = clampedWidth * 0.5f;

    for (int index = 0; index < kDustCount; ++index)
    {
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float spreadT = static_cast<float>(index / 2) / static_cast<float>((kDustCount / 2) - 1);
        SlamDustParticle particle;
        particle.width = std::lerp(24.0f, 46.0f, 1.0f - spreadT) + static_cast<float>(GetRand(12));
        particle.height = std::lerp(8.0f, 17.0f, 1.0f - spreadT) + static_cast<float>(GetRand(5));
        particle.x = centerX + side * std::lerp(0.12f, 0.52f, spreadT) * halfWidth - particle.width * 0.5f;
        particle.y = groundY - particle.height * 0.55f - static_cast<float>(GetRand(7));
        particle.velocityX = side * (120.0f + spreadT * 170.0f + static_cast<float>(GetRand(60)));
        particle.velocityY = -34.0f - static_cast<float>(GetRand(54));
        particle.rotation = side * (0.05f + spreadT * 0.14f);
        particle.rotationSpeed = side * (0.6f + spreadT * 0.7f);
        particle.life = 0.26f + static_cast<float>(GetRand(8)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.58f;
        particle.r = 0.72f;
        particle.g = 0.66f;
        particle.b = 0.56f;
        m_effects.slamDust.push_back(particle);
    }
}

void GameScene::SpawnBossRoarEffect(float centerX, float groundY, float width)
{
    constexpr int kShockCount = 14;
    constexpr int kBurstCount = 12;
    const float clampedWidth = std::max(128.0f, width * 1.18f);
    const float halfWidth = clampedWidth * 0.5f;

    for (int index = 0; index < kShockCount; ++index)
    {
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        const float spreadT = static_cast<float>(index / 2) / static_cast<float>((kShockCount / 2) - 1);
        SlamDustParticle particle;
        particle.width = std::lerp(48.0f, 98.0f, 1.0f - spreadT) + static_cast<float>(GetRand(20));
        particle.height = std::lerp(8.0f, 18.0f, 1.0f - spreadT) + static_cast<float>(GetRand(6));
        particle.x = centerX + side * std::lerp(0.08f, 0.86f, spreadT) * halfWidth - particle.width * 0.5f;
        particle.y = groundY - particle.height * 0.75f - static_cast<float>(GetRand(12));
        particle.velocityX = side * (220.0f + spreadT * 260.0f + static_cast<float>(GetRand(120)));
        particle.velocityY = -42.0f - static_cast<float>(GetRand(76));
        particle.rotation = side * (0.06f + spreadT * 0.18f);
        particle.rotationSpeed = side * (0.9f + spreadT * 1.1f);
        particle.life = 0.34f + static_cast<float>(GetRand(12)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.76f;
        particle.r = 0.94f;
        particle.g = 0.82f;
        particle.b = 0.58f;
        m_effects.slamDust.push_back(particle);
    }

    for (int index = 0; index < kBurstCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kBurstCount - 1);
        const float side = (index % 2 == 0) ? -1.0f : 1.0f;
        SlamDustParticle particle;
        particle.width = 7.0f + static_cast<float>(GetRand(10));
        particle.height = 42.0f + static_cast<float>(GetRand(58));
        particle.x = centerX + side * std::lerp(8.0f, halfWidth * 0.34f, t) - particle.width * 0.5f;
        particle.y = groundY - particle.height - 18.0f - static_cast<float>(GetRand(34));
        particle.velocityX = side * (44.0f + t * 180.0f + static_cast<float>(GetRand(70)));
        particle.velocityY = -170.0f - static_cast<float>(GetRand(190));
        particle.rotation = side * std::lerp(0.08f, 0.36f, t);
        particle.rotationSpeed = side * (1.4f + t * 2.2f);
        particle.life = 0.30f + static_cast<float>(GetRand(12)) * 0.01f;
        particle.maxLife = particle.life;
        particle.alphaScale = 0.68f;
        particle.r = 1.0f;
        particle.g = 0.93f;
        particle.b = 0.76f;
        m_effects.slamDust.push_back(particle);
    }
}

void GameScene::SpawnTeleportTrailEffect(
    float fromX,
    float fromY,
    float toX,
    float toY,
    float width,
    float height,
    const MidBoss2Component::Params& params)
{
    const float sourceCenterX = fromX + width * 0.5f;
    const float sourceCenterY = fromY + height * 0.5f;
    const float targetCenterX = toX + width * 0.5f;
    const float targetCenterY = toY + height * 0.5f;
    const float deltaX = targetCenterX - sourceCenterX;
    const float deltaY = targetCenterY - sourceCenterY;
    const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    const float directionLength = std::max(1.0f, distance);
    const float directionX = deltaX / directionLength;
    const float directionY = deltaY / directionLength;
    const float perpendicularX = -directionY;
    const float perpendicularY = directionX;
    const int totalSparkCount = std::clamp(params.teleportSparkCount, 0, 256);
    const int arrivalSparkCount = totalSparkCount > 0
        ? std::min(totalSparkCount, std::clamp((totalSparkCount + 2) / 4, 1, 24))
        : 0;
    const int trailCount = std::max(0, totalSparkCount - arrivalSparkCount);
    const float minSize = std::clamp(
        std::min(params.teleportSparkMinSize, params.teleportSparkMaxSize),
        0.1f,
        12.0f);
    const float maxSize = std::clamp(
        std::max(params.teleportSparkMinSize, params.teleportSparkMaxSize),
        minSize,
        12.0f);
    const float spreadScale = std::clamp(params.teleportSparkSpreadScale, 0.0f, 8.0f);
    const float sparkLifetime = std::clamp(params.teleportSparkLifetime, 0.01f, 5.0f);
    const float travelTime = std::max(0.26f, distance / 1120.0f);
    const float travelSpeed = distance / travelTime;
    const auto randomSizeScale = [&]()
    {
        return std::lerp(minSize, maxSize, static_cast<float>(GetRand(1000)) / 1000.0f);
    };

    for (int index = 0; index < trailCount; ++index)
    {
        const float t = trailCount <= 1 ? 1.0f : static_cast<float>(index) / static_cast<float>(trailCount - 1);
        const float jitterSeed = static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f;
        const float wave = std::sin(t * 6.28318530718f * 2.0f);
        const float lineX = std::lerp(sourceCenterX, targetCenterX, t);
        const float lineY = std::lerp(sourceCenterY, targetCenterY, t);
        const float spread = std::lerp(6.0f, 18.0f, 1.0f - std::fabs(0.5f - t) * 2.0f) * spreadScale;

        LaserSparkParticle spark;
        spark.x = lineX + perpendicularX * jitterSeed * spread + directionX * wave * 4.0f;
        spark.y = lineY + perpendicularY * jitterSeed * spread + directionY * wave * 4.0f;
        spark.velocityX = directionX * (travelSpeed + jitterSeed * 120.0f) + perpendicularX * jitterSeed * 90.0f * spreadScale;
        spark.velocityY = directionY * (travelSpeed + jitterSeed * 120.0f) + perpendicularY * jitterSeed * 90.0f * spreadScale;
        spark.life = sparkLifetime;
        spark.maxLife = spark.life;
        spark.gravityScale = 0.0f;
        spark.sizeScale = randomSizeScale();
        spark.drawCircle = true;
        spark.r = 0.58f;
        spark.g = 0.92f;
        spark.b = 1.0f;
        m_effects.laserSparks.push_back(spark);
    }

    for (int index = 0; index < arrivalSparkCount; ++index)
    {
        const float angle = static_cast<float>(index) / static_cast<float>(arrivalSparkCount) * 6.28318530718f;
        const float radius = (10.0f + static_cast<float>(GetRand(8))) * spreadScale;

        LaserSparkParticle spark;
        spark.x = targetCenterX + std::cos(angle) * radius;
        spark.y = targetCenterY + std::sin(angle) * radius;
        spark.velocityX = std::cos(angle) * (40.0f + static_cast<float>(GetRand(40)));
        spark.velocityY = std::sin(angle) * (40.0f + static_cast<float>(GetRand(40)));
        spark.life = sparkLifetime;
        spark.maxLife = spark.life;
        spark.gravityScale = 0.0f;
        spark.sizeScale = randomSizeScale();
        spark.drawCircle = true;
        spark.r = 0.88f;
        spark.g = 0.98f;
        spark.b = 1.0f;
        m_effects.laserSparks.push_back(spark);
    }
}

void GameScene::SpawnMidBoss2SpearFadeEffect(float centerX, float centerY, float width, float height)
{
    constexpr float kPi = 3.1415926535f;
    const float spreadX = std::max(10.0f, width * 0.22f);
    const float spreadY = std::max(10.0f, height * 0.18f);

    constexpr int kBubbleCount = 18;
    for (int index = 0; index < kBubbleCount; ++index)
    {
        const float angle = (static_cast<float>(GetRand(1000)) / 1000.0f) * kPi * 2.0f;
        const float radius = std::lerp(0.2f, 1.0f, static_cast<float>(GetRand(1000)) / 1000.0f);
        const float sideJitter = static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f;

        MidBoss2SpearMistParticle particle;
        particle.x = centerX + std::cos(angle) * spreadX * radius;
        particle.y = centerY + std::sin(angle) * spreadY * radius;
        particle.velocityX = std::cos(angle) * (12.0f + static_cast<float>(GetRand(30))) + sideJitter * 18.0f;
        particle.velocityY = -(70.0f + static_cast<float>(GetRand(110))) - radius * 46.0f;
        particle.life = 0.42f + static_cast<float>(GetRand(28)) * 0.01f;
        particle.maxLife = particle.life;
        particle.sizeScale = 0.34f + static_cast<float>(GetRand(32)) * 0.01f;
        particle.pulsePhase = static_cast<float>(GetRand(1000)) * 0.0062831853f;
        particle.r = 0.76f + static_cast<float>(GetRand(18)) * 0.01f;
        particle.g = 0.93f + static_cast<float>(GetRand(7)) * 0.01f;
        particle.b = 1.0f;
        m_effects.midBoss2SpearMist.push_back(particle);
    }

    constexpr int kTinyCount = 8;
    for (int index = 0; index < kTinyCount; ++index)
    {
        const float angle = (static_cast<float>(GetRand(1000)) / 1000.0f) * kPi * 2.0f;
        const float sideJitter = static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f;

        MidBoss2SpearMistParticle particle;
        particle.x = centerX + sideJitter * spreadX * 0.6f;
        particle.y = centerY + sideJitter * spreadY * 0.35f;
        particle.velocityX = std::cos(angle) * (8.0f + static_cast<float>(GetRand(22))) + sideJitter * 12.0f;
        particle.velocityY = -(110.0f + static_cast<float>(GetRand(90)));
        particle.life = 0.26f + static_cast<float>(GetRand(16)) * 0.01f;
        particle.maxLife = particle.life;
        particle.sizeScale = 0.18f + static_cast<float>(GetRand(18)) * 0.01f;
        particle.pulsePhase = static_cast<float>(GetRand(1000)) * 0.0062831853f;
        particle.r = 0.90f;
        particle.g = 0.98f;
        particle.b = 1.0f;
        m_effects.midBoss2SpearMist.push_back(particle);
    }
}

void GameScene::SpawnMidBoss3FistImpactEffect(float x, float y, float width, float height)
{
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    constexpr float kPi = 3.1415926535f;

    constexpr int kHotSparkCount = 34;
    for (int index = 0; index < kHotSparkCount; ++index)
    {
        const float baseAngle = (static_cast<float>(index) / static_cast<float>(kHotSparkCount)) * kPi * 2.0f;
        const float angle = baseAngle + (static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f) * 0.95f;
        const float speed = 220.0f + static_cast<float>(GetRand(340));
        const float spawnRadius = 10.0f + static_cast<float>(GetRand(28));

        LaserSparkParticle spark;
        spark.x = centerX + std::cos(angle) * spawnRadius;
        spark.y = centerY + std::sin(angle) * spawnRadius;
        spark.velocityX = std::cos(angle) * speed;
        spark.velocityY = std::sin(angle) * speed - 90.0f;
        spark.life = 0.26f + static_cast<float>(GetRand(18)) * 0.01f;
        spark.maxLife = spark.life;
        spark.gravityScale = 0.14f;
        spark.r = 1.0f;
        spark.g = 0.58f + static_cast<float>(GetRand(28)) * 0.01f;
        spark.b = 0.16f;
        m_effects.laserSparks.push_back(spark);
    }

    constexpr int kPlasmaCount = 18;
    for (int index = 0; index < kPlasmaCount; ++index)
    {
        const float angle = (static_cast<float>(GetRand(1000)) / 1000.0f) * kPi * 2.0f;
        const float speed = 130.0f + static_cast<float>(GetRand(230));

        LaserSparkParticle spark;
        spark.x = centerX + static_cast<float>(GetRand(25)) - 12.0f;
        spark.y = centerY + static_cast<float>(GetRand(25)) - 12.0f;
        spark.velocityX = std::cos(angle) * speed;
        spark.velocityY = std::sin(angle) * speed;
        spark.life = 0.18f + static_cast<float>(GetRand(12)) * 0.01f;
        spark.maxLife = spark.life;
        spark.gravityScale = 0.0f;
        const bool blue = index % 2 == 0;
        spark.r = blue ? 0.52f : 0.96f;
        spark.g = blue ? 0.86f : 0.48f;
        spark.b = 1.0f;
        m_effects.laserSparks.push_back(spark);
    }

    constexpr int kPlasmaStreakCount = 10;
    for (int index = 0; index < kPlasmaStreakCount; ++index)
    {
        const float angle = (static_cast<float>(GetRand(1000)) / 1000.0f) * kPi * 2.0f;
        const float length = 48.0f + static_cast<float>(GetRand(54));
        SlamDustParticle streak;
        streak.width = length;
        streak.height = 5.0f + static_cast<float>(GetRand(5));
        streak.x = centerX - streak.width * 0.5f + static_cast<float>(GetRand(37)) - 18.0f;
        streak.y = centerY - streak.height * 0.5f + static_cast<float>(GetRand(37)) - 18.0f;
        streak.velocityX = std::cos(angle) * (80.0f + static_cast<float>(GetRand(120)));
        streak.velocityY = std::sin(angle) * (80.0f + static_cast<float>(GetRand(120)));
        streak.rotation = angle;
        streak.rotationSpeed = (index % 2 == 0 ? -1.0f : 1.0f) * (1.4f + static_cast<float>(GetRand(100)) * 0.01f);
        streak.life = 0.16f + static_cast<float>(GetRand(10)) * 0.01f;
        streak.maxLife = streak.life;
        streak.alphaScale = 0.88f;
        const bool blue = index % 2 == 0;
        streak.r = blue ? 0.42f : 0.88f;
        streak.g = blue ? 0.82f : 0.34f;
        streak.b = 1.0f;
        m_effects.slamDust.push_back(streak);
    }

    constexpr int kSmokeCount = 20;
    for (int index = 0; index < kSmokeCount; ++index)
    {
        const float angle = (static_cast<float>(GetRand(1000)) / 1000.0f) * kPi * 2.0f;
        const float speed = 22.0f + static_cast<float>(GetRand(88));
        const float size = 34.0f + static_cast<float>(GetRand(58));

        SlamDustParticle smoke;
        smoke.width = size * (0.85f + static_cast<float>(GetRand(30)) * 0.01f);
        smoke.height = size * (0.55f + static_cast<float>(GetRand(30)) * 0.01f);
        smoke.x = centerX - smoke.width * 0.5f + static_cast<float>(GetRand(57)) - 28.0f;
        smoke.y = centerY - smoke.height * 0.5f + static_cast<float>(GetRand(43)) - 24.0f;
        smoke.velocityX = std::cos(angle) * speed;
        smoke.velocityY = -46.0f - static_cast<float>(GetRand(70)) + std::sin(angle) * speed * 0.35f;
        smoke.rotation = (static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f) * 0.5f;
        smoke.rotationSpeed = (static_cast<float>(GetRand(1000)) / 1000.0f - 0.5f) * 1.4f;
        smoke.life = 0.55f + static_cast<float>(GetRand(24)) * 0.01f;
        smoke.maxLife = smoke.life;
        smoke.alphaScale = 0.46f;
        const float shade = 0.08f + static_cast<float>(GetRand(8)) * 0.01f;
        smoke.r = shade;
        smoke.g = shade;
        smoke.b = shade + 0.02f;
        m_effects.slamDust.push_back(smoke);
    }
}

void GameScene::UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame)
{
    game_scene_player_visual_system::UpdatePresentation(
        m_player,
        player,
        deltaTime,
        moveAxis,
        wasGrounded,
        isDodging,
        landedThisFrame);
}

void GameScene::UpdateTuningPanel()
{
    if (!m_debug.showTuningPanel)
    {
        return;
    }

    auto entries = BuildGameSceneTuningEntries();
    const int kEntryCount = static_cast<int>(entries.size());

    if (Input_IsActionPressed(InputAction::MoveUp))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + kEntryCount - 1) % kEntryCount;
    }
    if (Input_IsActionPressed(InputAction::MoveDown))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + 1) % kEntryCount;
    }

    float delta = 0.0f;
    if (Input_IsActionDown(InputAction::MoveLeft))
    {
        delta -= entries[m_debug.tuningSelection].step;
    }
    if (Input_IsActionDown(InputAction::MoveRight))
    {
        delta += entries[m_debug.tuningSelection].step;
    }

    if (delta != 0.0f)
    {
        *entries[m_debug.tuningSelection].value = std::clamp(
            *entries[m_debug.tuningSelection].value + delta,
            entries[m_debug.tuningSelection].minValue,
            entries[m_debug.tuningSelection].maxValue);
        WriteTuningJsonFile();
    }

    if (!Input_IsMouseLeftPressed())
    {
        return;
    }

    const float mouseX = static_cast<float>(Input_GetMouseX());
    const float mouseY = static_cast<float>(Input_GetMouseY());
    if (!IsPointInside(mouseX, mouseY, kTuningPanelX, kTuningPanelY, kTuningPanelWidth, kTuningPanelHeight))
    {
        return;
    }

    for (int index = 0; index < kEntryCount; ++index)
    {
        const auto layout = GetTuningRowLayout(index);
        const float rowY = layout.y;
        if (IsPointInside(mouseX, mouseY, kTuningPanelX + kTuningMinusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight))
        {
            m_debug.tuningSelection = index;
            *entries[index].value = std::clamp(
                *entries[index].value - entries[index].step,
                entries[index].minValue,
                entries[index].maxValue);
            WriteTuningJsonFile();
            return;
        }
        if (IsPointInside(mouseX, mouseY, kTuningPanelX + kTuningPlusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight))
        {
            m_debug.tuningSelection = index;
            *entries[index].value = std::clamp(
                *entries[index].value + entries[index].step,
                entries[index].minValue,
                entries[index].maxValue);
            WriteTuningJsonFile();
            return;
        }
    }
}

void GameScene::UpdatePlayer(float deltaTime)
{
    Entity* player = FindEntityByTag(kTagPlayer);
    if (!player)
    {
        return;
    }

    auto* transform = player->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    const bool shieldBossIntroActive = IsShieldBossIntroCinematicActive();
    // Boss intro locks the player so the entrance reads like a short cutscene.
    const bool blockPlayerInput = m_photo.placement.active || shieldBossIntroActive;
    const auto controls = game_scene_player_system::SampleControls(blockPlayerInput);
    const float moveAxis = controls.moveAxis;
    const float tileSize = m_tileMap.GetTileSize();
    const float playerWidth = transform->width * transform->scale;
    const float playerHeight = transform->height * transform->scale;
    const bool wasGrounded = m_player.grounded || IsStandingOnGround(*transform);

    if (shieldBossIntroActive)
    {
        m_player.velocityX = 0.0f;
        m_player.velocityY = 0.0f;
        m_player.dodgeRemaining = 0.0f;
        UpdatePlayerAfterimages(deltaTime);
        UpdatePlayerPresentation(*player, deltaTime, 0.0f, wasGrounded, false, false);
        return;
    }

    const float dodgeDuration = wasGrounded
        ? GetPlayerDodgeDuration()
        : (gPlayerDodgeSpeed > 0.0f ? tileSize / gPlayerDodgeSpeed : 0.0f);

    game_scene_player_system::TickDodgeState(m_player, deltaTime);
    UpdatePlayerAfterimages(deltaTime);

    game_scene_player_system::UpdateFacingFromMoveAxis(m_player, moveAxis);

    if (controls.dodgePressed &&wasGrounded&& game_scene_player_system::TryBeginDodge(
        m_player,
        moveAxis,
        dodgeDuration,
        gPlayerDodgeCooldown))
    {
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Player dodged", 0.0f, 0.0f });
    }

    const bool isDodging = m_player.dodgeRemaining > 0.0f;
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const bool canJumpNow = !isDodging &&
        controls.jumpPressed &&
        (wasGrounded || m_player.coyoteTimeRemaining > 0.0f);
    if (canJumpNow)
    {
        m_player.velocityY = gPlayerJumpSpeed;
        m_player.grounded = false;
        m_player.coyoteTimeRemaining = 0.0f;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
    }

    std::vector<TransformComponent> photoBoxes;
    GetPhotoBoxBounds(photoBoxes);
    std::vector<TransformComponent> groundPlatformsForSnap;
    GetGroundPlatformBounds(groundPlatformsForSnap);
    std::vector<TransformComponent> solidObjects;
    BuildPlayerSolidObjectBounds(solidObjects);

    const float targetHorizontalVelocity = game_scene_player_system::GetHorizontalVelocity(
        m_player,
        moveAxis,
        gPlayerDodgeSpeed,
        gPlayerMoveSpeed);
    const float estimatedHorizontalVelocity =
        !isDodging &&
            !wasGrounded &&
            std::fabs(m_player.velocityX) > std::fabs(targetHorizontalVelocity) + 1.0f
            ? m_player.velocityX
            : targetHorizontalVelocity;
    const float estimatedVerticalVelocity = canJumpNow
        ? gPlayerJumpSpeed
        : std::min(gPlayerMaxFallSpeed, m_player.velocityY + gPlayerGravity * deltaTime);
    const float maxDisplacement = std::max(
        std::fabs(estimatedHorizontalVelocity) * deltaTime,
        std::fabs(estimatedVerticalVelocity) * deltaTime);
    const int subSteps = std::clamp(static_cast<int>(std::ceil(maxDisplacement / 8.0f)), 1, 8);
    const float stepDeltaTime = deltaTime / static_cast<float>(subSteps);

    bool groundedAtStepStart = wasGrounded;
    for (int stepIndex = 0; stepIndex < subSteps; ++stepIndex)
    {
        float horizontalVelocity = targetHorizontalVelocity;
        if (!isDodging &&
            !groundedAtStepStart &&
            std::fabs(m_player.velocityX) > std::fabs(targetHorizontalVelocity) + 1.0f)
        {
            const float airLaunchBlend = std::clamp(4.0f * stepDeltaTime, 0.0f, 1.0f);
            horizontalVelocity = m_player.velocityX + (targetHorizontalVelocity - m_player.velocityX) * airLaunchBlend;
        }
        m_player.velocityX = horizontalVelocity;
        m_player.grounded = groundedAtStepStart;
        if (groundedAtStepStart)
        {
            m_player.coyoteTimeRemaining = gCoyoteTimeSeconds;
        }
        else
        {
            m_player.coyoteTimeRemaining = std::max(0.0f, m_player.coyoteTimeRemaining - stepDeltaTime);
        }

        if (groundedAtStepStart && m_player.velocityY > 0.0f)
        {
            m_player.velocityY = 0.0f;
        }

        if (m_player.grounded && !canJumpNow)
        {
            m_player.velocityY = 0.0f;
        }
        else
        {
            m_player.velocityY = std::min(gPlayerMaxFallSpeed, m_player.velocityY + gPlayerGravity * stepDeltaTime);
        }

        const float previousX = transform->x;
        const float previousY = transform->y;
        const float previousBottom = previousY + playerHeight;
        const float verticalSnapDistance = std::max(gGroundSnapDistance, std::fabs(m_player.velocityY) * stepDeltaTime + 4.0f);
        const game_scene_player_movement_system::PlayerMovementContext movementContext{
            stepDeltaTime,
            tileSize,
            playerWidth,
            playerHeight,
            mapWidth,
            mapHeight,
            previousX,
            previousY,
            previousBottom,
            verticalSnapDistance,
        };
        const float movementHorizontalVelocity = m_player.velocityX;
        const auto intersectsPhotoBoxForHorizontalMove = [this](const TransformComponent& candidate)
        {
            // Keep horizontal PhotoBox checks at the real player position so side contacts stay solid.
            return IntersectsSolidPhotoBoxForMovement(candidate);
        };

        game_scene_player_movement_system::ResolveHorizontalTileCollisions(
            *transform,
            m_player,
            movementContext,
            [this, movementHorizontalVelocity](int column, int row)
            {
                return movementHorizontalVelocity > 0.0f
                    ? IsTileBlockingFromLeft(column, row)
                    : IsTileBlockingFromRight(column, row);
            },
            [&intersectsPhotoBoxForHorizontalMove](const TransformComponent& candidate)
            {
                return intersectsPhotoBoxForHorizontalMove(candidate);
            });

        game_scene_player_movement_system::ResolveHorizontalObjectCollisions(
            *transform,
            m_player,
            movementContext,
            photoBoxes,
            [&intersectsPhotoBoxForHorizontalMove](const TransformComponent& candidate)
            {
                return intersectsPhotoBoxForHorizontalMove(candidate);
            },
            solidObjects);

        if (m_player.velocityY >= 0.0f && groundedAtStepStart)
        {
            if (TrySnapToGroundUsingPlatforms(*transform, verticalSnapDistance, groundPlatformsForSnap))
            {
                m_player.grounded = true;
            }
        }

        game_scene_player_movement_system::ResolveVerticalMotion(
            *transform,
            m_player,
            groundedAtStepStart,
            movementContext,
            photoBoxes,
            solidObjects,
            [this](int column, int row)
            {
                return IsSolidTile(column, row);
            },
            [this](int column, int row)
            {
                return IsPlatformTile(column, row);
            },
            [this](int column, int row)
            {
                return IsSolidTile(column, row) || IsSlopeTile(column, row);
            },
            [this, &groundPlatformsForSnap](TransformComponent& targetTransform, float snapDistance)
            {
                return TrySnapToGroundUsingPlatforms(targetTransform, snapDistance, groundPlatformsForSnap);
            },
            [this](const TransformComponent& candidate)
            {
                return IntersectsSolidPhotoBoxForMovement(candidate);
            });

        groundedAtStepStart = m_player.grounded;

        if (isDodging)
        {
            TrySpawnPlayerAfterimage(*transform);
        }
    }

    const bool landedThisFrame = !wasGrounded && m_player.grounded;
    UpdatePlayerPresentation(*player, deltaTime, moveAxis, wasGrounded, isDodging, landedThisFrame);

    if (IsMidBoss3IntroCinematicActive())
    {
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            const auto* enemy = entity->GetComponent<EnemyComponent>();
            const auto* boss = entity->GetComponent<MidBoss3Component>();
            const auto* bossTransform = entity->GetComponent<TransformComponent>();
            if (!enemy ||
                !boss ||
                !bossTransform ||
                enemy->GetArchetype() != EnemyArchetype::MidBoss3 ||
                enemy->IsDefeated())
            {
                continue;
            }

            const float bossWidth = bossTransform->width * bossTransform->scale;
            const float bossHeight = bossTransform->height * bossTransform->scale;
            const float visibleWidth = GetCameraFollowSpanX(m_tileMap);
            const float visibleHeight = GetCameraVisibleHeight(m_tileMap);
            const float maxCameraX = std::max(0.0f, mapWidth - visibleWidth);
            const float maxCameraY = std::max(0.0f, mapHeight - visibleHeight);
            const float targetCameraX = std::clamp(
                bossTransform->x + bossWidth * 0.5f - visibleWidth * 0.5f,
                0.0f,
                maxCameraX);
            const float targetCameraY = std::clamp(
                bossTransform->y + bossHeight * 0.5f - visibleHeight * 0.5f,
                0.0f,
                maxCameraY);
            const float followRate = std::clamp(5.0f * deltaTime, 0.0f, 1.0f);
            m_flow.cameraX += (targetCameraX - m_flow.cameraX) * followRate;
            m_flow.cameraY += (targetCameraY - m_flow.cameraY) * followRate;
            m_camera.midBoss3CameraYLockInitialized = false;
            m_camera.midBoss3CameraYLock = 0.0f;
            return;
        }
    }

    const bool shieldBossIntroReturnActive = m_render.bossIntroCameraInfluence > 0.01f;
    const float shieldBossIntroReturnStartX = m_flow.cameraX;
    const float shieldBossIntroReturnStartY = m_flow.cameraY;

    const bool stabilizeMidBoss3CameraY = IsMidBoss3CameraStabilizeStage(m_lifecycle.currentMapCsvPath);
    const bool shieldBossCameraActive =
        IsShieldBossIntroCinematicActive() ||
        IsShieldBossBattleCameraActive();
    const bool useDeadZoneVerticalCamera =
        gCameraFollowY >= 0.5f &&
        !stabilizeMidBoss3CameraY &&
        !shieldBossCameraActive;
    const bool offsetCameraX = !stabilizeMidBoss3CameraY && !shieldBossCameraActive;
    const float cameraYBeforeFollow = m_flow.cameraY;
    if (stabilizeMidBoss3CameraY)
    {
        const float visibleHeight = GetCameraVisibleHeight(m_tileMap);
        const float maxCameraY = std::max(0.0f, mapHeight - visibleHeight);
        if (!m_camera.midBoss3CameraYLockInitialized)
        {
            m_camera.midBoss3CameraYLockInitialized = true;
            m_camera.midBoss3CameraYLock = std::clamp(m_flow.cameraY, 0.0f, maxCameraY);
        }
    }
    else
    {
        m_camera.midBoss3CameraYLockInitialized = false;
        m_camera.midBoss3CameraYLock = 0.0f;
    }

    const bool playerMovingHorizontally =
        std::fabs(moveAxis) > 0.01f ||
        std::fabs(m_player.velocityX) > 1.0f ||
        isDodging;
    if (offsetCameraX)
    {
        OffsetCameraX(m_player.facingRight, playerMovingHorizontally, deltaTime);
    }
    else
    {
        m_camera.cameraOffsetX = 0.0f;
    }

    const float viewScale = std::max(0.0001f, GetViewScale());
    const float screenCenteredCameraWidth = std::clamp(
        (static_cast<float>(SCREEN_WIDTH) - GetViewOriginX() * 2.0f) / viewScale,
        1.0f,
        gCameraViewWidth);
    const float cameraFollowSpeedX =
        playerMovingHorizontally && std::fabs(m_camera.cameraOffsetX) > 0.5f
        ? std::min(gCameraFollowSpeedX, std::max(0.01f, gCameraLookAheadCatchUpSpeedX))
        : gCameraFollowSpeedX;

    game_scene_player_movement_system::UpdateCamera(
        m_flow.cameraX,
        m_flow.cameraY,
        transform->x,
        transform->y,
        playerWidth,
        playerHeight,
        screenCenteredCameraWidth,
        GetCameraVisibleHeight(m_tileMap),
        mapWidth,
        mapHeight,
        GetCameraFollowOffsetY(m_tileMap) + gCameraTargetOffsetY,
        gCameraTargetOffsetX,
        deltaTime,
        cameraFollowSpeedX,
        gCameraFollowY >= 0.5f && !stabilizeMidBoss3CameraY,
        !useDeadZoneVerticalCamera,
        offsetCameraX,
        m_camera.cameraOffsetX);
    if (stabilizeMidBoss3CameraY)
    {
        m_flow.cameraY = cameraYBeforeFollow;
    }
    if (useDeadZoneVerticalCamera)
    {
        UpdateCameraByMarkers(*transform, deltaTime);
    }
    if (!offsetCameraX)
    {
        m_camera.cameraYRecenteringStrength = 0.0f;
    }
    if (shieldBossIntroReturnActive && !stabilizeMidBoss3CameraY)
    {
        // Point 3: ease from the cinematic point back to the normal gameplay camera.
        const float normalCameraX = m_flow.cameraX;
        const float normalCameraY = m_flow.cameraY;
        const float returnBlend = 1.0f - std::pow(0.001f, deltaTime * 0.95f);
        m_flow.cameraX = std::lerp(shieldBossIntroReturnStartX, normalCameraX, returnBlend);
        m_flow.cameraY = std::lerp(shieldBossIntroReturnStartY, normalCameraY, returnBlend);
    }

    if (stabilizeMidBoss3CameraY)
    {
        const float visibleHeight = GetCameraVisibleHeight(m_tileMap);
        const float maxCameraY = std::max(0.0f, mapHeight - visibleHeight);
        const float targetY = std::clamp(
            transform->y + playerHeight * 0.5f - visibleHeight * 0.5f + GetCameraFollowOffsetY(m_tileMap) + gCameraTargetOffsetY,
            0.0f,
            maxCameraY);
        const float tileSize = std::max(1.0f, m_tileMap.GetTileSize());
        const float deadZone = tileSize * 1.75f;
        const float deltaY = targetY - m_camera.midBoss3CameraYLock;
        if (m_player.grounded && std::fabs(deltaY) > deadZone)
        {
            const float desiredY = targetY - std::copysign(deadZone, deltaY);
            const float followRate = std::clamp(3.0f * deltaTime, 0.0f, 1.0f);
            m_camera.midBoss3CameraYLock += (desiredY - m_camera.midBoss3CameraYLock) * followRate;
        }
        m_flow.cameraY = std::clamp(m_camera.midBoss3CameraYLock, 0.0f, maxCameraY);
        return;
    }

    // Safety clamp: keep the player inside the vertical camera view even when marker/fixed-lock
    // transitions are active, so the player never drops out of frame.
    if (gCameraFollowY >= 0.5f)
    {
        const float visibleHeight = GetCameraVisibleHeight(m_tileMap);
        const float maxCameraY = std::max(0.0f, mapHeight - visibleHeight);
        const float tileSizeForMargin = std::max(1.0f, m_tileMap.GetTileSize());
        const float topMargin = tileSizeForMargin * 1.0f;
        const float bottomMargin = tileSizeForMargin * 1.5f;
        const float playerTop = transform->y;
        const float playerBottom = transform->y + playerHeight;

        // Hard catch-up for downward movement: if the player is close to leaving the lower edge,
        // snap camera Y enough to keep them inside a stable margin.
        const float lowerLimitY = m_flow.cameraY + visibleHeight - bottomMargin;
        if (playerBottom > lowerLimitY)
        {
            const float requiredCameraY = playerBottom - (visibleHeight - bottomMargin);
            m_flow.cameraY = std::max(m_flow.cameraY, requiredCameraY);
        }

        const float minAllowedCameraY = playerBottom - (visibleHeight - bottomMargin);
        const float maxAllowedCameraY = playerTop - topMargin;
        const float clampedToPlayerY = std::clamp(m_flow.cameraY, minAllowedCameraY, maxAllowedCameraY);
        m_flow.cameraY = std::clamp(clampedToPlayerY, 0.0f, maxCameraY);
    }
}

void GameScene::BuildPlayerSolidObjectBounds(std::vector<TransformComponent>& bounds) const
{
    bounds.clear();

    GetGroundPlatformBounds(bounds);

    std::vector<TransformComponent> batteryBounds;
    GetEntityBoundsByTag(kTagBattery, batteryBounds);
    bounds.insert(bounds.end(), batteryBounds.begin(), batteryBounds.end());

    std::vector<TransformComponent> logBounds;
    GetEntityBoundsByTag(kTagLog, logBounds);
    bounds.insert(bounds.end(), logBounds.begin(), logBounds.end());

    std::vector<TransformComponent> enemyBounds;
    GetEntityBoundsByTag(kTagEnemy, enemyBounds);
    bounds.insert(bounds.end(), enemyBounds.begin(), enemyBounds.end());
}

void GameScene::UpdateBarrels(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    const float tileSize = m_tileMap.GetTileSize();
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float activeLeft = std::max(0.0f, m_flow.cameraX - gBarrelActivationPaddingX);
    const float activeRight = std::min(mapWidth, m_flow.cameraX + gCameraViewWidth + gBarrelActivationPaddingX);
    const float activationDistance = tileSize * 10.0f;

    auto setBarrelVisible = [](Entity& barrelEntity, bool visible)
    {
        if (auto* tint = barrelEntity.GetComponent<TintComponent>())
        {
            tint->a = visible ? 1.0f : 0.0f;
        }
    };

    auto resetBarrel = [&](Entity& barrelEntity, BarrelComponent& barrel, TransformComponent& transform)
    {
        SpawnBarrelBreakEffect(transform.x, transform.y, transform.width * transform.scale, transform.height * transform.scale);
        m_eventBus.Publish({ EventType::PlaySoundRequest, &barrelEntity, nullptr, "barrel", 0.0f, 0.0f });
        if (!barrel.respawnEnabled)
        {
            barrel.destroyed = true;
        }
        barrel.active = false;
        barrel.cooldownActive = barrel.respawnEnabled && !barrel.destroyed;
        barrel.cooldownRemaining = barrel.cooldownActive ? 3.0f : 0.0f;
        barrel.velocityX = 0.0f;
        barrel.velocityY = 0.0f;
        barrel.grounded = false;
        barrel.accumulatedFallDistance = 0.0f;
        setBarrelVisible(barrelEntity, false);
    };

    std::vector<Entity*> barrelCollisionCandidates;
    barrelCollisionCandidates.reserve(
        m_world.EntitiesByTag(EntityTag::PhotoBox).size() +
        m_world.EntitiesByTag(EntityTag::Battery).size() +
        m_world.EntitiesByTag(EntityTag::BatterySwitch).size() +
        m_world.EntitiesByTag(EntityTag::Elevator).size() +
        m_world.EntitiesByTag(EntityTag::LaserSwitch).size() +
        m_world.EntitiesByTag(EntityTag::Shutter).size() +
        m_world.EntitiesByTag(EntityTag::ProtectiveWall).size() +
        m_world.EntitiesByTag(EntityTag::LaserTurret).size() +
        m_world.EntitiesByTag(EntityTag::LaserBeam).size() +
        m_world.EntitiesByTag(EntityTag::StageLight).size() +
        m_world.EntitiesByTag(EntityTag::MarkerLight).size() +
        m_world.EntitiesByTag(EntityTag::SepiaRubble).size() +
        m_world.EntitiesByTag(EntityTag::SepiaElevator).size() +
        m_world.EntitiesByTag(EntityTag::Shield).size() +
        m_world.EntitiesByTag(EntityTag::BossShield).size() +
        m_world.EntitiesByTag(EntityTag::Boss1Shield).size() +
        m_world.EntitiesByTag(EntityTag::MidBoss1Shield).size() +
        m_world.EntitiesByTag(EntityTag::CapturedShield).size() +
        m_world.EntitiesByTag(EntityTag::Barrel).size() +
        m_world.EntitiesByTag(EntityTag::Log).size());
    auto appendBarrelCollisionCandidates = [&](EntityTag tag)
    {
        for (Entity* candidate : m_world.EntitiesByTag(tag))
        {
            if (candidate)
            {
                barrelCollisionCandidates.push_back(candidate);
            }
        }
    };
    appendBarrelCollisionCandidates(EntityTag::PhotoBox);
    appendBarrelCollisionCandidates(EntityTag::Battery);
    appendBarrelCollisionCandidates(EntityTag::BatterySwitch);
    appendBarrelCollisionCandidates(EntityTag::Elevator);
    appendBarrelCollisionCandidates(EntityTag::LaserSwitch);
    appendBarrelCollisionCandidates(EntityTag::Shutter);
    appendBarrelCollisionCandidates(EntityTag::ProtectiveWall);
    appendBarrelCollisionCandidates(EntityTag::LaserTurret);
    appendBarrelCollisionCandidates(EntityTag::LaserBeam);
    appendBarrelCollisionCandidates(EntityTag::StageLight);
    appendBarrelCollisionCandidates(EntityTag::MarkerLight);
    appendBarrelCollisionCandidates(EntityTag::SepiaRubble);
    appendBarrelCollisionCandidates(EntityTag::SepiaElevator);
    appendBarrelCollisionCandidates(EntityTag::Shield);
    appendBarrelCollisionCandidates(EntityTag::BossShield);
    appendBarrelCollisionCandidates(EntityTag::Boss1Shield);
    appendBarrelCollisionCandidates(EntityTag::MidBoss1Shield);
    appendBarrelCollisionCandidates(EntityTag::CapturedShield);
    appendBarrelCollisionCandidates(EntityTag::Barrel);
    appendBarrelCollisionCandidates(EntityTag::Log);

    auto isBarrelObjectCollision = [&](const Entity& barrelEntity, const TransformComponent& barrelBounds, Entity*& outHit) -> bool
    {
        outHit = nullptr;
        for (Entity* candidate : barrelCollisionCandidates)
        {
            if (!candidate || candidate == &barrelEntity)
            {
                continue;
            }

            if (candidate->GetComponent<BarrelComponent>())
            {
                continue;
            }

            if (HasTag(*candidate, kTagPlayer) || HasTag(*candidate, kTagEnemy) || HasTag(*candidate, kTagBullet))
            {
                continue;
            }

            if (HasTag(*candidate, kTagPhotoBox))
            {
                const auto* layer = candidate->GetComponent<PhotoCopyLayerComponent>();
                if (layer && layer->layer != PhotoCopyLayer::Foreground)
                {
                    continue;
                }
            }

            const auto* otherTransform = candidate->GetComponent<TransformComponent>();
            if (!otherTransform)
            {
                continue;
            }

            if (IntersectsRect(barrelBounds, *otherTransform))
            {
                outHit = candidate;
                return true;
            }
        }

        return false;
    };

    auto intersectsDespawnTile = [&](const TransformComponent& bounds) -> bool
    {
        const float width = bounds.width * bounds.scale;
        const float height = bounds.height * bounds.scale;
        const int left = std::max(0, static_cast<int>((bounds.x + 2.0f) / tileSize));
        const int right = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((bounds.x + width - 2.0f) / tileSize));
        const int top = std::max(0, static_cast<int>((bounds.y + 2.0f) / tileSize));
        const int bottom = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bounds.y + height - 2.0f) / tileSize));
        for (int row = top; row <= bottom; ++row)
        {
            for (int column = left; column <= right; ++column)
            {
                const int tileValue = m_tileMap.GetTile(column, row);
                if (tileValue != 1 && tileValue != TileMap::kPitTileValue)
                {
                    continue;
                }

                TransformComponent tileBounds(
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize,
                    tileSize,
                    tileSize);
                if (IntersectsRect(bounds, tileBounds))
                {
                    return true;
                }
            }
        }
        return false;
    };

    std::vector<Entity*> barrelEnemyEntities;
    barrelEnemyEntities.reserve(m_world.EntitiesByTag(EntityTag::Enemy).size());
    for (Entity* enemyEntity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!enemyEntity)
        {
            continue;
        }

        auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled())
        {
            continue;
        }

        barrelEnemyEntities.push_back(enemyEntity);
    }

    std::vector<Entity*> barrelGimmickEntities;
    auto appendBarrelGimmickEntities = [&](EntityTag tag)
    {
        for (Entity* gimmickEntity : m_world.EntitiesByTag(tag))
        {
            if (gimmickEntity)
            {
                barrelGimmickEntities.push_back(gimmickEntity);
            }
        }
    };
    appendBarrelGimmickEntities(EntityTag::BatterySwitch);
    appendBarrelGimmickEntities(EntityTag::Elevator);
    appendBarrelGimmickEntities(EntityTag::LaserSwitch);
    appendBarrelGimmickEntities(EntityTag::Shutter);
    appendBarrelGimmickEntities(EntityTag::ProtectiveWall);
    appendBarrelGimmickEntities(EntityTag::SepiaElevator);

    std::vector<Entity*> barrelEntities;
    barrelEntities.reserve(
        m_world.EntitiesByTag(EntityTag::Barrel).size() +
        m_world.EntitiesByTag(EntityTag::Log).size());
    for (Entity* barrelEntity : m_world.EntitiesByTag(EntityTag::Barrel))
    {
        if (barrelEntity)
        {
            barrelEntities.push_back(barrelEntity);
        }
    }
    for (Entity* logEntity : m_world.EntitiesByTag(EntityTag::Log))
    {
        if (logEntity)
        {
            barrelEntities.push_back(logEntity);
        }
    }

    for (Entity* entity : barrelEntities)
    {
        if (!entity)
        {
            continue;
        }

        auto* barrel = entity->GetComponent<BarrelComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!barrel || !transform)
        {
            continue;
        }

        const bool isLog = HasTag(*entity, kTagLog);

        if (barrel->destroyed)
        {
            setBarrelVisible(*entity, false);
            continue;
        }

        const float barrelWidth = transform->width * transform->scale;
        const float barrelHeight = transform->height * transform->scale;
        if (barrel->respawnWhenOffscreen)
        {
            const float cameraLeft = m_flow.cameraX - kBarrelRespawnOffscreenMargin;
            const float cameraRight = m_flow.cameraX + gCameraViewWidth + kBarrelRespawnOffscreenMargin;
            const float cameraBottom = m_flow.cameraY + gCameraViewHeight + kBarrelRespawnOffscreenMargin;
            const bool isOffscreen =
                (transform->x + barrelWidth) < cameraLeft ||
                transform->x > cameraRight ||
                transform->y > cameraBottom;
            if (isOffscreen)
            {
                transform->x = std::clamp(barrel->spawnX, 0.0f, std::max(0.0f, mapWidth - barrelWidth));
                transform->y = std::clamp(barrel->spawnY, 0.0f, std::max(0.0f, mapHeight - barrelHeight));
                barrel->velocityX = 0.0f;
                barrel->velocityY = 0.0f;
                barrel->accumulatedFallDistance = 0.0f;
                barrel->grounded = false;
                barrel->destroyed = false;
                continue;
            }
        }

        if (!isLog && (transform->x + barrelWidth < activeLeft || transform->x > activeRight))
        {
            continue;
        }

        if (barrel->cooldownActive)
        {
            barrel->cooldownRemaining = std::max(0.0f, barrel->cooldownRemaining - deltaTime);
            if (barrel->cooldownRemaining <= 0.0f)
            {
                barrel->cooldownActive = false;
                transform->x = barrel->spawnX;
                transform->y = barrel->spawnY;
                setBarrelVisible(*entity, true);
            }
        }

        if (!barrel->active)
        {
            barrel->velocityX = 0.0f;
            barrel->velocityY = 0.0f;
            barrel->grounded = false;
            barrel->accumulatedFallDistance = 0.0f;

            if (!barrel->cooldownActive)
            {
                transform->x = barrel->spawnX;
                transform->y = barrel->spawnY;
            }

            if (!barrel->cooldownActive && player)
            {
                const auto* playerTransform = player->GetComponent<TransformComponent>();
                if (playerTransform)
                {
                    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                    const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
                    const float barrelCenterX = transform->x + barrelWidth * 0.5f;
                    const float barrelCenterY = transform->y + barrelHeight * 0.5f;
                    const float dx = playerCenterX - barrelCenterX;
                    const float dy = playerCenterY - barrelCenterY;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance <= activationDistance)
                    {
                        barrel->active = true;
                        setBarrelVisible(*entity, true);
                    }
                }
            }
            else if (!barrel->respawnEnabled)
            {
                barrel->active = true;
                setBarrelVisible(*entity, true);
            }

            continue;
        }

        const float previousY = transform->y;
        barrel->velocityY = std::min(barrel->maxFallSpeed, barrel->velocityY + barrel->gravity * deltaTime);
        transform->y += barrel->velocityY * deltaTime;
        const bool canCollideAfterDrop = transform->y >= barrel->spawnY + std::max(8.0f, tileSize * 0.25f);

        if (isLog)
        {
            const bool snapped = TrySnapToGround(*transform, std::max(gGroundSnapDistance, std::fabs(barrel->velocityY) * deltaTime + 4.0f));
            barrel->grounded = snapped;
            if (snapped)
            {
                barrel->velocityY = 0.0f;
            }

            if (transform->y + barrelHeight > mapHeight)
            {
                transform->y = mapHeight - barrelHeight;
                barrel->velocityY = 0.0f;
                barrel->grounded = true;
            }

            Entity* hitObject = nullptr;
            if (!barrel->grounded && isBarrelObjectCollision(*entity, *transform, hitObject))
            {
                transform->y = previousY;
                barrel->velocityY = 0.0f;
                barrel->grounded = TrySnapToGround(*transform, tileSize * 0.5f);
            }

            continue;
        }

        if (transform->y + barrelHeight >= mapHeight)
        {
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        if (canCollideAfterDrop && intersectsDespawnTile(*transform))
        {
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        if (player && IntersectsEntity(*entity, *player))
        {
            HandlePlayerDamage(*player, entity, "GameScene player damaged by barrel");
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        bool consumed = false;
        for (Entity* enemyEntity : barrelEnemyEntities)
        {
            if (!enemyEntity || enemyEntity == entity)
            {
                continue;
            }

            if (!IntersectsEntity(*entity, *enemyEntity))
            {
                continue;
            }

            HandleEnemyDamage(*enemyEntity, entity, barrel->contactDamage, "Barrel hit enemy");
            resetBarrel(*entity, *barrel, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

        for (Entity* gimmickEntity : barrelGimmickEntities)
        {
            if (!gimmickEntity || gimmickEntity == entity)
            {
                continue;
            }

            auto* gimmick = gimmickEntity->GetComponent<GimmickComponent>();
            if (!gimmick || !gimmick->IsEnabled() || !IntersectsEntity(*entity, *gimmickEntity))
            {
                continue;
            }

            if (gimmick->GetType() == GimmickType::Switch)
            {
                m_flow.goalUnlockedBySwitch = true;
                gimmick->Consume();
            }

            resetBarrel(*entity, *barrel, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

    }
}

void GameScene::UpdateFallingRocks(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    const float tileSize = m_tileMap.GetTileSize();
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float activeLeft = std::max(0.0f, m_flow.cameraX - gBarrelActivationPaddingX);
    const float activeRight = std::min(mapWidth, m_flow.cameraX + gCameraViewWidth + gBarrelActivationPaddingX);
    const float activationDistance = tileSize * 11.0f;
    const float kFallingRockRubbleLifetime = 3.0f;

    auto setFallingRockVisible = [](Entity& fallingrockEntity, bool visible)
        {
            if (auto* tint = fallingrockEntity.GetComponent<TintComponent>())
            {
                tint->a = visible ? 1.0f : 0.0f;
            }
        };

    const auto spawnFallingRockRubble = [&](const TransformComponent& sourceTransform)
        {
            const float sourceWidth = sourceTransform.width * sourceTransform.scale;
            const float sourceHeight = sourceTransform.height * sourceTransform.scale;
            const float rubbleSize = std::clamp(
                std::min(sourceWidth, sourceHeight),
                tileSize * 0.75f,
                tileSize * 2.0f);

            const int sepiaRubbleTextureId = m_assets.GetTexture("sepia_rubble");

            auto rubble = std::make_unique<Entity>();
            rubble->AddComponent<TagComponent>(kTagSepiaRubble);
            rubble->AddComponent<TransformComponent>(
                sourceTransform.x + sourceWidth * 0.5f - rubbleSize * 0.5f,
                sourceTransform.y + sourceHeight * 0.5f - rubbleSize * 0.5f,
                rubbleSize,
                rubbleSize);
            rubble->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
            rubble->AddComponent<SpriteRenderComponent>(sepiaRubbleTextureId >= 0 ? sepiaRubbleTextureId : m_whiteTexture);
            rubble->AddComponent<SepiaRubbleComponent>(SepiaRubbleSource::FallingRock);
            rubble->AddComponent<PhotoCopyLifetimeComponent>(kFallingRockRubbleLifetime);
            m_world.QueueSpawn(std::move(rubble));
        };

    auto resetFallingRock = [&](Entity& fallingRockEntity, FallingRockComponent& fallingRock, TransformComponent& transform)
        {
            SpawnBarrelBreakEffect(transform.x, transform.y, transform.width * transform.scale, transform.height * transform.scale);
            m_eventBus.Publish({ EventType::PlaySoundRequest, &fallingRockEntity, nullptr, "fallingrock", 0.0f, 0.0f });
            
            spawnFallingRockRubble(transform);

            const bool pastedFallingRock =
                fallingRockEntity.GetComponent<PhotoPasteOrderComponent>() != nullptr;
            if (pastedFallingRock)
            {
                fallingRock.destroyed = true;
            }
            fallingRock.active = false;
            fallingRock.cooldownActive = false;
            fallingRock.cooldownRemaining = 0.0f;
            fallingRock.velocityX = 0.0f;
            fallingRock.velocityY = 0.0f;
            fallingRock.grounded = false;
            fallingRock.accumulatedFallDistance = 0.0f;
            fallingRock.rubbleActive = true;
            fallingRock.rubbleRemaining = kFallingRockRubbleLifetime;
            fallingRock.pendingJumpPadBreak = false;
            setFallingRockVisible(fallingRockEntity, false);
        };

    std::vector<Entity*> fallingrockCollisionCandidates;
    fallingrockCollisionCandidates.reserve(
        m_world.EntitiesByTag(EntityTag::PhotoBox).size() +
        m_world.EntitiesByTag(EntityTag::Battery).size() +
        m_world.EntitiesByTag(EntityTag::BatterySwitch).size() +
        m_world.EntitiesByTag(EntityTag::Elevator).size() +
        m_world.EntitiesByTag(EntityTag::LaserSwitch).size() +
        m_world.EntitiesByTag(EntityTag::Shutter).size() +
        m_world.EntitiesByTag(EntityTag::ProtectiveWall).size() +
        m_world.EntitiesByTag(EntityTag::LaserTurret).size() +
        m_world.EntitiesByTag(EntityTag::LaserBeam).size() +
        m_world.EntitiesByTag(EntityTag::StageLight).size() +
        m_world.EntitiesByTag(EntityTag::MarkerLight).size() +
        m_world.EntitiesByTag(EntityTag::SepiaRubble).size() +
        m_world.EntitiesByTag(EntityTag::SepiaElevator).size() +
        m_world.EntitiesByTag(EntityTag::Shield).size() +
        m_world.EntitiesByTag(EntityTag::BossShield).size() +
        m_world.EntitiesByTag(EntityTag::Boss1Shield).size() +
        m_world.EntitiesByTag(EntityTag::MidBoss1Shield).size() +
        m_world.EntitiesByTag(EntityTag::CapturedShield).size() +
        m_world.EntitiesByTag(EntityTag::Barrel).size() +
        m_world.EntitiesByTag(EntityTag::FallingRock).size() +
        m_world.EntitiesByTag(EntityTag::Log).size());
    auto appendFallingRockCollisionCandidates = [&](EntityTag tag)
        {
            for (Entity* candidate : m_world.EntitiesByTag(tag))
            {
                if (candidate)
                {
                    fallingrockCollisionCandidates.push_back(candidate);
                }
            }
        };
    appendFallingRockCollisionCandidates(EntityTag::PhotoBox);
    appendFallingRockCollisionCandidates(EntityTag::Battery);
    appendFallingRockCollisionCandidates(EntityTag::BatterySwitch);
    appendFallingRockCollisionCandidates(EntityTag::Elevator);
    appendFallingRockCollisionCandidates(EntityTag::LaserSwitch);
    appendFallingRockCollisionCandidates(EntityTag::Shutter);
    appendFallingRockCollisionCandidates(EntityTag::ProtectiveWall);
    appendFallingRockCollisionCandidates(EntityTag::LaserTurret);
    appendFallingRockCollisionCandidates(EntityTag::LaserBeam);
    appendFallingRockCollisionCandidates(EntityTag::StageLight);
    appendFallingRockCollisionCandidates(EntityTag::MarkerLight);
    appendFallingRockCollisionCandidates(EntityTag::SepiaRubble);
    appendFallingRockCollisionCandidates(EntityTag::SepiaElevator);
    appendFallingRockCollisionCandidates(EntityTag::Shield);
    appendFallingRockCollisionCandidates(EntityTag::BossShield);
    appendFallingRockCollisionCandidates(EntityTag::Boss1Shield);
    appendFallingRockCollisionCandidates(EntityTag::MidBoss1Shield);
    appendFallingRockCollisionCandidates(EntityTag::CapturedShield);
    appendFallingRockCollisionCandidates(EntityTag::Barrel);
    appendFallingRockCollisionCandidates(EntityTag::FallingRock);
    appendFallingRockCollisionCandidates(EntityTag::Log);

    auto isFallingRockObjectCollision = [&](const Entity& fallingrockEntity, const TransformComponent& fallingrockBounds, Entity*& outHit) -> bool
        {
            outHit = nullptr;
            for (Entity* candidate : fallingrockCollisionCandidates)
            {
                if (!candidate || candidate == &fallingrockEntity)
                {
                    continue;
                }

                if (candidate->GetComponent<FallingRockComponent>())
                {
                    continue;
                }

                if (HasTag(*candidate, kTagPlayer) || HasTag(*candidate, kTagEnemy) || HasTag(*candidate, kTagBullet))
                {
                    continue;
                }

                if (HasTag(*candidate, kTagPhotoBox))
                {
                    const auto* layer = candidate->GetComponent<PhotoCopyLayerComponent>();
                    if (layer && layer->layer != PhotoCopyLayer::Foreground)
                    {
                        continue;
                    }
                }

                const auto* otherTransform = candidate->GetComponent<TransformComponent>();
                if (!otherTransform)
                {
                    continue;
                }

                if (IntersectsRect(fallingrockBounds, *otherTransform))
                {
                    outHit = candidate;
                    return true;
                }
            }

            return false;
        };

    auto intersectsDespawnTile = [&](const TransformComponent& bounds) -> bool
        {
            const float width = bounds.width * bounds.scale;
            const float height = bounds.height * bounds.scale;
            const int left = std::max(0, static_cast<int>((bounds.x + 2.0f) / tileSize));
            const int right = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((bounds.x + width - 2.0f) / tileSize));
            const int top = std::max(0, static_cast<int>((bounds.y + 2.0f) / tileSize));
            const int bottom = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bounds.y + height - 2.0f) / tileSize));
            for (int row = top; row <= bottom; ++row)
            {
                for (int column = left; column <= right; ++column)
                {
                    const int tileValue = m_tileMap.GetTile(column, row);
                    if (tileValue != 1 && tileValue != TileMap::kPitTileValue)
                    {
                        continue;
                    }

                    TransformComponent tileBounds(
                        static_cast<float>(column) * tileSize,
                        static_cast<float>(row) * tileSize,
                        tileSize,
                        tileSize);
                    if (IntersectsRect(bounds, tileBounds))
                    {
                        return true;
                    }
                }
            }
            return false;
        };

    std::vector<Entity*> fallingrockEnemyEntities;
    fallingrockEnemyEntities.reserve(m_world.EntitiesByTag(EntityTag::Enemy).size());
    for (Entity* enemyEntity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!enemyEntity)
        {
            continue;
        }

        auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled())
        {
            continue;
        }

        fallingrockEnemyEntities.push_back(enemyEntity);
    }

    std::vector<Entity*> fallingrockGimmickEntities;
    auto appendFallingRockGimmickEntities = [&](EntityTag tag)
        {
            for (Entity* gimmickEntity : m_world.EntitiesByTag(tag))
            {
                if (gimmickEntity)
                {
                    fallingrockGimmickEntities.push_back(gimmickEntity);
                }
            }
        };
    appendFallingRockGimmickEntities(EntityTag::BatterySwitch);
    appendFallingRockGimmickEntities(EntityTag::Elevator);
    appendFallingRockGimmickEntities(EntityTag::LaserSwitch);
    appendFallingRockGimmickEntities(EntityTag::Shutter);
    appendFallingRockGimmickEntities(EntityTag::ProtectiveWall);
    appendFallingRockGimmickEntities(EntityTag::SepiaElevator);

    std::vector<Entity*> fallingRockEntities;
    fallingRockEntities.reserve(
        m_world.EntitiesByTag(EntityTag::FallingRock).size());
    for (Entity* fallingRockEntity : m_world.EntitiesByTag(EntityTag::FallingRock))
    {
        if (fallingRockEntity)
        {
            fallingRockEntities.push_back(fallingRockEntity);
        }
    }

    auto tryLandFallingRockOnJumpPad = [&](Entity& fallingRockEntity, FallingRockComponent& fallingRock, TransformComponent& rockTransform, float previousY) -> bool
    {
        if (fallingRock.velocityY < 0.0f)
        {
            return false;
        }

        for (Entity* jumpPadEntity : m_world.EntitiesByTag(EntityTag::JumpPad))
        {
            if (!jumpPadEntity)
            {
                continue;
            }

            auto* jumpPad = jumpPadEntity->GetComponent<JumpPadComponent>();
            auto* jumpPadTransform = jumpPadEntity->GetComponent<TransformComponent>();
            if (!jumpPad || !jumpPadTransform)
            {
                continue;
            }

            JumpPadContact contact;
            const float tolerance = std::max(10.0f, std::fabs(fallingRock.velocityY) * deltaTime + tileSize * 0.3f);
            const float edgeTolerance = std::max(
                std::fabs(fallingRock.velocityY) * deltaTime + tileSize * 0.75f,
                tileSize * 0.55f);
            if (!TryResolveJumpPadContact(
                rockTransform,
                *jumpPadTransform,
                *jumpPad,
                tolerance,
                edgeTolerance,
                tileSize * 0.20f,
                tileSize * 0.10f,
                tileSize * 0.20f,
                &contact,
                previousY,
                true))
            {
                continue;
            }

            if (jumpPad->boardGrounded && fallingRock.pendingJumpPadBreak)
            {
                resetFallingRock(fallingRockEntity, fallingRock, rockTransform);
            }
            else
            {
                fallingRock.velocityY = 0.0f;
                fallingRock.grounded = true;
                fallingRock.pendingJumpPadBreak = jumpPad->boardGrounded;
            }
            return true;
        }

        return false;
    };

    for (Entity* entity : fallingRockEntities)
    {
        if (!entity)
        {
            continue;
        }

        auto* fallingRock = entity->GetComponent<FallingRockComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!fallingRock || !transform)
        {
            continue;
        }

        if (fallingRock->destroyed)
        {
            setFallingRockVisible(*entity, false);
            continue;
        }

        const float fallingrockWidth = transform->width * transform->scale;
        const float fallingrockHeight = transform->height * transform->scale;
        if (fallingRock->rubbleActive)
        {
            setFallingRockVisible(*entity, false);
            fallingRock->active = false;
            fallingRock->velocityX = 0.0f;
            fallingRock->velocityY = 0.0f;
            fallingRock->grounded = false;
            fallingRock->accumulatedFallDistance = 0.0f;
            fallingRock->rubbleRemaining = std::max(0.0f, fallingRock->rubbleRemaining - deltaTime);

            if (fallingRock->rubbleRemaining <= 0.0f)
            {
                fallingRock->rubbleActive = false;
                fallingRock->cooldownActive = !fallingRock->destroyed;
                fallingRock->cooldownRemaining = fallingRock->cooldownActive ? 1.0f : 0.0f;
            }

            continue;
        }
        if (fallingRock->respawnWhenOffscreen)
        {
            const float cameraLeft = m_flow.cameraX - kBarrelRespawnOffscreenMargin;
            const float cameraRight = m_flow.cameraX + gCameraViewWidth + kBarrelRespawnOffscreenMargin;
            const float cameraBottom = m_flow.cameraY + gCameraViewHeight + kBarrelRespawnOffscreenMargin;
            const bool isOffscreen =
                (transform->x + fallingrockWidth) < cameraLeft ||
                transform->x > cameraRight ||
                transform->y > cameraBottom;
            if (isOffscreen)
            {
                transform->x = std::clamp(fallingRock->spawnX, 0.0f, std::max(0.0f, mapWidth - fallingrockWidth));
                transform->y = std::clamp(fallingRock->spawnY, 0.0f, std::max(0.0f, mapHeight - fallingrockHeight));
                fallingRock->velocityX = 0.0f;
                fallingRock->velocityY = 0.0f;
                fallingRock->accumulatedFallDistance = 0.0f;
                fallingRock->grounded = false;
                fallingRock->destroyed = false;
                continue;
            }
        }

        if ((transform->x + fallingrockWidth < activeLeft || transform->x > activeRight))
        {
            continue;
        }

        if (fallingRock->cooldownActive)
        {
            fallingRock->cooldownRemaining = std::max(0.0f, fallingRock->cooldownRemaining - deltaTime);
            if (fallingRock->cooldownRemaining <= 0.0f)
            {
                fallingRock->cooldownActive = false;
                transform->x = fallingRock->spawnX;
                transform->y = fallingRock->spawnY;
                setFallingRockVisible(*entity, true);
            }
            else
            {
                continue;
            }
        }

        if (!fallingRock->active)
        {
            fallingRock->velocityX = 0.0f;
            fallingRock->velocityY = 0.0f;
            fallingRock->grounded = false;
            fallingRock->accumulatedFallDistance = 0.0f;

            if (!fallingRock->cooldownActive)
            {
                transform->x = fallingRock->spawnX;
                transform->y = fallingRock->spawnY;
            }

            if (!fallingRock->cooldownActive && player)
            {
                const auto* playerTransform = player->GetComponent<TransformComponent>();
                if (playerTransform)
                {
                    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                    const float fallingrockCenterX = transform->x + fallingrockWidth * 0.5f;
                    const float dx = playerCenterX - fallingrockCenterX;
                    if (std::fabs(dx) <= activationDistance)
                    {
                        fallingRock->active = true;
                        setFallingRockVisible(*entity, true);
                    }
                }
            }
            else if (!fallingRock->respawnEnabled)
            {
                fallingRock->active = true;
                setFallingRockVisible(*entity, true);
            }

            continue;
        }

        fallingRock->velocityY = std::min(fallingRock->maxFallSpeed, fallingRock->velocityY + fallingRock->gravity * deltaTime);
        const float previousY = transform->y;
        transform->y += fallingRock->velocityY * deltaTime;
        fallingRock->accumulatedFallDistance += std::max(0.0f, transform->y - previousY);
        const bool canCollideAfterDrop = transform->y >= fallingRock->spawnY + std::max(8.0f, tileSize * 0.25f);

        if (tryLandFallingRockOnJumpPad(*entity, *fallingRock, *transform, previousY))
        {
            continue;
        }
        fallingRock->pendingJumpPadBreak = false;

        if (transform->y + fallingrockHeight >= mapHeight)
        {
            resetFallingRock(*entity, *fallingRock, *transform);
            continue;
        }

        if (canCollideAfterDrop && intersectsDespawnTile(*transform))
        {
            resetFallingRock(*entity, *fallingRock, *transform);
            continue;
        }

        if (player && IntersectsEntity(*entity, *player))
        {
            HandlePlayerDamage(*player, entity, "GameScene player damaged by fallingrock");
            resetFallingRock(*entity, *fallingRock, *transform);
            continue;
        }

        bool consumed = false;
        for (Entity* enemyEntity : fallingrockEnemyEntities)
        {
            if (!enemyEntity || enemyEntity == entity)
            {
                continue;
            }

            if (!IntersectsEntity(*entity, *enemyEntity))
            {
                continue;
            }

            HandleEnemyDamage(*enemyEntity, entity, fallingRock->contactDamage, "FallingRock hit enemy");
            resetFallingRock(*entity, *fallingRock, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

        for (Entity* gimmickEntity : fallingrockGimmickEntities)
        {
            if (!gimmickEntity || gimmickEntity == entity)
            {
                continue;
            }

            auto* gimmick = gimmickEntity->GetComponent<GimmickComponent>();
            if (!gimmick || !gimmick->IsEnabled() || !IntersectsEntity(*entity, *gimmickEntity))
            {
                continue;
            }

            if (gimmick->GetType() == GimmickType::Switch)
            {
                m_flow.goalUnlockedBySwitch = true;
                gimmick->Consume();
            }

            resetFallingRock(*entity, *fallingRock, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

    }
}

void GameScene::UpdateHangingGravityObjects(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    const float mapHeight = GetMapPixelHeight();

    auto setHangingObjectVisible = [](Entity& hangingEntity, bool visible)
    {
        if (auto* tint = hangingEntity.GetComponent<TintComponent>())
        {
            tint->a = visible ? 1.0f : 0.0f;
        }
    };

    auto intersectsSolidTile = [&](const TransformComponent& bounds) -> bool
    {
        const float width = bounds.width * bounds.scale;
        const float height = bounds.height * bounds.scale;
        const int left = std::max(0, static_cast<int>((bounds.x + 2.0f) / tileSize));
        const int right = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((bounds.x + width - 2.0f) / tileSize));
        const int top = std::max(0, static_cast<int>((bounds.y + 2.0f) / tileSize));
        const int bottom = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bounds.y + height - 2.0f) / tileSize));
        for (int row = top; row <= bottom; ++row)
        {
            for (int column = left; column <= right; ++column)
            {
                if (m_tileMap.GetTile(column, row) != 1)
                {
                    continue;
                }

                TransformComponent tileBounds(
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize,
                    tileSize,
                    tileSize);
                if (IntersectsRect(bounds, tileBounds))
                {
                    return true;
                }
            }
        }
        return false;
    };

    auto breakHangingObject = [&](Entity& hangingEntity, HangingGravityObjectComponent& hanging, TransformComponent& transform)
    {
        SpawnBarrelBreakEffect(transform.x, transform.y, transform.width * transform.scale, transform.height * transform.scale);
        m_eventBus.Publish({ EventType::PlaySoundRequest, &hangingEntity, nullptr, "barrel", 0.0f, 0.0f });
        hanging.active = false;
        hanging.destroyed = true;
        hanging.wireAttached = false;
        hanging.velocityY = 0.0f;
        setHangingObjectVisible(hangingEntity, false);
    };

    auto isRidingHangingObject = [&](const TransformComponent& riderTransform, const TransformComponent& hangingTransform) -> bool
    {
        const float riderWidth = riderTransform.width * riderTransform.scale;
        const float riderHeight = riderTransform.height * riderTransform.scale;
        const float hangingWidth = hangingTransform.width * hangingTransform.scale;
        const float riderLeft = riderTransform.x + 6.0f;
        const float riderRight = riderTransform.x + riderWidth - 6.0f;
        const float riderBottom = riderTransform.y + riderHeight;
        const float hangingLeft = hangingTransform.x;
        const float hangingRight = hangingTransform.x + hangingWidth;
        const float hangingTop = hangingTransform.y;
        const float topTolerance = std::max(6.0f, tileSize * 0.25f);
        const bool horizontallyOverlapping = riderRight > hangingLeft && riderLeft < hangingRight;
        return horizontallyOverlapping &&
            riderTransform.y < hangingTop &&
            riderBottom <= hangingTop + topTolerance;
    };

    std::vector<Entity*> enemyEntities;
    enemyEntities.reserve(m_world.EntitiesByTag(EntityTag::Enemy).size());
    for (Entity* enemyEntity : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!enemyEntity)
        {
            continue;
        }

        auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled())
        {
            continue;
        }

        enemyEntities.push_back(enemyEntity);
    }

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::HangingGravityObject))
    {
        if (!entity)
        {
            continue;
        }

        auto* hanging = entity->GetComponent<HangingGravityObjectComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!hanging || !transform)
        {
            continue;
        }

        if (hanging->destroyed)
        {
            setHangingObjectVisible(*entity, false);
            continue;
        }

        if (hanging->wireAttached)
        {
            transform->x = hanging->spawnX;
            transform->y = hanging->spawnY;
            hanging->active = false;
            hanging->velocityY = 0.0f;
            setHangingObjectVisible(*entity, true);
            continue;
        }

        if (!hanging->active)
        {
            continue;
        }

        hanging->velocityY = std::min(hanging->maxFallSpeed, hanging->velocityY + hanging->gravity * deltaTime);
        transform->y += hanging->velocityY * deltaTime;

        const float objectHeight = transform->height * transform->scale;
        if (transform->y + objectHeight >= mapHeight)
        {
            breakHangingObject(*entity, *hanging, *transform);
            continue;
        }

        if (intersectsSolidTile(*transform))
        {
            breakHangingObject(*entity, *hanging, *transform);
            continue;
        }

        if (player && IntersectsEntity(*entity, *player))
        {
            const auto* playerTransform = player->GetComponent<TransformComponent>();
            if (!playerTransform || !isRidingHangingObject(*playerTransform, *transform))
            {
                HandlePlayerDamage(*player, entity, "GameScene player damaged by hanging gravity object", hanging->contactDamage);
                breakHangingObject(*entity, *hanging, *transform);
                continue;
            }
        }

        bool consumed = false;
        for (Entity* enemyEntity : enemyEntities)
        {
            if (!enemyEntity || enemyEntity == entity)
            {
                continue;
            }

            if (!IntersectsEntity(*entity, *enemyEntity))
            {
                continue;
            }

            const auto* enemyTransform = enemyEntity->GetComponent<TransformComponent>();
            if (enemyTransform && isRidingHangingObject(*enemyTransform, *transform))
            {
                continue;
            }

            HandleEnemyDamage(*enemyEntity, entity, hanging->contactDamage, "Hanging gravity object hit enemy");
            breakHangingObject(*entity, *hanging, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }
    }
}

void GameScene::ResetHangingGravityObjectsForRespawn()
{
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::HangingGravityObject))
    {
        if (!entity)
        {
            continue;
        }

        auto* hanging = entity->GetComponent<HangingGravityObjectComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!hanging || !transform)
        {
            continue;
        }

        transform->x = hanging->spawnX;
        transform->y = hanging->spawnY;
        hanging->velocityY = 0.0f;
        hanging->wireAttached = true;
        hanging->active = false;
        hanging->destroyed = false;
        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            tint->a = 1.0f;
        }
    }
}

void GameScene::UpdateJumpPads(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    auto* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    const float maxTiltRadians = std::clamp(gJumpPadMaxTiltDegrees, 0.0f, 89.0f) * 3.14159265f / 180.0f;

    for (Entity* jumpPadEntity : m_world.EntitiesByTag(EntityTag::JumpPad))
    {
        if (!jumpPadEntity)
        {
            continue;
        }

        auto* jumpPad = jumpPadEntity->GetComponent<JumpPadComponent>();
        auto* jumpPadTransform = jumpPadEntity->GetComponent<TransformComponent>();
        if (!jumpPad || !jumpPadTransform)
        {
            continue;
        }

        jumpPad->maxTiltRadians = maxTiltRadians;
        jumpPad->leftLoad = 0.0f;
        jumpPad->rightLoad = 0.0f;
        jumpPad->lastRockFallDistance = 0.0f;
        jumpPad->edgeRockContactGrace = std::max(0.0f, jumpPad->edgeRockContactGrace - deltaTime);
        if (jumpPad->edgeRockContactGrace <= 0.0f)
        {
            jumpPad->edgeRockSide = 0;
            jumpPad->edgeRockFallDistance = 0.0f;
        }

        bool playerOnPad = false;
        int playerSide = 0;
        if (playerTransform)
        {
            const float tolerance = std::max(12.0f, std::fabs(m_player.velocityY) * deltaTime + tileSize * 0.35f);
            JumpPadContact contact;
            if (m_player.velocityY >= -1.0f &&
                TryResolveJumpPadContact(
                    *playerTransform,
                    *jumpPadTransform,
                    *jumpPad,
                    tolerance,
                    tolerance,
                    0.0f,
                    tileSize * 0.12f,
                    0.0f,
                    &contact))
            {
                playerSide = contact.side;
                if (playerSide < 0)
                {
                    jumpPad->leftLoad += 1.0f;
                }
                else
                {
                    jumpPad->rightLoad += 1.0f;
                }
                playerOnPad = true;
                m_player.velocityY = 0.0f;
                m_player.grounded = true;
                m_player.coyoteTimeRemaining = gCoyoteTimeSeconds;
            }
        }

        bool rockOnPad = false;
        int rockSide = 0;
        for (Entity* rockEntity : m_world.EntitiesByTag(EntityTag::FallingRock))
        {
            if (!rockEntity)
            {
                continue;
            }

            auto* rock = rockEntity->GetComponent<FallingRockComponent>();
            auto* rockTransform = rockEntity->GetComponent<TransformComponent>();
            if (!rock || !rockTransform || rock->destroyed || rock->rubbleActive)
            {
                continue;
            }

            JumpPadContact contact;
            const float tolerance = std::max(12.0f, std::fabs(rock->velocityY) * deltaTime + tileSize * 0.40f);
            const float edgeTolerance = std::max(
                std::fabs(rock->velocityY) * deltaTime + tileSize * 0.75f,
                tileSize * 0.55f);
            if (!TryResolveJumpPadContact(
                *rockTransform,
                *jumpPadTransform,
                *jumpPad,
                tolerance,
                edgeTolerance,
                tileSize * 0.20f,
                tileSize * 0.10f,
                tileSize * 0.20f,
                &contact))
            {
                continue;
            }

            const int side = contact.side;
            if (side < 0)
            {
                jumpPad->leftLoad += 3.0f;
            }
            else
            {
                jumpPad->rightLoad += 3.0f;
            }
            rockOnPad = true;
            rockSide = side;
            jumpPad->lastRockFallDistance = std::max(jumpPad->lastRockFallDistance, rock->accumulatedFallDistance);
            if (contact.edgeContact)
            {
                jumpPad->edgeRockContactGrace = 0.10f;
                jumpPad->edgeRockSide = side;
                jumpPad->edgeRockFallDistance = std::max(jumpPad->edgeRockFallDistance, rock->accumulatedFallDistance);
            }
            rock->velocityY = 0.0f;
            rock->grounded = true;
        }

        if (!rockOnPad && jumpPad->edgeRockContactGrace > 0.0f && jumpPad->edgeRockSide != 0)
        {
            if (jumpPad->edgeRockSide < 0)
            {
                jumpPad->leftLoad += 3.0f;
            }
            else
            {
                jumpPad->rightLoad += 3.0f;
            }
            rockOnPad = true;
            rockSide = jumpPad->edgeRockSide;
            jumpPad->lastRockFallDistance = std::max(jumpPad->lastRockFallDistance, jumpPad->edgeRockFallDistance);
        }

        if (jumpPad->rightLoad > jumpPad->leftLoad + 0.01f)
        {
            jumpPad->targetTilt = jumpPad->maxTiltRadians;
        }
        else if (jumpPad->leftLoad > jumpPad->rightLoad + 0.01f)
        {
            jumpPad->targetTilt = -jumpPad->maxTiltRadians;
        }
        else
        {
            jumpPad->targetTilt = 0.0f;
        }

        const float speed = std::fabs(jumpPad->targetTilt) > 0.001f ? jumpPad->tiltSpeed : jumpPad->returnSpeed;
        const float blend = std::clamp(speed * deltaTime, 0.0f, 1.0f);
        jumpPad->tilt += (jumpPad->targetTilt - jumpPad->tilt) * blend;
        jumpPad->boardGrounded =
            std::fabs(jumpPad->targetTilt) > 0.001f &&
            jumpPad->maxTiltRadians > 0.0f &&
            std::fabs(jumpPad->tilt) >= jumpPad->maxTiltRadians - 0.015f;

        const bool canLaunchPlayer =
            player &&
            playerTransform &&
            playerOnPad &&
            rockOnPad &&
            playerSide != 0 &&
            rockSide != 0 &&
            playerSide != rockSide &&
            !jumpPad->launchConsumed &&
            ((playerSide < 0 && jumpPad->tilt > jumpPad->maxTiltRadians * 0.12f) ||
                (playerSide > 0 && jumpPad->tilt < -jumpPad->maxTiltRadians * 0.12f));
        if (canLaunchPlayer)
        {
            float launchVelocity = jumpPad->baseLaunchVelocity - jumpPad->lastRockFallDistance * jumpPad->fallDistanceLaunchScale;
            launchVelocity = std::max(jumpPad->maxLaunchVelocity, launchVelocity);
            const float horizontalLaunchSpeed = std::clamp(
                std::fabs(launchVelocity) * 0.7f,
                gPlayerMoveSpeed * 0.75f,
                gPlayerMoveSpeed * 2.8f);
            m_player.velocityY = launchVelocity;
            m_player.velocityX = (playerSide < 0 ? -1.0f : 1.0f) * horizontalLaunchSpeed;
            m_player.grounded = false;
            m_player.coyoteTimeRemaining = 0.0f;
            playerTransform->y -= std::max(2.0f, tileSize * 0.10f);
            jumpPad->launchConsumed = true;
            m_eventBus.Publish({ EventType::PlaySoundRequest, player, jumpPadEntity, "test_tone", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, player, jumpPadEntity, "JumpPad launched player", 0.0f, 0.0f });
        }

        if (!playerOnPad || !rockOnPad)
        {
            jumpPad->launchConsumed = false;
        }
    }
}

void GameScene::UpdateBatteries(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    std::vector<Entity*> enemies;
    enemies.reserve(m_world.EntitiesByTag(EntityTag::Enemy).size());
    for (Entity* candidate : m_world.EntitiesByTag(EntityTag::Enemy))
    {
        if (!candidate)
        {
            continue;
        }
        if (auto* enemy = candidate->GetComponent<EnemyComponent>())
        {
            if (!enemy->IsEnabled() || enemy->IsDefeated())
            {
                continue;
            }
        }
        enemies.push_back(candidate);
    }
    std::vector<TransformComponent> groundPlatformsForSnap;
    GetGroundPlatformBounds(groundPlatformsForSnap);

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Battery))
    {
        if (!entity)
        {
            continue;
        }
        UpdateSingleBattery(*entity, player, enemies, groundPlatformsForSnap, deltaTime, tileSize);
    }
}

void GameScene::UpdateLaserTurrets(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    Entity* player = FindEntityByTag(kTagPlayer);
    TransformComponent* playerLaserBlockTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    const auto& enemyTagEntities = m_world.EntitiesByTag(EntityTag::Enemy);
    const auto& batterySwitchEntities = m_world.EntitiesByTag(EntityTag::BatterySwitch);
    const auto& laserTurretEntities = m_world.EntitiesByTag(EntityTag::LaserTurret);
    const auto& laserBeamEntities = m_world.EntitiesByTag(EntityTag::LaserBeam);
    auto intersectsRect = [](const TransformComponent& a, const TransformComponent& b) -> bool
    {
        const float aWidth = a.width * a.scale;
        const float aHeight = a.height * a.scale;
        const float bWidth = b.width * b.scale;
        const float bHeight = b.height * b.scale;
        return a.x < b.x + bWidth &&
            a.x + aWidth > b.x &&
            a.y < b.y + bHeight &&
            a.y + aHeight > b.y;
    };

    std::vector<Entity*> enemyEntities;
    enemyEntities.reserve(enemyTagEntities.size());
    std::vector<Entity*> beamBlockerEntities;
    beamBlockerEntities.reserve(
        m_world.EntitiesByTag(EntityTag::Barrel).size() +
        m_world.EntitiesByTag(EntityTag::Battery).size() +
        m_world.EntitiesByTag(EntityTag::PhotoBox).size() +
        m_world.EntitiesByTag(EntityTag::PhotoSource).size() +
        batterySwitchEntities.size() +
        m_world.EntitiesByTag(EntityTag::Elevator).size() +
        m_world.EntitiesByTag(EntityTag::LaserSwitch).size() +
        m_world.EntitiesByTag(EntityTag::Shutter).size() +
        laserTurretEntities.size() +
        m_world.EntitiesByTag(EntityTag::SepiaElevator).size() +
        m_world.EntitiesByTag(EntityTag::ProtectiveWall).size());
    bool hasLaserPowerSwitch = false;
    bool laserPowerEnabled = false;
    for (Entity* entity : batterySwitchEntities)
    {
        if (!entity)
        {
            continue;
        }
        if (const auto* batterySwitch = entity->GetComponent<BatterySwitchComponent>())
        {
            if (batterySwitch->controlsLaserPower)
            {
                hasLaserPowerSwitch = true;
                laserPowerEnabled = laserPowerEnabled || batterySwitch->isPressed;
            }
        }
    }
    for (Entity* entity : enemyTagEntities)
    {
        if (!entity)
        {
            continue;
        }
        enemyEntities.push_back(entity);
    }
    auto appendBlockingEntities = [&](EntityTag tag)
    {
        for (Entity* entity : m_world.EntitiesByTag(tag))
        {
            if (entity)
            {
                beamBlockerEntities.push_back(entity);
            }
        }
    };
    appendBlockingEntities(EntityTag::Barrel);
    appendBlockingEntities(EntityTag::Battery);
    appendBlockingEntities(EntityTag::PhotoBox);
    appendBlockingEntities(EntityTag::PhotoSource);
    appendBlockingEntities(EntityTag::BatterySwitch);
    appendBlockingEntities(EntityTag::Elevator);
    appendBlockingEntities(EntityTag::LaserSwitch);
    appendBlockingEntities(EntityTag::Shutter);
    appendBlockingEntities(EntityTag::LaserTurret);
    appendBlockingEntities(EntityTag::SepiaElevator);
    for (Entity* entity : m_world.EntitiesByTag(EntityTag::ProtectiveWall))
    {
        if (entity && IsGroundPlatformEntity(*entity))
        {
            beamBlockerEntities.push_back(entity);
        }
    }

    for (Entity* turretCandidate : laserTurretEntities)
    {
        if (!turretCandidate)
        {
            continue;
        }

        auto* turretTransform = turretCandidate->GetComponent<TransformComponent>();
        auto* turret = turretCandidate->GetComponent<LaserTurretComponent>();
        if (!turretTransform || !turret)
        {
            continue;
        }

        if (auto* follow = turretCandidate->GetComponent<CapturedBoss2BeamFollowComponent>())
        {
            const auto* targetTransform = follow->target
                ? follow->target->GetComponent<TransformComponent>()
                : nullptr;
            if (targetTransform)
            {
                turretTransform->x = targetTransform->x + follow->offsetX;
                turretTransform->y = targetTransform->y + follow->offsetY;
            }
        }

        if (turret->warmupRemaining > 0.0f)
        {
            const float previousWarmup = turret->warmupRemaining;
            turret->warmupRemaining = std::max(0.0f, turret->warmupRemaining - deltaTime);
            if (turret->warmupRemaining <= 0.0f &&
                previousWarmup > 0.0f &&
                turretCandidate->GetComponent<CapturedBoss2BeamChargeComponent>())
            {
                constexpr float kCapturedBeamFireShakeSeconds = 0.16f;
                constexpr float kCapturedBeamFireShakeAmplitude = 24.0f;
                m_flow.screenShakeRemaining = std::max(m_flow.screenShakeRemaining, kCapturedBeamFireShakeSeconds);
                m_flow.screenShakeDuration = std::max(m_flow.screenShakeDuration, kCapturedBeamFireShakeSeconds);
                m_flow.screenShakeAmplitude = std::max(m_flow.screenShakeAmplitude, kCapturedBeamFireShakeAmplitude);
            }
        }

        Entity* beamEntity = turret->beamEntity;
        const auto fireDirection = turret->fireDirection;
        const bool firesLeft = fireDirection == LaserTurretFireDirection::Left;
        const bool firesUp = fireDirection == LaserTurretFireDirection::Up;
        const bool firesVertical = IsVerticalLaserDirection(fireDirection);
        if (!beamEntity || !HasTag(*beamEntity, EntityTag::LaserBeam))
        {
            beamEntity = nullptr;
            for (Entity* beamCandidate : laserBeamEntities)
            {
                if (!beamCandidate)
                {
                    continue;
                }
                auto* beamTransform = beamCandidate->GetComponent<TransformComponent>();
                if (!beamTransform)
                {
                    continue;
                }

                const float beamCenterX = beamTransform->x + beamTransform->width * beamTransform->scale * 0.5f;
                const float beamCenterY = beamTransform->y + beamTransform->height * beamTransform->scale * 0.5f;
                const float turretCenterX = turretTransform->x + turretTransform->width * turretTransform->scale * 0.5f;
                const float turretCenterY = turretTransform->y + turretTransform->height * turretTransform->scale * 0.5f;
                const bool matchesHorizontal =
                    !firesVertical &&
                    std::fabs(beamCenterY - turretCenterY) <= tileSize * 0.25f &&
                    (firesLeft
                        ? beamTransform->x <= turretTransform->x + turretTransform->width * turretTransform->scale
                        : beamTransform->x >= turretTransform->x);
                const bool matchesVertical =
                    firesVertical &&
                    std::fabs(beamCenterX - turretCenterX) <= tileSize * 0.25f &&
                    (firesUp
                        ? beamTransform->y <= turretTransform->y + turretTransform->height * turretTransform->scale
                        : beamTransform->y >= turretTransform->y);
                if (matchesHorizontal || matchesVertical)
                {
                    beamEntity = beamCandidate;
                    turret->beamEntity = beamEntity;
                    break;
                }
            }
        }

        if (!beamEntity)
        {
            continue;
        }

        auto* beamTransform = beamEntity->GetComponent<TransformComponent>();
        if (!beamTransform)
        {
            continue;
        }
        auto* beamDamage = beamEntity->GetComponent<LaserBeamComponent>();
        const bool beamPenetratesPlayer =
            turretCandidate->GetComponent<BossBeamCaptureComponent>() != nullptr;
        if (turret->requiresLaserPower && (!hasLaserPowerSwitch || !laserPowerEnabled))
        {
            beamTransform->width = 0.0f;
            beamTransform->height = 0.0f;
            turret->playerDamageTimer = 0.0f;
            turret->enemyDamageTimers.clear();
            if (beamDamage)
            {
                beamDamage->enemyDamageTimers.clear();
            }
            continue;
        }

        if (!turret->active || turret->warmupRemaining > 0.0f)
        {
            beamTransform->width = 0.0f;
            turret->playerDamageTimer = 0.0f;
            turret->enemyDamageTimers.clear();
            if (beamDamage)
            {
                beamDamage->enemyDamageTimers.clear();
            }
            continue;
        }

        const float turretWidth = turretTransform->width * turretTransform->scale;
        const float turretHeight = turretTransform->height * turretTransform->scale;
        const float beamThickness = (std::max)(1.0f, turret->beamThickness);
        const float beamDamagePerSecond = beamDamage ? beamDamage->damagePerSecond : turret->damagePerSecond;
        const float beamEnemyKnockbackSpeed = beamDamage ? beamDamage->enemyKnockbackSpeed : turret->enemyKnockbackSpeed;
        std::unordered_map<const Entity*, float>* enemyDamageTimers =
            beamDamage ? &beamDamage->enemyDamageTimers : &turret->enemyDamageTimers;
        const float damageInterval = 1.0f / (std::max)(0.1f, beamDamagePerSecond);
        float beamLength = 0.0f;
        TransformComponent activeBeam(0.0f, 0.0f, 0.0f, 0.0f);
        float sparkX = 0.0f;
        float sparkY = 0.0f;
        bool blocked = false;
        bool playerHitByLaser = false;
        Entity* blockedProtectiveWall = nullptr;
        auto cutHangingObjectWires = [&](const TransformComponent& beamBounds)
        {
            if (beamBounds.width <= 0.0f || beamBounds.height <= 0.0f)
            {
                return;
            }

            for (Entity* hangingEntity : m_world.EntitiesByTag(EntityTag::HangingGravityObject))
            {
                if (!hangingEntity)
                {
                    continue;
                }

                auto* hanging = hangingEntity->GetComponent<HangingGravityObjectComponent>();
                if (!hanging || !hanging->wireAttached || hanging->destroyed || hanging->wireLength <= 0.0f)
                {
                    continue;
                }

                TransformComponent wireBounds(
                    hanging->wireX - hanging->wireWidth * 0.5f,
                    hanging->wireTopY,
                    hanging->wireWidth,
                    hanging->wireLength);
                if (!IntersectsRect(beamBounds, wireBounds))
                {
                    continue;
                }

                hanging->wireAttached = false;
                hanging->active = true;
                hanging->velocityY = 0.0f;
            }
        };

        if (firesVertical)
        {
            const float beamStartY = turretTransform->y + turret->beamOriginOffsetY;
            const float beamX = turretTransform->x + turret->beamOriginOffsetX;
            const float beamAabbY = firesUp ? 0.0f : beamStartY;
            const float beamAabbHeight = firesUp
                ? std::max(0.0f, beamStartY)
                : std::max(0.0f, mapHeight - beamStartY);
            float hitY = firesUp ? 0.0f : mapHeight;

            const int columnLeft = std::max(0, static_cast<int>(std::floor(beamX / tileSize)));
            const int columnRight = std::min(
                m_tileMap.GetWidth() - 1,
                static_cast<int>(std::floor((beamX + beamThickness - 1.0f) / tileSize)));
            if (firesUp)
            {
                for (int row = std::min(
                        m_tileMap.GetHeight() - 1,
                        static_cast<int>(std::floor((beamStartY - 1.0f) / tileSize)));
                    row >= 0;
                    --row)
                {
                    bool rowBlocked = false;
                    for (int column = columnLeft; column <= columnRight; ++column)
                    {
                        if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                        {
                            continue;
                        }

                        hitY = std::max(hitY, static_cast<float>(row + 1) * tileSize);
                        rowBlocked = true;
                        break;
                    }
                    if (rowBlocked)
                    {
                        break;
                    }
                }
            }
            else
            {
                for (int row = std::max(0, static_cast<int>(std::floor(beamStartY / tileSize))); row < m_tileMap.GetHeight(); ++row)
                {
                    bool rowBlocked = false;
                    for (int column = columnLeft; column <= columnRight; ++column)
                    {
                        if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                        {
                            continue;
                        }

                        hitY = std::min(hitY, static_cast<float>(row) * tileSize);
                        rowBlocked = true;
                        break;
                    }
                    if (rowBlocked)
                    {
                        break;
                    }
                }
            }

            TransformComponent beamAabb(beamX, beamAabbY, beamThickness, beamAabbHeight);
            for (Entity* entity : beamBlockerEntities)
            {
                if (!entity || entity == turretCandidate || entity == beamEntity)
                {
                    continue;
                }
                if (!(entity->GetComponent<BarrelComponent>() ||
                    entity->GetComponent<BatteryComponent>() ||
                    IsGroundPlatformEntity(*entity) ||
                    HasTag(*entity, EntityTag::PhotoBox)))
                {
                    continue;
                }

                auto* transform = entity->GetComponent<TransformComponent>();
                if (!transform)
                {
                    continue;
                }

                const float objectHeight = transform->height * transform->scale;
                if (firesUp)
                {
                    if (transform->y >= beamStartY)
                    {
                        continue;
                    }
                    if (!intersectsRect(beamAabb, *transform))
                    {
                        continue;
                    }

                    const float objectHitY = transform->y + objectHeight;
                    if (objectHitY > hitY)
                    {
                        hitY = objectHitY;
                        blockedProtectiveWall = HasTag(*entity, EntityTag::ProtectiveWall)
                            ? entity
                            : nullptr;
                    }
                }
                else
                {
                    if (transform->y + objectHeight <= beamStartY)
                    {
                        continue;
                    }
                    if (!intersectsRect(beamAabb, *transform))
                    {
                        continue;
                    }

                    if (transform->y < hitY)
                    {
                        hitY = transform->y;
                        blockedProtectiveWall = HasTag(*entity, EntityTag::ProtectiveWall)
                            ? entity
                            : nullptr;
                    }
                }
            }

            if (playerLaserBlockTransform && !beamPenetratesPlayer)
            {
                const float playerHeight = playerLaserBlockTransform->height * playerLaserBlockTransform->scale;
                if (firesUp)
                {
                    if (playerLaserBlockTransform->y < beamStartY &&
                        intersectsRect(beamAabb, *playerLaserBlockTransform))
                    {
                        hitY = std::max(hitY, playerLaserBlockTransform->y + playerHeight);
                    }
                }
                else
                {
                    if (playerLaserBlockTransform->y + playerHeight > beamStartY &&
                        intersectsRect(beamAabb, *playerLaserBlockTransform))
                    {
                        hitY = std::min(hitY, playerLaserBlockTransform->y);
                    }
                }
            }

            beamLength = firesUp
                ? std::max(0.0f, beamStartY - hitY)
                : std::max(0.0f, hitY - beamStartY);
            beamTransform->x = beamX;
            beamTransform->y = firesUp ? hitY : beamStartY;
            beamTransform->width = beamThickness;
            beamTransform->height = beamLength;
            activeBeam = TransformComponent(beamX, firesUp ? hitY : beamStartY, beamThickness, beamLength);
            blocked = firesUp ? hitY > 0.1f : hitY < mapHeight - 0.1f;
            if (playerLaserBlockTransform)
            {
                const float playerHeight = playerLaserBlockTransform->height * playerLaserBlockTransform->scale;
                playerHitByLaser =
                    beamLength > 0.0f &&
                    intersectsRect(activeBeam, *playerLaserBlockTransform) &&
                    (firesUp
                        ? playerLaserBlockTransform->y + playerHeight >= hitY - 0.5f
                        : playerLaserBlockTransform->y <= hitY + 0.5f);
            }
            sparkX = beamX + beamThickness * 0.5f;
            sparkY = hitY;
        }
        else
        {
            const float beamStartX = turretTransform->x + turret->beamOriginOffsetX;
            const float beamY = turretTransform->y + turret->beamOriginOffsetY - beamThickness * 0.5f;
            float hitX = firesLeft ? 0.0f : mapWidth;

            const int rowTop = std::max(0, static_cast<int>(std::floor(beamY / tileSize)));
            const int rowBottom = std::min(
                m_tileMap.GetHeight() - 1,
                static_cast<int>(std::floor((beamY + beamThickness - 1.0f) / tileSize)));
            const int startColumn = std::clamp(static_cast<int>(std::floor(beamStartX / tileSize)), 0, m_tileMap.GetWidth() - 1);
            for (int column = startColumn;
                firesLeft ? (column >= 0) : (column < m_tileMap.GetWidth());
                firesLeft ? --column : ++column)
            {
                bool blockedByTile = false;
                for (int row = rowTop; row <= rowBottom; ++row)
                {
                    if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                    {
                        continue;
                    }

                    hitX = firesLeft
                        ? std::max(hitX, static_cast<float>(column + 1) * tileSize)
                        : std::min(hitX, static_cast<float>(column) * tileSize);
                    blockedByTile = true;
                    break;
                }
                if (blockedByTile)
                {
                    break;
                }
            }

            const float beamAabbLeft = firesLeft ? 0.0f : beamStartX;
            const float beamAabbRight = firesLeft ? beamStartX : mapWidth;
            TransformComponent beamAabb(beamAabbLeft, beamY, std::max(0.0f, beamAabbRight - beamAabbLeft), beamThickness);
            for (Entity* entity : beamBlockerEntities)
            {
                if (!entity || entity == turretCandidate || entity == beamEntity)
                {
                    continue;
                }
                if (!(entity->GetComponent<BarrelComponent>() ||
                    entity->GetComponent<BatteryComponent>() ||
                    IsGroundPlatformEntity(*entity) ||
                    HasTag(*entity, EntityTag::PhotoBox)))
                {
                    continue;
                }

                auto* transform = entity->GetComponent<TransformComponent>();
                if (!transform)
                {
                    continue;
                }

                const float objectWidth = transform->width * transform->scale;
                if ((!firesLeft && transform->x + objectWidth <= beamStartX) ||
                    (firesLeft && transform->x >= beamStartX))
                {
                    continue;
                }
                if (!intersectsRect(beamAabb, *transform))
                {
                    continue;
                }

                const float objectHitX = firesLeft
                    ? transform->x + objectWidth
                    : transform->x;
                const bool nearerHit = firesLeft
                    ? objectHitX > hitX
                    : objectHitX < hitX;
                if (nearerHit)
                {
                    hitX = objectHitX;
                    blockedProtectiveWall = HasTag(*entity, EntityTag::ProtectiveWall)
                        ? entity
                        : nullptr;
                }
            }

            if (playerLaserBlockTransform && !beamPenetratesPlayer)
            {
                const float playerWidth = playerLaserBlockTransform->width * playerLaserBlockTransform->scale;
                if (!(firesLeft
                    ? playerLaserBlockTransform->x >= beamStartX
                    : playerLaserBlockTransform->x + playerWidth <= beamStartX) &&
                    intersectsRect(beamAabb, *playerLaserBlockTransform))
                {
                    hitX = firesLeft
                        ? std::max(hitX, playerLaserBlockTransform->x + playerWidth)
                        : std::min(hitX, playerLaserBlockTransform->x);
                }
            }

            beamLength = firesLeft
                ? std::max(0.0f, beamStartX - hitX)
                : std::max(0.0f, hitX - beamStartX);
            beamTransform->x = firesLeft ? hitX : beamStartX;
            beamTransform->y = beamY;
            beamTransform->width = beamLength;
            beamTransform->height = beamThickness;
            activeBeam = TransformComponent(beamTransform->x, beamY, beamLength, beamThickness);
            blocked = firesLeft ? hitX > 0.1f : hitX < mapWidth - 0.1f;
            if (playerLaserBlockTransform)
            {
                const float playerWidth = playerLaserBlockTransform->width * playerLaserBlockTransform->scale;
                playerHitByLaser =
                    beamLength > 0.0f &&
                    intersectsRect(beamAabb, *playerLaserBlockTransform) &&
                    (firesLeft
                        ? playerLaserBlockTransform->x + playerWidth >= hitX - 0.5f
                        : playerLaserBlockTransform->x <= hitX + 0.5f);
            }
            sparkX = hitX;
            sparkY = beamY + beamThickness * 0.5f;
        }
        cutHangingObjectWires(activeBeam);
        if (player)
        {
            if (auto* playerTransform = player->GetComponent<TransformComponent>())
            {
                if (beamLength > 0.0f && (intersectsRect(activeBeam, *playerTransform) || playerHitByLaser))
                {
                    turret->playerDamageTimer += deltaTime;
                    m_flow.playerTouchingHazard = true;
                    while (turret->playerDamageTimer >= damageInterval)
                    {
                        HandlePlayerDamage(*player, turretCandidate, "Laser damaged player", 1);
                        turret->playerDamageTimer -= damageInterval;
                    }
                }
                else
                {
                    turret->playerDamageTimer = 0.0f;
                }
            }
        }

        const bool bossBeamCanDamageWall =
            turretCandidate->GetComponent<BossBeamCaptureComponent>() != nullptr;
        if (bossBeamCanDamageWall && blockedProtectiveWall)
        {
            if (auto* wall = blockedProtectiveWall->GetComponent<ProtectiveWallComponent>())
            {
                wall->damageAccumulator += deltaTime;
                while (!wall->IsDestroyed() && wall->damageAccumulator >= damageInterval)
                {
                    wall->ApplyDamage(1);
                    wall->damageAccumulator -= damageInterval;
                }
            }
        }

        std::unordered_map<const Entity*, float> nextEnemyDamageTimers;
        nextEnemyDamageTimers.reserve(enemyEntities.size());
        for (Entity* enemyEntity : enemyEntities)
        {
            if (!enemyEntity)
            {
                continue;
            }

            auto* enemyTransform = enemyEntity->GetComponent<TransformComponent>();
            if (!enemyTransform || beamLength <= 0.0f || !intersectsRect(activeBeam, *enemyTransform))
            {
                continue;
            }

            float timer = deltaTime;
            auto timerIt = enemyDamageTimers->find(enemyEntity);
            if (timerIt != enemyDamageTimers->end())
            {
                timer = timerIt->second + deltaTime;
            }
            while (timer >= damageInterval)
            {
                HandleEnemyDamage(*enemyEntity, beamEntity, 1, "Laser damaged enemy");
                if (beamEnemyKnockbackSpeed > 0.0f)
                {
                    enemyTransform->x += (firesLeft ? -1.0f : 1.0f) * beamEnemyKnockbackSpeed * damageInterval;
                }
                timer -= damageInterval;
            }
            nextEnemyDamageTimers[enemyEntity] = timer;
        }
        enemyDamageTimers->swap(nextEnemyDamageTimers);
        turret->sparkTimer -= deltaTime;
        if (blocked && turret->sparkTimer <= 0.0f)
        {
            turret->sparkTimer = 0.08f;
            for (int index = 0; index < 2; ++index)
            {
                LaserSparkParticle spark;
                spark.x = sparkX;
                spark.y = sparkY;
                spark.velocityX = firesVertical
                    ? -90.0f + static_cast<float>(GetRand(180))
                    : (firesLeft
                        ? 60.0f + static_cast<float>(GetRand(140))
                        : -60.0f - static_cast<float>(GetRand(140)));
                spark.velocityY = firesVertical
                    ? (firesUp
                        ? 60.0f + static_cast<float>(GetRand(140))
                        : -60.0f - static_cast<float>(GetRand(140)))
                    : -90.0f + static_cast<float>(GetRand(180));
                spark.life = 0.18f;
                spark.maxLife = 0.18f;
                m_effects.laserSparks.push_back(spark);
            }
        }
    }
}

bool GameScene::IsBatteryCollidingWithWorld(const TransformComponent& bounds, const Entity* self, float tileSize) const
{
    const float width = bounds.width * bounds.scale;
    const float height = bounds.height * bounds.scale;
    const int left = std::max(0, static_cast<int>((bounds.x + 2.0f) / tileSize));
    const int right = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((bounds.x + width - 2.0f) / tileSize));
    const int top = std::max(0, static_cast<int>((bounds.y + 2.0f) / tileSize));
    const int bottom = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bounds.y + height - 2.0f) / tileSize));
    for (int row = top; row <= bottom; ++row)
    {
        for (int column = left; column <= right; ++column)
        {
            if (IsSolidTile(column, row))
            {
                return true;
            }
        }
    }

    if (IntersectsSolidPhotoBoxForMovement(bounds))
    {
        return true;
    }

    auto testTagGroup = [&](EntityTag tag) -> bool
    {
        for (Entity* entity : m_world.EntitiesByTag(tag))
        {
            if (!entity || entity == self)
            {
                continue;
            }
            if (!(entity->GetComponent<BarrelComponent>() ||
                entity->GetComponent<BatteryComponent>() ||
                IsGroundPlatformEntity(*entity)))
            {
                continue;
            }
            const auto* transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                continue;
            }

            const bool isDynamicPlatform =
                HasTag(*entity, kTagBatterySwitch) ||
                HasTag(*entity, kTagElevator) ||
                HasTag(*entity, kTagConveyorBelt);
            if (isDynamicPlatform)
            {
                const float boundsBottom = bounds.y + height;
                const float platformTop = transform->y;
                const float topTolerance = std::max(6.0f, tileSize * 0.22f);
                if (boundsBottom <= platformTop + topTolerance)
                {
                    continue;
                }
            }

            if (IntersectsRect(bounds, *transform))
            {
                return true;
            }
        }
        return false;
    };

    if (testTagGroup(EntityTag::Barrel) ||
        testTagGroup(EntityTag::Log) ||
        testTagGroup(EntityTag::Battery) ||
        testTagGroup(EntityTag::PhotoSource) ||
        testTagGroup(EntityTag::BatterySwitch) ||
        testTagGroup(EntityTag::Elevator) ||
        testTagGroup(EntityTag::LaserSwitch) ||
        testTagGroup(EntityTag::Shutter) ||
        testTagGroup(EntityTag::LaserTurret) ||
        testTagGroup(EntityTag::SepiaElevator) ||
        testTagGroup(EntityTag::ConveyorBelt))
    {
        return true;
    }

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::ProtectiveWall))
    {
        if (!entity || entity == self || !IsGroundPlatformEntity(*entity))
        {
            continue;
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform)
        {
            continue;
        }

        if (IntersectsRect(bounds, *transform))
        {
            return true;
        }
    }

    return false;
}

bool GameScene::IsBatteryOnTopOfSwitchOrDynamicEntity(const TransformComponent& bounds, const Entity* self, float tileSize) const
{
    const float boundsWidth = bounds.width * bounds.scale;
    const float boundsHeight = bounds.height * bounds.scale;
    const float boundsLeft = bounds.x + 2.0f;
    const float boundsRight = bounds.x + boundsWidth - 2.0f;
    const float boundsBottom = bounds.y + boundsHeight;
    const float topTolerance = std::max(6.0f, tileSize * 0.22f);

    auto testTopGroup = [&](EntityTag tag) -> bool
    {
        for (Entity* other : m_world.EntitiesByTag(tag))
        {
            if (!other || other == self)
            {
                continue;
            }

            const auto* otherTransform = other->GetComponent<TransformComponent>();
            if (!otherTransform)
            {
                continue;
            }

            const float platformWidth = otherTransform->width * otherTransform->scale;
            const float platformLeft = otherTransform->x;
            const float platformRight = otherTransform->x + platformWidth;
            const bool overlapX = boundsRight > platformLeft && boundsLeft < platformRight;
            const bool onTop = std::fabs(boundsBottom - otherTransform->y) <= topTolerance;
            if (overlapX && onTop)
            {
                return true;
            }
        }
        return false;
    };

    if (testTopGroup(EntityTag::BatterySwitch) || testTopGroup(EntityTag::Elevator)|| testTopGroup(EntityTag::ConveyorBelt))
    {
        return true;
    }
    return false;
}

bool GameScene::SnapBatteryToSwitchOrDynamicEntity(TransformComponent& bounds, const Entity* self, float tileSize) const
{
    const float width = bounds.width * bounds.scale;
    const float height = bounds.height * bounds.scale;
    const float left = bounds.x + 2.0f;
    const float right = bounds.x + width - 2.0f;
    const float bottom = bounds.y + height;
    const float topTolerance = std::max(8.0f, tileSize * 0.28f);

    auto snapToTopGroup = [&](EntityTag tag) -> bool
    {
        for (Entity* other : m_world.EntitiesByTag(tag))
        {
            if (!other || other == self)
            {
                continue;
            }

            const auto* otherTransform = other->GetComponent<TransformComponent>();
            if (!otherTransform)
            {
                continue;
            }

            const float platformWidth = otherTransform->width * otherTransform->scale;
            const float platformLeft = otherTransform->x;
            const float platformRight = otherTransform->x + platformWidth;
            const bool overlapX = right > platformLeft && left < platformRight;
            if (!overlapX)
            {
                continue;
            }

            if (std::fabs(bottom - otherTransform->y) <= topTolerance)
            {
                bounds.y = otherTransform->y - height;
                return true;
            }
        }
        return false;
    };

    if (snapToTopGroup(EntityTag::BatterySwitch) || snapToTopGroup(EntityTag::Elevator) || snapToTopGroup(EntityTag::ConveyorBelt))
    {
        return true;
    }

    return false;
}

float GameScene::GetBatteryPushDirectionFromPlayer(const TransformComponent& playerTransform, const TransformComponent& batteryTransform) const
{
    const float actorHeight = playerTransform.height * playerTransform.scale;
    const float batteryHeight = batteryTransform.height * batteryTransform.scale;
    const float actorTop = playerTransform.y;
    const float actorBottom = playerTransform.y + actorHeight;
    const float batteryTop = batteryTransform.y;
    const float batteryBottom = batteryTransform.y + batteryHeight;
    const float verticalTolerance = 4.0f;
    const bool isSidePushContact = actorBottom > batteryTop + verticalTolerance
        && actorTop < batteryBottom - verticalTolerance;
    if (!isSidePushContact)
    {
        return 0.0f;
    }

    const float playerWidth = playerTransform.width * playerTransform.scale;
    const float batteryWidth = batteryTransform.width * batteryTransform.scale;
    const float playerLeft = playerTransform.x;
    const float playerRight = playerTransform.x + playerWidth;
    const float batteryLeft = batteryTransform.x;
    const float batteryRight = batteryTransform.x + batteryWidth;
    const float sideTolerance = 6.0f;

    const bool touchingLeftSide = std::fabs(playerRight - batteryLeft) <= sideTolerance;
    const bool touchingRightSide = std::fabs(playerLeft - batteryRight) <= sideTolerance;
    if (touchingLeftSide)
    {
        return 1.0f;
    }
    if (touchingRightSide)
    {
        return -1.0f;
    }

    return 0.0f;
}

bool GameScene::IsConveyorUnderBattery(const TransformComponent& batteryTransform, float tileSize, int& outDirectionX, float& velocityX) const
{
    for (Entity* conveyorEntity : m_world.EntitiesByTag(EntityTag::ConveyorBelt))
    {
        if (!conveyorEntity)
            continue;

        auto* conveyor = conveyorEntity->GetComponent<BeltConveyorComponent>();
        auto* conveyorTransform = conveyorEntity->GetComponent<TransformComponent>();
        if (!conveyor || !conveyorTransform)
            continue;

        const float conveyorWidth = conveyorTransform->width * conveyorTransform->scale;
        const float batteryWidth = batteryTransform.width * batteryTransform.scale;
        const float conveyorLeft = conveyorTransform->x;
        const float conveyorRight = conveyorTransform->x + conveyorWidth;
        const float batteryLeft = batteryTransform.x;
        const float batteryRight = batteryTransform.x + batteryWidth;
        const float batteryHeight =
            batteryTransform.height * batteryTransform.scale;
        const float batteryBottom =
            batteryTransform.y + batteryHeight;
        const float conveyorTop = conveyorTransform->y;
        
        const bool overlapX = batteryRight > conveyorLeft && batteryLeft < conveyorRight;
        const float topTolerance = std::max(6.0f, tileSize * 0.22f);
        const bool onTop = std::fabs(batteryBottom - conveyorTop) <= topTolerance;

        if (!overlapX || !onTop)
        {
            continue;
        }

        outDirectionX = conveyor->directionX;
        velocityX = conveyor->velocity;
        return true;
    }

    return false;
}

void GameScene::UpdateSingleBattery(
    Entity& batteryEntity,
    Entity* player,
    const std::vector<Entity*>& enemies,
    const std::vector<TransformComponent>& groundPlatforms,
    float deltaTime,
    float tileSize)
{
    auto* battery = batteryEntity.GetComponent<BatteryComponent>();
    auto* transform = batteryEntity.GetComponent<TransformComponent>();
    if (!battery || !transform)
    {
        return;
    }

    const float width = transform->width * transform->scale;
    const float height = transform->height * transform->scale;
    const float previousX = transform->x;
    const float previousY = transform->y;

    // 接地中は支持面を先に確認し、開始フレームの大きなdeltaTimeによる床抜けを防ぐ。
    const bool keptGrounded =
        battery->grounded &&
        battery->velocityY <= 0.0f &&
        TrySnapToGroundUsingPlatforms(*transform, gGroundSnapDistance, groundPlatforms);
    float fallVelocity = 0.0f;
    bool snapped = keptGrounded;
    if (keptGrounded)
    {
        battery->velocityY = 0.0f;
    }
    else
    {
        fallVelocity = std::min(
            battery->maxFallSpeed,
            battery->velocityY + battery->gravity * deltaTime);
        battery->velocityY = fallVelocity;
        transform->y += battery->velocityY * deltaTime;
        snapped = TrySnapToGroundUsingPlatforms(
            *transform,
            gGroundSnapDistance,
            groundPlatforms);
    }

    battery->grounded = snapped;
    if (snapped && battery->velocityY > 0.0f)
    {
        battery->velocityY = 0.0f;
    }

    const float mapHeight = GetMapPixelHeight();
    if (transform->y + height > mapHeight)
    {
        transform->y = mapHeight - height;
        battery->grounded = true;
        battery->velocityY = 0.0f;
    }

    const bool fallingHitActive = !battery->grounded && fallVelocity >= battery->fallDamageSpeed;
    bool touchedActor = false;

    if (player && IntersectsEntity(batteryEntity, *player))
    {
        touchedActor = true;
        if (fallingHitActive)
        {
            HandlePlayerDamage(*player, &batteryEntity, "Battery impact damaged player", battery->contactDamage);
        }
    }

    for (Entity* enemyEntity : enemies)
    {
        if (!enemyEntity || enemyEntity == &batteryEntity)
        {
            continue;
        }
        if (!IntersectsEntity(batteryEntity, *enemyEntity))
        {
            continue;
        }
        touchedActor = true;
        if (fallingHitActive)
        {
            HandleEnemyDamage(*enemyEntity, &batteryEntity, battery->contactDamage, "Battery impact damaged enemy");
        }
    }

    if (!fallingHitActive)
    {
        const bool onTopOfSwitchOrElevator = IsBatteryOnTopOfSwitchOrDynamicEntity(*transform, &batteryEntity, tileSize);
        if (onTopOfSwitchOrElevator)
        {
            battery->grounded = true;
            battery->velocityY = 0.0f;
            SnapBatteryToSwitchOrDynamicEntity(*transform, &batteryEntity, tileSize);
        }

        float pushDirection = 0.0f;
        if (player)
        {
            const auto* playerTransform = player->GetComponent<TransformComponent>();
            if (playerTransform)
            {
                pushDirection = GetBatteryPushDirectionFromPlayer(*playerTransform, *transform);
            }
        }

        if (std::fabs(pushDirection) > 0.1f)
        {
            battery->velocityX = pushDirection * battery->pushSpeed;
        }
        else
        {
            battery->velocityX *= 0.82f;
            if (std::fabs(battery->velocityX) < 6.0f)
            {
                battery->velocityX = 0.0f;
            }
        }
    }
    else
    {
        battery->velocityX *= 0.90f;
    }

    transform->x += battery->velocityX * deltaTime;
    transform->x = std::clamp(transform->x, 0.0f, std::max(0.0f, GetMapPixelWidth() - width));

    if (IsBatteryCollidingWithWorld(*transform, &batteryEntity, tileSize))
    {
        bool steppedUp = false;
        const float maxStepHeight = tileSize * 0.5f;
        const bool onTopOfSwitchOrElevator = IsBatteryOnTopOfSwitchOrDynamicEntity(*transform, &batteryEntity, tileSize);
        if (battery->grounded && maxStepHeight > 0.0f && !onTopOfSwitchOrElevator)
        {
            TransformComponent stepCandidate = *transform;
            stepCandidate.y = std::max(0.0f, stepCandidate.y - maxStepHeight);
            if (!IsBatteryCollidingWithWorld(stepCandidate, &batteryEntity, tileSize))
            {
                transform->y = stepCandidate.y;
                steppedUp = true;
            }
        }

        if (!steppedUp)
        {
            transform->x = previousX;
            battery->velocityX = 0.0f;
        }
    }

    if (fallingHitActive && touchedActor)
    {
        battery->velocityY = std::max(0.0f, battery->velocityY * 0.35f);
        transform->y = std::max(previousY, transform->y - tileSize * 0.08f);
    }

    if (SnapBatteryToSwitchOrDynamicEntity(*transform, &batteryEntity, tileSize))
    {
        battery->grounded = true;
        battery->velocityY = 0.0f;
    }

    int direction = 0;
    float velocity = 0.0f;
    if (IsConveyorUnderBattery(*transform, tileSize, direction,velocity))
    {
        battery->velocityX = velocity * direction;
    }
}

