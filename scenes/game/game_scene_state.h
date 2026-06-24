#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "components_combat.h"
#include "game_scene_photo_state.h"

class ResourceManager;

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
    float gravityScale = 0.35f;
    float sizeScale = 1.0f;
    bool drawCircle = false;
    float r = 1.0f;
    float g = 0.76f;
    float b = 0.28f;
};

struct SlamDustParticle
{
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
    float alphaScale = 0.62f;
    float r = 0.72f;
    float g = 0.64f;
    float b = 0.52f;
};

struct BeamShockwaveParticle
{
    float x = 0.0f;
    float y = 0.0f;
    float startRadius = 0.0f;
    float endRadius = 0.0f;
    float thickness = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
    float directionX = 0.0f;
    float r = 0.72f;
    float g = 0.94f;
    float b = 1.0f;
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
    bool shieldBossDefeatedThisScene = false;
    bool cameraMode = false;
    int enemyCount = 0;
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
    bool stageBgmCrossFadePending = false;
    float stageBgmCrossFadeDelayRemaining = 0.0f;
    float captureModeZoomBlend = 0.0f;
};


struct GameSceneUiState
{
    float shutterFlashRemaining = 0.0f;
    float developedPhotoPreviewRemaining = 0.0f;
    float photoTrayReveal = 0.0f;
    float captureFinderScale = 1.0f;
    float captureRapidTimer = 0.0f;
    float captureLockoutRemaining = 0.0f;
    int captureRapidCount = 0;
    CameraFlashState cameraFlash;
    float hpDisplayRatio = 1.0f;
    float hpDamageLagRatio = 1.0f;
    float hpDamageFlash = 0.0f;
    int hpLastRaw = -1;
    bool hpUiInitialized = false;
    bool merchantShopOpen = false;
    int merchantSelection = 0;
    float merchantMessageTimer = 0.0f;
    std::string merchantMessage;
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
    bool pasteAnimationActive = false;
    bool pasteAnimationReleased = false;
    bool pasteAnimationEnemyAttack = false;
    std::vector<PlayerAfterimage> afterimages;
};

struct GameSceneDebugState
{
    bool showCollisionDebug = true;
    bool showTuningPanel = false;
    bool showEscapeMenu = false;
    bool showBackdropGrid = true;
    bool hideNonPhotoUi = false;
    int escapeMenuSelection = 0;
    bool effectPlacementPulseEnabled = true;
    bool effectPasteStickEnabled = true;
    bool effectPasteRingEnabled = true;
    bool sepiaFilmFilterDryRunEnabled = false;
    bool bgmEnabled = true;
    float bgmRestoreVolume = 1.0f;
    bool screenShakeEnabled = true;
    bool playerHealthDamageEnabled = true;
    int tuningSelection = 0;
    float tuningReloadTimer = 0.0f;
    std::filesystem::file_time_type tuningFileWriteTime{};
    bool hasTuningFileWriteTime = false;
    std::string saveStatusMessage;
    float saveStatusTimer = 0.0f;
};

struct GameSceneEffectsState
{
    std::vector<BarrelDebrisParticle> barrelDebris;
    std::vector<LaserSparkParticle> laserSparks;
    std::vector<SlamDustParticle> slamDust;
    std::vector<BeamShockwaveParticle> beamShockwaves;
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

struct GameSceneTuningState
{
    float cameraViewWidth = 1120.0f;
    float cameraViewHeight = 630.0f;
    float defaultCameraViewWidth = 1920.0f;
    float defaultCameraViewHeight = 1080.0f;
    float cameraFollowSpeedX = 14.0f;
    float cameraFollowSpeedY = 10.0f;
    float cameraFollowY = 1.0f;
    float playerMoveSpeed = 320.0f;
    float playerJumpSpeed = -1048.0f;
    float playerGravity = 1900.0f;
    float playerMaxFallSpeed = 980.0f;
    float playerDodgeSpeed = 780.0f;
    float playerDodgeDistance = 124.8f;
    float playerDodgeInvincibilitySeconds = 0.16f;
    float playerDodgeCooldown = 0.45f;
    float coyoteTimeSeconds = 0.10f;
    float groundSnapDistance = 8.0f;
    float groundStepUpHeight = 0.25f;
    float shutterFlashSeconds = 0.18f;
    float captureWidthTiles = 5.0f;
    float captureHeightTiles = 3.0f;
    float captureRapidShotLimit = 4.0f;
    float captureRapidWindowSeconds = 1.2f;
    float captureOverheatLockSeconds = 1.5f;
    float printedPhotoPaddingX = 16.0f;
    float printedPhotoPaddingTop = 16.0f;
    float printedPhotoFooterHeight = 52.0f;
    float printedPhotoMinWidth = 120.0f;
    float printedPhotoMinHeight = 144.0f;
    float printedPhotoMatteInset = 3.0f;
    float pickupTimeBonus = 8.0f;
    float barrelGravity = 1900.0f;
    float barrelMaxFallSpeed = 980.0f;
    float barrelRollSpeed = 220.0f;
    float barrelGroundFriction = 720.0f;
    int barrelContactDamage = 1;
    float barrelBreakMinFallDistance = 99999.0f;
    float barrelBreakMinImpactSpeed = 99999.0f;
    float barrelActivationPaddingX = 320.0f;
    float pastedObjectLifetimeSeconds = 10.0f;
    float pastedObjectPasteAnimationSeconds = 0.24f;
    float jumpPadMaxTiltDegrees = 18.0f;
    MidBoss2Component::Params midBoss2Params;
};

struct GameSceneLifecycleState
{
    bool hasPendingStageTransition = false;
    std::string pendingStageTransitionMapCsv;
    char pendingStageTransitionSpawnMarker = '\0';
    char pendingStageTransitionMarker = '\0';
    std::string currentMapCsvPath = "assets/maps/stages/stage_58x25.csv";
    char lastStageTransitionMarker = '\0';
    bool darknessStageEnabled = false;
    ResourceManager* loadingResources = nullptr;
    bool loadingActive = false;
    bool loadingFinished = false;
    mutable int loadingWarmupFramesRemaining = 0;
    int loadingStep = 0;
    float loadingElapsed = 0.0f;
    float loadingProgress = 0.0f;
    std::string currentTileTextureKey = "tile_forest_ground";
    bool shieldBossBgmCrossFadeStarted = false;
};

struct GameSceneRenderState
{
    float shakeOffsetX = 0.0f;
    float shakeOffsetY = 0.0f;
    float viewScaleMultiplier = 1.0f;
    float slamCameraZoomBoost = 0.0f;
    float bossIntroCameraZoomBoost = 0.0f;
    float bossIntroCameraInfluence = 0.0f;
    float bossIntroCameraTargetX = 0.0f;
    float bossIntroCameraTargetY = 0.0f;
    float shieldBossIntroCurtainProgress = 0.0f;
    bool bossIntroCameraAnchorActive = false;
    bool zoomAnchorScreenCenter = false;
    float zoomAnchorX = 0.0f;
    float zoomAnchorY = 0.0f;
};

struct GameSceneSaveState
{
    bool hasData = false;
    std::string mapCsvPath;
    bool hasCheckpoint = false;
    int activeCheckpointId = -1;
    float stageStartX = 0.0f;
    float stageStartY = 0.0f;
    float respawnX = 0.0f;
    float respawnY = 0.0f;
    float playerX = 0.0f;
    float playerY = 0.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    int sessionMaxHp = 3;
    int sessionCurrentHp = 3;
    int sessionParts = 0;
    int sessionPhotoStorageSlots = 2;
    bool sessionHasRecoveryFilter = false;
    float sessionTimeLimit = 60.0f;
    float sessionTimeRemaining = 60.0f;
    PhotoState photo;
};
