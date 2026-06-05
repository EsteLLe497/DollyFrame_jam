#include "pch.h"

#include "game_scene_render_ui_helpers.h"
#include "game_scene_draw_helpers.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kPadDeadZone = 0.18f;
    constexpr float kPadCursorMaxSpeed = 920.0f;
    constexpr float kPadCursorResponse = 14.0f;
    constexpr float kPadCursorDamping = 10.0f;
    constexpr float kPadCursorMouseReturnDelay = 0.28f;

}

RgbColor game_scene_detail::GetEditorMarkerColor(char marker)
{
    switch (marker)
    {
    case 'W': return { 120, 212, 255 };
    case 'R': return { 255, 128, 128 };
    case '$': return { 255, 82, 72 };
    case '?': return { 255, 248, 150 };
    case '!': return { 255, 248, 150 };
    case '%': return { 255, 202, 92 };
    case 'A': return { 176, 176, 255 };
    case 'D': return { 96, 230, 150 };
    case 'S': return { 255, 176, 88 };
    case 'B': return { 172, 142, 255 };
    case 'V': return { 146, 255, 170 };
    case 'C': return { 246, 238, 122 };
    case 'F': return { 255, 142, 210 };
    case 'M': return { 255, 142, 210 };
    case 'Y': return { 240, 208, 90 };
    case 'H': return { 214, 124, 255 };
    case 'I': return { 188, 108, 255 };
    case 'K': return { 250, 112, 96 };
    case 'L': return { 140, 186, 230 };
    case 'Q': return { 118, 166, 214 };
    case 'J': return { 154, 162, 178 };
    case 'O': return { 255, 214, 72 };
    case 'X': return { 255, 156, 72 };
    case 'U': return { 255, 104, 104 };
    case 'Z': return { 255, 84, 128 };
    case 'P': return { 255, 232, 84 };
    case '*': return { 255, 232, 84 };
    case '@': return { 255, 220, 96 };
    case 'N': return { 255, 248, 150 };
    case 'G': return { 255, 235, 128 };
    case 'T': return { 122, 230, 255 };
    case 'E': return { 180, 255, 196 };
    default: break;
    }

    return { 236, 236, 236 };
}

void game_scene_detail::UpdatePadCursor(
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

const char* game_scene_detail::GetStageGuideText(float playerX)
{
    static_cast<void>(playerX);
    return "Sandbox: choose filter 1-4, capture, then place up to three copy groups.";
}

void game_scene_detail::DrawCapturedPreviewItem(
    int fallbackTextureId,
    const CapturedPhotoItem& item,
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    float alpha)
{
    Shader_ResetStyle();
    Shader_SetTint(item.tintR, item.tintG, item.tintB, std::min(1.0f, item.tintA) * alpha);
    if (item.spawnArchetype == CapturedSpawnArchetype::Projectile)
    {
        const float projectileAngle = std::atan2(item.projectileVelocityY, item.projectileVelocityX);
        const int color = GetColor(
            static_cast<int>(std::round(item.tintR * 255.0f)),
            static_cast<int>(std::round(item.tintG * 255.0f)),
            static_cast<int>(std::round(item.tintB * 255.0f)));
        DrawProjectileItem(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            item.flipX,
            projectileAngle,
            color);
        Shader_ResetStyle();
        return;
    }

    if (photo_shared::DrawDamagePlatformItemPreview(
            item,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            std::min(1.0f, item.tintA) * alpha))
    {
        Shader_ResetStyle();
        return;
    }

    if (photo_shared::DrawSpikeStripItemPreview(
            item,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            std::min(1.0f, item.tintA) * alpha))
    {
        Shader_ResetStyle();
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
        Shader_ResetStyle();
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
    Shader_ResetStyle();
}
