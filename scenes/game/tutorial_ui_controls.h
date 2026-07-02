// =========================================================
// ファイルの情報[tutorial_ui_controls.h]
//
// 制作者:Masatora Tanaka		日付：2026/07/02
// =========================================================
#pragma once

#include <string>

struct GameSceneUiTutorialTuning;

// =========================================================
// チュートリアル操作ボタンの矩形
// =========================================================
struct TutorialButtonRect
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

// =========================================================
// 操作ボタンの配置と当たり判定
// =========================================================
TutorialButtonRect getTutorialNavigationButtonRect(
    const GameSceneUiTutorialTuning& ui,
    bool leftButton);
TutorialButtonRect getTutorialCloseButtonRect(const GameSceneUiTutorialTuning& ui);
bool tutorialButtonContainsPoint(const TutorialButtonRect& rect, int x, int y);

// =========================================================
// 会話待機アイコンと説明画面ボタンの描画
// =========================================================
void drawTutorialWaitIcon(float x, float y, float alpha);
void drawTutorialNavigationButton(
    const TutorialButtonRect& rect,
    bool pointsLeft,
    bool enabled);
void drawTutorialCloseButton(
    const TutorialButtonRect& rect,
    const std::string& label,
    float fontSize);
