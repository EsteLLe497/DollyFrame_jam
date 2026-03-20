#pragma once

#include <filesystem>
#include <vector>

struct PlayerAfterimage
{
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float scale = 1.0f;
    bool flipX = false;
    float life = 0.0f;
};

struct GameSceneFlowState
{
    bool playerTouchingTarget = false;
    bool playerTouchingHazard = false;
    bool resultQueued = false;
    float timeLimit = 60.0f;
    float timeRemaining = 60.0f;
    float cameraX = 0.0f;
    float goalPulse = 0.0f;
    float pickupPulse = 0.0f;
    float captureSlowRemaining = 0.0f;
    float placementSlowRemaining = 0.0f;
    bool goalUnlocked = false;
    bool cameraMode = false;
    int enemyCount = 0;
    float shutterFlashRemaining = 0.0f;
    float developedPhotoPreviewRemaining = 0.0f;
    float photoTrayReveal = 0.0f;
    float lastDeltaTime = 0.0f;
};

struct GameScenePlayerState
{
    bool grounded = false;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float dodgeRemaining = 0.0f;
    float dodgeCooldownRemaining = 0.0f;
    float dodgeDirection = 1.0f;
    float coyoteTimeRemaining = 0.0f;
    bool facingRight = true;
    float runAnimationTime = 0.0f;
    float visualScaleX = 1.0f;
    float visualScaleY = 1.0f;
    float visualOffsetY = 0.0f;
    float visualRotation = 0.0f;
    float landingImpact = 0.0f;
    float jumpStretch = 0.0f;
    float dodgeStretch = 0.0f;
    std::vector<PlayerAfterimage> afterimages;
};

struct GameSceneDebugState
{
    bool showCollisionDebug = false;
    bool showTuningPanel = false;
    int tuningSelection = 0;
    float tuningReloadTimer = 0.0f;
    std::filesystem::file_time_type tuningFileWriteTime{};
    bool hasTuningFileWriteTime = false;
};
