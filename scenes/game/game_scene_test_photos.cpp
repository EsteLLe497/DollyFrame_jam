// =========================================================
// ファイルの情報[game_scene_test_photos.cpp]
//
// 制作者:Masatora Tanaka        日付:2026/06/23
// =========================================================
#include "pch.h"

#include "game_scene_test_photos.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "DxLib.h"
#include "game_scene.h"
#include "imgui.h"
#include "resource_manager.h"
#include "sprite.h"
#include "texture.h"

namespace
{
    // =========================================================
    // 初期配置用の設定
    // =========================================================
    struct TestPhotoInitialSetting
    {
        bool visible;             // falseにすると写真を非表示にできる
        const wchar_t* filePath;  // 写真名はここを差し替える
        float x;                  // スクリーン座標X
        float y;                  // スクリーン座標Y
        float width;              // 仮枠用の横幅。画像があれば元サイズで上書き
        float height;             // 仮枠用の高さ。画像があれば元サイズで上書き
    };

    // =========================================================
    // テスト写真の初期値
    // =========================================================
    constexpr std::array<TestPhotoInitialSetting, kGameSceneTestPhotoCount> kInitialTestPhotos =
    {{
            //                                                 X      Y      Size    アスペクト比維持のため変更すんな
        { true, L"assets\\texture\\testUI\\UIprov_Camera.png", 23.0f, 23.0f, 200.0f, 96.0f },
        { false, L"", 0.0f, 0.0f, 0.0f, 0.0f },
        { false, L"", 0.0f, 0.0f, 0.0f, 0.0f },
        { false, L"", 0.0f, 0.0f, 0.0f, 0.0f },
        { false, L"", 0.0f, 0.0f, 0.0f, 0.0f },
        { false, L"", 0.0f, 0.0f, 0.0f, 0.0f },
        { true, L"assets\\texture\\testUI\\UIprov_ItemCounter.png", 1598.5f, 17.0f, 300.0f, 96.0f },
        { true, L"assets\\texture\\testUI\\UIprov_CaptureSpace_Out.png", 24.5f, 854.0f, 610.0f, 96.0f },
        { true, L"assets\\texture\\testUI\\UIprov_CaptureSpace_In.png", 44.5f, 875.5f, 570.0f, 96.0f },
        { true, L"assets\\texture\\testUI\\UIprov_PhotoFrame.png", 923.5f, 522.5f, 159.0f, 96.0f },
        { true, L"assets\\texture\\testUI\\UIprov_NotPhotoState.png", 68.0f, 906.0f, 149.0f, 96.0f },
        { true, L"assets\\texture\\testUI\\UIprov_NotPhotoState.png", 253.0f, 906.0f, 149.0f, 96.0f },
        { true, L"assets\\texture\\testUI\\UIprov_NotPhotoState.png", 441.5f, 906.0f, 149.0f, 96.0f },
        { false, L"assets\\texture\\testUI\\test_photo_14.png", 660.0f, 490.0f, 144.0f, 96.0f },
        { false, L"assets\\texture\\testUI\\test_photo_15.png", 840.0f, 490.0f, 144.0f, 96.0f },
    }};

    // =========================================================
    // 画像の元サイズを取得
    // =========================================================
    bool TryGetTextureSize(const GameSceneTestPhoto& photo, float& outWidth, float& outHeight)
    {
        const int textureWidth = TextureGetWidth(photo.textureId);
        const int textureHeight = TextureGetHeight(photo.textureId);
        if (textureWidth <= 0 || textureHeight <= 0)
        {
            return false;
        }

        outWidth = static_cast<float>(textureWidth);
        outHeight = static_cast<float>(textureHeight);
        return true;
    }

    // =========================================================
    // 画像比率を保った高さを取得
    // =========================================================
    float GetAspectLockedHeight(const GameSceneTestPhoto& photo)
    {
        float textureWidth = 0.0f;
        float textureHeight = 0.0f;
        if (TryGetTextureSize(photo, textureWidth, textureHeight))
        {
            return std::max(1.0f, photo.width) * textureHeight / textureWidth;
        }

        return std::max(1.0f, photo.height);
    }

    // =========================================================
    // 仮写真の枠描画
    // =========================================================
    void DrawMissingPhotoFrame(int photoNumber, float x, float y, float width, float height)
    {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 78);
        DrawBoxAA(x, y, x + width, y + height, GetColor(255, 255, 255), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawBoxAA(x, y, x + width, y + height, GetColor(255, 236, 120), FALSE);

        char label[32]{};
        std::snprintf(label, sizeof(label), "Photo%02d", photoNumber);
        DrawString(
            static_cast<int>(std::round(x + 8.0f)),
            static_cast<int>(std::round(y + 8.0f)),
            label,
            GetColor(255, 236, 120));
    }
}

// =========================================================
// 初期化
// =========================================================
void InitializeGameSceneTestPhotos(GameSceneTestPhotoState& state, ResourceManager& resources)
{
    state = GameSceneTestPhotoState{};

    for (int index = 0; index < kGameSceneTestPhotoCount; ++index)
    {
        const auto& setting = kInitialTestPhotos[static_cast<size_t>(index)];
        auto& photo = state.photos[static_cast<size_t>(index)];

        photo.visible = setting.visible;
        photo.filePath = setting.filePath;
        photo.x = setting.x;
        photo.y = setting.y;
        photo.width = setting.width;
        photo.height = setting.height;
        photo.textureId = (photo.filePath.empty() || photo.filePath[0] == L'\0')
            ? -1
            : resources.LoadTexture(photo.filePath);

        // UI扱いなので、初期サイズは画像ピクセルサイズそのまま使う。
        TryGetTextureSize(photo, photo.width, photo.height);
    }
}

// =========================================================
// 終了処理
// =========================================================
void ShutdownGameSceneTestPhotos(GameSceneTestPhotoState& state)
{
    state = GameSceneTestPhotoState{};
}

// =========================================================
// 描画
// =========================================================
void DrawGameSceneTestPhotos(const GameSceneTestPhotoState& state)
{
    if (!state.enabled)
    {
        return;
    }

    for (int index = 0; index < kGameSceneTestPhotoCount; ++index)
    {
        const auto& photo = state.photos[static_cast<size_t>(index)];
        if (!photo.visible)
        {
            continue;
        }

        const float drawWidth = std::max(1.0f, photo.width);
        const float drawHeight = GetAspectLockedHeight(photo);
        if (photo.textureId >= 0)
        {
            SpriteDraw(
                photo.textureId,
                photo.x,
                photo.y,
                drawWidth,
                drawHeight,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
        }
        else
        {
            DrawMissingPhotoFrame(index + 1, photo.x, photo.y, drawWidth, drawHeight);
        }
    }
}

// =========================================================
// ImGui調整
// =========================================================
void DrawGameSceneTestPhotoDebugUI(GameSceneTestPhotoState& state)
{
    ImGui::SetNextWindowSize(ImVec2(520.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Test Photos"))
    {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Enable Test Photos", &state.enabled);
    ImGui::TextUnformatted("Screen UI mode. X/Y are screen pixels.");
    ImGui::TextUnformatted("Initial size uses the original image pixel size.");

    ImGui::SeparatorText("Photo 1-15");
    for (int index = 0; index < kGameSceneTestPhotoCount; ++index)
    {
        auto& photo = state.photos[static_cast<size_t>(index)];

        ImGui::PushID(index);
        ImGui::Separator();
        ImGui::Checkbox("Visible", &photo.visible);
        ImGui::SameLine();
        ImGui::Text("Photo %02d", index + 1);
        ImGui::DragFloat("X", &photo.x, 0.5f, -10000.0f, 10000.0f, "%.1f");
        ImGui::DragFloat("Y", &photo.y, 0.5f, -10000.0f, 10000.0f, "%.1f");
        if (ImGui::DragFloat("Width", &photo.width, 0.5f, 1.0f, 4096.0f, "%.1f"))
        {
            photo.height = GetAspectLockedHeight(photo);
        }
        photo.height = GetAspectLockedHeight(photo);
        ImGui::Text("Height: %.1f (aspect locked)", photo.height);
        ImGui::PopID();
    }

    ImGui::End();
}

// =========================================================
// テスト写真リソース初期化
// =========================================================
void GameScene::InitializeTestPhotoResources(ResourceManager& resources)
{
    InitializeGameSceneTestPhotos(m_testPhotos, resources);
}

// =========================================================
// テスト写真描画
// =========================================================
void GameScene::DrawTestPhotos() const
{
    DrawGameSceneTestPhotos(m_testPhotos);
}

// =========================================================
// テスト写真調整UI
// =========================================================
void GameScene::DrawTestPhotoPanel()
{
    DrawGameSceneTestPhotoDebugUI(m_testPhotos);
}
