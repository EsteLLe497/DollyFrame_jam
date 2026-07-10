#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace game_scene_detail;

namespace
{
    constexpr int kLoadingDotCount = 10;
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
        return "READY";
    case 4:
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
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(3, 5, 10), TRUE);

    const float pulse = 0.5f + 0.5f * std::sin(m_lifecycle.loadingElapsed * 4.0f);
    const int centerX = SCREEN_WIDTH / 2;
    const int centerY = SCREEN_HEIGHT / 2;
    const int textColor = GetColor(218, 232, 255);
    const int mutedColor = GetColor(106, 132, 168);
    const int barBackColor = GetColor(18, 28, 44);
    const int barFillColor = GetColor(92, 184, 255);

    for (int i = 0; i < kLoadingDotCount; ++i)
    {
        const float angle = m_lifecycle.loadingElapsed * 3.2f + static_cast<float>(i) * (6.2831853f / static_cast<float>(kLoadingDotCount));
        const float phase = 0.5f + 0.5f * std::sin(angle + m_lifecycle.loadingElapsed * 2.0f);
        const int alpha = std::clamp(static_cast<int>(72.0f + phase * 164.0f), 0, 255);
        const int x = centerX + static_cast<int>(std::round(std::cos(angle) * 44.0f));
        const int y = centerY - 54 + static_cast<int>(std::round(std::sin(angle) * 18.0f));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawCircleAA(static_cast<float>(x), static_cast<float>(y), 5.0f + phase * 2.0f, 24, barFillColor, TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    char label[64]{};
    std::snprintf(label, sizeof(label), "LOADING %s", GetLoadingStepLabel(m_lifecycle.loadingStep));
    DrawString(centerX - 92, centerY + 6, label, textColor);

    const int barWidth = 360;
    const int barHeight = 10;
    const int barLeft = centerX - barWidth / 2;
    const int barTop = centerY + 42;
    const float displayedProgress = m_lifecycle.loadingFinished
        ? 1.0f
        : std::clamp(m_lifecycle.loadingProgress + pulse * 0.025f, 0.0f, 0.985f);
    const int fillRight = barLeft + static_cast<int>(std::round(static_cast<float>(barWidth) * displayedProgress));
    DrawBox(barLeft, barTop, barLeft + barWidth, barTop + barHeight, barBackColor, TRUE);
    DrawBox(barLeft, barTop, fillRight, barTop + barHeight, barFillColor, TRUE);
    DrawBox(barLeft, barTop, barLeft + barWidth, barTop + barHeight, GetColor(74, 96, 128), FALSE);

    DrawString(centerX - 132, centerY + 68, "Preparing stage assets...", mutedColor);
}

EventBus* GameScene::GetEventBus()
{
    const ActiveGameSceneScope activeScene(*this);
    return &m_eventBus;
}

