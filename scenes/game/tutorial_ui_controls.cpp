// =========================================================
// ファイルの情報[tutorial_ui_controls.cpp]
//
// 制作者:Masatora Tanaka		日付：2026/07/02
// =========================================================
#include "pch.h"

#include "tutorial_ui_controls.h"

#include <algorithm>
#include <cmath>

#include "DxLib.h"
#include "game_scene_state.h"
#include "input.h"

// =========================================================
// 左右ナビゲーションボタンの配置を取得
// =========================================================
TutorialButtonRect getTutorialNavigationButtonRect(
    const GameSceneUiTutorialTuning& ui,
    bool leftButton)
{
    constexpr float buttonSize = 72.0f;
    constexpr float horizontalOffset = 290.0f;
    const float centerX =
        ui.frameX + ui.frameWidth * 0.5f +
        (leftButton ? -horizontalOffset : horizontalOffset);
    const float centerY = ui.promptY;
    return {
        centerX - buttonSize * 0.5f,
        centerY - buttonSize * 0.5f,
        centerX + buttonSize * 0.5f,
        centerY + buttonSize * 0.5f,
    };
}

// =========================================================
// 閉じるボタンの配置を取得
// =========================================================
TutorialButtonRect getTutorialCloseButtonRect(const GameSceneUiTutorialTuning& ui)
{
    constexpr float buttonWidth = 440.0f;
    constexpr float buttonHeight = 68.0f;
    const float centerX = ui.frameX + ui.frameWidth * 0.5f;
    const float centerY = ui.promptY;
    return {
        centerX - buttonWidth * 0.5f,
        centerY - buttonHeight * 0.5f,
        centerX + buttonWidth * 0.5f,
        centerY + buttonHeight * 0.5f,
    };
}

// =========================================================
// 操作ボタンの当たり判定
// =========================================================
bool tutorialButtonContainsPoint(const TutorialButtonRect& rect, int x, int y)
{
    return static_cast<float>(x) >= rect.left &&
        static_cast<float>(x) <= rect.right &&
        static_cast<float>(y) >= rect.top &&
        static_cast<float>(y) <= rect.bottom;
}

// =========================================================
// 会話の進行待機アイコンを描画
// =========================================================
void drawTutorialWaitIcon(float x, float y, float alpha)
{
    const float time = static_cast<float>(GetNowCount()) * 0.006f;
    const float bounce = std::sin(time) * 4.0f;
    const int drawAlpha = static_cast<int>(std::round(
        std::clamp(alpha * (0.72f + 0.28f * std::sin(time)), 0.0f, 1.0f) * 255.0f));

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, drawAlpha);
    DrawTriangleAA(
        x - 14.0f,
        y - 6.0f + bounce,
        x + 14.0f,
        y - 6.0f + bounce,
        x,
        y + 10.0f + bounce,
        GetColor(255, 220, 142),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

// =========================================================
// 説明画面の左右ナビゲーションボタンを描画
// =========================================================
void drawTutorialNavigationButton(
    const TutorialButtonRect& rect,
    bool pointsLeft,
    bool enabled)
{
    const bool hovered =
        enabled &&
        tutorialButtonContainsPoint(rect, Input_GetMouseX(), Input_GetMouseY());
    const int centerX = static_cast<int>(std::round((rect.left + rect.right) * 0.5f));
    const int centerY = static_cast<int>(std::round((rect.top + rect.bottom) * 0.5f));
    const int radiusX = static_cast<int>(std::round((rect.right - rect.left) * 0.5f));
    const int radiusY = static_cast<int>(std::round((rect.bottom - rect.top) * 0.5f));
    const unsigned int fillColor = enabled
        ? (hovered ? GetColor(240, 205, 132) : GetColor(218, 178, 104))
        : GetColor(170, 156, 132);
    const unsigned int arrowColor = enabled
        ? GetColor(72, 48, 28)
        : GetColor(126, 116, 100);

    DrawOval(centerX, centerY, radiusX, radiusY, GetColor(88, 58, 30), TRUE);
    DrawOval(centerX, centerY, radiusX - 3, radiusY - 3, fillColor, TRUE);
    const float direction = pointsLeft ? 1.0f : -1.0f;
    DrawTriangleAA(
        static_cast<float>(centerX) + direction * 12.0f,
        static_cast<float>(centerY) - 15.0f,
        static_cast<float>(centerX) + direction * 12.0f,
        static_cast<float>(centerY) + 15.0f,
        static_cast<float>(centerX) - direction * 11.0f,
        static_cast<float>(centerY),
        arrowColor,
        TRUE);
}

// =========================================================
// 説明画面の横長閉じるボタンを描画
// =========================================================
void drawTutorialCloseButton(
    const TutorialButtonRect& rect,
    const std::string& label,
    float fontSize)
{
    const bool hovered = tutorialButtonContainsPoint(
        rect,
        Input_GetMouseX(),
        Input_GetMouseY());
    const int centerY = static_cast<int>(std::round((rect.top + rect.bottom) * 0.5f));
    const int radius = static_cast<int>(std::round((rect.bottom - rect.top) * 0.5f));
    const int leftCenterX = static_cast<int>(std::round(rect.left)) + radius;
    const int rightCenterX = static_cast<int>(std::round(rect.right)) - radius;
    const unsigned int fillColor = hovered
        ? GetColor(240, 205, 132)
        : GetColor(218, 178, 104);

    // 長方形と左右の円で、頂点数を増やさず横長カプセル形状にします。
    DrawBox(leftCenterX, centerY - radius, rightCenterX, centerY + radius, GetColor(88, 58, 30), TRUE);
    DrawOval(leftCenterX, centerY, radius, radius, GetColor(88, 58, 30), TRUE);
    DrawOval(rightCenterX, centerY, radius, radius, GetColor(88, 58, 30), TRUE);
    DrawBox(leftCenterX, centerY - radius + 3, rightCenterX, centerY + radius - 3, fillColor, TRUE);
    DrawOval(leftCenterX, centerY, radius - 3, radius - 3, fillColor, TRUE);
    DrawOval(rightCenterX, centerY, radius - 3, radius - 3, fillColor, TRUE);

    const int previousFontSize = GetFontSize();
    SetFontSize(std::max(8, static_cast<int>(std::round(fontSize))));
    const int textWidth = GetDrawStringWidth(label.c_str(), static_cast<int>(label.size()));
    const int textHeight = GetFontSize();
    DrawString(
        static_cast<int>(std::round((rect.left + rect.right - static_cast<float>(textWidth)) * 0.5f)),
        centerY - textHeight / 2,
        label.c_str(),
        GetColor(72, 48, 28));
    SetFontSize(previousFontSize);
}
