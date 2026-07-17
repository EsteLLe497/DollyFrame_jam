#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace game_scene_detail;

namespace
{
    constexpr float kResultTransitionDuration = 0.95f;

    const char* GetLoadingStepLabel(int step)
    {
        switch (step)
        {
        case 0:
            return "TUNING";
        case 1:
            return "RESOURCES";
        case 2:
            return "STAGE";
        case 3:
            return "TEXTURES";
        case 4:
            return "FINALIZE";
        case 5:
        case 6:
            return "READY";
        default:
            return "LOADING";
        }
    }
}

const char* GameScene::GetSceneId() const
{
    return "game";
}

void GameScene::Update(float deltaTime)
{
    ZoneScoped;
    const ActiveGameSceneScope activeScene(*this);

    if (m_lifecycle.loadingActive)
    {
        UpdateLoading(deltaTime);
        return;
    }

    if (m_flow.resultQueued)
    {
        UpdateResultTransition(deltaTime);
        return;
    }

    BeginFrameUpdate(deltaTime);
    if (TryHandleModalUpdates(deltaTime))
    {
        return;
    }

    const float effectiveGameplayDeltaTime = PrepareGameplayDeltaTime(deltaTime);
    TickEntities(effectiveGameplayDeltaTime);
    FinalizeGameplayFrame(effectiveGameplayDeltaTime);
}

void GameScene::Draw()
{
    const ActiveGameSceneScope activeScene(*this);
    if (m_lifecycle.loadingActive)
    {
        if (m_lifecycle.loadingFinished && m_lifecycle.loadingWarmupFramesRemaining > 0)
        {
            PrepareFrameRendering();
            UpdatePostProcessPlayerLight();
            DrawWorldAndUiLayers();
            ResetFrameRendering();
            --m_lifecycle.loadingWarmupFramesRemaining;
        }
        else
        {
            DirectXSetPostProcessPlayerLight(
                static_cast<float>(kVirtualScreenWidth) * 0.5f,
                static_cast<float>(kVirtualScreenHeight) * 0.5f,
                0.0f,
                120.0f,
                170.0f);
            DirectXCompositeSceneToBackBuffer(static_cast<float>(GetNowCount()) * 0.001f);
        }

        DrawLoadingScreen();
        if (m_lifecycle.loadingFinished && m_lifecycle.loadingWarmupFramesRemaining <= 0)
        {
            m_lifecycle.loadingActive = false;
        }
        return;
    }

    PrepareFrameRendering();
    UpdatePostProcessPlayerLight();
    DrawWorldAndUiLayers();
    ResetFrameRendering();
    DrawResultTransitionOverlay();
}

void GameScene::UpdateResultTransition(float deltaTime)
{
    m_flow.resultTransitionTimer += deltaTime;
    if (!m_flow.resultTransitionSceneRequested && m_flow.resultTransitionTimer >= kResultTransitionDuration)
    {
        m_flow.resultTransitionSceneRequested = true;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "result", 0.0f, 0.0f });
    }
}

void GameScene::DrawResultTransitionOverlay() const
{
    if (!m_flow.resultQueued)
    {
        return;
    }

    const float rawT = std::clamp(m_flow.resultTransitionTimer / kResultTransitionDuration, 0.0f, 1.0f);
    const float invT = 1.0f - rawT;
    const float t = 1.0f - invT * invT * invT * invT * invT;
    const int screenW = SCREEN_WIDTH;
    const int screenH = SCREEN_HEIGHT;
    const int dimAlpha = std::clamp(static_cast<int>(std::round(36.0f + t * 116.0f)), 0, 170);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, dimAlpha);
    DrawBox(0, 0, screenW, screenH, GetColor(0, 0, 0), TRUE);

    constexpr int filmHeight = 132;
    constexpr int filmRailHeight = 28;
    constexpr int filmHoleWidth = 20;
    constexpr int filmHoleHeight = 16;
    constexpr int filmHoleGap = 34;
    constexpr int filmFrameWidth = 270;
    const int filmLength = screenW + 520;
    const int filmTravel = screenW + 760;
    const int filmLeft = static_cast<int>(std::round(static_cast<float>(screenW + 260) - t * static_cast<float>(filmTravel)));
    const int topFilmY = 36;
    const int bottomFilmY = screenH - 36 - filmHeight;
    const int filmColor = GetColor(12, 12, 12);
    const int frameColor = GetColor(248, 248, 248);
    const int dividerColor = GetColor(10, 10, 10);
    const int holeColor = GetColor(248, 248, 248);

    const auto drawFilmStrip = [&](int y, int phaseOffset)
    {
        const int left = filmLeft + phaseOffset;
        const int right = left + filmLength;
        if (right < -120 || left > screenW + 120)
        {
            return;
        }

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 248);
        DrawBox(left, y, right, y + filmHeight, filmColor, TRUE);

        const int firstHoleX = left + 18 + (static_cast<int>(std::round(rawT * 320.0f)) % filmHoleGap);
        for (int x = firstHoleX - filmHoleGap * 2; x < right + filmHoleGap; x += filmHoleGap)
        {
            DrawBox(x, y + 6, x + filmHoleWidth, y + 6 + filmHoleHeight, holeColor, TRUE);
            DrawBox(x, y + filmHeight - 6 - filmHoleHeight, x + filmHoleWidth, y + filmHeight - 6, holeColor, TRUE);
        }

        const int frameTop = y + filmRailHeight + 4;
        const int frameBottom = y + filmHeight - filmRailHeight - 4;
        for (int frameX = left; frameX < right; frameX += filmFrameWidth)
        {
            DrawBox(frameX + 8, frameTop + 6, frameX + filmFrameWidth - 8, frameBottom - 6, frameColor, TRUE);
            DrawBox(frameX + filmFrameWidth - 3, frameTop, frameX + filmFrameWidth + 3, frameBottom, dividerColor, TRUE);
        }

        DrawBox(left, y + filmRailHeight, right, y + filmRailHeight + 4, dividerColor, TRUE);
        DrawBox(left, y + filmHeight - filmRailHeight - 4, right, y + filmHeight - filmRailHeight, dividerColor, TRUE);
    };

    drawFilmStrip(topFilmY, 0);
    drawFilmStrip(bottomFilmY, 180);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void GameScene::UpdatePostProcessPlayerLight() const
{
    if (!m_lifecycle.forestStageEnabled || m_lifecycle.loadingActive)
    {
        DirectXSetPostProcessPlayerLight(
            static_cast<float>(kVirtualScreenWidth) * 0.5f,
            static_cast<float>(kVirtualScreenHeight) * 0.5f,
            0.0f,
            120.0f,
            170.0f);
        return;
    }

    const Entity* player = FindEntityByTag(kTagPlayer);
    const auto* transform = player ? player->GetComponent<TransformComponent>() : nullptr;
    const auto* sprite = player ? player->GetComponent<SpriteRenderComponent>() : nullptr;
    if (!transform || !sprite)
    {
        DirectXSetPostProcessPlayerLight(
            static_cast<float>(kVirtualScreenWidth) * 0.5f,
            static_cast<float>(kVirtualScreenHeight) * 0.5f,
            0.0f,
            120.0f,
            170.0f);
        return;
    }

    const float viewScale = GetViewScale();
    const float playerCenterX =
        transform->x +
        sprite->GetRenderOffsetX() +
        transform->width * transform->scale * sprite->GetRenderScaleX() * 0.5f;
    const float playerCenterY =
        transform->y +
        sprite->GetRenderOffsetY() +
        transform->height * transform->scale * sprite->GetRenderScaleY() * 0.5f;
    const float screenX = GetViewOriginX() + (playerCenterX - m_flow.cameraX) * viewScale;
    const float screenY = GetViewOriginY() + (playerCenterY - m_flow.cameraY) * viewScale;

    DirectXSetPostProcessPlayerLight(screenX, screenY, 1.0f, 22.0f, 156.0f);
}

void GameScene::DrawLoadingScreen() const
{
    const float pulse = 0.5f + 0.5f * std::sin(m_lifecycle.loadingElapsed * 3.0f);
    const float displayedProgress = m_lifecycle.loadingFinished
        ? 1.0f
        : std::clamp(m_lifecycle.loadingProgress + pulse * 0.025f, 0.0f, 0.985f);
    const int centerX = SCREEN_WIDTH / 2;
    const int centerY = SCREEN_HEIGHT / 2;

    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(16, 11, 9), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(28.0f + pulse * 18.0f));
    DrawCircleAA(static_cast<float>(centerX), static_cast<float>(centerY), 520.0f, 64, GetColor(132, 76, 38), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    constexpr int stripHeight = 326;
    constexpr int stripTop = 184;
    constexpr int railHeight = 44;
    constexpr int frameWidth = 250;
    constexpr int frameHeight = 206;
    constexpr int frameGap = 18;
    constexpr int framePitch = frameWidth + frameGap;
    constexpr int holeGap = 42;

    constexpr float stepsPerSecond = 1.7f;
    const float rawStep = m_lifecycle.loadingElapsed * stepsPerSecond;
    const int completedSteps = static_cast<int>(std::floor(rawStep));
    const float stepPhase = rawStep - static_cast<float>(completedSteps);
    const float advancePhase = std::clamp(stepPhase / 0.22f, 0.0f, 1.0f);
    const float easedAdvance = 1.0f - std::pow(1.0f - advancePhase, 3.0f);
    const float travel = (static_cast<float>(completedSteps) + easedAdvance) * static_cast<float>(framePitch);
    const int travelWithinPitch = static_cast<int>(std::round(travel)) % framePitch;
    const int firstFrameNumber = completedSteps + (advancePhase >= 0.999f ? 1 : 0);
    const int stripLeft = -framePitch - travelWithinPitch;

    DrawBox(0, stripTop, SCREEN_WIDTH, stripTop + stripHeight, GetColor(8, 8, 8), TRUE);
    DrawBox(0, stripTop + railHeight, SCREEN_WIDTH, stripTop + railHeight + 3, GetColor(74, 64, 53), TRUE);
    DrawBox(0, stripTop + stripHeight - railHeight - 3, SCREEN_WIDTH, stripTop + stripHeight - railHeight, GetColor(74, 64, 53), TRUE);

    const int holeOffset = travelWithinPitch % holeGap;
    for (int x = -holeGap - holeOffset; x < SCREEN_WIDTH + holeGap; x += holeGap)
    {
        DrawBox(x, stripTop + 10, x + 24, stripTop + 26, GetColor(211, 188, 150), TRUE);
        DrawBox(x, stripTop + stripHeight - 26, x + 24, stripTop + stripHeight - 10, GetColor(211, 188, 150), TRUE);
    }

    const char* textureKeys[3] = {};
    if (m_lifecycle.ruinsStageEnabled)
    {
        textureKeys[0] = "ruins_bg";
        textureKeys[1] = "ruins_layer2";
        textureKeys[2] = "ruins_layer3";
    }
    else
    {
        textureKeys[0] = "forest_bg";
        textureKeys[1] = "forest1_bg";
        textureKeys[2] = "forest2_bg";
    }

    const int frameTop = stripTop + railHeight + 16;
    for (int index = 0; index < 8; ++index)
    {
        const int frameLeft = stripLeft + index * framePitch;
        const int frameRight = frameLeft + frameWidth;
        if (frameRight < 0 || frameLeft > SCREEN_WIDTH)
        {
            continue;
        }

        const int sequenceNumber = firstFrameNumber + index - 1;
        const int textureIndex = ((sequenceNumber % 3) + 3) % 3;
        const int texture = m_assets.GetTexture(textureKeys[textureIndex]);
        DrawBox(frameLeft - 5, frameTop - 5, frameRight + 5, frameTop + frameHeight + 5, GetColor(190, 170, 139), TRUE);
        DrawBox(frameLeft, frameTop, frameRight, frameTop + frameHeight, GetColor(51, 40, 34), TRUE);
        if (texture >= 0)
        {
            Shader_ResetStyle();
            const float exposure = 0.92f + 0.08f * std::sin(m_lifecycle.loadingElapsed * 5.0f + static_cast<float>(index));
            Shader_SetTint(exposure, exposure * 0.94f, exposure * 0.82f, 1.0f);
            SpriteDraw(texture,
                static_cast<float>(frameLeft),
                static_cast<float>(frameTop),
                static_cast<float>(frameWidth),
                static_cast<float>(frameHeight),
                0.0f, 0.0f, 1.0f, 1.0f);
            Shader_ResetStyle();
        }
        DrawBox(frameLeft, frameTop, frameRight, frameTop + frameHeight, GetColor(228, 207, 172), FALSE);
    }

    const int gateLeft = centerX - frameWidth / 2 - 12;
    const int gateRight = centerX + frameWidth / 2 + 12;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 46);
    DrawBox(gateLeft, stripTop + railHeight + 4, gateRight, stripTop + stripHeight - railHeight - 4, GetColor(255, 205, 112), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(gateLeft, stripTop + railHeight + 4, gateRight, stripTop + stripHeight - railHeight - 4, GetColor(255, 220, 142), FALSE);

    const char* loadingText = m_lifecycle.loadingFinished
        ? "旅の記録を読み込みました"
        : "旅の記録を読み込んでいます…";
    const int textWidth = GetDrawStringWidth(loadingText, -1);
    DrawString(centerX - textWidth / 2, 104, loadingText, GetColor(239, 220, 186));

    constexpr int progressWidth = 420;
    const int progressLeft = centerX - progressWidth / 2;
    const int progressTop = SCREEN_HEIGHT - 76;
    DrawBox(progressLeft, progressTop, progressLeft + progressWidth, progressTop + 6, GetColor(92, 73, 56), TRUE);
    DrawBox(progressLeft, progressTop,
        progressLeft + static_cast<int>(std::round(progressWidth * displayedProgress)),
        progressTop + 6,
        GetColor(224, 174, 92),
        TRUE);

    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        char debugLabel[64]{};
        std::snprintf(debugLabel, sizeof(debugLabel), "LOADING %s  %d%%",
            GetLoadingStepLabel(m_lifecycle.loadingStep),
            static_cast<int>(std::round(displayedProgress * 100.0f)));
        DrawString(24, SCREEN_HEIGHT - 42, debugLabel, GetColor(166, 132, 102));
    }

    if (m_lifecycle.loadingFinished)
    {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 44);
        DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(255, 244, 220), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

EventBus* GameScene::GetEventBus()
{
    const ActiveGameSceneScope activeScene(*this);
    return &m_eventBus;
}

