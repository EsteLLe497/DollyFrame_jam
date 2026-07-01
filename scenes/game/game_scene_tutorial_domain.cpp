#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <sstream>

#include "DxLib.h"
#include "imgui_layer.h"
#include "misc/cpp/imgui_stdlib.h"
#include "shader.h"
#include "sprite.h"

using namespace game_scene_detail;

namespace
{
    enum class TutorialDrawKind
    {
        Dim,
        DialogueBox,
        DialoguePortrait,
        DialogueName,
        DialogueText,
        DialoguePrompt,
        Frame,
        Heading,
        ContentImage,
        Title,
        Body,
        Prompt,
    };

    struct TutorialDrawElement
    {
        int layer = 0;
        int order = 0;
        TutorialDrawKind kind = TutorialDrawKind::Dim;
    };

    struct TutorialCharacterProfile
    {
        const char* name = "";
        const char* portraitPath = "";
    };

    constexpr std::array<TutorialCharacterProfile, 1> kTutorialCharacters = {{
        { "あまりりす", "assets/texture/tutorialUI/Amaryllis.png" },
    }};

    const TutorialCharacterProfile& GetTutorialCharacterProfile(int characterIndex)
    {
        const int clampedIndex = std::clamp(
            characterIndex,
            0,
            static_cast<int>(kTutorialCharacters.size()) - 1);
        return kTutorialCharacters[static_cast<size_t>(clampedIndex)];
    }

    size_t CountUtf8Characters(const std::string& text)
    {
        size_t characterCount = 0;
        for (unsigned char byte : text)
        {
            if ((byte & 0xc0u) != 0x80u)
            {
                ++characterCount;
            }
        }
        return characterCount;
    }

    std::string Utf8Prefix(const std::string& text, size_t characterCount)
    {
        if (characterCount >= CountUtf8Characters(text))
        {
            return text;
        }

        size_t byteIndex = 0;
        size_t currentCharacter = 0;
        while (byteIndex < text.size() && currentCharacter < characterCount)
        {
            const unsigned char leadByte = static_cast<unsigned char>(text[byteIndex]);
            size_t characterBytes = 1;
            if ((leadByte & 0xe0u) == 0xc0u) characterBytes = 2;
            else if ((leadByte & 0xf0u) == 0xe0u) characterBytes = 3;
            else if ((leadByte & 0xf8u) == 0xf0u) characterBytes = 4;
            byteIndex = std::min(text.size(), byteIndex + characterBytes);
            ++currentCharacter;
        }
        return text.substr(0, byteIndex);
    }

    void DrawTutorialImage(int textureId, float x, float y, float width, float height, float alpha = 1.0f)
    {
        if (textureId < 0 || width <= 0.0f || height <= 0.0f)
        {
            return;
        }

        Shader_ResetStyle();
        Shader_SetTint(1.0f, 1.0f, 1.0f, std::clamp(alpha, 0.0f, 1.0f));
        SpriteDraw(textureId, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_ResetStyle();
    }

    void DrawTutorialFrame(int textureId, float x, float y, float width, float height)
    {
        if (textureId < 0 || width <= 0.0f || height <= 0.0f)
        {
            return;
        }

        constexpr float sourceBorderU = 0.10f;
        constexpr float sourceBorderV = 0.22f;
        const float borderX = std::min(width * 0.24f, 100.0f);
        const float borderY = std::min(height * 0.24f, 100.0f);
        const float destinationXs[4] = { x, x + borderX, x + width - borderX, x + width };
        const float destinationYs[4] = { y, y + borderY, y + height - borderY, y + height };
        const float sourceUs[4] = { 0.0f, sourceBorderU, 1.0f - sourceBorderU, 1.0f };
        const float sourceVs[4] = { 0.0f, sourceBorderV, 1.0f - sourceBorderV, 1.0f };

        Shader_ResetStyle();
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                SpriteDraw(
                    textureId,
                    destinationXs[column],
                    destinationYs[row],
                    destinationXs[column + 1] - destinationXs[column],
                    destinationYs[row + 1] - destinationYs[row],
                    sourceUs[column],
                    sourceVs[row],
                    sourceUs[column + 1] - sourceUs[column],
                    sourceVs[row + 1] - sourceVs[row]);
            }
        }
        Shader_ResetStyle();
    }

    void DrawTutorialText(
        const std::string& text,
        float x,
        float y,
        float fontSize,
        float lineSpacing,
        unsigned int color,
        float clipWidth = 0.0f,
        float alpha = 1.0f)
    {
        const int previousFontSize = GetFontSize();
        SetFontSize(std::max(8, static_cast<int>(std::round(fontSize))));
        SetDrawBlendMode(
            DX_BLENDMODE_ALPHA,
            static_cast<int>(std::round(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
        if (clipWidth > 0.0f)
        {
            SetDrawArea(
                std::max(0, static_cast<int>(std::floor(x))),
                std::max(0, static_cast<int>(std::floor(y))),
                std::min(SCREEN_WIDTH, static_cast<int>(std::ceil(x + clipWidth))),
                SCREEN_HEIGHT);
        }

        std::istringstream lines(text);
        std::string line;
        int lineIndex = 0;
        while (std::getline(lines, line))
        {
            DrawString(
                static_cast<int>(std::round(x)),
                static_cast<int>(std::round(y + lineSpacing * static_cast<float>(lineIndex))),
                line.c_str(),
                color);
            ++lineIndex;
        }

        if (clipWidth > 0.0f)
        {
            SetDrawArea(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        SetFontSize(previousFontSize);
    }

    void DrawPortraitPlaceholder(float x, float y, float size, float alpha)
    {
        const int drawAlpha = static_cast<int>(std::round(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, drawAlpha);
        DrawBox(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(x + size)),
            static_cast<int>(std::round(y + size)),
            GetColor(34, 26, 32),
            TRUE);
        DrawBox(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(x + size)),
            static_cast<int>(std::round(y + size)),
            GetColor(202, 164, 104),
            FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    bool IntersectsTutorialTrigger(
        const TransformComponent& player,
        int column,
        int row,
        int widthTiles,
        float tileSize)
    {
        const float playerWidth = player.width * player.scale;
        const float playerHeight = player.height * player.scale;
        const float triggerX = static_cast<float>(column) * tileSize;
        const float triggerY = static_cast<float>(row) * tileSize;
        const float triggerWidth = static_cast<float>(std::max(1, widthTiles)) * tileSize;
        return player.x < triggerX + triggerWidth &&
            player.x + playerWidth > triggerX &&
            player.y < triggerY + tileSize &&
            player.y + playerHeight > triggerY;
    }
}

bool GameScene::UpdateTutorialModal(float deltaTime)
{
    if (m_tutorial.phase == TutorialPresentationPhase::Inactive)
    {
        return false;
    }

    if (m_tutorial.phase == TutorialPresentationPhase::Conversation)
    {
        const auto& ui = m_ui.tuning.tutorial;
        const float fadeDuration = std::max(0.01f, ui.dialogueFadeDuration);
        m_tutorial.dialogueFadeElapsed += std::max(0.0f, deltaTime);
        if (m_tutorial.dialogueFadeElapsed >= fadeDuration)
        {
            m_tutorial.dialogueRevealElapsed += std::max(0.0f, deltaTime);
        }

        const size_t totalCharacters = CountUtf8Characters(ui.dialogueText);
        const size_t revealedCharacters = static_cast<size_t>(std::floor(
            m_tutorial.dialogueRevealElapsed *
            std::max(1.0f, ui.dialogueCharactersPerSecond)));
        const bool textFinished = revealedCharacters >= totalCharacters;
        if (textFinished &&
            (Input_IsActionPressed(InputAction::Confirm) || Input_IsSouthButtonPressed()))
        {
            m_tutorial.phase = TutorialPresentationPhase::TutorialWindow;
        }
        return true;
    }

    if (Input_IsActionPressed(InputAction::Confirm) || Input_IsSouthButtonPressed())
    {
        CompleteCameraTutorial();
    }
    return true;
}

void GameScene::BeginCameraTutorialConversation()
{
    m_tutorial.previewConversation = false;
    m_tutorial.previewWindow = false;
    m_tutorial.dialogueFadeElapsed = 0.0f;
    m_tutorial.dialogueRevealElapsed = 0.0f;
    m_tutorial.phase = TutorialPresentationPhase::Conversation;
}

void GameScene::EnsureTutorialPortraitTexture()
{
    const int characterIndex = std::clamp(
        m_ui.tuning.tutorial.dialogueCharacter,
        0,
        static_cast<int>(kTutorialCharacters.size()) - 1);
    if (m_tutorial.loadedPortraitCharacter != characterIndex)
    {
        if (m_tutorial.portraitTextureId >= 0)
        {
            DeleteGraph(m_tutorial.portraitTextureId);
        }
        m_tutorial.portraitTextureId = -1;
        m_tutorial.loadedPortraitCharacter = characterIndex;
    }

    if (m_tutorial.portraitTextureId < 0)
    {
        const auto& character = GetTutorialCharacterProfile(characterIndex);
        if (std::filesystem::exists(character.portraitPath))
        {
            m_tutorial.portraitTextureId = LoadGraph(character.portraitPath);
        }
    }
}

void GameScene::TryStartCameraTutorial()
{
    if (m_tutorial.phase != TutorialPresentationPhase::Inactive ||
        GameSession_Get().cameraTutorialCompleted)
    {
        return;
    }

    const Entity* player = FindEntityByTag(kTagPlayer);
    const auto* transform = player ? player->GetComponent<TransformComponent>() : nullptr;
    const float tileSize = m_tileMap.GetTileSize();
    if (!transform || tileSize <= 0.0f)
    {
        return;
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (!IsTutorialStartMarker(m_tileMap.GetMarker(column, row)))
            {
                continue;
            }

            const int widthTiles = std::max(1, m_tileMap.GetMarkerParameter(column, row));
            if (IntersectsTutorialTrigger(*transform, column, row, widthTiles, tileSize))
            {
                BeginCameraTutorialConversation();
                return;
            }
        }
    }
}

void GameScene::CompleteCameraTutorial()
{
    m_tutorial.previewConversation = false;
    m_tutorial.previewWindow = false;
    m_tutorial.phase = TutorialPresentationPhase::Inactive;
    GameSession_SetCameraTutorialCompleted(true);
    SaveProgressState();
}

void GameScene::DrawTutorialOverlay()
{
    const bool drawConversation =
        m_tutorial.phase == TutorialPresentationPhase::Conversation ||
        m_tutorial.previewConversation;
    const bool drawWindow =
        m_tutorial.phase == TutorialPresentationPhase::TutorialWindow ||
        m_tutorial.previewWindow;
    if (!drawConversation && !drawWindow)
    {
        return;
    }

    const auto& ui = m_ui.tuning.tutorial;
    const int frameTexture = m_assets.GetTexture("tutorial_frame_window");
    const int headingTexture = m_assets.GetTexture("tutorial_heading");
    const int contentImageTexture = m_assets.GetTexture("tutorial_content_image");
    const int textBoxTexture = m_assets.GetTexture("tutorial_text_box");

    if (drawConversation)
    {
        EnsureTutorialPortraitTexture();
        const auto& character = GetTutorialCharacterProfile(ui.dialogueCharacter);
        const bool preview = m_tutorial.previewConversation &&
            m_tutorial.phase != TutorialPresentationPhase::Conversation;
        const float fadeAlpha = preview
            ? 1.0f
            : std::clamp(
                m_tutorial.dialogueFadeElapsed / std::max(0.01f, ui.dialogueFadeDuration),
                0.0f,
                1.0f);
        const size_t totalCharacters = CountUtf8Characters(ui.dialogueText);
        const size_t revealedCharacters = preview
            ? totalCharacters
            : static_cast<size_t>(std::floor(
                m_tutorial.dialogueRevealElapsed *
                std::max(1.0f, ui.dialogueCharactersPerSecond)));
        const bool textFinished = revealedCharacters >= totalCharacters;
        const std::string visibleDialogue = Utf8Prefix(ui.dialogueText, revealedCharacters);

        std::array<TutorialDrawElement, 6> elements = {{
            { 0, 0, TutorialDrawKind::Dim },
            { ui.dialogueBoxLayer, 1, TutorialDrawKind::DialogueBox },
            { ui.dialoguePortraitLayer, 2, TutorialDrawKind::DialoguePortrait },
            { ui.dialogueNameLayer, 3, TutorialDrawKind::DialogueName },
            { ui.dialogueTextLayer, 4, TutorialDrawKind::DialogueText },
            { ui.dialoguePromptLayer, 5, TutorialDrawKind::DialoguePrompt },
        }};
        std::stable_sort(
            elements.begin(),
            elements.end(),
            [](const TutorialDrawElement& left, const TutorialDrawElement& right)
            {
                return left.layer == right.layer ? left.order < right.order : left.layer < right.layer;
            });

        for (const auto& element : elements)
        {
            switch (element.kind)
            {
            case TutorialDrawKind::Dim:
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(std::clamp(ui.dimAlpha, 0.0f, 1.0f) * 255.0f)));
                DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(8, 10, 15), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                break;
            case TutorialDrawKind::DialogueBox:
                DrawTutorialImage(
                    textBoxTexture,
                    ui.dialogueBoxX,
                    ui.dialogueBoxY,
                    ui.dialogueBoxWidth,
                    ui.dialogueBoxHeight,
                    fadeAlpha);
                break;
            case TutorialDrawKind::DialoguePortrait:
                if (m_tutorial.portraitTextureId >= 0)
                {
                    DrawTutorialImage(
                        m_tutorial.portraitTextureId,
                        ui.dialoguePortraitX,
                        ui.dialoguePortraitY,
                        ui.dialoguePortraitSize,
                        ui.dialoguePortraitSize,
                        fadeAlpha);
                }
                else
                {
                    DrawPortraitPlaceholder(
                        ui.dialoguePortraitX,
                        ui.dialoguePortraitY,
                        ui.dialoguePortraitSize,
                        fadeAlpha);
                }
                break;
            case TutorialDrawKind::DialogueName:
                DrawTutorialText(
                    character.name,
                    ui.dialogueNameX,
                    ui.dialogueNameY,
                    ui.dialogueNameFontSize,
                    ui.dialogueNameFontSize,
                    GetColor(255, 238, 206),
                    0.0f,
                    fadeAlpha);
                break;
            case TutorialDrawKind::DialogueText:
                DrawTutorialText(
                    visibleDialogue,
                    ui.dialogueTextX,
                    ui.dialogueTextY,
                    ui.dialogueTextFontSize,
                    ui.dialogueLineSpacing,
                    GetColor(255, 250, 242),
                    std::max(0.0f, ui.dialogueBoxWidth - (ui.dialogueTextX - ui.dialogueBoxX) * 2.0f),
                    fadeAlpha);
                break;
            case TutorialDrawKind::DialoguePrompt:
                if (textFinished)
                {
                    const float pulse = 0.72f + 0.28f * std::sin(static_cast<float>(GetNowCount()) * 0.006f);
                    DrawTutorialText(
                        ui.confirmText,
                        ui.dialoguePromptX,
                        ui.dialoguePromptY,
                        ui.promptFontSize,
                        ui.promptFontSize,
                        GetColor(255, 220, 142),
                        0.0f,
                        fadeAlpha * pulse);
                }
                break;
            default:
                break;
            }
        }
    }

    if (drawWindow)
    {
        std::array<TutorialDrawElement, 7> elements = {{
            { 0, 0, TutorialDrawKind::Dim },
            { ui.frameLayer, 1, TutorialDrawKind::Frame },
            { ui.headingLayer, 2, TutorialDrawKind::Heading },
            { ui.contentImageLayer, 3, TutorialDrawKind::ContentImage },
            { ui.titleLayer, 4, TutorialDrawKind::Title },
            { ui.bodyLayer, 5, TutorialDrawKind::Body },
            { ui.promptLayer, 6, TutorialDrawKind::Prompt },
        }};
        std::stable_sort(
            elements.begin(),
            elements.end(),
            [](const TutorialDrawElement& left, const TutorialDrawElement& right)
            {
                return left.layer == right.layer ? left.order < right.order : left.layer < right.layer;
            });

        for (const auto& element : elements)
        {
            switch (element.kind)
            {
            case TutorialDrawKind::Dim:
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(std::clamp(ui.dimAlpha, 0.0f, 1.0f) * 255.0f)));
                DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(8, 10, 15), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                break;
            case TutorialDrawKind::Frame:
                DrawTutorialFrame(frameTexture, ui.frameX, ui.frameY, ui.frameWidth, ui.frameHeight);
                break;
            case TutorialDrawKind::Heading:
                DrawTutorialImage(headingTexture, ui.headingX, ui.headingY, ui.headingWidth, ui.headingHeight);
                break;
            case TutorialDrawKind::ContentImage:
                DrawTutorialImage(
                    contentImageTexture,
                    ui.contentImageX,
                    ui.contentImageY,
                    ui.contentImageWidth,
                    ui.contentImageHeight);
                break;
            case TutorialDrawKind::Title:
                DrawTutorialText(
                    ui.title,
                    ui.titleX,
                    ui.titleY,
                    ui.titleFontSize,
                    ui.titleFontSize,
                    GetColor(78, 48, 22));
                break;
            case TutorialDrawKind::Body:
                DrawTutorialText(
                    ui.bodyText,
                    ui.bodyX,
                    ui.bodyY,
                    ui.bodyFontSize,
                    ui.bodyLineSpacing,
                    GetColor(64, 46, 30),
                    ui.bodyWidth);
                break;
            case TutorialDrawKind::Prompt:
                DrawTutorialText(
                    ui.confirmText,
                    ui.promptX,
                    ui.promptY,
                    ui.promptFontSize,
                    ui.promptFontSize,
                    GetColor(88, 56, 26));
                break;
            default:
                break;
            }
        }
    }
}

void GameScene::DrawTutorialAdjustmentPanel()
{
    if (!ImGui::CollapsingHeader("チュートリアル", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    auto& ui = m_ui.tuning.tutorial;
    ImGui::Checkbox("会話プレビュー##tutorial", &m_tutorial.previewConversation);
    ImGui::SameLine();
    ImGui::Checkbox("説明画面プレビュー##tutorial", &m_tutorial.previewWindow);
    if (m_tutorial.previewConversation)
    {
        m_tutorial.previewWindow = false;
    }
    if (ImGui::Button("今すぐ会話から再生##tutorial"))
    {
        BeginCameraTutorialConversation();
    }
    ImGui::SameLine();
    if (ImGui::Button("一度だけフラグを解除##tutorial"))
    {
        GameSession_SetCameraTutorialCompleted(false);
        SaveProgressState();
    }
    ImGui::Text("完了フラグ: %s", GameSession_Get().cameraTutorialCompleted ? "完了" : "未完了");

    ImGui::SliderFloat("背景暗転##tutorial", &ui.dimAlpha, 0.0f, 1.0f, "%.2f");
    const char* characterNames[] = { "あまりりす" };
    ImGui::Combo(
        "会話キャラクター##tutorial",
        &ui.dialogueCharacter,
        characterNames,
        static_cast<int>(std::size(characterNames)));
    const auto& character = GetTutorialCharacterProfile(ui.dialogueCharacter);
    ImGui::Text("表示名: %s", character.name);
    ImGui::Text("立ち絵: %s", character.portraitPath);
    ImGui::InputTextMultiline("会話文##tutorial", &ui.dialogueText, ImVec2(-FLT_MIN, 90.0f));
    ImGui::InputText("見出し##tutorial", &ui.title);
    ImGui::InputTextMultiline("説明文##tutorial", &ui.bodyText, ImVec2(-FLT_MIN, 130.0f));
    ImGui::InputText("確認文##tutorial", &ui.confirmText);

    const auto drag = [](const char* label, float& value, float speed, float minValue, float maxValue)
    {
        ImGui::DragFloat(label, &value, speed, minValue, maxValue, "%.1f");
    };
    const auto dragLayer = [](const char* label, int& value)
    {
        ImGui::DragInt(label, &value, 1.0f, -100, 100);
    };

    if (ImGui::TreeNode("会話レイアウト##tutorial"))
    {
        drag("枠X##tutorial_dialogue", ui.dialogueBoxX, 1.0f, -2000.0f, 4000.0f);
        drag("枠Y##tutorial_dialogue", ui.dialogueBoxY, 1.0f, -2000.0f, 3000.0f);
        drag("枠幅##tutorial_dialogue", ui.dialogueBoxWidth, 1.0f, 10.0f, 4000.0f);
        drag("枠高さ##tutorial_dialogue", ui.dialogueBoxHeight, 1.0f, 10.0f, 3000.0f);
        drag("立ち絵X##tutorial_dialogue", ui.dialoguePortraitX, 1.0f, -2000.0f, 4000.0f);
        drag("立ち絵Y##tutorial_dialogue", ui.dialoguePortraitY, 1.0f, -2000.0f, 3000.0f);
        drag("立ち絵サイズ##tutorial_dialogue", ui.dialoguePortraitSize, 1.0f, 10.0f, 2000.0f);
        drag("話者X##tutorial_dialogue", ui.dialogueNameX, 1.0f, -2000.0f, 4000.0f);
        drag("話者Y##tutorial_dialogue", ui.dialogueNameY, 1.0f, -2000.0f, 3000.0f);
        drag("本文X##tutorial_dialogue", ui.dialogueTextX, 1.0f, -2000.0f, 4000.0f);
        drag("本文Y##tutorial_dialogue", ui.dialogueTextY, 1.0f, -2000.0f, 3000.0f);
        drag("確認X##tutorial_dialogue", ui.dialoguePromptX, 1.0f, -2000.0f, 4000.0f);
        drag("確認Y##tutorial_dialogue", ui.dialoguePromptY, 1.0f, -2000.0f, 3000.0f);
        drag("話者文字サイズ##tutorial_dialogue", ui.dialogueNameFontSize, 1.0f, 8.0f, 160.0f);
        drag("本文文字サイズ##tutorial_dialogue", ui.dialogueTextFontSize, 1.0f, 8.0f, 160.0f);
        drag("本文行間##tutorial_dialogue", ui.dialogueLineSpacing, 1.0f, 8.0f, 240.0f);
        drag("フェード秒数##tutorial_dialogue", ui.dialogueFadeDuration, 0.01f, 0.01f, 5.0f);
        drag("1秒あたり文字数##tutorial_dialogue", ui.dialogueCharactersPerSecond, 1.0f, 1.0f, 240.0f);
        dragLayer("枠レイヤー##tutorial_dialogue", ui.dialogueBoxLayer);
        dragLayer("立ち絵レイヤー##tutorial_dialogue", ui.dialoguePortraitLayer);
        dragLayer("話者レイヤー##tutorial_dialogue", ui.dialogueNameLayer);
        dragLayer("本文レイヤー##tutorial_dialogue", ui.dialogueTextLayer);
        dragLayer("確認レイヤー##tutorial_dialogue", ui.dialoguePromptLayer);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("説明画面レイアウト##tutorial"))
    {
        drag("外枠X##tutorial_window", ui.frameX, 1.0f, -2000.0f, 4000.0f);
        drag("外枠Y##tutorial_window", ui.frameY, 1.0f, -2000.0f, 3000.0f);
        drag("外枠幅##tutorial_window", ui.frameWidth, 1.0f, 10.0f, 4000.0f);
        drag("外枠高さ##tutorial_window", ui.frameHeight, 1.0f, 10.0f, 3000.0f);
        drag("見出し画像X##tutorial_window", ui.headingX, 1.0f, -2000.0f, 4000.0f);
        drag("見出し画像Y##tutorial_window", ui.headingY, 1.0f, -2000.0f, 3000.0f);
        drag("見出し画像幅##tutorial_window", ui.headingWidth, 1.0f, 10.0f, 4000.0f);
        drag("見出し画像高さ##tutorial_window", ui.headingHeight, 1.0f, 10.0f, 3000.0f);
        drag("説明画像X##tutorial_window", ui.contentImageX, 1.0f, -2000.0f, 4000.0f);
        drag("説明画像Y##tutorial_window", ui.contentImageY, 1.0f, -2000.0f, 3000.0f);
        drag("説明画像幅##tutorial_window", ui.contentImageWidth, 1.0f, 10.0f, 4000.0f);
        drag("説明画像高さ##tutorial_window", ui.contentImageHeight, 1.0f, 10.0f, 3000.0f);
        drag("タイトルX##tutorial_window", ui.titleX, 1.0f, -2000.0f, 4000.0f);
        drag("タイトルY##tutorial_window", ui.titleY, 1.0f, -2000.0f, 3000.0f);
        drag("本文X##tutorial_window", ui.bodyX, 1.0f, -2000.0f, 4000.0f);
        drag("本文Y##tutorial_window", ui.bodyY, 1.0f, -2000.0f, 3000.0f);
        drag("本文幅##tutorial_window", ui.bodyWidth, 1.0f, 10.0f, 4000.0f);
        drag("本文行間##tutorial_window", ui.bodyLineSpacing, 1.0f, 8.0f, 240.0f);
        drag("確認X##tutorial_window", ui.promptX, 1.0f, -2000.0f, 4000.0f);
        drag("確認Y##tutorial_window", ui.promptY, 1.0f, -2000.0f, 3000.0f);
        drag("タイトル文字サイズ##tutorial_window", ui.titleFontSize, 1.0f, 8.0f, 160.0f);
        drag("本文文字サイズ##tutorial_window", ui.bodyFontSize, 1.0f, 8.0f, 160.0f);
        drag("確認文字サイズ##tutorial_window", ui.promptFontSize, 1.0f, 8.0f, 160.0f);
        dragLayer("外枠レイヤー##tutorial_window", ui.frameLayer);
        dragLayer("見出し画像レイヤー##tutorial_window", ui.headingLayer);
        dragLayer("説明画像レイヤー##tutorial_window", ui.contentImageLayer);
        dragLayer("タイトルレイヤー##tutorial_window", ui.titleLayer);
        dragLayer("本文レイヤー##tutorial_window", ui.bodyLayer);
        dragLayer("確認レイヤー##tutorial_window", ui.promptLayer);
        ImGui::TreePop();
    }
}
