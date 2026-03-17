#include "shader_showcase_scene.h"

#include <cmath>

#include <tracy/Tracy.hpp>

#include "directX.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"

namespace
{
    constexpr float kPanelSize = 156.0f;
    constexpr float kPanelGapX = 28.0f;
    constexpr float kPanelGapY = 56.0f;
    constexpr float kPanelStartX = 110.0f;
    constexpr float kPanelStartY = 168.0f;
    constexpr int kPageCount = 4;
}

ShaderShowcaseScene::ShaderShowcaseScene()
    : m_whiteTexture(-1)
    , m_blockTexture(-1)
    , m_titleTexture(-1)
    , m_ringTexture(-1)
    , m_burstTexture(-1)
    , m_thunderTexture(-1)
    , m_particleTexture(-1)
    , m_cloudTexture(-1)
    , m_normalTexture(-1)
    , m_windTexture(-1)
    , m_laserTexture(-1)
    , m_labelTexture(-1)
    , m_time(0.0f)
    , m_autoPulse(true)
    , m_pageIndex(0)
{
}

const char* ShaderShowcaseScene::GetSceneId() const
{
    return "shader_showcase";
}

void ShaderShowcaseScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_blockTexture = resources.LoadTexture(L"assets\\texture\\block.png");
    m_titleTexture = resources.LoadTexture(L"assets\\texture\\タイトル.png");
    m_ringTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Ring01.png");
    m_burstTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Burst01.png");
    m_thunderTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Thunder01.png");
    m_particleTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Particle01.png");
    m_cloudTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Cloud01.png");
    m_normalTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Normal1.png");
    m_windTexture = resources.LoadTexture(L"assets\\effects\\Texture\\wind02.png");
    m_laserTexture = resources.LoadTexture(L"assets\\effects\\Texture\\LaserMain01.png");
    m_labelTexture = m_whiteTexture;
    m_eventBus.Clear();
    m_time = 0.0f;
    m_autoPulse = true;
    m_pageIndex = 0;
    Logger::Info("ShaderShowcaseScene entered");
}

void ShaderShowcaseScene::Update(float deltaTime)
{
    ZoneScoped;
    m_eventBus.Clear();
    m_time += deltaTime;

    if (Input_IsKeyPressed('T'))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }
    if (Input_IsKeyPressed('G'))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    if (Input_IsKeyPressed('D'))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "demo", 0.0f, 0.0f });
    }
    if (Input_IsKeyPressed('P'))
    {
        m_autoPulse = !m_autoPulse;
    }
    if (Input_IsKeyPressed(VK_RIGHT) || Input_IsKeyPressed('E'))
    {
        m_pageIndex = (m_pageIndex + 1) % kPageCount;
    }
    if (Input_IsKeyPressed(VK_LEFT) || Input_IsKeyPressed('Q'))
    {
        m_pageIndex = (m_pageIndex + kPageCount - 1) % kPageCount;
    }
}

void ShaderShowcaseScene::Draw()
{
    DrawBackdrop();
    DrawCurrentPage();
    Shader_ResetStyle();
}

void ShaderShowcaseScene::DrawDebugUI()
{
    ImGui::Begin("Shader Showcase");
    ImGui::Text("2D shader gallery");
    ImGui::Text("T: title  G: game  D: demo");
    ImGui::Text("Q / Left: prev page  E / Right: next page");
    ImGui::Text("P: toggle additive pulse");
    ImGui::Separator();
    ImGui::Text("Page %d / %d", m_pageIndex + 1, kPageCount);
    if (m_pageIndex == 0)
    {
        ImGui::Text("Normal / Grayscale / Outline / Additive / Flash");
    }
    else if (m_pageIndex == 1)
    {
        ImGui::Text("UV Scroll / Dissolve / Mask Clip / Distortion / Palette Swap");
    }
    else if (m_pageIndex == 2)
    {
        ImGui::Text("Posterize / Chromatic Aberration / Glitch / Pixelate / Wave");
    }
    else
    {
        ImGui::Text("Rim Light / Gradient Map / Noise Reveal / Heat Overlay / Parallax / Normal Map");
    }
    ImGui::Separator();
    ImGui::Text("Status legend: Supported / Approximate / Unsupported");
    ImGui::End();
}

EventBus* ShaderShowcaseScene::GetEventBus()
{
    return &m_eventBus;
}

void ShaderShowcaseScene::DrawBackdrop() const
{
    const float screenWidth = static_cast<float>(SCREEN_WIDTH);
    const float screenHeight = static_cast<float>(SCREEN_HEIGHT);
    const float contentX = 64.0f;
    const float contentY = 92.0f;
    const float contentWidth = screenWidth - 128.0f;
    const float contentHeight = screenHeight - 124.0f;

    Shader_ResetStyle();
    Shader_SetTint(0.05f, 0.07f, 0.10f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.89f, 0.39f, 0.18f, 1.0f);
    SpriteDraw(m_whiteTexture, contentX, 58.0f, contentWidth, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.10f, 0.15f, 0.24f, 1.0f);
    SpriteDraw(m_whiteTexture, contentX, contentY, contentWidth, contentHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.14f, 0.20f, 0.30f, 1.0f);
    SpriteDraw(m_whiteTexture, contentX + 18.0f, contentY + 18.0f, contentWidth - 36.0f, 66.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, contentX + 18.0f, contentY + 106.0f, contentWidth - 36.0f, contentHeight - 126.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    if (m_titleTexture >= 0)
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, 0.92f);
        SpriteDraw(m_titleTexture, contentX + 28.0f, contentY + 10.0f, 320.0f, 64.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    Shader_SetTint(0.18f, 0.26f, 0.38f, 1.0f);
    SpriteDraw(m_whiteTexture, contentX + 368.0f, contentY + 20.0f, 140.0f, 14.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, contentX + 524.0f, contentY + 20.0f, 140.0f, 14.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    const float dotStartX = contentX + contentWidth - 172.0f;
    for (int i = 0; i < kPageCount; ++i)
    {
        const bool active = i == m_pageIndex;
        Shader_SetTint(active ? 0.96f : 0.36f, active ? 0.82f : 0.46f, active ? 0.24f : 0.58f, 1.0f);
        SpriteDraw(m_whiteTexture, dotStartX + i * 34.0f, contentY + 18.0f, active ? 22.0f : 16.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    Shader_SetTint(0.14f, 0.20f, 0.30f, 1.0f);
    SpriteDraw(m_whiteTexture, contentX, screenHeight - 22.0f, contentWidth, 8.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

void ShaderShowcaseScene::DrawCurrentPage() const
{
    const float usableY = 218.0f;

    if (m_pageIndex == 0)
    {
        const float rowWidthTop = kPanelSize * 3.0f + kPanelGapX * 2.0f;
        const float rowWidthBottom = kPanelSize * 2.0f + kPanelGapX;
        const float x0 = (static_cast<float>(SCREEN_WIDTH) - rowWidthTop) * 0.5f;
        const float x1 = x0 + (kPanelSize + kPanelGapX);
        const float x2 = x1 + (kPanelSize + kPanelGapX);
        const float y0 = usableY;
        const float y1 = y0 + (kPanelSize + kPanelGapY);
        const float bottomStartX = (static_cast<float>(SCREEN_WIDTH) - rowWidthBottom) * 0.5f;

        DrawNormalPanel(x0, y0, kPanelSize);
        DrawGrayscalePanel(x1, y0, kPanelSize);
        DrawOutlinePanel(x2, y0, kPanelSize);
        DrawAdditivePanel(bottomStartX, y1, kPanelSize);
        DrawFlashPanel(bottomStartX + kPanelSize + kPanelGapX, y1, kPanelSize);
        return;
    }

    if (m_pageIndex == 1)
    {
        const float rowWidthTop = kPanelSize * 3.0f + kPanelGapX * 2.0f;
        const float rowWidthBottom = kPanelSize * 2.0f + kPanelGapX;
        const float x0 = (static_cast<float>(SCREEN_WIDTH) - rowWidthTop) * 0.5f;
        const float x1 = x0 + (kPanelSize + kPanelGapX);
        const float x2 = x1 + (kPanelSize + kPanelGapX);
        const float y0 = usableY;
        const float y1 = y0 + (kPanelSize + kPanelGapY);
        const float bottomStartX = (static_cast<float>(SCREEN_WIDTH) - rowWidthBottom) * 0.5f;

        DrawUVScrollPanel(x0, y0, kPanelSize);
        DrawDissolvePanel(x1, y0, kPanelSize);
        DrawMaskClipPanel(x2, y0, kPanelSize);
        DrawDistortionPanel(bottomStartX, y1, kPanelSize);
        DrawPaletteSwapPanel(bottomStartX + kPanelSize + kPanelGapX, y1, kPanelSize);
        return;
    }

    if (m_pageIndex == 2)
    {
        constexpr float largeSize = 208.0f;
        constexpr float largeX0 = 92.0f;
        constexpr float largeX1 = 356.0f;
        constexpr float largeX2 = 620.0f;
        constexpr float largeY0 = 164.0f;
        constexpr float largeY1 = 434.0f;

        DrawPosterizePanel(largeX0, largeY0, largeSize);
        DrawChromaticAberrationPanel(largeX1, largeY0, largeSize);
        DrawGlitchPanel(largeX2, largeY0, largeSize);
        DrawPixelatePanel(largeX0, largeY1, largeSize);
        DrawWavePanel(largeX1, largeY1, largeSize);
        return;
    }

    const float rowWidthTop = kPanelSize * 3.0f + kPanelGapX * 2.0f;
    const float x0 = (static_cast<float>(SCREEN_WIDTH) - rowWidthTop) * 0.5f;
    const float x1 = x0 + (kPanelSize + kPanelGapX);
    const float x2 = x1 + (kPanelSize + kPanelGapX);
    const float y0 = usableY;
    const float y1 = y0 + (kPanelSize + kPanelGapY);

    DrawRimLightPanel(x0, y0, kPanelSize);
    DrawGradientMapPanel(x1, y0, kPanelSize);
    DrawNoiseRevealPanel(x2, y0, kPanelSize);
    const float rowWidthBottom = kPanelSize * 3.0f + kPanelGapX * 2.0f;
    const float bottomStartX = (static_cast<float>(SCREEN_WIDTH) - rowWidthBottom) * 0.5f;
    DrawHeatOverlayPanel(bottomStartX, y1, kPanelSize);
    DrawParallaxPanel(bottomStartX + kPanelSize + kPanelGapX, y1, kPanelSize);
    DrawNormalMapLightingPanel(bottomStartX + (kPanelSize + kPanelGapX) * 2.0f, y1, kPanelSize);
}

void ShaderShowcaseScene::DrawPanelFrame(float x, float y, float size) const
{
    Shader_ResetStyle();
    Shader_SetTint(0.03f, 0.05f, 0.09f, 0.55f);
    SpriteDraw(m_whiteTexture, x + 8.0f, y + 10.0f, size, size, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.20f, 0.28f, 0.40f, 1.0f);
    SpriteDraw(m_whiteTexture, x, y, size, size, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.11f, 0.16f, 0.25f, 1.0f);
    SpriteDraw(m_whiteTexture, x + 4.0f, y + 4.0f, size - 8.0f, size - 8.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.13f, 0.20f, 0.31f, 1.0f);
    SpriteDraw(m_whiteTexture, x + 12.0f, y + 12.0f, size - 24.0f, size - 24.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

void ShaderShowcaseScene::DrawLabelBar(float x, float y, float size, float r, float g, float b) const
{
    Shader_ResetStyle();
    Shader_SetTint(r, g, b, 1.0f);
    SpriteDraw(m_whiteTexture, x, y + size + 10.0f, size, 8.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

void ShaderShowcaseScene::DrawPanelLabel(float x, float y, float size, int labelIndex) const
{
    const float labelX = x;
    const float labelY = y + size + 22.0f;
    const float labelHeight = 20.0f;
    const float charWidth = 10.0f;
    const float gap = 4.0f;
    const float totalWidth = charWidth * 4.0f + gap * 3.0f;
    const float startX = labelX + (size - totalWidth) * 0.5f;

    Shader_ResetStyle();
    Shader_SetTint(0.10f, 0.14f, 0.20f, 1.0f);
    SpriteDraw(m_labelTexture, labelX + size * 0.5f - 46.0f, labelY - 2.0f, 92.0f, labelHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    for (int i = 0; i < 4; ++i)
    {
        const int bit = (labelIndex >> (3 - i)) & 1;
        Shader_SetTint(bit ? 0.94f : 0.28f, bit ? 0.82f : 0.32f, bit ? 0.24f : 0.40f, 1.0f);
        SpriteDraw(m_labelTexture, startX + i * (charWidth + gap), labelY, charWidth, 14.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }
}

void ShaderShowcaseScene::DrawSupportBadge(float x, float y, float size, ShaderEffect2D effect) const
{
    const ShaderSupportLevel support = Shader_GetEffectSupport(effect);

    float r = 0.24f;
    float g = 0.84f;
    float b = 0.42f;
    if (support == ShaderSupportLevel::Approximate)
    {
        r = 0.95f;
        g = 0.76f;
        b = 0.20f;
    }
    else if (support == ShaderSupportLevel::Unsupported)
    {
        r = 0.96f;
        g = 0.30f;
        b = 0.28f;
    }

    const float badgeW = 38.0f;
    const float badgeH = 14.0f;
    const float badgeX = x + size - badgeW - 10.0f;
    const float badgeY = y + 10.0f;

    Shader_ResetStyle();
    Shader_SetTint(0.05f, 0.08f, 0.12f, 0.90f);
    SpriteDraw(m_whiteTexture, badgeX, badgeY, badgeW, badgeH, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(r, g, b, 1.0f);

    if (support == ShaderSupportLevel::Supported)
    {
        SpriteDraw(m_whiteTexture, badgeX + 6.0f, badgeY + 4.0f, 8.0f, 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.75f);
        SpriteDraw(m_whiteTexture, badgeX + 11.0f, badgeY + 2.0f, 16.0f, 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, -0.78f);
    }
    else if (support == ShaderSupportLevel::Approximate)
    {
        SpriteDraw(m_whiteTexture, badgeX + 7.0f, badgeY + 5.0f, 24.0f, 4.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }
    else
    {
        SpriteDraw(m_whiteTexture, badgeX + 8.0f, badgeY + 5.0f, 20.0f, 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.75f);
        SpriteDraw(m_whiteTexture, badgeX + 8.0f, badgeY + 5.0f, 20.0f, 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, -0.75f);
    }
}

void ShaderShowcaseScene::DrawNormalPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Normal);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.25f);
    DrawLabelBar(x, y, size, 0.95f, 0.50f, 0.22f);
    DrawPanelLabel(x, y, size, 1);
}

void ShaderShowcaseScene::DrawGrayscalePanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Grayscale);
    Shader_SetEffect(ShaderEffect2D::Grayscale);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, -m_time * 0.3f);
    DrawLabelBar(x, y, size, 0.58f, 0.58f, 0.58f);
    DrawPanelLabel(x, y, size, 2);
}

void ShaderShowcaseScene::DrawOutlinePanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Outline);
    Shader_SetTint(0.18f, 0.40f, 0.66f, 0.35f);
    SpriteDraw(m_burstTexture, x + 12.0f, y + 12.0f, size - 24.0f, size - 24.0f, 0.0f, 0.0f, 1.0f, 1.0f, -m_time * 0.3f);
    Shader_SetOutline(1.0f, 0.82f, 0.24f, 1.0f, 3.0f);
    Shader_SetTint(0.92f, 0.96f, 1.0f, 1.0f);
    SpriteDraw(m_ringTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.55f);
    DrawLabelBar(x, y, size, 1.0f, 0.82f, 0.24f);
    DrawPanelLabel(x, y, size, 3);
}

void ShaderShowcaseScene::DrawAdditivePanel(float x, float y, float size) const
{
    const float glow = m_autoPulse ? (0.45f + 0.55f * std::sinf(m_time * 2.6f)) : 0.9f;
    const float glowScale = 1.0f + glow * 0.12f;
    const float glowSize = (size - 52.0f) * glowScale;
    const float glowOffset = 26.0f - (glowSize - (size - 52.0f)) * 0.5f;

    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Normal);
    Shader_SetTint(0.88f, 0.92f, 1.0f, 0.75f);
    SpriteDraw(m_blockTexture, x + 26.0f, y + 26.0f, size - 52.0f, size - 52.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.15f);
    Shader_SetBlendMode(ShaderBlendMode2D::Additive);
    Shader_SetTint(0.35f + glow * 0.35f, 0.70f, 1.0f, 0.82f);
    SpriteDraw(m_burstTexture, x + glowOffset, y + glowOffset, glowSize, glowSize, 0.0f, 0.0f, 1.0f, 1.0f, -m_time * 0.4f);
    Shader_SetTint(0.16f, 0.72f + glow * 0.20f, 1.0f, 0.65f);
    SpriteDraw(m_ringTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.65f);
    DrawLabelBar(x, y, size, 0.35f, 0.70f, 1.0f);
    DrawPanelLabel(x, y, size, 4);
}

void ShaderShowcaseScene::DrawFlashPanel(float x, float y, float size) const
{
    const float flash = 0.35f + 0.65f * std::fabs(std::sinf(m_time * 5.0f));
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Flash);
    Shader_SetFlash(1.0f, 0.92f, 0.72f, 1.0f, flash);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, -m_time * 0.4f);
    DrawLabelBar(x, y, size, 1.0f, 0.92f, 0.72f);
    DrawPanelLabel(x, y, size, 5);
}

void ShaderShowcaseScene::DrawUVScrollPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::UVScroll);
    Shader_SetUVScroll(m_time * 0.28f, -m_time * 0.16f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_thunderTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 2.2f, 2.2f, 0.0f);
    DrawLabelBar(x, y, size, 0.90f, 0.55f, 0.22f);
    DrawPanelLabel(x, y, size, 6);
}

void ShaderShowcaseScene::DrawDissolvePanel(float x, float y, float size) const
{
    const float threshold = 0.1f + 0.75f * (0.5f + 0.5f * std::sinf(m_time * 0.9f));
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Dissolve);
    Shader_SetDissolve(threshold, 0.16f, 1.0f, 0.45f, 0.12f, 1.0f);
    Shader_SetTint(0.95f, 0.95f, 1.0f, 1.0f);
    SpriteDraw(m_particleTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.25f);
    DrawLabelBar(x, y, size, 1.0f, 0.45f, 0.12f);
    DrawPanelLabel(x, y, size, 7);
}

void ShaderShowcaseScene::DrawMaskClipPanel(float x, float y, float size) const
{
    const float threshold = 0.15f + 0.65f * (0.5f + 0.5f * std::sinf(m_time * 1.1f));
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::MaskClip);
    Shader_SetMaskClip(threshold, 0.15f);
    Shader_SetTint(0.92f, 0.96f, 1.0f, 1.0f);
    SpriteDraw(m_cloudTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    DrawLabelBar(x, y, size, 0.74f, 0.86f, 1.0f);
    DrawPanelLabel(x, y, size, 8);
}

void ShaderShowcaseScene::DrawDistortionPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Distortion);
    Shader_SetDistortion(0.018f, 0.014f, m_time, 0.65f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_windTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.4f, 1.4f, 0.0f);
    DrawLabelBar(x, y, size, 0.42f, 0.76f, 1.0f);
    DrawPanelLabel(x, y, size, 9);
}

void ShaderShowcaseScene::DrawPaletteSwapPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::PaletteSwap);
    Shader_SetPaletteSwap(1.0f, 1.0f, 1.0f, 1.0f, 0.15f, 0.92f, 0.36f, 1.0f, 0.45f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_ringTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.4f);
    DrawLabelBar(x, y, size, 0.15f, 0.92f, 0.36f);
    DrawPanelLabel(x, y, size, 10);
}

void ShaderShowcaseScene::DrawPosterizePanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Posterize);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 0.95f);
    SpriteDraw(m_burstTexture, x + 14.0f, y + 18.0f, (size - 34.0f) * 0.5f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.18f);
    Shader_SetTint(0.18f, 0.22f, 0.30f, 1.0f);
    SpriteDraw(m_whiteTexture, x + size * 0.5f - 1.0f, y + 16.0f, 2.0f, size - 32.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetPosterize(2.0f, 2.1f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_burstTexture, x + size * 0.5f + 4.0f, y + 18.0f, (size - 34.0f) * 0.5f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.18f);
    Shader_SetBlendMode(ShaderBlendMode2D::Additive);
    Shader_SetTint(0.45f, 0.78f, 1.0f, 0.55f);
    SpriteDraw(m_ringTexture, x + size * 0.5f + 14.0f, y + 30.0f, (size - 54.0f) * 0.5f, size - 60.0f, 0.0f, 0.0f, 1.0f, 1.0f, -m_time * 0.35f);
    DrawLabelBar(x, y, size, 0.86f, 0.64f, 0.20f);
    DrawPanelLabel(x, y, size, 11);
}

void ShaderShowcaseScene::DrawChromaticAberrationPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::ChromaticAberration);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_titleTexture, x + 12.0f, y + 32.0f, (size - 30.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 24.0f, y + 84.0f, (size - 54.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.15f);
    Shader_SetTint(0.18f, 0.22f, 0.30f, 1.0f);
    SpriteDraw(m_whiteTexture, x + size * 0.5f - 1.0f, y + 16.0f, 2.0f, size - 32.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetChromaticAberration(0.032f, m_time);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_titleTexture, x + size * 0.5f + 4.0f, y + 32.0f, (size - 30.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + size * 0.5f + 16.0f, y + 84.0f, (size - 54.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.15f);
    DrawLabelBar(x, y, size, 0.95f, 0.20f, 0.55f);
    DrawPanelLabel(x, y, size, 12);
}

void ShaderShowcaseScene::DrawGlitchPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Glitch);
    Shader_SetGlitch(0.024f, m_time, 0.55f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    DrawLabelBar(x, y, size, 0.92f, 0.18f, 0.18f);
    DrawPanelLabel(x, y, size, 13);
}

void ShaderShowcaseScene::DrawPixelatePanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Pixelate);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_titleTexture, x + 12.0f, y + 32.0f, (size - 30.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 20.0f, y + 84.0f, (size - 46.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f);
    Shader_SetTint(0.18f, 0.22f, 0.30f, 1.0f);
    SpriteDraw(m_whiteTexture, x + size * 0.5f - 1.0f, y + 16.0f, 2.0f, size - 32.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetPixelate(28.0f, 28.0f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_titleTexture, x + size * 0.5f + 4.0f, y + 32.0f, (size - 30.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + size * 0.5f + 12.0f, y + 84.0f, (size - 46.0f) * 0.5f, 42.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f);
    DrawLabelBar(x, y, size, 0.52f, 0.52f, 0.92f);
    DrawPanelLabel(x, y, size, 14);
}

void ShaderShowcaseScene::DrawWavePanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Wave);
    Shader_SetWave(0.015f, 0.010f, 20.0f, m_time);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_laserTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.4f, 1.4f, 0.0f);
    DrawLabelBar(x, y, size, 0.36f, 0.82f, 0.94f);
    DrawPanelLabel(x, y, size, 15);
}

void ShaderShowcaseScene::DrawRimLightPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::RimLight);
    Shader_SetRimLight(1.0f, 0.86f, 0.24f, 1.0f, 2.5f);
    Shader_SetTint(0.94f, 0.96f, 1.0f, 1.0f);
    SpriteDraw(m_ringTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.35f);
    DrawLabelBar(x, y, size, 1.0f, 0.86f, 0.24f);
    DrawPanelLabel(x, y, size, 16);
}

void ShaderShowcaseScene::DrawGradientMapPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::GradientMap);
    Shader_SetGradientMap(0.10f, 0.14f, 0.45f, 1.0f, 1.0f, 0.70f, 0.16f, 1.0f, 1.35f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_time * 0.25f);
    DrawLabelBar(x, y, size, 1.0f, 0.70f, 0.16f);
    DrawPanelLabel(x, y, size, 17);
}

void ShaderShowcaseScene::DrawNoiseRevealPanel(float x, float y, float size) const
{
    const float threshold = 0.08f + 0.78f * (0.5f + 0.5f * std::sinf(m_time * 0.85f));
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::NoiseReveal);
    Shader_SetNoiseReveal(threshold, 0.18f, m_time * 12.0f, 0.22f, 1.0f, 0.68f, 1.0f);
    Shader_SetTint(0.92f, 0.98f, 1.0f, 1.0f);
    SpriteDraw(m_particleTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    DrawLabelBar(x, y, size, 0.22f, 1.0f, 0.68f);
    DrawPanelLabel(x, y, size, 18);
}

void ShaderShowcaseScene::DrawHeatOverlayPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::HeatOverlay);
    Shader_SetHeatOverlay(0.9f, m_time);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    DrawLabelBar(x, y, size, 1.0f, 0.22f, 0.08f);
    DrawPanelLabel(x, y, size, 19);
}

void ShaderShowcaseScene::DrawParallaxPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::Parallax);
    Shader_SetParallax(0.10f, 0.32f, 0.56f, m_time);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_cloudTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.5f, 1.5f, 0.0f);
    DrawLabelBar(x, y, size, 0.62f, 0.82f, 1.0f);
    DrawPanelLabel(x, y, size, 20);
}

void ShaderShowcaseScene::DrawNormalMapLightingPanel(float x, float y, float size) const
{
    DrawPanelFrame(x, y, size);
    DrawSupportBadge(x, y, size, ShaderEffect2D::NormalMapLighting);
    Shader_SetAuxTexture(m_normalTexture);
    Shader_SetNormalMapLighting(-0.45f, -0.55f, 0.70f, 0.30f, 1.10f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteDraw(m_blockTexture, x + 18.0f, y + 18.0f, size - 36.0f, size - 36.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    Shader_SetAuxTexture(-1);
    DrawLabelBar(x, y, size, 0.94f, 0.88f, 0.42f);
    DrawPanelLabel(x, y, size, 21);
}
