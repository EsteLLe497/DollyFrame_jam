#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>
#include <array>
#include <sstream>

#include "DxLib.h"
#include "imgui_layer.h"
#include "misc/cpp/imgui_stdlib.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"
#include "tutorial_ui_controls.h"

using namespace game_scene_detail;

namespace
{
    constexpr const char* kTutorialCsvPath = "assets/tutorials/tutorials.csv";
    constexpr int kTutorialTriggerWidthTiles = 3;

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

    const TutorialPageData* findTutorialPage(
        const GameSceneTutorialState& tutorial,
        TutorialPageType pageType)
    {
        const auto page = std::find_if(
            tutorial.pages.begin(),
            tutorial.pages.end(),
            [pageType](const TutorialPageData& value)
            {
                return value.type == pageType;
            });
        return page != tutorial.pages.end() ? &(*page) : nullptr;
    }

    const TutorialPageData* getCurrentTutorialPage(const GameSceneTutorialState& tutorial)
    {
        if (tutorial.currentPageIndex >= tutorial.pages.size())
        {
            return nullptr;
        }
        return &tutorial.pages[tutorial.currentPageIndex];
    }

    size_t getTutorialPageIndex(
        const GameSceneTutorialState& tutorial,
        const TutorialPageData* page)
    {
        if (!page || tutorial.pages.empty())
        {
            return tutorial.pages.size();
        }
        return static_cast<size_t>(page - tutorial.pages.data());
    }

    size_t countTutorialWindowPages(const GameSceneTutorialState& tutorial)
    {
        return static_cast<size_t>(std::count_if(
            tutorial.pages.begin(),
            tutorial.pages.end(),
            [](const TutorialPageData& page)
            {
                return page.type == TutorialPageType::Window;
            }));
    }

    bool findPreviousTutorialWindowIndex(
        const GameSceneTutorialState& tutorial,
        size_t currentIndex,
        size_t& outIndex)
    {
        for (size_t index = currentIndex; index > 0; --index)
        {
            if (tutorial.pages[index - 1].type == TutorialPageType::Window)
            {
                outIndex = index - 1;
                return true;
            }
        }
        return false;
    }

    bool findNextTutorialWindowIndex(
        const GameSceneTutorialState& tutorial,
        size_t currentIndex,
        size_t& outIndex)
    {
        for (size_t index = currentIndex + 1; index < tutorial.pages.size(); ++index)
        {
            if (tutorial.pages[index].type == TutorialPageType::Window)
            {
                outIndex = index;
                return true;
            }
        }
        return false;
    }

    void applyCurrentTutorialPage(GameSceneTutorialState& tutorial)
    {
        const TutorialPageData* page = getCurrentTutorialPage(tutorial);
        if (!page)
        {
            tutorial.phase = TutorialPresentationPhase::Inactive;
            return;
        }

        tutorial.dialogueFadeElapsed = 0.0f;
        tutorial.dialogueRevealElapsed = 0.0f;
        tutorial.phase = page->type == TutorialPageType::Conversation
            ? TutorialPresentationPhase::Conversation
            : TutorialPresentationPhase::TutorialWindow;
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

    // =========================================================
    // 元画像の縦横比を維持して指定範囲の中央へ描画
    // =========================================================
    void drawTutorialImageAspectFit(
        int textureId,
        float x,
        float y,
        float maxWidth,
        float maxHeight,
        float alpha = 1.0f)
    {
        if (textureId < 0 || maxWidth <= 0.0f || maxHeight <= 0.0f)
        {
            return;
        }

        const int textureWidth = TextureGetWidth(textureId);
        const int textureHeight = TextureGetHeight(textureId);
        if (textureWidth <= 0 || textureHeight <= 0)
        {
            return;
        }

        const float scale = std::min(
            maxWidth / static_cast<float>(textureWidth),
            maxHeight / static_cast<float>(textureHeight));
        const float drawWidth = static_cast<float>(textureWidth) * scale;
        const float drawHeight = static_cast<float>(textureHeight) * scale;
        const float drawX = x + (maxWidth - drawWidth) * 0.5f;
        const float drawY = y + (maxHeight - drawHeight) * 0.5f;
        DrawTutorialImage(textureId, drawX, drawY, drawWidth, drawHeight, alpha);
    }

    // =========================================================
    // チュートリアル背景を元画像の比率を維持して描画
    // =========================================================
    void DrawTutorialFrame(int textureId, float x, float y, float width, float height)
    {
        drawTutorialImageAspectFit(textureId, x, y, width, height);
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
        const TutorialPageData* page = getCurrentTutorialPage(m_tutorial);
        if (!page)
        {
            CompleteCameraTutorial();
            return true;
        }
        const auto& ui = m_ui.tuning.tutorial;
        const float fadeDuration = std::max(0.01f, ui.dialogueFadeDuration);
        m_tutorial.dialogueFadeElapsed += std::max(0.0f, deltaTime);
        if (m_tutorial.dialogueFadeElapsed >= fadeDuration)
        {
            m_tutorial.dialogueRevealElapsed += std::max(0.0f, deltaTime);
        }

        const size_t totalCharacters = CountUtf8Characters(page->text);
        const size_t revealedCharacters = static_cast<size_t>(std::floor(
            m_tutorial.dialogueRevealElapsed *
            std::max(1.0f, ui.dialogueCharactersPerSecond)));
        const bool textFinished = revealedCharacters >= totalCharacters;
        if (textFinished &&
            (Input_IsActionPressed(InputAction::Confirm) ||
                Input_IsSouthButtonPressed() ||
                Input_IsMouseLeftPressed()))
        {
            ++m_tutorial.currentPageIndex;
            if (m_tutorial.currentPageIndex >= m_tutorial.pages.size())
            {
                CompleteCameraTutorial();
            }
            else
            {
                applyCurrentTutorialPage(m_tutorial);
            }
        }
        return true;
    }

    const auto& ui = m_ui.tuning.tutorial;
    size_t previousWindowIndex = 0;
    size_t nextWindowIndex = 0;
    const bool hasPreviousWindow = findPreviousTutorialWindowIndex(
        m_tutorial,
        m_tutorial.currentPageIndex,
        previousWindowIndex);
    const bool hasNextWindow = findNextTutorialWindowIndex(
        m_tutorial,
        m_tutorial.currentPageIndex,
        nextWindowIndex);
    const bool hasMultipleWindows = countTutorialWindowPages(m_tutorial) > 1;
    const bool mousePressed = Input_IsMouseLeftPressed();
    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const bool previousClicked =
        mousePressed &&
        hasMultipleWindows &&
        hasPreviousWindow &&
        tutorialButtonContainsPoint(getTutorialNavigationButtonRect(ui, true), mouseX, mouseY);
    const bool nextClicked =
        mousePressed &&
        hasMultipleWindows &&
        hasNextWindow &&
        tutorialButtonContainsPoint(getTutorialNavigationButtonRect(ui, false), mouseX, mouseY);

    if ((Input_IsActionPressed(InputAction::MoveLeft) || previousClicked) && hasPreviousWindow)
    {
        m_tutorial.currentPageIndex = previousWindowIndex;
        applyCurrentTutorialPage(m_tutorial);
        return true;
    }
    if ((Input_IsActionPressed(InputAction::MoveRight) || nextClicked) && hasNextWindow)
    {
        m_tutorial.currentPageIndex = nextWindowIndex;
        applyCurrentTutorialPage(m_tutorial);
        return true;
    }

    const bool closeClicked =
        mousePressed &&
        !hasNextWindow &&
        tutorialButtonContainsPoint(getTutorialCloseButtonRect(ui), mouseX, mouseY);
    if (!hasNextWindow &&
        (Input_IsActionPressed(InputAction::Confirm) ||
            Input_IsSouthButtonPressed() ||
            closeClicked))
    {
        CompleteCameraTutorial();
    }
    return true;
}

bool GameScene::loadTutorialData(int tutorialNumber)
{
    const int normalizedNumber = std::clamp(tutorialNumber, 1, 99);
    const std::string tutorialId = "tutorial_" + std::to_string(normalizedNumber);
    std::vector<TutorialPageData> pages;
    if (!loadTutorialPagesFromCsv(kTutorialCsvPath, tutorialId, pages))
    {
        return false;
    }

    releaseTutorialVideo(m_tutorial.videoPlayer);
    m_tutorial.pages = std::move(pages);
    m_tutorial.loadedTutorialNumber = normalizedNumber;
    m_tutorial.currentPageIndex = 0;
    if (m_tutorial.phase != TutorialPresentationPhase::Inactive)
    {
        applyCurrentTutorialPage(m_tutorial);
    }
    return true;
}

bool GameScene::beginTutorialConversation(int tutorialNumber)
{
    const int normalizedNumber = std::clamp(tutorialNumber, 1, 99);
    if ((m_tutorial.pages.empty() ||
            m_tutorial.loadedTutorialNumber != normalizedNumber) &&
        !loadTutorialData(normalizedNumber))
    {
        return false;
    }
    releaseTutorialVideo(m_tutorial.videoPlayer);
    m_tutorial.activeTutorialNumber = normalizedNumber;
    m_tutorial.previewConversation = false;
    m_tutorial.previewWindow = false;
    m_tutorial.currentPageIndex = 0;
    applyCurrentTutorialPage(m_tutorial);
    return true;
}

void GameScene::EnsureTutorialPortraitTexture()
{
    const TutorialPageData* page =
        m_tutorial.phase == TutorialPresentationPhase::Conversation
        ? getCurrentTutorialPage(m_tutorial)
        : findTutorialPage(m_tutorial, TutorialPageType::Conversation);
    const std::string portraitPath = page ? page->portraitPath : std::string{};
    if (m_tutorial.loadedPortraitPath != portraitPath)
    {
        m_tutorial.portraitTextureId = -1;
        m_tutorial.loadedPortraitPath = portraitPath;
    }

    if (m_tutorial.portraitTextureId < 0 && !portraitPath.empty())
    {
        // SpriteDrawが扱う独自テクスチャIDへ変換し、リソースキャッシュで再利用します。
        m_tutorial.portraitTextureId = m_assets.getTextureByPath(portraitPath);
    }
}

void GameScene::TryStartCameraTutorial()
{
    if (m_tutorial.phase != TutorialPresentationPhase::Inactive)
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

            const int tutorialNumber = std::clamp(
                std::max(1, m_tileMap.GetMarkerParameter(column, row)),
                1,
                99);
            if (gameSessionIsTutorialCompleted(tutorialNumber))
            {
                continue;
            }

            if (IntersectsTutorialTrigger(
                    *transform,
                    column,
                    row,
                    kTutorialTriggerWidthTiles,
                    tileSize))
            {
                if (beginTutorialConversation(tutorialNumber))
                {
                    return;
                }
            }
        }
    }
}

void GameScene::CompleteCameraTutorial()
{
    const int completedTutorialNumber = m_tutorial.activeTutorialNumber;
    releaseTutorialVideo(m_tutorial.videoPlayer);
    m_tutorial.previewConversation = false;
    m_tutorial.previewWindow = false;
    m_tutorial.currentPageIndex = 0;
    m_tutorial.phase = TutorialPresentationPhase::Inactive;
    m_tutorial.activeTutorialNumber = 0;
    gameSessionSetTutorialCompleted(completedTutorialNumber, true);
    SaveProgressState();
}

void GameScene::DrawTutorialOverlay()
{
    const TutorialPageData* conversationPage =
        m_tutorial.phase == TutorialPresentationPhase::Conversation
        ? getCurrentTutorialPage(m_tutorial)
        : findTutorialPage(m_tutorial, TutorialPageType::Conversation);
    const TutorialPageData* windowPage =
        m_tutorial.phase == TutorialPresentationPhase::TutorialWindow
        ? getCurrentTutorialPage(m_tutorial)
        : findTutorialPage(m_tutorial, TutorialPageType::Window);
    const bool drawConversation =
        conversationPage &&
        (m_tutorial.phase == TutorialPresentationPhase::Conversation ||
            m_tutorial.previewConversation);
    const bool drawWindow =
        windowPage &&
        (m_tutorial.phase == TutorialPresentationPhase::TutorialWindow ||
            m_tutorial.previewWindow);
    if (!drawConversation && !drawWindow)
    {
        return;
    }

    const auto& ui = m_ui.tuning.tutorial;
    const int frameTexture = m_assets.GetTexture("tutorial_frame_window");
    const int headingTexture = m_assets.GetTexture("tutorial_heading");
    const std::string contentTextureKey =
        windowPage && !windowPage->contentTextureKey.empty()
        ? windowPage->contentTextureKey
        : "tutorial_content_image";
    const int contentImageTexture = m_assets.GetTexture(contentTextureKey);
    const int textBoxTexture = m_assets.GetTexture("tutorial_text_box");

    if (drawConversation)
    {
        EnsureTutorialPortraitTexture();
        const bool preview = m_tutorial.previewConversation &&
            m_tutorial.phase != TutorialPresentationPhase::Conversation;
        const float fadeProgress = preview
            ? 1.0f
            : std::clamp(
                m_tutorial.dialogueFadeElapsed / std::max(0.01f, ui.dialogueFadeDuration),
                0.0f,
                1.0f);
        // 線形補間より開始と終了が柔らかいスムーズステップで表示します。
        const float fadeAlpha =
            fadeProgress * fadeProgress * (3.0f - 2.0f * fadeProgress);
        const size_t totalCharacters = CountUtf8Characters(conversationPage->text);
        const size_t revealedCharacters = preview
            ? totalCharacters
            : static_cast<size_t>(std::floor(
                m_tutorial.dialogueRevealElapsed *
                std::max(1.0f, ui.dialogueCharactersPerSecond)));
        const bool textFinished = revealedCharacters >= totalCharacters;
        const std::string visibleDialogue = Utf8Prefix(conversationPage->text, revealedCharacters);

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
                SetDrawBlendMode(
                    DX_BLENDMODE_ALPHA,
                    static_cast<int>(std::round(
                        std::clamp(ui.dimAlpha * fadeAlpha, 0.0f, 1.0f) * 255.0f)));
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
                    conversationPage->speaker,
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
                    drawTutorialWaitIcon(
                        ui.dialoguePromptX,
                        ui.dialoguePromptY,
                        fadeAlpha);
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
                drawTutorialImageAspectFit(
                    headingTexture,
                    ui.headingX,
                    ui.headingY,
                    ui.headingWidth,
                    ui.headingHeight);
                break;
            case TutorialDrawKind::ContentImage:
                if (prepareTutorialVideo(m_tutorial.videoPlayer, windowPage->contentVideoPath))
                {
                    drawTutorialVideo(
                        m_tutorial.videoPlayer,
                        ui.contentImageX,
                        ui.contentImageY,
                        ui.contentImageWidth,
                        ui.contentImageHeight);
                }
                else
                {
                    DrawTutorialImage(
                        contentImageTexture,
                        ui.contentImageX,
                        ui.contentImageY,
                        ui.contentImageWidth,
                        ui.contentImageHeight);
                }
                break;
            case TutorialDrawKind::Title:
                DrawTutorialText(
                    windowPage->title,
                    ui.titleX,
                    ui.titleY,
                    ui.titleFontSize,
                    ui.titleFontSize,
                    GetColor(78, 48, 22));
                break;
            case TutorialDrawKind::Body:
                DrawTutorialText(
                    windowPage->text,
                    ui.bodyX,
                    ui.bodyY,
                    ui.bodyFontSize,
                    ui.bodyLineSpacing,
                    GetColor(64, 46, 30),
                    ui.bodyWidth);
                break;
            case TutorialDrawKind::Prompt:
            {
                const size_t windowPageIndex = getTutorialPageIndex(m_tutorial, windowPage);
                size_t previousWindowIndex = 0;
                size_t nextWindowIndex = 0;
                const bool hasPreviousWindow = findPreviousTutorialWindowIndex(
                    m_tutorial,
                    windowPageIndex,
                    previousWindowIndex);
                const bool hasNextWindow = findNextTutorialWindowIndex(
                    m_tutorial,
                    windowPageIndex,
                    nextWindowIndex);
                const bool hasMultipleWindows = countTutorialWindowPages(m_tutorial) > 1;

                if (hasMultipleWindows)
                {
                    drawTutorialNavigationButton(
                        getTutorialNavigationButtonRect(ui, true),
                        true,
                        hasPreviousWindow);
                    drawTutorialNavigationButton(
                        getTutorialNavigationButtonRect(ui, false),
                        false,
                        hasNextWindow);
                }
                if (!hasNextWindow)
                {
                    drawTutorialCloseButton(
                        getTutorialCloseButtonRect(ui),
                        windowPage->confirmText.empty() ? "閉じる" : windowPage->confirmText,
                        ui.promptFontSize);
                }
                break;
            }
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
    if (ImGui::Button("チュートリアル1を再生##tutorial"))
    {
        beginTutorialConversation(1);
    }
    ImGui::SameLine();
    if (ImGui::Button("1番の完了フラグを解除##tutorial"))
    {
        GameSession_SetCameraTutorialCompleted(false);
        SaveProgressState();
    }
    ImGui::Text(
        "チュートリアル1: %s",
        gameSessionIsTutorialCompleted(1) ? "完了" : "未完了");

    ImGui::SliderFloat("背景暗転##tutorial", &ui.dimAlpha, 0.0f, 1.0f, "%.2f");
    ImGui::Text("表示内容: %s", kTutorialCsvPath);
    ImGui::Text("読込ページ数: %zu", m_tutorial.pages.size());
    if (ImGui::Button("CSVを再読込##tutorial"))
    {
        loadTutorialData(std::max(1, m_tutorial.loadedTutorialNumber));
    }

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
        drag("待機アイコンX##tutorial_dialogue", ui.dialoguePromptX, 1.0f, -2000.0f, 4000.0f);
        drag("待機アイコンY##tutorial_dialogue", ui.dialoguePromptY, 1.0f, -2000.0f, 3000.0f);
        drag("話者文字サイズ##tutorial_dialogue", ui.dialogueNameFontSize, 1.0f, 8.0f, 160.0f);
        drag("本文文字サイズ##tutorial_dialogue", ui.dialogueTextFontSize, 1.0f, 8.0f, 160.0f);
        drag("本文行間##tutorial_dialogue", ui.dialogueLineSpacing, 1.0f, 8.0f, 240.0f);
        drag("フェード秒数##tutorial_dialogue", ui.dialogueFadeDuration, 0.01f, 0.01f, 5.0f);
        drag("1秒あたり文字数##tutorial_dialogue", ui.dialogueCharactersPerSecond, 1.0f, 1.0f, 240.0f);
        dragLayer("枠レイヤー##tutorial_dialogue", ui.dialogueBoxLayer);
        dragLayer("立ち絵レイヤー##tutorial_dialogue", ui.dialoguePortraitLayer);
        dragLayer("話者レイヤー##tutorial_dialogue", ui.dialogueNameLayer);
        dragLayer("本文レイヤー##tutorial_dialogue", ui.dialogueTextLayer);
        dragLayer("待機アイコンレイヤー##tutorial_dialogue", ui.dialoguePromptLayer);
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
        drag("下部ボタンY##tutorial_window", ui.promptY, 1.0f, -2000.0f, 3000.0f);
        drag("タイトル文字サイズ##tutorial_window", ui.titleFontSize, 1.0f, 8.0f, 160.0f);
        drag("本文文字サイズ##tutorial_window", ui.bodyFontSize, 1.0f, 8.0f, 160.0f);
        drag("閉じる文字サイズ##tutorial_window", ui.promptFontSize, 1.0f, 8.0f, 160.0f);
        dragLayer("外枠レイヤー##tutorial_window", ui.frameLayer);
        dragLayer("見出し画像レイヤー##tutorial_window", ui.headingLayer);
        dragLayer("説明画像レイヤー##tutorial_window", ui.contentImageLayer);
        dragLayer("タイトルレイヤー##tutorial_window", ui.titleLayer);
        dragLayer("本文レイヤー##tutorial_window", ui.bodyLayer);
        dragLayer("下部ボタンレイヤー##tutorial_window", ui.promptLayer);
        ImGui::TreePop();
    }
}
