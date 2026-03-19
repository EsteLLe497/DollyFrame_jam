#pragma once

#include "components.h"

class Entity;

const char* GetPhotoFilterThemeLabel(PhotoFilterTheme theme);
const char* GetPhotoFilterThemeEffectText(PhotoFilterTheme theme);
void GetPhotoFilterThemeOverlayColor(PhotoFilterTheme theme, float& r, float& g, float& b);
void GetPhotoFilterThemePreviewOutlineColor(PhotoFilterTheme theme, float& r, float& g, float& b);
PhotoFilterTheme GetNextPhotoFilterTheme(PhotoFilterTheme current);
const char* GetPhotoCaptureLogMessage(PhotoFilterTheme theme);
void ApplyPhotoFilterThemeToPreviewItem(
    PhotoFilterTheme theme,
    PhotoCopyOrigin origin,
    PhotoCopyRole& role,
    PhotoCopyLayer& layer,
    float& tintR,
    float& tintG,
    float& tintB,
    float& tintA);

bool ApplyPhotoFilterToCapturedTarget(Entity& target, PhotoFilterTheme theme);
bool ApplyPhotoFilterToPhotoBox(Entity& photoBox, PhotoFilterTheme theme);
