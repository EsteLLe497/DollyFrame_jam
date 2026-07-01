// =========================================================
// ファイルの情報[tutorial_csv_data.h]
//
// 制作者:Masatora Tanaka		日付：2026/07/01
// =========================================================
#pragma once

#include <string>
#include <vector>

// =========================================================
// チュートリアルページ種別
// =========================================================
enum class TutorialPageType
{
    Conversation,
    Window,
};

// =========================================================
// CSVから読み込む1ページ分の表示データ
// =========================================================
struct TutorialPageData
{
    TutorialPageType type = TutorialPageType::Conversation;
    std::string speaker;
    std::string portraitPath;
    std::string title;
    std::string text;
    std::string confirmText;
    std::string contentTextureKey;
    std::string contentVideoPath;
};

// =========================================================
// 指定IDのチュートリアルをCSVから読み込む
// =========================================================
bool loadTutorialPagesFromCsv(
    const std::string& csvPath,
    const std::string& tutorialId,
    std::vector<TutorialPageData>& outPages);
