// =========================================================
// ファイルの情報[tutorial_video_player.cpp]
//
// 制作者:Masatora Tanaka		日付：2026/07/02
// =========================================================
#include "pch.h"

#include "tutorial_video_player.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "DxLib.h"
#include "logger.h"

// =========================================================
// 動画を解放
// =========================================================
void releaseTutorialVideo(TutorialVideoPlayer& player)
{
    if (player.graphHandle >= 0)
    {
        PauseMovieToGraph(player.graphHandle);
        DeleteGraph(player.graphHandle);
    }
    player.graphHandle = -1;
    player.loadedPath.clear();
}

// =========================================================
// 指定された動画をループ再生
// =========================================================
bool prepareTutorialVideo(TutorialVideoPlayer& player, const std::string& videoPath)
{
    if (videoPath.empty())
    {
        releaseTutorialVideo(player);
        return false;
    }
    if (player.loadedPath == videoPath && player.graphHandle >= 0)
    {
        return true;
    }

    releaseTutorialVideo(player);
    if (!std::filesystem::exists(videoPath))
    {
        Logger::Warn("Tutorial video file was not found: " + videoPath);
        return false;
    }

    player.graphHandle = OpenMovieToGraph(videoPath.c_str());
    if (player.graphHandle < 0)
    {
        Logger::Warn("Tutorial video could not be opened: " + videoPath);
        return false;
    }

    player.loadedPath = videoPath;
    SetMovieVolumeToGraph(0, player.graphHandle); // UI動画はBGMへ干渉しないよう無音にします。
    if (PlayMovieToGraph(player.graphHandle, DX_PLAYTYPE_LOOP) == -1)
    {
        Logger::Warn("Tutorial video could not be played: " + videoPath);
        releaseTutorialVideo(player);
        return false;
    }

    Logger::Info("Tutorial video started: " + videoPath);
    return true;
}

// =========================================================
// 縦横比を維持して動画を説明画像領域へ描画
// =========================================================
void drawTutorialVideo(
    TutorialVideoPlayer& player,
    float x,
    float y,
    float width,
    float height)
{
    if (player.graphHandle < 0 || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    UpdateMovieToGraph(player.graphHandle);

    int sourceWidth = 0;
    int sourceHeight = 0;
    GetGraphSize(player.graphHandle, &sourceWidth, &sourceHeight);
    if (sourceWidth <= 0 || sourceHeight <= 0)
    {
        return;
    }

    const float scale = std::min(
        width / static_cast<float>(sourceWidth),
        height / static_cast<float>(sourceHeight));
    const float drawWidth = static_cast<float>(sourceWidth) * scale;
    const float drawHeight = static_cast<float>(sourceHeight) * scale;
    const int left = static_cast<int>(std::round(x + (width - drawWidth) * 0.5f));
    const int top = static_cast<int>(std::round(y + (height - drawHeight) * 0.5f));
    const int right = static_cast<int>(std::round(static_cast<float>(left) + drawWidth));
    const int bottom = static_cast<int>(std::round(static_cast<float>(top) + drawHeight));

    DrawBox(
        static_cast<int>(std::round(x)),
        static_cast<int>(std::round(y)),
        static_cast<int>(std::round(x + width)),
        static_cast<int>(std::round(y + height)),
        GetColor(8, 8, 10),
        TRUE);
    DrawExtendGraph(left, top, right, bottom, player.graphHandle, FALSE);
}
