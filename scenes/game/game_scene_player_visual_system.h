#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

#include "components.h"
#include "game_scene_internal.h"
#include "game_scene_state.h"

namespace game_scene_player_visual_system
{
using namespace game_scene_detail;

inline constexpr float kPlayerAfterimageLifetime = 0.18f;
inline constexpr float kPlayerAfterimageSpawnInterval = 0.03f;
inline constexpr int kMaxPlayerAfterimages = 8;
inline constexpr int kPlayerMoveSheetColumns = 4;
inline constexpr int kPlayerMoveSheetRows = 3;
inline constexpr int kPlayerMoveFrameCount = 12;
inline constexpr float kPlayerMoveFps = 18.0f;
inline constexpr int kPlayerDodgeFrameCount = 4;
inline constexpr float kPlayerDodgeFps = 28.0f;
inline constexpr int kPlayerJumpSheetColumns = 5;
inline constexpr int kPlayerJumpSheetRows = 4;
inline constexpr int kPlayerJumpAscentStartFrame = 0;
inline constexpr int kPlayerJumpAscentFrameCount = 12;
inline constexpr float kPlayerJumpAscentFps = 22.0f;
inline constexpr int kPlayerJumpFallStartFrame = 12;
inline constexpr int kPlayerJumpFallFrameCount = 8;
inline constexpr float kPlayerJumpFallFps = 14.0f;
inline constexpr int kPlayerCaptureSheetColumns = 7;
inline constexpr int kPlayerCaptureSheetRows = 3;
inline constexpr int kPlayerCaptureTotalFrames = 21;
inline constexpr int kPlayerCaptureHoldLastFrame = 10;
inline constexpr int kPlayerCaptureHoldFrameCount = kPlayerCaptureHoldLastFrame + 1;
inline constexpr float kPlayerCaptureHoldFps = 18.0f;
inline constexpr int kPlayerCaptureReleaseStartFrame = kPlayerCaptureHoldLastFrame + 1;
inline constexpr int kPlayerCaptureReleaseFrameCount = kPlayerCaptureTotalFrames - kPlayerCaptureReleaseStartFrame;
inline constexpr float kPlayerCaptureReleaseFps = 18.0f;
inline constexpr int kPlayerPasteSheetColumns = 7;
inline constexpr int kPlayerPasteSheetRows = 4;
inline constexpr int kPlayerPasteTotalFrames = 28;
inline constexpr int kPlayerPasteHoldLastFrame = 12;
inline constexpr int kPlayerPasteHoldFrameCount = kPlayerPasteHoldLastFrame + 1;
inline constexpr float kPlayerPasteHoldFps = 18.0f;
inline constexpr int kPlayerPasteReleaseStartFrame = kPlayerPasteHoldLastFrame + 1;
inline constexpr int kPlayerPasteReleaseFrameCount = kPlayerPasteTotalFrames - kPlayerPasteReleaseStartFrame;
inline constexpr float kPlayerPasteReleaseFps = 18.0f;
inline constexpr int kPlayerAttackSheetColumns = 5;
inline constexpr int kPlayerAttackSheetRows = 3;
inline constexpr int kPlayerAttackFrameCount = 15;
inline constexpr float kPlayerAttackFps = 18.0f;
inline constexpr int kPlayerIdleSheetColumns = 5;
inline constexpr int kPlayerIdleSheetRows = 9;
inline constexpr int kPlayerIdleFrameCount = 45;
inline constexpr float kPlayerIdleFps = 12.0f;

inline void ConfigurePlayerSpriteAnimation(Entity& player, int idleTextureId = -1, int moveTextureId = -1, int jumpTextureId = -1, int captureTextureId = -1, int pasteTextureId = -1, int attackTextureId = -1)
{
    auto* sprite = player.GetComponent<SpriteRenderComponent>();
    if (!sprite)
    {
        return;
    }

    auto* animation = player.GetComponent<SpriteSheetAnimationComponent>();
    if (!animation)
    {
        animation = &player.AddComponent<SpriteSheetAnimationComponent>();
    }

    const int textureId = sprite->GetTextureId();
    const int resolvedIdleTextureId = idleTextureId >= 0 ? idleTextureId : textureId;
    const int resolvedMoveTextureId = moveTextureId >= 0 ? moveTextureId : textureId;
    const int resolvedJumpTextureId = jumpTextureId >= 0 ? jumpTextureId : textureId;
    const int resolvedCaptureTextureId = captureTextureId >= 0 ? captureTextureId : textureId;
    const int resolvedPasteTextureId = pasteTextureId >= 0 ? pasteTextureId : textureId;
    const int resolvedAttackTextureId = attackTextureId >= 0 ? attackTextureId : resolvedPasteTextureId;
    animation->DefineClip("idle", resolvedIdleTextureId, kPlayerIdleSheetColumns, kPlayerIdleSheetRows, 0, kPlayerIdleFrameCount, kPlayerIdleFps, true);
    animation->DefineClip("run", resolvedMoveTextureId, kPlayerMoveSheetColumns, kPlayerMoveSheetRows, 0, kPlayerMoveFrameCount, kPlayerMoveFps, true);
    animation->DefineClip("jump", resolvedJumpTextureId, kPlayerJumpSheetColumns, kPlayerJumpSheetRows, kPlayerJumpAscentStartFrame, kPlayerJumpAscentFrameCount, kPlayerJumpAscentFps, false);
    animation->DefineClip("fall", resolvedJumpTextureId, kPlayerJumpSheetColumns, kPlayerJumpSheetRows, kPlayerJumpFallStartFrame, kPlayerJumpFallFrameCount, kPlayerJumpFallFps, false);
    animation->DefineClip("capture_hold", resolvedCaptureTextureId, kPlayerCaptureSheetColumns, kPlayerCaptureSheetRows, 0, kPlayerCaptureHoldFrameCount, kPlayerCaptureHoldFps, false);
    animation->DefineClip("capture_release", resolvedCaptureTextureId, kPlayerCaptureSheetColumns, kPlayerCaptureSheetRows, kPlayerCaptureReleaseStartFrame, kPlayerCaptureReleaseFrameCount, kPlayerCaptureReleaseFps, false);
    animation->DefineClip("paste_hold", resolvedPasteTextureId, kPlayerPasteSheetColumns, kPlayerPasteSheetRows, 0, kPlayerPasteHoldFrameCount, kPlayerPasteHoldFps, false);
    animation->DefineClip("paste_release", resolvedPasteTextureId, kPlayerPasteSheetColumns, kPlayerPasteSheetRows, kPlayerPasteReleaseStartFrame, kPlayerPasteReleaseFrameCount, kPlayerPasteReleaseFps, false);
    animation->DefineClip("paste_attack_release", resolvedAttackTextureId, kPlayerAttackSheetColumns, kPlayerAttackSheetRows, 0, kPlayerAttackFrameCount, kPlayerAttackFps, false);
    animation->DefineClip("dodge", resolvedMoveTextureId, kPlayerMoveSheetColumns, kPlayerMoveSheetRows, 0, kPlayerDodgeFrameCount, kPlayerDodgeFps, false);
    animation->Play("idle", true);
}

inline void ClearPhotoPoseAnimations(GameScenePlayerState& playerState)
{
    playerState.captureAnimationActive = false;
    playerState.captureAnimationReleased = false;
    playerState.pasteAnimationActive = false;
    playerState.pasteAnimationReleased = false;
    playerState.pasteAnimationEnemyAttack = false;
}

inline void ResetSpriteAnimationToIdle(GameScenePlayerState& playerState, Entity& player)
{
    ClearPhotoPoseAnimations(playerState);
    playerState.dodgeRemaining = 0.0f;
    playerState.dodgeCooldownRemaining = 0.0f;
    playerState.visualOffsetY = 0.0f;
    playerState.visualRotation = 0.0f;
    playerState.visualScaleX = 1.0f;
    playerState.visualScaleY = 1.0f;
    playerState.landingImpact = 0.0f;
    playerState.jumpStretch = 0.0f;
    playerState.dodgeStretch = 0.0f;
    playerState.afterimages.clear();

    if (auto* animation = player.GetComponent<SpriteSheetAnimationComponent>())
    {
        animation->Play("idle", true);
    }
    if (auto* sprite = player.GetComponent<SpriteRenderComponent>())
    {
        sprite->SetFlipX(!playerState.facingRight);
        sprite->SetRenderOffset(0.0f, 0.0f);
        sprite->SetRenderScale(1.0f, 1.0f);
        sprite->SetRenderRotationOffset(0.0f);
    }
}

inline void UpdateAnimation(
    GameScenePlayerState& playerState,
    GameSceneFlowState& flowState,
    Entity& player,
    bool isDodging)
{
    auto* sprite = player.GetComponent<SpriteRenderComponent>();
    auto* animation = player.GetComponent<SpriteSheetAnimationComponent>();
    if (!sprite || !animation)
    {
        return;
    }

    if (playerState.captureAnimationActive && playerState.pasteAnimationActive)
    {
        if (flowState.cameraMode)
        {
            playerState.pasteAnimationActive = false;
            playerState.pasteAnimationReleased = false;
            playerState.pasteAnimationEnemyAttack = false;
        }
        else
        {
            playerState.captureAnimationActive = false;
            playerState.captureAnimationReleased = false;
        }
        playerState.afterimages.clear();
    }

    const char* clipName = "idle";
    const float horizontalSpeed = std::fabs(playerState.velocityX);
    if (playerState.captureAnimationActive)
    {
        clipName = playerState.captureAnimationReleased ? "capture_release" : "capture_hold";
    }
    else if (playerState.pasteAnimationActive)
    {
        clipName = playerState.pasteAnimationReleased
            ? (playerState.pasteAnimationEnemyAttack ? "paste_attack_release" : "paste_release")
            : "paste_hold";
    }
    else if (isDodging)
    {
        clipName = "dodge";
    }
    else if (!playerState.grounded)
    {
        clipName = playerState.velocityY < 0.0f ? "jump" : "fall";
    }
    else if (horizontalSpeed > 40.0f)
    {
        clipName = "run";
    }

    if (playerState.captureAnimationActive &&
        playerState.captureAnimationReleased &&
        animation->GetCurrentClipName() != "capture_release")
    {
        clipName = "capture_release";
        animation->Play(clipName, true);
    }
    else if (playerState.pasteAnimationActive &&
        playerState.pasteAnimationReleased &&
        animation->GetCurrentClipName() != (playerState.pasteAnimationEnemyAttack ? "paste_attack_release" : "paste_release"))
    {
        clipName = playerState.pasteAnimationEnemyAttack ? "paste_attack_release" : "paste_release";
        animation->Play(clipName, true);
    }
    else
    {
        animation->Play(clipName);
    }

    if (playerState.captureAnimationActive &&
        playerState.captureAnimationReleased &&
        animation->GetCurrentClipName() == "capture_release" &&
        animation->GetCurrentFrameIndex() >= (kPlayerCaptureReleaseStartFrame + kPlayerCaptureReleaseFrameCount - 1))
    {
        playerState.captureAnimationActive = false;
        playerState.captureAnimationReleased = false;
    }
    if (playerState.pasteAnimationActive &&
        playerState.pasteAnimationReleased &&
        (((animation->GetCurrentClipName() == "paste_release") &&
            animation->GetCurrentFrameIndex() >= (kPlayerPasteReleaseStartFrame + kPlayerPasteReleaseFrameCount - 1)) ||
            ((animation->GetCurrentClipName() == "paste_attack_release") &&
                animation->GetCurrentFrameIndex() >= (kPlayerAttackFrameCount - 1))))
    {
        playerState.pasteAnimationActive = false;
        playerState.pasteAnimationReleased = false;
        playerState.pasteAnimationEnemyAttack = false;
    }

    const std::string_view currentClip(clipName);
    const bool usesNewCharacterSheet =
        currentClip == "idle" ||
        currentClip == "run" ||
        currentClip == "dodge" ||
        currentClip == "jump" ||
        currentClip == "fall" ||
        currentClip == "capture_hold" ||
        currentClip == "capture_release" ||
        currentClip == "paste_hold" ||
        currentClip == "paste_release" ||
        currentClip == "paste_attack_release";
    sprite->SetFlipX(usesNewCharacterSheet ? !playerState.facingRight : playerState.facingRight);
}

inline void UpdatePresentation(
    GameScenePlayerState& playerState,
    Entity& player,
    float deltaTime,
    float moveAxis,
    bool wasGrounded,
    bool isDodging,
    bool landedThisFrame)
{
    auto* sprite = player.GetComponent<SpriteRenderComponent>();
    if (!sprite)
    {
        return;
    }

    static_cast<void>(moveAxis);
    constexpr float kJumpStretchScaleX = -0.045f;
    constexpr float kJumpStretchScaleY = 0.095f;
    constexpr float kLandingStretchScaleX = 0.115f;
    constexpr float kLandingStretchScaleY = -0.075f;
    constexpr float kDodgeStretchScaleX = 0.16f;
    constexpr float kDodgeStretchScaleY = -0.10f;
    constexpr float kDodgeLeanRadians = 0.055f;
    constexpr float kJumpStretchDecay = 13.0f;
    constexpr float kLandingImpactDecay = 16.0f;
    constexpr float kDodgeStretchEnterResponse = 42.0f;
    constexpr float kDodgeStretchExitResponse = 18.0f;

    if (wasGrounded && !playerState.grounded && playerState.velocityY < 0.0f)
    {
        playerState.jumpStretch = std::max(playerState.jumpStretch, 1.0f);
    }
    if (landedThisFrame)
    {
        playerState.landingImpact = std::max(playerState.landingImpact, 1.0f);
        playerState.jumpStretch = 0.0f;
    }

    const float jumpDecay = 1.0f - std::pow(0.001f, std::max(0.0f, deltaTime) * kJumpStretchDecay);
    const float landingDecay = 1.0f - std::pow(0.001f, std::max(0.0f, deltaTime) * kLandingImpactDecay);
    const float dodgeResponse = isDodging ? kDodgeStretchEnterResponse : kDodgeStretchExitResponse;
    const float dodgeBlend = 1.0f - std::pow(0.001f, std::max(0.0f, deltaTime) * dodgeResponse);
    playerState.jumpStretch = std::lerp(playerState.jumpStretch, 0.0f, jumpDecay);
    playerState.landingImpact = std::lerp(playerState.landingImpact, 0.0f, landingDecay);
    playerState.dodgeStretch = std::lerp(playerState.dodgeStretch, isDodging ? 1.0f : 0.0f, dodgeBlend);

    playerState.visualScaleX =
        1.0f +
        playerState.jumpStretch * kJumpStretchScaleX +
        playerState.landingImpact * kLandingStretchScaleX +
        playerState.dodgeStretch * kDodgeStretchScaleX;
    playerState.visualScaleY =
        1.0f +
        playerState.jumpStretch * kJumpStretchScaleY +
        playerState.landingImpact * kLandingStretchScaleY +
        playerState.dodgeStretch * kDodgeStretchScaleY;

    const auto* transform = player.GetComponent<TransformComponent>();
    const float baseWidth = transform ? transform->width * transform->scale : 0.0f;
    const float baseHeight = transform ? transform->height * transform->scale : 0.0f;
    const float visualOffsetX = baseWidth * (1.0f - playerState.visualScaleX) * 0.5f;
    playerState.visualOffsetY = baseHeight * (1.0f - playerState.visualScaleY);
    playerState.visualRotation = playerState.dodgeDirection * playerState.dodgeStretch * kDodgeLeanRadians;
    sprite->SetRenderScale(playerState.visualScaleX, playerState.visualScaleY);
    sprite->SetRenderOffset(visualOffsetX, playerState.visualOffsetY);
    sprite->SetRenderRotationOffset(playerState.visualRotation);
}

inline void UpdateAfterimages(GameScenePlayerState& playerState, float deltaTime)
{
    for (PlayerAfterimage& afterimage : playerState.afterimages)
    {
        afterimage.life = std::max(0.0f, afterimage.life - std::max(0.0f, deltaTime));
    }
    std::erase_if(
        playerState.afterimages,
        [](const PlayerAfterimage& afterimage)
        {
            return afterimage.life <= 0.0f;
        });
}

inline void TrySpawnAfterimage(
    GameScenePlayerState& playerState,
    const TransformComponent& transform,
    const SpriteRenderComponent& sprite)
{
    if (!playerState.afterimages.empty() &&
        playerState.afterimages.back().life > kPlayerAfterimageLifetime - kPlayerAfterimageSpawnInterval)
    {
        return;
    }

    PlayerAfterimage afterimage;
    afterimage.x = transform.x + sprite.GetRenderOffsetX();
    afterimage.y = transform.y + sprite.GetRenderOffsetY();
    afterimage.rotation = transform.rotation + sprite.GetRenderRotationOffset();
    afterimage.scale = transform.scale;
    afterimage.renderScaleX = sprite.GetRenderScaleX();
    afterimage.renderScaleY = sprite.GetRenderScaleY();
    afterimage.flipX = sprite.GetFlipX();
    afterimage.life = kPlayerAfterimageLifetime;
    afterimage.textureId = sprite.GetTextureId();
    afterimage.sourceX = sprite.GetSourceX();
    afterimage.sourceY = sprite.GetSourceY();
    afterimage.sourceWidth = sprite.GetSourceWidth();
    afterimage.sourceHeight = sprite.GetSourceHeight();
    playerState.afterimages.push_back(afterimage);

    if (playerState.afterimages.size() > kMaxPlayerAfterimages)
    {
        playerState.afterimages.erase(playerState.afterimages.begin());
    }
}
}
