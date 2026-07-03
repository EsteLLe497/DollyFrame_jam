#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "components_combat.h"
#include "game_scene_photo_state.h"
#include "tutorial_csv_data.h"
#include "tutorial_video_player.h"

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

struct MidBoss2SpearMistParticle
{
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
    float sizeScale = 1.0f;
    float pulsePhase = 0.0f;
    float r = 0.70f;
    float g = 0.95f;
    float b = 1.0f;
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
    bool unlocked = false;
    bool enabled = true;
    float pulseRemaining = 0.0f;
    float pulseDuration = 0.0f;
};

struct GameSceneUiCaptureFinderTuning
{
    float scaleMin = 1.0f;
    float scaleMax = 2.0f;
    float scaleStep = 0.1f;
    float zoomBlendResponse = 7.0f;
};

struct GameSceneUiCaptureOverlayTuning
{
    float frameInset = 10.0f;
    float cornerLength = 34.0f;
    float cornerThickness = 3.0f;
    float guideInset = 24.0f;
    float frameBandThickness = 8.0f;
    float vignetteEdge0 = 34.0f;
    float vignetteEdge1 = 72.0f;
    float vignetteEdge2 = 118.0f;
    float vignetteEdge3 = 164.0f;
    float vignetteBoost = 1.22f;
    float warningPanelX = 18.0f;
    float warningPanelY = 18.0f;
    float warningPanelWidth = 196.0f;
    float warningPanelHeight = 56.0f;
    float warningTitleX = 12.0f;
    float warningTitleY = 10.0f;
    float warningCountX = 12.0f;
    float warningCountY = 28.0f;
    float warningTimerX = 112.0f;
    float pulseInset = 20.0f;
};

struct GameSceneUiTutorialTuning
{
    float dimAlpha = 0.68f;

    float dialogueBoxX = 420.0f;
    float dialogueBoxY = 700.0f;
    float dialogueBoxWidth = 1400.0f;
    float dialogueBoxHeight = 360.0f;
    float dialogueNameX = 510.0f;
    float dialogueNameY = 760.0f;
    float dialogueTextX = 510.0f;
    float dialogueTextY = 820.0f;
    float dialoguePromptX = 1580.0f;
    float dialoguePromptY = 990.0f;
    float dialogueNameFontSize = 30.0f;
    float dialogueTextFontSize = 38.0f;
    float dialogueLineSpacing = 48.0f;
    float dialoguePortraitX = 100.0f;
    float dialoguePortraitY = 700.0f;
    float dialoguePortraitSize = 360.0f;
    float dialogueFadeDuration = 1.20f;
    float dialogueCharactersPerSecond = 28.0f;
    int dialogueBoxLayer = 10;
    int dialoguePortraitLayer = 15;
    int dialogueNameLayer = 20;
    int dialogueTextLayer = 20;
    int dialoguePromptLayer = 30;

    float frameX = 260.0f;
    float frameY = 40.0f;
    float frameWidth = 1400.0f;
    float frameHeight = 1000.0f;
    float headingX = 530.0f;
    float headingY = 70.0f;
    float headingWidth = 860.0f;
    float headingHeight = 150.0f;
    float titleX = 760.0f;
    float titleY = 118.0f;
    float contentImageX = 610.0f;
    float contentImageY = 250.0f;
    float contentImageWidth = 700.0f;
    float contentImageHeight = 388.0f;
    float bodyX = 500.0f;
    float bodyY = 680.0f;
    float bodyWidth = 920.0f;
    float bodyLineSpacing = 54.0f;
    float promptX = 850.0f;
    float promptY = 930.0f;
    float titleFontSize = 42.0f;
    float bodyFontSize = 32.0f;
    float promptFontSize = 28.0f;
    int frameLayer = 10;
    int headingLayer = 20;
    int contentImageLayer = 25;
    int titleLayer = 30;
    int bodyLayer = 30;
    int promptLayer = 40;

};

struct GameSceneUiPhotoTrayTuning
{
    float slotStartX = 68.0f;
    float slotStartY = 906.0f;
    float slotWidth = 149.0f;
    float slotHeight = 96.0f;
    float slotGapX = 36.0f;
    float previewPadding = 8.0f;
    float previewScale = 1.0f;
    float emptyTextX = 12.0f;
    float emptyTextY = 35.0f;
    float lockTextX = 14.0f;
    float lockTextY = 36.0f;
    float revealSpeed = 12.0f;
    float revealThreshold = 0.05f;
};

struct GameSceneUiDevelopedPhotoPreviewTuning
{
    float lifetime = 4.2f;
    float cardWidth = 220.0f;
    float cardHeight = 248.0f;
    float cardRightMargin = 42.0f;
    float cardStartYOffset = 30.0f;
    float cardCruiseY = 34.0f;
    float cardShadowOffset = 8.0f;
    float cardOutlineOffset = 10.0f;
    float frameInset = 16.0f;
    float imageHeight = 150.0f;
    float imageTopStripHeight = 20.0f;
    float imageMiddleStripY = 22.0f;
    float cardRiseEase = 0.56f;
    float cardPauseStart = 0.44f;
    float cardPauseEnd = 0.68f;
    float cardPauseAmplitude = 10.0f;
    float cardOvershootY = 14.0f;
    float popScale = 0.015f;
    float orbLaunchXOffset = 18.0f;
    float orbLaunchYOffset = 0.48f;
    float orbControl1YOffset = 172.0f;
    float orbControl2YOffset = 138.0f;
    float orbControl2XOffset = 4.0f;
};

struct GameSceneUiHpTuning
{
    float slotStartX = 244.0f;
    float slotStartY = 19.0f;
    float slotWidth = 100.0f;
    float slotHeight = 96.0f;
    float slotGapX = 16.0f;
    float heartSize = 72.0f;
    float heartYOffset = 2.0f;
    float heartShadowOffsetX = 4.0f;
    float heartShadowOffsetY = 5.0f;
    float heartGlowExpand = 2.0f;
    float heartLagGlowExpand = 4.0f;
    float labelOffsetX = -214.0f;
    float labelOffsetY = 8.0f;
    float hpTextOffsetY = 34.0f;
    float displayRiseSpeedDown = 10.0f;
    float displayRiseSpeedUp = 14.0f;
    float lagSpeed = 10.0f;
    float flashDecaySpeed = 4.5f;
};

struct GameSceneUiPartsHudTuning
{
    float panelWidth = 176.0f;
    float panelHeight = 58.0f;
    float marginRight = 30.0f;
    float marginBottom = 28.0f;
    float iconX = 18.0f;
    float iconY = 17.0f;
    float iconSize = 22.0f;
    float iconInnerInset = 5.0f;
    float labelX = 52.0f;
    float labelY = 10.0f;
    float valueY = 30.0f;
};

struct GameSceneUiBossHpTuning
{
    float panelWidth = 360.0f;
    float barHeight = 24.0f;
    float panelPadding = 12.0f;
    float marginTop = 30.0f;
    float panelExtraHeight = 38.0f;
    float titleOffsetY = -18.0f;
    float hpTextOffsetY = 4.0f;
};

struct GameSceneUiAttackCaptureTuning
{
    float panelX = 32.0f;
    float panelY = 124.0f;
    float panelSize = 96.0f;
    float iconRadius = 28.0f;
    float titleX = 12.0f;
    float titleY = 8.0f;
    float countRightOffset = 30.0f;
    float countBottomOffset = 24.0f;
};

struct GameSceneUiEscapeMenuTuning
{
    float panelWidth = 560.0f;
    float panelHeight = 660.0f;
    float rowStartOffset = 86.0f;
    float rowHeight = 38.0f;
    float rowPaddingX = 18.0f;
    float rowBottomInset = 4.0f;
    float titleX = 22.0f;
    float titleY = 18.0f;
    float helpY = 44.0f;
    float rowTextX = 34.0f;
    float rowTextY = 10.0f;
};

struct GameSceneUiMerchantTuning
{
    float panelWidth = 980.0f;
    float panelHeight = 620.0f;
    float rowHeight = 76.0f;
    float listLeftOffset = 48.0f;
    float listTopOffset = 142.0f;
    float listRightOffset = 540.0f;
    float detailLeftOffset = 590.0f;
    float detailTopOffset = 142.0f;
    float detailBottomOffset = 92.0f;
    float promptHalfWidth = 88.0f;
    float promptHeight = 32.0f;
    float promptTextX = 16.0f;
    float promptTextY = 9.0f;
    float promptRiseOffsetY = 4.0f;
    float promptPulseSpeed = 6.0f;
};

struct GameSceneUiFilterPanelTuning
{
    float panelWidth = 308.0f;
    float panelHeight = 78.0f;
    float marginRight = 22.0f;
    float marginTop = 18.0f;
    float swatchX = 10.0f;
    float swatchY = 10.0f;
    float swatchSize = 34.0f;
    float titleX = 56.0f;
    float titleY = 10.0f;
    float effectY = 32.0f;
    float hintX = 12.0f;
    float hintY = 54.0f;
};

struct GameSceneUiBatteryCounterTuning
{
    float panelWidth = 58.0f;
    float panelHeight = 22.0f;
    float offsetY = 8.0f;
    float tileOffsetMultiplier = 0.45f;
    float iconSize = 22.0f;
    float iconInnerInset = 5.0f;
    float labelX = 52.0f;
    float labelY = 10.0f;
};

struct GameSceneUiStageGuideTuning
{
    float x = 24.0f;
    float yOffsetFromBottom = 42.0f;
};

struct GameSceneUiMapEditorTuning
{
    float panelLeft = 22.0f;
    float panelTop = 22.0f;
    float panelRight = 560.0f;
    float panelBottom = 286.0f;
};

struct GameSceneUiTuningState
{
    GameSceneUiCaptureFinderTuning captureFinder;
    GameSceneUiCaptureOverlayTuning captureOverlay;
    GameSceneUiTutorialTuning tutorial;
    GameSceneUiPhotoTrayTuning photoTray;
    GameSceneUiDevelopedPhotoPreviewTuning developedPhotoPreview;
    GameSceneUiHpTuning hp;
    GameSceneUiPartsHudTuning partsHud;
    GameSceneUiBossHpTuning bossHp;
    GameSceneUiAttackCaptureTuning attackCapture;
    GameSceneUiEscapeMenuTuning escapeMenu;
    GameSceneUiMerchantTuning merchant;
    GameSceneUiFilterPanelTuning filterPanel;
    GameSceneUiBatteryCounterTuning batteryCounter;
    GameSceneUiStageGuideTuning stageGuide;
    GameSceneUiMapEditorTuning mapEditor;
};

enum class TutorialPresentationPhase
{
    Inactive,
    Conversation,
    TutorialWindow,
};

struct GameSceneTutorialState
{
    TutorialPresentationPhase phase = TutorialPresentationPhase::Inactive;
    bool previewConversation = false;
    bool previewWindow = false;
    float dialogueFadeElapsed = 0.0f;
    float dialogueRevealElapsed = 0.0f;
    std::vector<TutorialPageData> pages;
    size_t currentPageIndex = 0;
    int portraitTextureId = -1;
    std::string loadedPortraitPath;
    TutorialVideoPlayer videoPlayer;
    int activeTutorialNumber = 0;
    int loadedTutorialNumber = 0;
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
    float cameraFilterAnimationElapsed = 1.0f;
    PhotoFilterTheme cameraFilterAnimationFrom = PhotoFilterTheme::None;
    PhotoFilterTheme cameraFilterAnimationTo = PhotoFilterTheme::None;
    PhotoFilterTheme cameraFilterHudTheme = PhotoFilterTheme::None;
    PhotoFilterTheme cameraFilterLastSelectedTheme = PhotoFilterTheme::None;
    bool cameraFilterHudInitialized = false;
    float hpDisplayRatio = 1.0f;
    float hpDamageLagRatio = 1.0f;
    float hpDamageFlash = 0.0f;
    int hpLastRaw = -1;
    bool hpUiInitialized = false;
    bool merchantShopOpen = false;
    int merchantSelection = 0;
    float merchantMessageTimer = 0.0f;
    std::string merchantMessage;
    GameSceneUiTuningState tuning;
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
    std::vector<MidBoss2SpearMistParticle> midBoss2SpearMist;
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
    float captureFrameWidthPx = 215.9f;
    float captureFrameHeightPx = 151.3f;
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
    float pastedObjectLifetimeSeconds = 20.0f;
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
    std::string currentMapCsvPath = "assets/maps/stages/forest.csv";
    char lastStageTransitionMarker = '\0';
    bool darknessStageEnabled = false;
    bool forestStageEnabled = false;
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
    bool sessionHasCameraFlash = false;
    bool cameraTutorialCompleted = false;
    std::vector<int> completedTutorialNumbers;
    float sessionTimeLimit = 60.0f;
    float sessionTimeRemaining = 60.0f;
    PhotoState photo;
};
