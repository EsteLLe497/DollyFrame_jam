#pragma once

#include "asset_manifest.h"
#include "event_bus.h"
#include "scene.h"
#include "shader.h"

class ShaderShowcaseScene final : public Scene
{
public:
    ShaderShowcaseScene();
    ~ShaderShowcaseScene() override = default;

    const char* GetSceneId() const override;
    void OnEnter(ResourceManager& resources) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawDebugUI() override;
    EventBus* GetEventBus() override;

private:
    void DrawBackdrop() const;
    void DrawCurrentPage() const;
    void DrawPanelFrame(float x, float y, float size) const;
    void DrawLabelBar(float x, float y, float size, float r, float g, float b) const;
    void DrawPanelLabel(float x, float y, float size, int labelIndex) const;
    void DrawSupportBadge(float x, float y, float size, ShaderEffect2D effect) const;

    void DrawNormalPanel(float x, float y, float size) const;
    void DrawGrayscalePanel(float x, float y, float size) const;
    void DrawOutlinePanel(float x, float y, float size) const;
    void DrawAdditivePanel(float x, float y, float size) const;
    void DrawFlashPanel(float x, float y, float size) const;
    void DrawUVScrollPanel(float x, float y, float size) const;
    void DrawDissolvePanel(float x, float y, float size) const;
    void DrawMaskClipPanel(float x, float y, float size) const;
    void DrawDistortionPanel(float x, float y, float size) const;
    void DrawPaletteSwapPanel(float x, float y, float size) const;
    void DrawPosterizePanel(float x, float y, float size) const;
    void DrawChromaticAberrationPanel(float x, float y, float size) const;
    void DrawGlitchPanel(float x, float y, float size) const;
    void DrawPixelatePanel(float x, float y, float size) const;
    void DrawWavePanel(float x, float y, float size) const;
    void DrawRimLightPanel(float x, float y, float size) const;
    void DrawGradientMapPanel(float x, float y, float size) const;
    void DrawNoiseRevealPanel(float x, float y, float size) const;
    void DrawHeatOverlayPanel(float x, float y, float size) const;
    void DrawParallaxPanel(float x, float y, float size) const;
    void DrawNormalMapLightingPanel(float x, float y, float size) const;

    AssetManifest m_assets;
    EventBus m_eventBus;
    int m_whiteTexture;
    int m_blockTexture;
    int m_titleTexture;
    int m_ringTexture;
    int m_burstTexture;
    int m_thunderTexture;
    int m_particleTexture;
    int m_cloudTexture;
    int m_normalTexture;
    int m_windTexture;
    int m_laserTexture;
    int m_labelTexture;
    float m_time;
    bool m_autoPulse;
    int m_pageIndex;
};
