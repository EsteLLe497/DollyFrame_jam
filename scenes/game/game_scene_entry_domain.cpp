#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace game_scene_detail;

namespace
{
    constexpr int kLoadingDotCount = 10;

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
            DrawWorldAndUiLayers();
            ResetFrameRendering();
            --m_lifecycle.loadingWarmupFramesRemaining;
        }

        DrawLoadingScreen();
        if (m_lifecycle.loadingFinished && m_lifecycle.loadingWarmupFramesRemaining <= 0)
        {
            m_lifecycle.loadingActive = false;
        }
        return;
    }

    PrepareFrameRendering();
    DrawWorldAndUiLayers();
    ResetFrameRendering();
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

