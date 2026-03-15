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
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"

namespace game_scene_detail
{
constexpr float kPixelsPerMeter = 100.0f;
constexpr float kBaseViewWidth = 960.0f;
constexpr float kBaseViewHeight = 480.0f;
constexpr float kPlayerMoveSpeed = 320.0f;
constexpr float kPlayerJumpSpeed = -760.0f;
constexpr float kPlayerGravity = 1900.0f;
constexpr float kPlayerMaxFallSpeed = 980.0f;
constexpr float kCoyoteTimeSeconds = 0.10f;
constexpr float kGroundSnapDistance = 8.0f;
constexpr float kShutterFlashSeconds = 0.18f;
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

inline float GetViewScale()
{
    const float maxWidth = static_cast<float>(SCREEN_WIDTH) - 128.0f;
    const float maxHeight = static_cast<float>(SCREEN_HEIGHT) - 128.0f;
    return std::max(1.0f, std::min(maxWidth / kBaseViewWidth, maxHeight / kBaseViewHeight));
}

inline float GetViewWidth()
{
    return kBaseViewWidth * GetViewScale();
}

inline float GetViewHeight()
{
    return kBaseViewHeight * GetViewScale();
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
