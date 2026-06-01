#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "components.h"

enum class CapturedSpawnArchetype
{
    None,
    Log,
    Barrel,
    Battery,
    Projectile,
    LaserTurret,
    WalkerMelee,
    ShieldNormal,
    ShieldRushBurst,
    ShieldJumpBurst,
    SepiaGround,
};

enum class PhotoPlacementRuleGroup
{
    Group1,
    Group2,
    Group3,
};

enum class PhotoPlacementForbiddenTarget : std::uint8_t
{
    None = 0,
    Floor = 1 << 0,
    Enemy = 1 << 1,
};

struct PhotoPlacementRuleDefinition
{
    PhotoPlacementRuleGroup group = PhotoPlacementRuleGroup::Group1;
    std::uint8_t forbiddenMask = 0;
};

inline constexpr std::uint8_t ToPlacementForbiddenMask(PhotoPlacementForbiddenTarget target)
{
    return static_cast<std::uint8_t>(target);
}

inline constexpr bool HasPlacementForbiddenTarget(std::uint8_t mask, PhotoPlacementForbiddenTarget target)
{
    return (mask & ToPlacementForbiddenMask(target)) != 0;
}

inline constexpr std::array<PhotoPlacementRuleDefinition, 3> kPhotoPlacementRuleDefinitions = {
    PhotoPlacementRuleDefinition{
        PhotoPlacementRuleGroup::Group1,
        ToPlacementForbiddenMask(PhotoPlacementForbiddenTarget::Enemy),
    },
    PhotoPlacementRuleDefinition{
        PhotoPlacementRuleGroup::Group2,
        ToPlacementForbiddenMask(PhotoPlacementForbiddenTarget::Floor) |
            ToPlacementForbiddenMask(PhotoPlacementForbiddenTarget::Enemy),
    },
    PhotoPlacementRuleDefinition{
        PhotoPlacementRuleGroup::Group3,
        ToPlacementForbiddenMask(PhotoPlacementForbiddenTarget::None),  // 制限なし
    },
};

inline constexpr const PhotoPlacementRuleDefinition* FindPhotoPlacementRuleDefinition(PhotoPlacementRuleGroup group)
{
    for (const auto& definition : kPhotoPlacementRuleDefinitions)
    {
        if (definition.group == group)
        {
            return &definition;
        }
    }
    return nullptr;
}

inline constexpr std::uint8_t GetPlacementForbiddenMask(PhotoPlacementRuleGroup group)
{
    if (const auto* definition = FindPhotoPlacementRuleDefinition(group))
    {
        return definition->forbiddenMask;
    }

    return ToPlacementForbiddenMask(PhotoPlacementForbiddenTarget::Enemy);
}

struct CapturedPhotoItem
{
    struct OutlinePoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    int textureId = -1;
    PhotoCopyRole role = PhotoCopyRole::Solid;
    PhotoCopyLayer layer = PhotoCopyLayer::Foreground;
    PhotoCopyOrigin origin = PhotoCopyOrigin::Generic;
    PhotoFilterTheme appliedTheme = PhotoFilterTheme::None;
    float relativeX = 0.0f;
    float relativeY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float sourceX = 0.0f;
    float sourceY = 0.0f;
    float sourceWidth = 1.0f;
    float sourceHeight = 1.0f;
    float tintR = 1.0f;
    float tintG = 1.0f;
    float tintB = 1.0f;
    float tintA = 1.0f;
    int sourceTileValue = 0;
    int damagePlatformTileSpan = 0;
    int spikeStripTileSpan = 0;
    int sepiaRestoredTileValue = 0;
    bool sepiaRestoredMarkerObject = false;
    float rotation = 0.0f;
    bool flipX = false;
    bool vanishOnCapture = false;
    bool enemyAttackPaste = false;
    CapturedSpawnArchetype spawnArchetype = CapturedSpawnArchetype::None;
    PhotoPlacementRuleGroup placementRuleGroup = PhotoPlacementRuleGroup::Group1;
    float projectileVelocityX = 0.0f;
    float projectileVelocityY = 0.0f;
    int projectileDamage = 1;
    bool spearProjectile = false;
    bool spearStuck = false;
    float spearDirectionX = 0.0f;
    float spearDirectionY = -1.0f;
    float spearTravelDistance = 0.0f;
    float laserBeamThickness = 0.0f;
    float laserDamagePerSecond = 1.0f;
    float laserEnemyKnockbackSpeed = 0.0f;
    float lightRadius = 0.0f;
    float lightIntensity = 0.0f;
    std::vector<OutlinePoint> collisionOutline;
};

struct PhotoCaptureState
{
    bool hasPhoto = false;
    PhotoFilterTheme selectedTheme = PhotoFilterTheme::None;
    PhotoFilterTheme capturedTheme = PhotoFilterTheme::None;
    std::vector<CapturedPhotoItem> items;
    bool containsEnemyAttackPaste = false;
    int textureId = -1;
    float width = 64.0f;
    float height = 64.0f;
    float sourceX = 0.0f;
    float sourceY = 0.0f;
    float sourceWidth = 1.0f;
    float sourceHeight = 1.0f;
    float tintR = 0.86f;
    float tintG = 0.92f;
    float tintB = 1.0f;
    float tintA = 1.0f;
};

struct PhotoPlacementState
{
    bool active = false;
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    PhotoCopyLayer layer = PhotoCopyLayer::Foreground;
    bool flipX = false;
    bool bridgeEnabled = false;
    float rotation = 0.0f;
    int sessionId = 0;
    bool blockedByUi = false;
    float invalidFlashRemaining = 0.0f;
    float confirmFlashRemaining = 0.0f;
};

struct PhotoGroupState
{
    bool hasSpawnedCopy = false;
    int nextGroupId = 1;
    int activeGroupCount = 0;
    int nextPasteOrder = 1;
};

struct PendingPhotoStoreState
{
    bool active = false;
    int slotIndex = 0;
    PhotoCaptureState capture;
};

struct PhotoState
{
    PhotoCaptureState capture;
    PhotoCaptureState attackCapture;
    std::array<PhotoCaptureState, 3> savedCaptures;
    int selectedCaptureSlot = 0;
    int nextCaptureSlot = 0;
    PendingPhotoStoreState pendingStore;
    PhotoPlacementState placement;
    PhotoGroupState groups;
};
