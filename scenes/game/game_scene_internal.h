#pragma once

#include "game_scene.h"

#include <cctype>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

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
struct StageTransitionLink
{
    std::string sourceMapCsv;
    char marker = '\0';
    std::string destinationMapCsv;
    char spawnMarker = '\0';
};

inline std::vector<StageTransitionLink> gStageTransitionLinks;

inline constexpr const char* kTuningFilePath = "assets/tuning.json";
inline constexpr const char* kGameProgressSavePath = "savegame.json";
constexpr float kPixelsPerMeter = 100.0f;
constexpr float kSurfaceContactEpsilon = 1.0f;
constexpr float kHorizontalCollisionEpsilon = 1.0f;

inline GameScene* gActiveGameScene = nullptr;

inline GameScene* GetActiveGameScene()
{
    return gActiveGameScene;
}

inline void SetActiveGameScene(GameScene* scene)
{
    gActiveGameScene = scene;
}

struct ActiveGameSceneScope
{
    explicit ActiveGameSceneScope(GameScene& scene)
        : previous(GetActiveGameScene())
    {
        SetActiveGameScene(&scene);
    }

    ~ActiveGameSceneScope()
    {
        SetActiveGameScene(previous);
    }

    GameScene* previous = nullptr;
};

#define gCameraViewWidth (game_scene_detail::GetActiveGameScene()->Tuning().cameraViewWidth)
#define gCameraViewHeight (game_scene_detail::GetActiveGameScene()->Tuning().cameraViewHeight)
#define gDefaultCameraViewWidth (game_scene_detail::GetActiveGameScene()->Tuning().defaultCameraViewWidth)
#define gDefaultCameraViewHeight (game_scene_detail::GetActiveGameScene()->Tuning().defaultCameraViewHeight)
#define gCameraFollowSpeedX (game_scene_detail::GetActiveGameScene()->Tuning().cameraFollowSpeedX)
#define gCameraFollowSpeedY (game_scene_detail::GetActiveGameScene()->Tuning().cameraFollowSpeedY)
#define gCameraFollowY (game_scene_detail::GetActiveGameScene()->Tuning().cameraFollowY)
#define gPlayerMoveSpeed (game_scene_detail::GetActiveGameScene()->Tuning().playerMoveSpeed)
#define gPlayerJumpSpeed (game_scene_detail::GetActiveGameScene()->Tuning().playerJumpSpeed)
#define gPlayerGravity (game_scene_detail::GetActiveGameScene()->Tuning().playerGravity)
#define gPlayerMaxFallSpeed (game_scene_detail::GetActiveGameScene()->Tuning().playerMaxFallSpeed)
#define gPlayerDodgeSpeed (game_scene_detail::GetActiveGameScene()->Tuning().playerDodgeSpeed)
#define gPlayerDodgeDistance (game_scene_detail::GetActiveGameScene()->Tuning().playerDodgeDistance)
#define gPlayerDodgeInvincibilitySeconds (game_scene_detail::GetActiveGameScene()->Tuning().playerDodgeInvincibilitySeconds)
#define gPlayerDodgeCooldown (game_scene_detail::GetActiveGameScene()->Tuning().playerDodgeCooldown)
#define gCoyoteTimeSeconds (game_scene_detail::GetActiveGameScene()->Tuning().coyoteTimeSeconds)
#define gGroundSnapDistance (game_scene_detail::GetActiveGameScene()->Tuning().groundSnapDistance)
#define gGroundStepUpHeight (game_scene_detail::GetActiveGameScene()->Tuning().groundStepUpHeight)
#define gShutterFlashSeconds (game_scene_detail::GetActiveGameScene()->Tuning().shutterFlashSeconds)
#define gCaptureWidthTiles (game_scene_detail::GetActiveGameScene()->Tuning().captureWidthTiles)
#define gCaptureHeightTiles (game_scene_detail::GetActiveGameScene()->Tuning().captureHeightTiles)
#define gCaptureRapidShotLimit (game_scene_detail::GetActiveGameScene()->Tuning().captureRapidShotLimit)
#define gCaptureRapidWindowSeconds (game_scene_detail::GetActiveGameScene()->Tuning().captureRapidWindowSeconds)
#define gCaptureOverheatLockSeconds (game_scene_detail::GetActiveGameScene()->Tuning().captureOverheatLockSeconds)
#define gPrintedPhotoPaddingX (game_scene_detail::GetActiveGameScene()->Tuning().printedPhotoPaddingX)
#define gPrintedPhotoPaddingTop (game_scene_detail::GetActiveGameScene()->Tuning().printedPhotoPaddingTop)
#define gPrintedPhotoFooterHeight (game_scene_detail::GetActiveGameScene()->Tuning().printedPhotoFooterHeight)
#define gPrintedPhotoMinWidth (game_scene_detail::GetActiveGameScene()->Tuning().printedPhotoMinWidth)
#define gPrintedPhotoMinHeight (game_scene_detail::GetActiveGameScene()->Tuning().printedPhotoMinHeight)
#define gPrintedPhotoMatteInset (game_scene_detail::GetActiveGameScene()->Tuning().printedPhotoMatteInset)
#define gPickupTimeBonus (game_scene_detail::GetActiveGameScene()->Tuning().pickupTimeBonus)
#define gBarrelGravity (game_scene_detail::GetActiveGameScene()->Tuning().barrelGravity)
#define gBarrelMaxFallSpeed (game_scene_detail::GetActiveGameScene()->Tuning().barrelMaxFallSpeed)
#define gBarrelRollSpeed (game_scene_detail::GetActiveGameScene()->Tuning().barrelRollSpeed)
#define gBarrelGroundFriction (game_scene_detail::GetActiveGameScene()->Tuning().barrelGroundFriction)
#define gBarrelContactDamage (game_scene_detail::GetActiveGameScene()->Tuning().barrelContactDamage)
#define gBarrelBreakMinFallDistance (game_scene_detail::GetActiveGameScene()->Tuning().barrelBreakMinFallDistance)
#define gBarrelBreakMinImpactSpeed (game_scene_detail::GetActiveGameScene()->Tuning().barrelBreakMinImpactSpeed)
#define gBarrelActivationPaddingX (game_scene_detail::GetActiveGameScene()->Tuning().barrelActivationPaddingX)
#define gPastedObjectLifetimeSeconds (game_scene_detail::GetActiveGameScene()->Tuning().pastedObjectLifetimeSeconds)
#define gPastedObjectPasteAnimationSeconds (game_scene_detail::GetActiveGameScene()->Tuning().pastedObjectPasteAnimationSeconds)
#define gJumpPadMaxTiltDegrees (game_scene_detail::GetActiveGameScene()->Tuning().jumpPadMaxTiltDegrees)

inline float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

inline void RotatePoint(float centerX, float centerY, float rotation, float& x, float& y)
{
    if (std::fabs(rotation) <= 0.0001f)
    {
        return;
    }

    const float localX = x - centerX;
    const float localY = y - centerY;
    const float cosTheta = std::cos(rotation);
    const float sinTheta = std::sin(rotation);
    x = centerX + (localX * cosTheta - localY * sinTheta);
    y = centerY + (localX * sinTheta + localY * cosTheta);
}

inline std::string GetMapDisplayName(const std::string& path)
{
    const size_t slashPos = path.find_last_of("/\\");
    const std::string fileName = slashPos == std::string::npos ? path : path.substr(slashPos + 1);
    const size_t dotPos = fileName.find_last_of('.');
    return dotPos == std::string::npos ? fileName : fileName.substr(0, dotPos);
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
    return std::array<GameSceneTuningEntry, 29>
    {{
        { "Camera Width", &gCameraViewWidth, 20.0f, 640.0f, 1920.0f },
        { "Camera Height", &gCameraViewHeight, 20.0f, 360.0f, 1080.0f },
        { "Cam Follow X", &gCameraFollowSpeedX, 0.5f, 1.0f, 40.0f },
        { "Cam Follow Y", &gCameraFollowSpeedY, 0.5f, 1.0f, 40.0f },
        { "Cam Y Track", &gCameraFollowY, 1.0f, 0.0f, 1.0f },
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
        { "Capture Limit", &gCaptureRapidShotLimit, 1.0f, 1.0f, 20.0f },
        { "Capture Window", &gCaptureRapidWindowSeconds, 0.1f, 0.1f, 10.0f },
        { "Capture Lock", &gCaptureOverheatLockSeconds, 0.1f, 0.0f, 10.0f },
        { "Print Pad X", &gPrintedPhotoPaddingX, 1.0f, 0.0f, 80.0f },
        { "Print Pad Top", &gPrintedPhotoPaddingTop, 1.0f, 0.0f, 80.0f },
        { "Print Footer", &gPrintedPhotoFooterHeight, 2.0f, 0.0f, 160.0f },
        { "Print Min W", &gPrintedPhotoMinWidth, 4.0f, 32.0f, 320.0f },
        { "Print Min H", &gPrintedPhotoMinHeight, 4.0f, 32.0f, 400.0f },
        { "Matte Inset", &gPrintedPhotoMatteInset, 0.5f, 0.0f, 24.0f },
        { "Pickup Bonus", &gPickupTimeBonus, 1.0f, 0.0f, 60.0f },
        { "JumpPad Tilt", &gJumpPadMaxTiltDegrees, 1.0f, 5.0f, 35.0f },
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
    case 11:
        r = 0.22f;
        g = 0.40f;
        b = 0.76f;
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
    return tag && tag->Is(value);
}

inline bool HasTag(const TagComponent* tag, const char* value)
{
    return tag && tag->Is(value);
}

inline bool HasTag(const Entity& entity, EntityTag value)
{
    const auto* tag = entity.GetComponent<TagComponent>();
    return tag && tag->Is(value);
}

inline bool HasTag(const TagComponent* tag, EntityTag value)
{
    return tag && tag->Is(value);
}

inline constexpr const char* kTagPlayer = "Player";
inline constexpr const char* kTagEnemy = "Enemy";
inline constexpr const char* kTagPhotoBox = "PhotoBox";
inline constexpr const char* kTagGoal = "Goal";
inline constexpr const char* kTagPhotoSource = "PhotoSource";
inline constexpr const char* kTagHazard = "Hazard";
inline constexpr const char* kTagBullet = "Bullet";
inline constexpr const char* kTagDropItem = "DropItem";
inline constexpr const char* kTagBattery = "Battery";
inline constexpr const char* kTagBatteryGenerator = "BatteryGenerator";
inline constexpr const char* kTagLog = "Log";
inline constexpr const char* kTagFallingRock = "FallingRock";
inline constexpr const char* kTagJumpPad = "JumpPad";
inline constexpr const char* kTagBatterySwitch = "BatterySwitch";
inline constexpr const char* kTagElevator = "Elevator";
inline constexpr const char* kTagDamagePlatform = "DamagePlatform";
inline constexpr const char* kTagDamagePlatformSpike = "DamagePlatformSpike";
inline constexpr const char* kTagLaserSwitch = "LaserSwitch";
inline constexpr const char* kTagShutter = "Shutter";
inline constexpr const char* kTagProtectiveWall = "ProtectiveWall";
inline constexpr const char* kTagLaserTurret = "LaserTurret";
inline constexpr const char* kTagLaserBeam = "LaserBeam";
inline constexpr const char* kTagMarkerLight = "MarkerLight";
inline constexpr const char* kTagStageLight = "StageLight";
inline constexpr const char* kTagSepiaRubble = "SepiaRubble";
inline constexpr const char* kTagSepiaElevator = "SepiaElevator";
inline constexpr const char* kTagMidBoss3Fist = "MidBoss3Fist";

inline constexpr std::array<char, 32> kMarkerPresets = {
    '\0', 'G', 'S', 'E', 'T', 'W', 'R', 'A', 'D', 'B', 'V', 'C', 'M', 'Y', 'H', 'I', 'K', 'L', 'Q', '?', '!', 'U', 'Z', 'J', 'O', 'X', '*', 'F', '@', '&','>','<'
};
inline constexpr int kMarkerPresetCount = static_cast<int>(kMarkerPresets.size());

inline bool IsMarkerInSet(char marker, std::string_view set)
{
    const char normalized = static_cast<char>(std::toupper(static_cast<unsigned char>(marker)));
    return set.find(normalized) != std::string_view::npos;
}

struct TileMarker
{
    int column = 0;
    int row = 0;
    char marker = '\0';
    int parameter = 0;
};

inline std::vector<TileMarker> CollectTileMarkers(const TileMap& tileMap, bool uppercaseMarkers = false)
{
    std::vector<TileMarker> markers;
    markers.reserve(static_cast<size_t>(tileMap.GetWidth() * tileMap.GetHeight()) / 8);

    for (int row = 0; row < tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < tileMap.GetWidth(); ++column)
        {
            char marker = tileMap.GetMarker(column, row);
            if (marker == '\0')
            {
                continue;
            }
            if (uppercaseMarkers)
            {
                marker = static_cast<char>(std::toupper(static_cast<unsigned char>(marker)));
            }

            markers.push_back(TileMarker{
                column,
                row,
                marker,
                tileMap.GetMarkerParameter(column, row) });
        }
    }

    return markers;
}

inline int MarkerToPresetIndex(char marker)
{
    const char normalized = static_cast<char>(std::toupper(static_cast<unsigned char>(marker)));
    for (int index = 0; index < kMarkerPresetCount; ++index)
    {
        if (kMarkerPresets[static_cast<size_t>(index)] == normalized)
        {
            return index;
        }
    }

    return 0;
}

inline char PresetIndexToMarker(int index)
{
    if (index >= 0 && index < kMarkerPresetCount)
    {
        return kMarkerPresets[static_cast<size_t>(index)];
    }

    return '\0';
}

inline bool IsEnemyMarker(char marker)
{
    return IsMarkerInSet(marker, "WRNAD!?$%");
}

inline bool IsBatteryMarker(char marker)
{
    return IsMarkerInSet(marker, "Y");
}

inline bool IsBatteryGeneratorMarker(char marker)
{
    return IsMarkerInSet(marker, "Y");
}

inline bool IsLogMarker(char marker)
{
    return IsMarkerInSet(marker, "M");
}

inline bool IsFallingRockMarker(char marker)
{
    return IsMarkerInSet(marker, "S");
}

inline bool IsJumpPadMarker(char marker)
{
    return IsMarkerInSet(marker, "T");
}

inline bool IsMarkerLightMarker(char marker)
{
    return IsMarkerInSet(marker, "PF");
}

inline bool IsStageLightMarker(char marker)
{
    return marker == '@';
}

inline bool IsElevatorMarker(char marker)
{
    return IsMarkerInSet(marker, "KLQ");
}

inline bool IsDamageFootholdMarker(char marker)
{
    return IsMarkerInSet(marker, "HI");
}

inline bool IsLaserTurretMarker(char marker)
{
    return IsMarkerInSet(marker, "UZ");
}

inline bool IsShutterOrLaserSwitchMarker(char marker)
{
    return IsMarkerInSet(marker, "JOX");
}

inline bool IsProtectiveWallMarker(char marker)
{
    return marker == '&';
}

inline bool IsSepiaRubbleMarker(char marker)
{
    return marker == '>';
}

inline bool IsSepiaBackgroundMarker(char marker)
{
    return marker == '<';
}

inline bool IsParameterizedEditorMarker(char marker)
{
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(marker))))
    {
    case 'P':
    case 'F':
    case 'K':
    case 'L':
    case 'Q':
    case 'J':
    case 'O':
    case 'X':
    case '&':
    case 'U':
    case 'Z':
    case '>':
    case '<':
    case 'S':
        return true;
    default:
        return false;
    }
}

inline int NormalizeEditorMarkerParameter(char marker, int parameter)
{
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(marker))))
    {
    case 'P':
    case 'F':
        return std::clamp(parameter, -99, 99);
    case 'K':
    case 'L':
    case 'Q':
    case 'O':
    case 'X':
        return std::clamp(parameter, 1, 9);
    case 'J':
        return std::clamp(parameter, -9, 99);
    case 'U':
        return std::clamp(parameter, -2, 1);
    case 'Z':
        return std::clamp(parameter, 0, 1);
    case '&':
        return std::clamp(parameter, -99, 99);
    case '>':
        return std::clamp(parameter, 0, 99);
    case '<':
        return std::clamp(parameter, 0, 999);
    default:
        return 0;
    }
}

inline bool IsRestoreSepiaObjectMarker(char marker)
{
    const char normalizedMarker = static_cast<char>(
        std::toupper(static_cast<unsigned char>(marker)));
    
    switch (normalizedMarker)
    {
    case 'W':
    case 'R':
    case 'N':
    case 'A':
    case 'D':
    case '!':
    case '?':
    case '$':
    case '%':
    case 'L':
    case 'Q':
    case '+':
    case 'S':
        return true;
    default:
        break;
    }

    return false;
}

inline int EncodeStageLightMarkerParameter(int lightTiles, int fixtureTiles)
{
    constexpr int kDefaultLightTiles = 3;
    constexpr int kDefaultFixtureTiles = 1;
    const int clampedLightTiles = std::clamp(lightTiles, 1, 9);
    const int clampedFixtureTiles = std::clamp(fixtureTiles, 1, 4);
    if (clampedLightTiles == kDefaultLightTiles && clampedFixtureTiles == kDefaultFixtureTiles)
    {
        return 0;
    }
    if (clampedFixtureTiles == kDefaultFixtureTiles)
    {
        return clampedLightTiles;
    }
    return clampedLightTiles * 10 + clampedFixtureTiles;
}

inline bool IsDamagePlatformMarker(char marker)
{
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(marker)));
    return upper == 'H' || upper == 'I';
}

inline int GetDamagePlatformTileSpanFromMarker(char marker)
{
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(marker)));
    return upper == 'I' ? 2 : 3;
}

inline std::vector<b2Vec2> BuildDamagePlatformBaseOutline()
{
    return std::vector<b2Vec2>{
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };
}

inline std::vector<b2Vec2> BuildDamagePlatformSpikeOutline(int tileSpan)
{
    const int spikeCount = (std::max)(1, tileSpan);
    std::vector<b2Vec2> outline;
    outline.reserve(static_cast<size_t>(spikeCount * 2 + 1));
    outline.push_back({ 0.0f, 0.5f });
    for (int spikeIndex = 0; spikeIndex < spikeCount; ++spikeIndex)
    {
        const float peakX = (static_cast<float>(spikeIndex) + 0.5f) / static_cast<float>(spikeCount);
        const float valleyX = static_cast<float>(spikeIndex + 1) / static_cast<float>(spikeCount);
        outline.push_back({ peakX, 0.0f });
        outline.push_back({ valleyX, 0.5f });
    }
    return outline;
}

inline PhotoCopyRole GetEntityCopyRole(const Entity& entity)
{
    if (HasTag(entity, kTagGoal))
    {
        return PhotoCopyRole::GoalRelay;
    }
    if (HasTag(entity, kTagPhotoSource))
    {
        return PhotoCopyRole::Pickup;
    }
    if (HasTag(entity, kTagLog))
    {
        return PhotoCopyRole::Solid;
    }
    if (HasTag(entity, kTagDamagePlatformSpike))
    {
        return PhotoCopyRole::Hazard;
    }
    if (HasTag(entity, kTagDamagePlatform))
    {
        return PhotoCopyRole::Solid;
    }
    if (HasTag(entity, kTagLaserSwitch) || HasTag(entity, kTagShutter) || HasTag(entity, kTagProtectiveWall))
    {
        return PhotoCopyRole::Solid;
    }
    if (HasTag(entity, kTagLaserTurret))
    {
        return PhotoCopyRole::Solid;
    }
    if (HasTag(entity, kTagMarkerLight))
    {
        return PhotoCopyRole::Solid;
    }
    if (HasTag(entity, kTagHazard) || HasTag(entity, kTagEnemy))
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
    if (HasTag(entity, kTagEnemy))
    {
        return PhotoCopyOrigin::Enemy;
    }
    if (HasTag(entity, kTagHazard))
    {
        return PhotoCopyOrigin::Hazard;
    }
    if (HasTag(entity, kTagLog))
    {
        return PhotoCopyOrigin::Generic;
    }
    if (HasTag(entity, kTagDamagePlatformSpike))
    {
        return PhotoCopyOrigin::Hazard;
    }
    if (HasTag(entity, kTagDamagePlatform))
    {
        return PhotoCopyOrigin::Generic;
    }
    if (HasTag(entity, kTagLaserSwitch) || HasTag(entity, kTagShutter) || HasTag(entity, kTagProtectiveWall))
    {
        return PhotoCopyOrigin::Generic;
    }
    if (HasTag(entity, kTagLaserTurret))
    {
        return PhotoCopyOrigin::Generic;
    }
    if (HasTag(entity, kTagGoal))
    {
        return PhotoCopyOrigin::Goal;
    }
    if (HasTag(entity, kTagPhotoSource))
    {
        return PhotoCopyOrigin::Pickup;
    }
    return PhotoCopyOrigin::Generic;
}

// 3/21・ｽﾇ会ｿｽ・ｽF・ｽ・ｽ^・ｽC・ｽ・ｽ・ｽﾌ表・ｽ・ｽY・ｽ・ｽ・ｽW・ｽ・ｽ謫ｾ(・ｽc・ｽV・ｽ・ｽr)
inline bool TryGetSlopeSurfaceYShared(
    const TileMap& tileMap,
    int column,
    int row,
    float worldX,
    float& outSurfaceY)
{
    const float tileSize = tileMap.GetTileSize();
    constexpr int kMaxSpan = 10;
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
