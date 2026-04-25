#include "directX.h"

#include "DxLib.h"

#include <algorithm>

int SCREEN_WIDTH = 1920;
int SCREEN_HEIGHT = 1080;

namespace
{
    int g_BackBufferW = SCREEN_WIDTH;
    int g_BackBufferH = SCREEN_HEIGHT;
    BlendMode2D g_blendMode = BlendMode2D::Alpha;
    int g_sceneRenderTarget = -1;
    int g_lightMaskRenderTarget = -1;
    int g_lightBlurRenderTarget = -1;
    int g_lightExtractPixelShader = -1;
    int g_lightBlurVPixelShader = -1;
    int g_lightBlurHPixelShader = -1;
    int g_postProcessPixelShader = -1;
    int g_postProcessConstantBuffer = -1;
    int g_darknessOverlayPixelShader = -1;
    int g_darknessOverlayConstantBuffer = -1;
    bool g_postProcessReady = false;
    bool g_postProcessEnabled = true;
    DarknessOverlayParams g_darknessOverlayParams = {};

    struct PostProcessParams
    {
        float param0;
        float param1;
        float param2;
        float param3;
    };

    struct DarknessOverlayShaderParams
    {
        float screenWidth;
        float screenHeight;
        float pad0;
        float pad1;
        float viewLeft;
        float viewTop;
        float viewRight;
        float viewBottom;
        float darknessOpacity;
        float lightCount;
        float pad2;
        float pad3;
        float colorR;
        float colorG;
        float colorB;
        float pad4;
        float lightParams[kMaxDarknessOverlayLights * 4];
        float lightIntensityPack[kMaxDarknessOverlayLights];
        float lightColors[kMaxDarknessOverlayLights * 4];
        float lightShapeData[kMaxDarknessOverlayLights * 4];
    };

    void ReleasePostProcessResources()
    {
        if (g_lightExtractPixelShader >= 0)
        {
            DeleteShader(g_lightExtractPixelShader);
            g_lightExtractPixelShader = -1;
        }
        if (g_lightBlurVPixelShader >= 0)
        {
            DeleteShader(g_lightBlurVPixelShader);
            g_lightBlurVPixelShader = -1;
        }
        if (g_lightBlurHPixelShader >= 0)
        {
            DeleteShader(g_lightBlurHPixelShader);
            g_lightBlurHPixelShader = -1;
        }
        if (g_postProcessPixelShader >= 0)
        {
            DeleteShader(g_postProcessPixelShader);
            g_postProcessPixelShader = -1;
        }
        if (g_postProcessConstantBuffer >= 0)
        {
            DeleteShaderConstantBuffer(g_postProcessConstantBuffer);
            g_postProcessConstantBuffer = -1;
        }
        if (g_darknessOverlayPixelShader >= 0)
        {
            DeleteShader(g_darknessOverlayPixelShader);
            g_darknessOverlayPixelShader = -1;
        }
        if (g_darknessOverlayConstantBuffer >= 0)
        {
            DeleteShaderConstantBuffer(g_darknessOverlayConstantBuffer);
            g_darknessOverlayConstantBuffer = -1;
        }
        if (g_sceneRenderTarget >= 0)
        {
            DeleteGraph(g_sceneRenderTarget);
            g_sceneRenderTarget = -1;
        }
        if (g_lightMaskRenderTarget >= 0)
        {
            DeleteGraph(g_lightMaskRenderTarget);
            g_lightMaskRenderTarget = -1;
        }
        if (g_lightBlurRenderTarget >= 0)
        {
            DeleteGraph(g_lightBlurRenderTarget);
            g_lightBlurRenderTarget = -1;
        }
        g_postProcessReady = false;
    }

    void CreatePostProcessResources()
    {
        ReleasePostProcessResources();

        g_sceneRenderTarget = MakeScreen(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE);
        if (g_sceneRenderTarget < 0)
        {
            return;
        }

        g_lightMaskRenderTarget = MakeScreen(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE);
        if (g_lightMaskRenderTarget < 0)
        {
            return;
        }

        g_lightBlurRenderTarget = MakeScreen(SCREEN_WIDTH, SCREEN_HEIGHT, TRUE);
        if (g_lightBlurRenderTarget < 0)
        {
            return;
        }

        g_lightExtractPixelShader = LoadPixelShader("assets/shaders/light_extract_ps.cso");
        if (g_lightExtractPixelShader < 0)
        {
            return;
        }

        g_lightBlurVPixelShader = LoadPixelShader("assets/shaders/light_blur_vertical_ps.cso");
        if (g_lightBlurVPixelShader < 0)
        {
            return;
        }

        g_lightBlurHPixelShader = LoadPixelShader("assets/shaders/light_blur_horizontal_ps.cso");
        if (g_lightBlurHPixelShader < 0)
        {
            return;
        }

        g_postProcessPixelShader = LoadPixelShader("assets/shaders/lighting_composite_ps.cso");
        if (g_postProcessPixelShader < 0)
        {
            return;
        }

        g_postProcessConstantBuffer = CreateShaderConstantBuffer(sizeof(PostProcessParams));
        if (g_postProcessConstantBuffer < 0)
        {
            return;
        }

        g_darknessOverlayPixelShader = LoadPixelShader("assets/shaders/darkness_overlay_ps.cso");
        if (g_darknessOverlayPixelShader < 0)
        {
            return;
        }

        g_darknessOverlayConstantBuffer = CreateShaderConstantBuffer(sizeof(DarknessOverlayShaderParams));
        if (g_darknessOverlayConstantBuffer < 0)
        {
            return;
        }

        g_postProcessReady = true;
    }

    void DrawFullscreenCompositeQuad()
    {
        VERTEX2DSHADER vertices[4] = {};
        const COLOR_U8 white = GetColorU8(255, 255, 255, 255);

        vertices[0].pos = VGet(0.0f, 0.0f, 0.0f);
        vertices[0].rhw = 1.0f;
        vertices[0].dif = white;
        vertices[0].spc = white;
        vertices[0].u = 0.0f;
        vertices[0].v = 0.0f;

        vertices[1].pos = VGet(static_cast<float>(SCREEN_WIDTH), 0.0f, 0.0f);
        vertices[1].rhw = 1.0f;
        vertices[1].dif = white;
        vertices[1].spc = white;
        vertices[1].u = 1.0f;
        vertices[1].v = 0.0f;

        vertices[2].pos = VGet(0.0f, static_cast<float>(SCREEN_HEIGHT), 0.0f);
        vertices[2].rhw = 1.0f;
        vertices[2].dif = white;
        vertices[2].spc = white;
        vertices[2].u = 0.0f;
        vertices[2].v = 1.0f;

        vertices[3].pos = VGet(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f);
        vertices[3].rhw = 1.0f;
        vertices[3].dif = white;
        vertices[3].spc = white;
        vertices[3].u = 1.0f;
        vertices[3].v = 1.0f;

        DrawPrimitive2DToShader(vertices, 4, DX_PRIMTYPE_TRIANGLESTRIP);
    }
}

void* DirectXGetSwapChain(void)
{
    return nullptr;
}

void DirectXInitialize(HWND hWnd)
{
    static_cast<void>(hWnd);
    g_BackBufferW = SCREEN_WIDTH;
    g_BackBufferH = SCREEN_HEIGHT;
    CreatePostProcessResources();
    SetDrawScreen(DX_SCREEN_BACK);
    ClearDrawScreen();
}

void DirectXFinalaize(void)
{
    ReleasePostProcessResources();
}

void DirectXResize(int width, int height)
{
    if (width > 0)
    {
        SCREEN_WIDTH = width;
        g_BackBufferW = width;
    }
    if (height > 0)
    {
        SCREEN_HEIGHT = height;
        g_BackBufferH = height;
    }

    CreatePostProcessResources();
}

void* DirectXGetDevice(void)
{
    return nullptr;
}

void* DirectXGetDeviceContext(void)
{
    return nullptr;
}

void Clear(void)
{
    ClearDrawScreen();
}

void DirectXBeginSceneRender(void)
{
    DirectXResetDarknessOverlay();
    if (g_postProcessEnabled && g_postProcessReady && g_sceneRenderTarget >= 0)
    {
        SetDrawScreen(g_sceneRenderTarget);
    }
    else
    {
        SetDrawScreen(DX_SCREEN_BACK);
    }
    ClearDrawScreen();
}

void DirectXCompositeSceneToBackBuffer(float timeSeconds)
{
    if (!g_postProcessEnabled)
    {
        return;
    }

    if (!g_postProcessReady ||
        g_sceneRenderTarget < 0 ||
        g_lightMaskRenderTarget < 0 ||
        g_lightBlurRenderTarget < 0 ||
        g_lightExtractPixelShader < 0 ||
        g_lightBlurVPixelShader < 0 ||
        g_lightBlurHPixelShader < 0 ||
        g_postProcessPixelShader < 0 ||
        g_postProcessConstantBuffer < 0)
    {
        SetDrawScreen(DX_SCREEN_BACK);
        return;
    }

    auto* buffer = static_cast<PostProcessParams*>(GetBufferShaderConstantBuffer(g_postProcessConstantBuffer));
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);

    SetDrawScreen(g_lightMaskRenderTarget);
    ClearDrawScreen();
    if (buffer)
    {
        buffer->param0 = timeSeconds;
        buffer->param1 = 0.0f;
        buffer->param2 = 0.0f;
        buffer->param3 = 0.0f;
        UpdateShaderConstantBuffer(g_postProcessConstantBuffer);
        SetShaderConstantBuffer(g_postProcessConstantBuffer, DX_SHADERTYPE_PIXEL, 0);
    }
    SetUseTextureToShader(0, g_sceneRenderTarget);
    SetUseTextureToShader(1, -1);
    SetUsePixelShader(g_lightExtractPixelShader);
    SetUseVertexShader(-1);
    DrawFullscreenCompositeQuad();

    SetDrawScreen(g_lightBlurRenderTarget);
    ClearDrawScreen();
    if (buffer)
    {
        buffer->param0 = 1.0f / static_cast<float>(SCREEN_HEIGHT);
        buffer->param1 = 0.90f;
        buffer->param2 = 0.0f;
        buffer->param3 = 0.0f;
        UpdateShaderConstantBuffer(g_postProcessConstantBuffer);
        SetShaderConstantBuffer(g_postProcessConstantBuffer, DX_SHADERTYPE_PIXEL, 0);
    }
    SetUseTextureToShader(0, g_lightMaskRenderTarget);
    SetUseTextureToShader(1, -1);
    SetUsePixelShader(g_lightBlurVPixelShader);
    DrawFullscreenCompositeQuad();

    SetDrawScreen(g_lightMaskRenderTarget);
    ClearDrawScreen();
    if (buffer)
    {
        buffer->param0 = 1.0f / static_cast<float>(SCREEN_WIDTH);
        buffer->param1 = 0.86f;
        buffer->param2 = 0.0f;
        buffer->param3 = 0.0f;
        UpdateShaderConstantBuffer(g_postProcessConstantBuffer);
        SetShaderConstantBuffer(g_postProcessConstantBuffer, DX_SHADERTYPE_PIXEL, 0);
    }
    SetUseTextureToShader(0, g_lightBlurRenderTarget);
    SetUseTextureToShader(1, -1);
    SetUsePixelShader(g_lightBlurHPixelShader);
    DrawFullscreenCompositeQuad();

    SetDrawScreen(DX_SCREEN_BACK);
    ClearDrawScreen();
    if (buffer)
    {
        buffer->param0 = timeSeconds;
        buffer->param1 = 0.08f; // vignette
        buffer->param2 = 0.008f; // grain
        buffer->param3 = 0.52f; // light blend
        UpdateShaderConstantBuffer(g_postProcessConstantBuffer);
        SetShaderConstantBuffer(g_postProcessConstantBuffer, DX_SHADERTYPE_PIXEL, 0);
    }
    SetUseTextureToShader(0, g_sceneRenderTarget);
    SetUseTextureToShader(1, g_lightMaskRenderTarget);
    SetUsePixelShader(g_postProcessPixelShader);
    SetUseVertexShader(-1);
    DrawFullscreenCompositeQuad();

    SetUsePixelShader(-1);
    SetUseTextureToShader(0, -1);
    SetUseTextureToShader(1, -1);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

bool DirectXHasPostProcess(void)
{
    return g_postProcessReady;
}

void DirectXSetPostProcessEnabled(bool enabled)
{
    g_postProcessEnabled = enabled;
}

void DirectXTogglePostProcess(void)
{
    g_postProcessEnabled = !g_postProcessEnabled;
}

bool DirectXIsPostProcessEnabled(void)
{
    return g_postProcessEnabled;
}

bool DirectXHasDarknessOverlay(void)
{
    return g_darknessOverlayPixelShader >= 0 && g_darknessOverlayConstantBuffer >= 0;
}

void DirectXResetDarknessOverlay(void)
{
    g_darknessOverlayParams = {};
}

void DirectXSetDarknessOverlay(const DarknessOverlayParams& params)
{
    g_darknessOverlayParams = params;
}

void DirectXDrawDarknessOverlay(void)
{
    if (!g_darknessOverlayParams.enabled ||
        g_darknessOverlayPixelShader < 0 ||
        g_darknessOverlayConstantBuffer < 0)
    {
        return;
    }

    auto* buffer = static_cast<DarknessOverlayShaderParams*>(GetBufferShaderConstantBuffer(g_darknessOverlayConstantBuffer));
    if (!buffer)
    {
        return;
    }

    buffer->screenWidth = static_cast<float>(SCREEN_WIDTH);
    buffer->screenHeight = static_cast<float>(SCREEN_HEIGHT);
    buffer->pad0 = 0.0f;
    buffer->pad1 = 0.0f;
    buffer->viewLeft = g_darknessOverlayParams.viewLeft;
    buffer->viewTop = g_darknessOverlayParams.viewTop;
    buffer->viewRight = g_darknessOverlayParams.viewRight;
    buffer->viewBottom = g_darknessOverlayParams.viewBottom;
    buffer->darknessOpacity = std::clamp(g_darknessOverlayParams.darknessOpacity, 0.0f, 1.0f);
    buffer->lightCount = static_cast<float>(std::clamp(g_darknessOverlayParams.lightCount, 0, kMaxDarknessOverlayLights));
    buffer->pad2 = 0.0f;
    buffer->pad3 = 0.0f;
    buffer->colorR = std::clamp(g_darknessOverlayParams.colorR, 0.0f, 1.0f);
    buffer->colorG = std::clamp(g_darknessOverlayParams.colorG, 0.0f, 1.0f);
    buffer->colorB = std::clamp(g_darknessOverlayParams.colorB, 0.0f, 1.0f);
    buffer->pad4 = 0.0f;
    for (int i = 0; i < kMaxDarknessOverlayLights * 4; ++i)
    {
        buffer->lightParams[i] = 0.0f;
    }
    for (int i = 0; i < kMaxDarknessOverlayLights; ++i)
    {
        buffer->lightIntensityPack[i] = 0.0f;
    }
    for (int i = 0; i < kMaxDarknessOverlayLights * 4; ++i)
    {
        buffer->lightColors[i] = 0.0f;
        buffer->lightShapeData[i] = 0.0f;
    }

    const int lightCount = std::clamp(g_darknessOverlayParams.lightCount, 0, kMaxDarknessOverlayLights);
    for (int lightIndex = 0; lightIndex < lightCount; ++lightIndex)
    {
        const DarknessOverlayLight& light = g_darknessOverlayParams.lights[lightIndex];
        const int baseIndex = lightIndex * 4;
        buffer->lightParams[baseIndex + 0] = light.centerX;
        buffer->lightParams[baseIndex + 1] = light.centerY;
        buffer->lightParams[baseIndex + 2] = light.innerRadius;
        buffer->lightParams[baseIndex + 3] = (std::max)(light.outerRadius, light.innerRadius + 0.001f);
        buffer->lightIntensityPack[lightIndex] = std::clamp(light.intensity, 0.0f, 1.0f);
        buffer->lightColors[baseIndex + 0] = std::clamp(light.colorR, 0.0f, 1.0f);
        buffer->lightColors[baseIndex + 1] = std::clamp(light.colorG, 0.0f, 1.0f);
        buffer->lightColors[baseIndex + 2] = std::clamp(light.colorB, 0.0f, 1.0f);
        buffer->lightColors[baseIndex + 3] = std::clamp(light.intensity, 0.0f, 1.0f);
        buffer->lightShapeData[baseIndex + 0] = light.shapeType;
        buffer->lightShapeData[baseIndex + 1] = light.extentX;
        buffer->lightShapeData[baseIndex + 2] = light.extentY;
        buffer->lightShapeData[baseIndex + 3] = light.outerRadius;
    }
    UpdateShaderConstantBuffer(g_darknessOverlayConstantBuffer);
    SetShaderConstantBuffer(g_darknessOverlayConstantBuffer, DX_SHADERTYPE_PIXEL, 0);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetUseTextureToShader(0, -1);
    SetUseTextureToShader(1, -1);
    SetUsePixelShader(g_darknessOverlayPixelShader);
    SetUseVertexShader(-1);
    DrawFullscreenCompositeQuad();

    SetUsePixelShader(-1);
    SetUseTextureToShader(0, -1);
    SetUseTextureToShader(1, -1);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void Present(void)
{
    SetDrawScreen(DX_SCREEN_BACK);
    ScreenFlip();
}

void DirectXSetBlendMode(BlendMode2D blendMode)
{
    g_blendMode = blendMode;
    const int dxBlendMode = blendMode == BlendMode2D::Additive ? DX_BLENDMODE_ADD : DX_BLENDMODE_ALPHA;
    SetDrawBlendMode(dxBlendMode, 255);
}

void DirectXGetBackbufferSize(int& w, int& h)
{
    w = g_BackBufferW;
    h = g_BackBufferH;
}
