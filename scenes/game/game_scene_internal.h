#pragma once

#include "game_scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <tracy/Tracy.hpp>

#include "components.h"
#include "directX.h"
#include "game_session.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "photo_filter_rules.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"

namespace game_scene_detail
{
inline constexpr const char* kTuningFilePath = "assets/tuning.json";
constexpr float kPixelsPerMeter = 100.0f;
inline float gCameraViewWidth = 1120.0f;
inline float gCameraViewHeight = 630.0f;
inline float gPlayerMoveSpeed = 320.0f;
inline float gPlayerJumpSpeed = -760.0f;
inline float gPlayerGravity = 1900.0f;
inline float gPlayerMaxFallSpeed = 980.0f;
inline float gPlayerDodgeSpeed = 780.0f;
inline float gPlayerDodgeDistance = 124.8f;
inline float gPlayerDodgeInvincibilitySeconds = 0.16f;
inline float gPlayerDodgeCooldown = 0.45f;
inline float gCoyoteTimeSeconds = 0.10f;
inline float gGroundSnapDistance = 8.0f;
inline float gGroundStepUpHeight = 0.25f;
inline float gShutterFlashSeconds = 0.18f;
inline float gCaptureWidthTiles = 5.0f;
inline float gCaptureHeightTiles = 3.0f;
inline float gPrintedPhotoPaddingX = 16.0f;
inline float gPrintedPhotoPaddingTop = 16.0f;
inline float gPrintedPhotoFooterHeight = 52.0f;
inline float gPrintedPhotoMinWidth = 120.0f;
inline float gPrintedPhotoMinHeight = 144.0f;
inline float gPrintedPhotoMatteInset = 3.0f;
inline float gPickupTimeBonus = 8.0f;
inline float gBarrelGravity = 1900.0f;
inline float gBarrelMaxFallSpeed = 980.0f;
inline float gBarrelRollSpeed = 220.0f;
inline float gBarrelGroundFriction = 720.0f;
inline int gBarrelContactDamage = 1;
inline float gBarrelBreakMinFallDistance = 99999.0f;
inline float gBarrelBreakMinImpactSpeed = 99999.0f;
constexpr float kSurfaceContactEpsilon = 1.0f;
constexpr float kHorizontalCollisionEpsilon = 1.0f;

inline float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

struct GameSceneTuningEntry
{
    const char* label;
    float* value;
    float step;
    float minValue;
    float maxValue;
};

inline float GetPlayerDodgeDuration()
{
    return gPlayerDodgeSpeed > 0.0f
        ? gPlayerDodgeDistance / gPlayerDodgeSpeed
        : 0.0f;
}

inline auto BuildGameSceneTuningEntries()
{
    return std::array<GameSceneTuningEntry, 22>
    {{
        { "Camera Width", &gCameraViewWidth, 20.0f, 640.0f, 1920.0f },
        { "Camera Height", &gCameraViewHeight, 20.0f, 360.0f, 1080.0f },
        { "Move Speed", &gPlayerMoveSpeed, 10.0f, 80.0f, 960.0f },
        { "Jump Speed", &gPlayerJumpSpeed, 20.0f, -1600.0f, -120.0f },
        { "Gravity", &gPlayerGravity, 50.0f, 200.0f, 4000.0f },
        { "Max Fall", &gPlayerMaxFallSpeed, 20.0f, 200.0f, 2400.0f },
        { "Dodge Speed", &gPlayerDodgeSpeed, 10.0f, 0.0f, 1600.0f },
        { "Dodge Dist", &gPlayerDodgeDistance, 4.0f, 0.0f, 480.0f },
        { "Dodge I-Frame", &gPlayerDodgeInvincibilitySeconds, 0.01f, 0.0f, 1.0f },
        { "Dodge Cooldown", &gPlayerDodgeCooldown, 0.01f, 0.0f, 2.0f },
        { "Coyote", &gCoyoteTimeSeconds, 0.01f, 0.0f, 0.4f },
        { "Ground Snap", &gGroundSnapDistance, 0.5f, 0.0f, 24.0f },
        { "Step Up", &gGroundStepUpHeight, 0.25f, 0.0f, 8.0f },
        { "Capture W Tiles", &gCaptureWidthTiles, 0.25f, 1.0f, 16.0f },
        { "Capture H Tiles", &gCaptureHeightTiles, 0.25f, 1.0f, 16.0f },
        { "Print Pad X", &gPrintedPhotoPaddingX, 1.0f, 0.0f, 80.0f },
        { "Print Pad Top", &gPrintedPhotoPaddingTop, 1.0f, 0.0f, 80.0f },
        { "Print Footer", &gPrintedPhotoFooterHeight, 2.0f, 0.0f, 160.0f },
        { "Print Min W", &gPrintedPhotoMinWidth, 4.0f, 32.0f, 320.0f },
        { "Print Min H", &gPrintedPhotoMinHeight, 4.0f, 32.0f, 400.0f },
        { "Matte Inset", &gPrintedPhotoMatteInset, 0.5f, 0.0f, 24.0f },
        { "Pickup Bonus", &gPickupTimeBonus, 1.0f, 0.0f, 60.0f },
    }};
}

inline void GetTileCaptureTint(int tileValue, float& r, float& g, float& b, float& a)
{
    a = 1.0f;
    switch (tileValue)
    {
    case 1:
        r = 0.22f;
        g = 0.40f;
        b = 0.76f;
        break;
    case 2:
        r = 0.784f;
        g = 0.941f;
        b = 1.0f;
        break;
    case 3:
        r = 0.34f;
        g = 0.86f;
        b = 0.66f;
        break;
        break;
    case 4:
        r = 0.88f;
        g = 0.24f;
        b = 0.22f;
        break;
    case 5:
        r = 0.86f;
        g = 0.80f;
        b = 0.26f;
        break;
    case 6:
        r = 0.54f;
        g = 0.84f;
        b = 0.34f;
        break;
    case 7:
        r = 0.34f;
        g = 0.86f;
        b = 0.66f;
        break;
    case 8:
        r = 0.54f;
        g = 0.84f;
        b = 0.34f;
        break;
    case 9:
        r = 0.34f;
        g = 0.86f;
        b = 0.66f;
        break;
    case TileMap::kPitTileValue:
        r = 0.08f;
        g = 0.09f;
        b = 0.13f;
        break;
    default:
        r = 0.70f;
        g = 0.74f;
        b = 0.82f;
        break;
    }
}

inline PhotoCopyRole GetTileCopyRole(int tileValue)
{
    if (tileValue == 4)
    {
        return PhotoCopyRole::Hazard;
    }
    if (tileValue == TileMap::kPitTileValue)
    {
        return PhotoCopyRole::Hazard;
    }
    if (tileValue == 5)
    {
        return PhotoCopyRole::GoalRelay;
    }
    return PhotoCopyRole::Solid;
}

inline PhotoCopyOrigin GetTileCopyOrigin(int tileValue)
{
    static_cast<void>(tileValue);
    return PhotoCopyOrigin::Tile;
}

inline float GetTintBrightness(float r, float g, float b)
{
    return r * 0.299f + g * 0.587f + b * 0.114f;
}

inline PhotoCopyLayer GetLayerFromTint(float r, float g, float b)
{
    if (GetTintBrightness(r, g, b) <= 0.33f)
    {
        return PhotoCopyLayer::Shadow;
    }
    return PhotoCopyLayer::Foreground;
}

inline PhotoCopyRole GetRoleFromTint(float r, float g, float b)
{
    if (GetTintBrightness(r, g, b) < 0.33f)
    {
        return PhotoCopyRole::Solid;
    }
    if (r >= g && r >= b)
    {
        return PhotoCopyRole::Hazard;
    }
    if (g >= r && g >= b)
    {
        return PhotoCopyRole::Pickup;
    }
    if (r > 0.7f && g > 0.65f && b < 0.35f)
    {
        return PhotoCopyRole::GoalRelay;
    }
    return PhotoCopyRole::Solid;
}

inline bool IntersectsRect(const TransformComponent& a, const TransformComponent& b)
{
    const float aWidth = a.width * a.scale;
    const float aHeight = a.height * a.scale;
    const float bWidth = b.width * b.scale;
    const float bHeight = b.height * b.scale;
    return a.x < b.x + bWidth &&
        a.x + aWidth > b.x &&
        a.y < b.y + bHeight &&
        a.y + aHeight > b.y;
}

inline bool HasTag(const Entity& entity, const char* value)
{
    const auto* tag = entity.GetComponent<TagComponent>();
    return tag && tag->tag == value;
}

inline PhotoCopyRole GetEntityCopyRole(const Entity& entity)
{
    if (HasTag(entity, "Goal"))
    {
        return PhotoCopyRole::GoalRelay;
    }
    if (HasTag(entity, "PhotoSource"))
    {
        return PhotoCopyRole::Pickup;
    }
    if (HasTag(entity, "Hazard") || HasTag(entity, "Enemy"))
    {
        return PhotoCopyRole::Hazard;
    }
    if (const auto* tint = entity.GetComponent<TintComponent>())
    {
        return GetRoleFromTint(tint->r, tint->g, tint->b);
    }
    return PhotoCopyRole::Solid;
}

inline PhotoCopyOrigin GetEntityCopyOrigin(const Entity& entity)
{
    if (HasTag(entity, "Enemy"))
    {
        return PhotoCopyOrigin::Enemy;
    }
    if (HasTag(entity, "Hazard"))
    {
        return PhotoCopyOrigin::Hazard;
    }
    if (HasTag(entity, "Goal"))
    {
        return PhotoCopyOrigin::Goal;
    }
    if (HasTag(entity, "PhotoSource"))
    {
        return PhotoCopyOrigin::Pickup;
    }
    return PhotoCopyOrigin::Generic;
}

inline float GetViewScale()
{
    const float maxWidth = static_cast<float>(SCREEN_WIDTH) - 128.0f;
    const float maxHeight = static_cast<float>(SCREEN_HEIGHT) - 128.0f;
    return std::max(1.0f, std::min(maxWidth / gCameraViewWidth, maxHeight / gCameraViewHeight));
}

inline float GetViewWidth()
{
    return gCameraViewWidth * GetViewScale();
}

inline float GetViewHeight()
{
    return gCameraViewHeight * GetViewScale();
}

inline float GetViewOriginX()
{
    return std::round((static_cast<float>(SCREEN_WIDTH) - GetViewWidth()) * 0.5f);
}

inline float GetViewOriginY()
{
    return std::round((static_cast<float>(SCREEN_HEIGHT) - GetViewHeight()) * 0.5f);
}

// 3/21追加：坂タイルの表面Y座標を取得(田之上俊)
inline bool TryGetSlopeSurfaceYShared(
    const TileMap& tileMap,
    int column,
    int row,
    float worldX,
    float& outSurfaceY)
{
    const float tileSize = tileMap.GetTileSize();
    constexpr int kMaxSpan = 5;
    const int originColumnStart = std::max(0, column - (kMaxSpan - 1));
    const int originRowStart = std::max(0, row - (kMaxSpan - 1));

    bool foundSurface = false;
    float bestSurfaceY = 0.0f;

    for (int originRow = originRowStart; originRow <= row; ++originRow)
    {
        for (int originColumn = originColumnStart; originColumn <= column; ++originColumn)
        {
            const TileTriangleShape triangle = TileMap::GetTriangleShape(tileMap.GetTile(originColumn, originRow));
            if (!triangle.isTriangle) continue;

            const float left = static_cast<float>(originColumn) * tileSize;
            const float top = static_cast<float>(originRow) * tileSize;
            const float width = static_cast<float>(triangle.widthTiles) * tileSize;
            const float height = static_cast<float>(triangle.heightTiles) * tileSize;

            if (worldX < left || worldX > left + width) continue;

            const float localX = std::clamp(worldX - left, 0.0f, width);
            const float normalizedX = width > 0.0f ? localX / width : 0.0f;
            const float surfaceY = triangle.risesRight
                ? top + height - normalizedX * height
                : top + normalizedX * height;

            if (!foundSurface || surfaceY < bestSurfaceY)
            {
                bestSurfaceY = surfaceY;
                foundSurface = true;
            }
        }
    }

    if (!foundSurface) return false;
    outSurfaceY = bestSurfaceY;
    return true;
}

void WriteTuningJsonFile();
void LoadTuningJsonFile();
}
