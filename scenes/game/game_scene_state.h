#pragma once

#include <filesystem>
#include <string>
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

struct BarrelDebrisParticle
{
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float size = 0.0f;
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

struct LaserSparkParticle
{
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
};

struct CameraFlashState
{
    bool unlocked = true;
    bool enabled = true;
    //bool unlocked = false;
    //bool enabled = false;
    float pulseRemaining = 0.0f;
    float pulseDuration = 0.0f;
};

struct GameSceneFlowState
{
    bool playerTouchingTarget = false;
    bool playerTouchingHazard = false;
    bool resultQueued = false;
    float timeLimit = 60.0f;
    float timeRemaining = 60.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float goalPulse = 0.0f;
    float pickupPulse = 0.0f;
    float captureSlowRemaining = 0.0f;
    float placementSlowRemaining = 0.0f;
    bool goalUnlocked = false;
    bool goalUnlockedBySwitch = false;
    bool cameraMode = false;
    int enemyCount = 0;
    float shutterFlashRemaining = 0.0f;
    float developedPhotoPreviewRemaining = 0.0f;
    float photoTrayReveal = 0.0f;
    float captureFinderScale = 1.0f;
    float lastDeltaTime = 0.0f;
    int cameraModeSessionId = 0;
    bool pitRestartActive = false;
    float pitRestartTimer = 0.0f;
    float pitRestartFadeInTimer = 0.0f;
    bool stageTransitionActive = false;
    float stageTransitionTimer = 0.0f;
    float stageTransitionFadeInTimer = 0.0f;
    bool hasCheckpoint = false;
    int activeCheckpointId = -1;
    float stageStartX = 0.0f;
    float stageStartY = 0.0f;
    float respawnX = 0.0f;
    float respawnY = 0.0f;
    float hitStopRemaining = 0.0f;
    float screenShakeRemaining = 0.0f;
    float screenShakeDuration = 0.0f;
    float screenShakeAmplitude = 0.0f;
    float captureModeZoomBlend = 0.0f;
    CameraFlashState cameraFlash;
    // HPバー演出用: 現在値表示と遅延表示を分離して減少演出を作る。
    float hpDisplayRatio = 1.0f;
    float hpDamageLagRatio = 1.0f;
    float hpDamageFlash = 0.0f;
    int hpLastRaw = -1;
    bool hpUiInitialized = false;
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
    bool captureAnimationActive = false;
    bool captureAnimationReleased = false;
    std::vector<PlayerAfterimage> afterimages;
};

struct GameSceneDebugState
{
    bool showCollisionDebug = true;
    bool showTuningPanel = false;
    bool showEscapeMenu = false;
    int escapeMenuSelection = 0;
    bool effectPlacementPulseEnabled = true;
    bool effectPasteStickEnabled = true;
    bool effectPasteRingEnabled = true;
    bool bgmEnabled = true;
    float bgmRestoreVolume = 0.6f;
    bool screenShakeEnabled = true;
    bool playerHealthDamageEnabled = true;
    int tuningSelection = 0;
    float tuningReloadTimer = 0.0f;
    std::filesystem::file_time_type tuningFileWriteTime{};
    bool hasTuningFileWriteTime = false;
};

struct GameSceneEffectsState
{
    std::vector<BarrelDebrisParticle> barrelDebris;
    std::vector<LaserSparkParticle> laserSparks;
};

struct GameSceneMapEditorState
{
    enum class BrushTarget
    {
        Tile,
        Marker,
    };

    bool active = false;
    BrushTarget brushTarget = BrushTarget::Tile;
    int selectedTileValue = 1;
    char selectedMarker = 'G';
    int selectedMarkerParameter = 1;
    int selectedStageLightTiles = 3;
    int selectedStageLightFixtureTiles = 1;
    std::string statusMessage;
    float statusMessageTimer = 0.0f;
};
