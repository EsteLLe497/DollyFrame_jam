// =========================================================
// ファイルの情報[tutorial_video_player.h]
//
// 制作者:Masatora Tanaka		日付：2026/07/02
// =========================================================
#pragma once

#include <string>

// =========================================================
// チュートリアル動画の再生状態
// =========================================================
struct TutorialVideoPlayer
{
    int graphHandle = -1;
    std::string loadedPath;
};

// =========================================================
// 動画の読込・更新・描画・解放
// =========================================================
bool prepareTutorialVideo(TutorialVideoPlayer& player, const std::string& videoPath);
void drawTutorialVideo(
    TutorialVideoPlayer& player,
    float x,
    float y,
    float width,
    float height);
void releaseTutorialVideo(TutorialVideoPlayer& player);
