// =========================================================
// ファイルの情報[game_scene_test_photos.h]
//
// 制作者:Masatora Tanaka        日付:2026/06/23
// =========================================================
#pragma once

#include <array>
#include <string>

class GameScene;
class ResourceManager;

// =========================================================
// テスト写真の定数
// =========================================================
constexpr int kGameSceneTestPhotoCount = 15;      // 配置するテスト写真の最大数
constexpr int kGameSceneTestPhotoEditableCount = 9; // ImGuiで細かく調整する写真数

// =========================================================
// テスト写真1枚分の情報
// =========================================================
struct GameSceneTestPhoto
{
    bool visible = true;              // falseにすると描画をすぐ消せる
    int textureId = -1;               // 読み込んだテクスチャID
    std::wstring filePath;            // 差し替え用の写真ファイル名
    float x = 0.0f;                   // ワールド座標X
    float y = 0.0f;                   // ワールド座標Y
    float width = 128.0f;             // 横幅
    float height = 128.0f;            // 縦幅
};

// =========================================================
// テスト写真全体の状態
// =========================================================
struct GameSceneTestPhotoState
{
    bool enabled = true; // 全テスト写真の表示スイッチ
    std::array<GameSceneTestPhoto, kGameSceneTestPhotoCount> photos;
};

// =========================================================
// 初期化
// =========================================================
void InitializeGameSceneTestPhotos(GameSceneTestPhotoState& state, ResourceManager& resources);

// =========================================================
// 終了処理
// =========================================================
void ShutdownGameSceneTestPhotos(GameSceneTestPhotoState& state);

// =========================================================
// 描画
// =========================================================
void DrawGameSceneTestPhotos(
    const GameSceneTestPhotoState& state);

// =========================================================
// ImGui調整
// =========================================================
void DrawGameSceneTestPhotoDebugUI(GameSceneTestPhotoState& state);
