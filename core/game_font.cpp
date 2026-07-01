// =========================================================
// ファイルの情報[game_font.cpp]
//
// 制作者:Masatora Tanaka		日付：2026/07/02
// =========================================================
#include "pch.h"

#include "game_font.h"

#include <filesystem>

#include "DxLib.h"
#include "logger.h"

// =========================================================
// グローバル変数
// =========================================================
namespace
{
    constexpr const char* kGameFontPath =
        "assets/fonts/LogoTypeGothicCondense.ttf";
    constexpr const char* kGameFontFamily =
        "07ロゴたいぷゴシックCondense";

    HANDLE g_gameFontResource = nullptr; // 実行中だけ登録するフォントリソース
}

// =========================================================
// ゲーム共通フォントのパスを取得
// =========================================================
const char* getGameFontPath()
{
    return kGameFontPath;
}

// =========================================================
// ゲーム共通フォントの初期化
// =========================================================
bool initializeGameFont()
{
    if (g_gameFontResource)
    {
        return true;
    }
    if (!std::filesystem::exists(kGameFontPath))
    {
        Logger::Error(std::string("Game font file was not found: ") + kGameFontPath);
        return false;
    }

    g_gameFontResource = AddFontFile(kGameFontPath);
    if (!g_gameFontResource)
    {
        Logger::Error(std::string("Game font could not be registered: ") + kGameFontPath);
        return false;
    }

    if (ChangeFont(kGameFontFamily, DX_CHARSET_UTF8) == -1)
    {
        Logger::Error(std::string("Game font family could not be selected: ") + kGameFontFamily);
        RemoveFontFile(g_gameFontResource);
        g_gameFontResource = nullptr;
        return false;
    }

    // UI文字の輪郭を保ちつつ、拡大時のジャギーを抑えます。
    ChangeFontType(DX_FONTTYPE_ANTIALIASING_4X4);
    Logger::Info(std::string("Game font initialized: ") + kGameFontFamily);
    return true;
}

// =========================================================
// ゲーム共通フォントの終了処理
// =========================================================
void shutdownGameFont()
{
    if (!g_gameFontResource)
    {
        return;
    }

    RemoveFontFile(g_gameFontResource);
    g_gameFontResource = nullptr;
}
