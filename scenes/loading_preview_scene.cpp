#include "pch.h"

#include "loading_preview_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "directX.h"
#include "audio.h"
#include "imgui.h"
#include "input.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"

namespace
{
    constexpr float kPreviewDuration = 4.0f;
    constexpr int kPreviewModeCount = 3;
    constexpr int kStageCount = 3;

    float SmoothStep(float value)
    {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    void DrawCenteredText(int centerX, int y, const char* text, int color)
    {
        const int width = GetDrawStringWidth(text, -1);
        DrawString(centerX - width / 2, y, text, color);
    }
}

LoadingPreviewScene::LoadingPreviewScene()
    : m_whiteTexture(-1)
    , m_stageTextures{ -1, -1, -1 }
    , m_mode(0)
    , m_stageIndex(0)
    , m_elapsed(0.0f)
    , m_returnRequested(false)
{
}

const char* LoadingPreviewScene::GetSceneId() const
{
    return "loading_preview";
}

void LoadingPreviewScene::OnEnter(ResourceManager& resources)
{
    Audio_FadeOutBgm(1.2f);
    m_whiteTexture = resources.CreateSolidTexture(1, 1, 0xFFFFFFFF);
    m_stageTextures[0] = resources.LoadTexture(L"assets\\texture\\BG\\forest\\BG_Forest.png");
    m_stageTextures[1] = resources.LoadTexture(L"assets\\texture\\BG\\ruins\\ruins.png");
    m_stageTextures[2] = resources.LoadTexture(L"assets\\texture\\BG\\forest\\sinrin11.png");
    m_eventBus.Clear();
    m_mode = 0;
    m_stageIndex = 0;
    m_elapsed = 0.0f;
    m_returnRequested = false;
}

void LoadingPreviewScene::Update(float deltaTime)
{
    if (m_returnRequested)
    {
        m_returnRequested = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
        return;
    }

    m_elapsed += std::max(0.0f, deltaTime);
    if (m_elapsed >= kPreviewDuration)
    {
        m_elapsed = std::fmod(m_elapsed, kPreviewDuration);
    }

    if (Input_IsKeyPressed(VK_RIGHT) || Input_IsKeyPressed('E'))
    {
        m_mode = (m_mode + 1) % kPreviewModeCount;
        m_elapsed = 0.0f;
    }
    if (Input_IsKeyPressed(VK_LEFT) || Input_IsKeyPressed('Q'))
    {
        m_mode = (m_mode + kPreviewModeCount - 1) % kPreviewModeCount;
        m_elapsed = 0.0f;
    }
    if (Input_IsKeyPressed(VK_DOWN) || Input_IsKeyPressed('S'))
    {
        m_stageIndex = (m_stageIndex + 1) % kStageCount;
        m_elapsed = 0.0f;
    }
    if (Input_IsKeyPressed(VK_UP) || Input_IsKeyPressed('W'))
    {
        m_stageIndex = (m_stageIndex + kStageCount - 1) % kStageCount;
        m_elapsed = 0.0f;
    }
    if (Input_IsKeyPressed('R'))
    {
        m_elapsed = 0.0f;
    }
    if (Input_IsKeyPressed('T'))
    {
        ReturnToTitle();
    }
}

void LoadingPreviewScene::Draw()
{
    const float progress = SmoothStep(std::clamp(m_elapsed / (kPreviewDuration * 0.82f), 0.0f, 1.0f));
    if (m_mode == 0)
    {
        DrawDarkroomPreview(progress);
    }
    else if (m_mode == 1)
    {
        DrawViewfinderPreview(progress);
    }
    else
    {
        DrawFilmstripPreview(progress);
    }
}

void LoadingPreviewScene::DrawDarkroomPreview(float progress) const
{
    const int centerX = SCREEN_WIDTH / 2;
    const int centerY = SCREEN_HEIGHT / 2;
    const float pulse = 0.5f + 0.5f * std::sin(m_elapsed * 4.0f);

    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(19, 4, 7), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(30.0f + pulse * 20.0f));
    DrawCircleAA(static_cast<float>(centerX), 70.0f, 460.0f, 64, GetColor(142, 32, 28), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(0, 0, SCREEN_WIDTH, 96, GetColor(9, 2, 4), TRUE);
    DrawLine(0, 106, SCREEN_WIDTH, 82, GetColor(86, 52, 47));

    constexpr int photoWidth = 548;
    constexpr int photoHeight = 430;
    const int left = centerX - photoWidth / 2;
    const int top = centerY - photoHeight / 2 - 8;
    const int right = left + photoWidth;
    const int bottom = top + photoHeight;
    DrawBox(left + 14, top + 18, right + 14, bottom + 18, GetColor(3, 1, 2), TRUE);
    DrawBox(left, top, right, bottom, GetColor(239, 226, 198), TRUE);

    const int imageLeft = left + 30;
    const int imageTop = top + 28;
    const int imageRight = right - 30;
    const int imageBottom = top + 306;
    DrawBox(imageLeft, imageTop, imageRight, imageBottom, GetColor(45, 30, 28), TRUE);
    const int revealTop = imageBottom - static_cast<int>((imageBottom - imageTop) * progress);
    SetDrawArea(imageLeft, revealTop, imageRight, imageBottom);
    DrawBox(imageLeft, imageTop, imageRight, imageTop + 142, GetColor(154, 94, 67), TRUE);
    DrawBox(imageLeft, imageTop + 142, imageRight, imageBottom, GetColor(76, 53, 40), TRUE);
    DrawCircle(centerX + 116, imageTop + 74, 38, GetColor(218, 154, 96), TRUE);
    DrawCircle(imageLeft + 100, imageBottom - 35, 112, GetColor(43, 40, 29), TRUE);
    DrawCircle(imageRight - 94, imageBottom - 42, 128, GetColor(32, 31, 25), TRUE);
    DrawBox(imageLeft, imageBottom - 52, imageRight, imageBottom, GetColor(26, 25, 22), TRUE);
    SetDrawAreaFull();

    DrawCenteredText(centerX, bottom - 82,
        progress >= 0.995f ? "現像が完了しました" : "記憶を現像しています…",
        GetColor(67, 44, 37));
    const int barLeft = centerX - 195;
    const int barTop = bottom - 42;
    DrawBox(barLeft, barTop, barLeft + 390, barTop + 5, GetColor(190, 169, 139), TRUE);
    DrawBox(barLeft, barTop, barLeft + static_cast<int>(390.0f * progress), barTop + 5, GetColor(119, 46, 41), TRUE);

    DrawCenteredText(centerX, 24, "LOAD SCREEN PREVIEW  1/2 — DARKROOM", GetColor(206, 156, 128));
    DrawCenteredText(centerX, SCREEN_HEIGHT - 34, "Q/E・←/→: 案切替   R: 再生   T/Esc: 戻る", GetColor(190, 152, 132));
}

void LoadingPreviewScene::DrawViewfinderPreview(float progress) const
{
    const int texture = m_stageTextures[m_stageIndex];
    const float blur = (1.0f - progress) * 18.0f;
    const float sharpAlpha = 0.18f + progress * 0.82f;

    Shader_ResetStyle();
    Shader_SetTint(0.12f, 0.15f, 0.16f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
    if (texture >= 0)
    {
        const float offsets[][2] = {
            { -blur, 0.0f }, { blur, 0.0f }, { 0.0f, -blur }, { 0.0f, blur },
            { -blur * 0.7f, -blur * 0.7f }, { blur * 0.7f, blur * 0.7f }
        };
        for (const auto& offset : offsets)
        {
            Shader_SetTint(0.82f, 0.78f, 0.68f, (1.0f - progress) * 0.16f);
            SpriteDraw(texture, offset[0], offset[1], static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
        }
        Shader_SetTint(1.0f, 0.96f, 0.86f, sharpAlpha);
        SpriteDraw(texture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
    }
    Shader_ResetStyle();

    const int marginX = 82;
    const int marginY = 62;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 176);
    DrawBox(0, 0, SCREEN_WIDTH, marginY, GetColor(3, 5, 5), TRUE);
    DrawBox(0, SCREEN_HEIGHT - marginY, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(3, 5, 5), TRUE);
    DrawBox(0, marginY, marginX, SCREEN_HEIGHT - marginY, GetColor(3, 5, 5), TRUE);
    DrawBox(SCREEN_WIDTH - marginX, marginY, SCREEN_WIDTH, SCREEN_HEIGHT - marginY, GetColor(3, 5, 5), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    const int white = GetColor(234, 242, 226);
    const int corner = 54;
    DrawLine(marginX, marginY, marginX + corner, marginY, white);
    DrawLine(marginX, marginY, marginX, marginY + corner, white);
    DrawLine(SCREEN_WIDTH - marginX, marginY, SCREEN_WIDTH - marginX - corner, marginY, white);
    DrawLine(SCREEN_WIDTH - marginX, marginY, SCREEN_WIDTH - marginX, marginY + corner, white);
    DrawLine(marginX, SCREEN_HEIGHT - marginY, marginX + corner, SCREEN_HEIGHT - marginY, white);
    DrawLine(marginX, SCREEN_HEIGHT - marginY, marginX, SCREEN_HEIGHT - marginY - corner, white);
    DrawLine(SCREEN_WIDTH - marginX, SCREEN_HEIGHT - marginY, SCREEN_WIDTH - marginX - corner, SCREEN_HEIGHT - marginY, white);
    DrawLine(SCREEN_WIDTH - marginX, SCREEN_HEIGHT - marginY, SCREEN_WIDTH - marginX, SCREEN_HEIGHT - marginY - corner, white);

    const int centerX = SCREEN_WIDTH / 2;
    const int centerY = SCREEN_HEIGHT / 2;
    const int focusSize = static_cast<int>(34.0f + (1.0f - progress) * 126.0f);
    const int focusColor = progress > 0.92f ? GetColor(132, 240, 164) : white;
    DrawBox(centerX - focusSize, centerY - focusSize / 2, centerX + focusSize, centerY + focusSize / 2, focusColor, FALSE);
    DrawLine(centerX - 12, centerY, centerX + 12, centerY, focusColor);
    DrawLine(centerX, centerY - 12, centerX, centerY + 12, focusColor);

    const char* stageLabels[] = { "NEXT — FOREST", "NEXT — RUINS", "NEXT — DEEP FOREST" };
    DrawCenteredText(centerX, 24, "LOAD SCREEN PREVIEW  2/2 — VIEWFINDER", white);
    DrawString(marginX + 18, SCREEN_HEIGHT - marginY - 36, stageLabels[m_stageIndex], white);
    DrawFormatString(SCREEN_WIDTH - marginX - 154, SCREEN_HEIGHT - marginY - 36, focusColor,
        progress > 0.92f ? "FOCUS LOCK" : "FOCUS  %02d", static_cast<int>(progress * 100.0f));
    DrawCenteredText(centerX, SCREEN_HEIGHT - 34,
        "Q/E・←/→: 案切替   W/S・↑/↓: ステージ   R: 再生   T/Esc: 戻る",
        GetColor(202, 214, 204));
}

void LoadingPreviewScene::DrawFilmstripPreview(float progress) const
{
    static_cast<void>(progress);
    const int centerX = SCREEN_WIDTH / 2;
    const int centerY = SCREEN_HEIGHT / 2;
    const float pulse = 0.5f + 0.5f * std::sin(m_elapsed * 3.0f);

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
    constexpr int holeWidth = 24;
    constexpr int holeHeight = 16;
    constexpr int holeGap = 42;

    // 送る瞬間だけ加速し、その後は静止することで映写機らしいコマ送りにする。
    constexpr float stepsPerSecond = 1.7f;
    const float rawStep = m_elapsed * stepsPerSecond;
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
        DrawBox(x, stripTop + 10, x + holeWidth, stripTop + 10 + holeHeight, GetColor(211, 188, 150), TRUE);
        DrawBox(x, stripTop + stripHeight - 10 - holeHeight, x + holeWidth, stripTop + stripHeight - 10, GetColor(211, 188, 150), TRUE);
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
        const int textureIndex = ((sequenceNumber % kStageCount) + kStageCount) % kStageCount;
        const int texture = m_stageTextures[textureIndex];
        DrawBox(frameLeft - 5, frameTop - 5, frameRight + 5, frameTop + frameHeight + 5, GetColor(190, 170, 139), TRUE);
        DrawBox(frameLeft, frameTop, frameRight, frameTop + frameHeight, GetColor(33, 28, 24), TRUE);
        if (texture >= 0)
        {
            Shader_ResetStyle();
            const float exposurePulse = 0.92f + 0.08f * std::sin(m_elapsed * 5.0f + static_cast<float>(index));
            Shader_SetTint(exposurePulse, exposurePulse * 0.94f, exposurePulse * 0.82f, 1.0f);
            SpriteDraw(
                texture,
                static_cast<float>(frameLeft),
                static_cast<float>(frameTop),
                static_cast<float>(frameWidth),
                static_cast<float>(frameHeight),
                0.0f,
                0.0f,
                1.0f,
                1.0f);
            Shader_ResetStyle();
        }
        DrawBox(frameLeft, frameTop, frameRight, frameTop + frameHeight, GetColor(228, 207, 172), FALSE);

        char frameNumber[16]{};
        std::snprintf(frameNumber, sizeof(frameNumber), "%02d", (textureIndex + 1) * 8);
        DrawString(frameLeft + 8, frameTop + frameHeight - 22, frameNumber, GetColor(245, 228, 194));
    }

    // 中央のゲートが、現在映写される一コマを示す。
    const int gateLeft = centerX - frameWidth / 2 - 12;
    const int gateRight = centerX + frameWidth / 2 + 12;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 46);
    DrawBox(gateLeft, stripTop + railHeight + 4, gateRight, stripTop + stripHeight - railHeight - 4, GetColor(255, 205, 112), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(gateLeft, stripTop + railHeight + 4, gateRight, stripTop + stripHeight - railHeight - 4, GetColor(255, 220, 142), FALSE);

    DrawCenteredText(centerX, 24, "LOAD SCREEN PREVIEW  3/3 — FILM STRIP", GetColor(226, 196, 154));
    DrawCenteredText(centerX, 112, "旅の記録を読み込んでいます…", GetColor(239, 220, 186));
    DrawCenteredText(centerX, SCREEN_HEIGHT - 34,
        "Q/E・←/→: 案切替   R: 再生   T/Esc: 戻る",
        GetColor(200, 177, 146));
}

void LoadingPreviewScene::DrawDebugUI()
{
    ImGui::Begin("ロード画面プレビュー");
    ImGui::TextUnformatted("F6 でタイトルから開けます。");
    ImGui::RadioButton("暗室ポラロイド", &m_mode, 0);
    ImGui::RadioButton("ファインダー合焦", &m_mode, 1);
    ImGui::RadioButton("フィルムストリップ", &m_mode, 2);
    if (ImGui::Button("最初から再生"))
    {
        m_elapsed = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("タイトルへ戻る"))
    {
        ReturnToTitle();
    }
    ImGui::End();
}

bool LoadingPreviewScene::OnCancelAction()
{
    ReturnToTitle();
    return true;
}

EventBus* LoadingPreviewScene::GetEventBus()
{
    return &m_eventBus;
}

void LoadingPreviewScene::ReturnToTitle()
{
    m_returnRequested = true;
}
