#pragma once

#include "DxLib.h"

#include "game_scene_internal.h"

namespace game_scene_detail
{
inline void DrawTriangleItem(
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    bool risesRight,
    bool flipX,
    float rotation,
    int color)
{
    const bool finalRisesRight = flipX ? !risesRight : risesRight;
    float ax = 0.0f;
    float ay = 0.0f;
    float bx = 0.0f;
    float by = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;

    if (finalRisesRight)
    {
        ax = drawX;
        ay = drawY + drawHeight;
        bx = drawX + drawWidth;
        by = drawY + drawHeight;
        cx = drawX + drawWidth;
        cy = drawY;
    }
    else
    {
        ax = drawX;
        ay = drawY;
        bx = drawX;
        by = drawY + drawHeight;
        cx = drawX + drawWidth;
        cy = drawY + drawHeight;
    }

    const float centerX = drawX + drawWidth * 0.5f;
    const float centerY = drawY + drawHeight * 0.5f;
    RotatePoint(centerX, centerY, rotation, ax, ay);
    RotatePoint(centerX, centerY, rotation, bx, by);
    RotatePoint(centerX, centerY, rotation, cx, cy);
    DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
}

inline void DrawProjectileItem(
    float drawX,
    float drawY,
    float drawWidth,
    float drawHeight,
    bool flipX,
    float rotation,
    int color)
{
    float ax = flipX ? drawX + drawWidth : drawX;
    float ay = drawY;
    float bx = flipX ? drawX + drawWidth : drawX;
    float by = drawY + drawHeight;
    float cx = flipX ? drawX : drawX + drawWidth;
    float cy = drawY + drawHeight * 0.5f;

    const float centerX = drawX + drawWidth * 0.5f;
    const float centerY = drawY + drawHeight * 0.5f;
    RotatePoint(centerX, centerY, rotation, ax, ay);
    RotatePoint(centerX, centerY, rotation, bx, by);
    RotatePoint(centerX, centerY, rotation, cx, cy);
    DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
}
}
