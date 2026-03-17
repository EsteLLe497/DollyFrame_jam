#pragma once

#include "game_scene.h"

#include <algorithm>
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
constexpr float kPixelsPerMeter = 100.0f;
inline float gCameraViewWidth = 1120.0f;
inline float gCameraViewHeight = 630.0f;
inline float gPlayerMoveSpeed = 320.0f;
inline float gPlayerJumpSpeed = -760.0f;
inline float gPlayerGravity = 1900.0f;
inline float gPlayerMaxFallSpeed = 980.0f;
inline float gPlayerDodgeSpeed = 780.0f;
inline float gPlayerDodgeDuration = 0.16f;
inline float gPlayerDodgeCooldown = 0.45f;
inline float gCoyoteTimeSeconds = 0.10f;
inline float gGroundSnapDistance = 8.0f;
inline float gShutterFlashSeconds = 0.18f;
inline float gCaptureWidthScale = 1.85f;
inline float gCaptureHeightScale = 1.15f;
inline float gPickupTimeBonus = 8.0f;
constexpr float kSurfaceContactEpsilon = 1.0f;
constexpr float kHorizontalCollisionEpsilon = 1.0f;

inline float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
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
        r = 0.92f;
        g = 0.54f;
        b = 0.20f;
        break;
    case 3:
        r = 0.22f;
        g = 0.72f;
        b = 0.48f;
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
}
