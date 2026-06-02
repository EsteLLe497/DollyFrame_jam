#include "pch.h"
#include "game_scene_camerawork.h"

void fixedCameraRange::SetStartTiles(int startCol, int startRow, float tileSize)
{
    startX = startCol * tileSize;
    startY = startRow * tileSize;
}

void fixedCameraRange::SetEndTiles(int endCol, int endRow, float tileSize)
{
    endX = (endCol + 1) * tileSize;
    endY = (endRow + 1) * tileSize;
}

void fixedCameraRange::SetFixedCameraPosition(float x, float y)
{
    cameraX = x;
    cameraY = y;
}

void fixedCameraRange::SetFollowPlayer(bool follow)
{
    followPlayer = follow;
}

void fixedCameraRange::SetCameraNum(int num)
{
    cameraNum = num;
}

bool fixedCameraRange::IsInRange(float px, float py) const
{
    return (px >= startX && px < endX) && (py >= startY && py < endY);
}

float fixedCameraRange::GetWidth() const
{
    return endX - startX;
}

void fixedCameraRange::SetZoomWidth(float width)
{
    zoomWidth = width;
}

void fixedCameraRange::SetZoomHeight(float height)
{
    zoomHeight = height;
}
