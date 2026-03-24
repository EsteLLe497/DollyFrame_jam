#pragma once

#include <array>
#include <vector>

#include "components.h"

enum class CapturedSpawnArchetype
{
    None,
    Barrel,
    Projectile,
};

struct CapturedPhotoItem
{
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
    float rotation = 0.0f;
    bool flipX = false;
    bool vanishOnCapture = false;
    CapturedSpawnArchetype spawnArchetype = CapturedSpawnArchetype::None;
    float projectileVelocityX = 0.0f;
    float projectileVelocityY = 0.0f;
    int projectileDamage = 1;
};

struct PhotoCaptureState
{
    bool hasPhoto = false;
    PhotoFilterTheme selectedTheme = PhotoFilterTheme::None;
    PhotoFilterTheme capturedTheme = PhotoFilterTheme::None;
    std::vector<CapturedPhotoItem> items;
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
};

struct PhotoGroupState
{
    bool hasSpawnedCopy = false;
    int nextGroupId = 1;
    int activeGroupCount = 0;
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
    std::array<PhotoCaptureState, 3> savedCaptures;
    int selectedCaptureSlot = 0;
    int nextCaptureSlot = 0;
    PendingPhotoStoreState pendingStore;
    PhotoPlacementState placement;
    PhotoGroupState groups;
};
