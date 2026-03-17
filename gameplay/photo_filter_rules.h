#pragma once

#include "components.h"

class Entity;

const char* GetPhotoFilterThemeLabel(PhotoFilterTheme theme);
PhotoFilterTheme GetNextPhotoFilterTheme(PhotoFilterTheme current);
const char* GetPhotoCaptureLogMessage(PhotoFilterTheme theme);

bool ApplyPhotoFilterToCapturedTarget(Entity& target, PhotoFilterTheme theme);
bool ApplyPhotoFilterToPhotoBox(Entity& photoBox, PhotoFilterTheme theme);
