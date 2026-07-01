#include "pch.h"

#include "directX.h"

#include "DxLib.h"

#include <algorithm>
#include <cmath>

int SCREEN_WIDTH = kVirtualScreenWidth;
int SCREEN_HEIGHT = kVirtualScreenHeight;

namespace
{
    int g_OutputWidth = kVirtualScreenWidth;
    int g_OutputHeight = kVirtualScreenHeight;
    int g_BackBufferW = kVirtualScreenWidth;
    int g_BackBufferH = kVirtualScreenHeight;
    int g_PresentationX = 0;
    int g_PresentationY = 0;
    int g_PresentationW = kVirtualScreenWidth;
    int g_PresentationH = kVirtualScreenHeight;
    BlendMode2D g_blendMode = BlendMode2D::Alpha;
    int g_sceneRenderTarget = -1;
    int g_compositeRenderTarget = -1;
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
    bool g_sceneCompositedThisFrame = false;
    float g_postProcessVignetteStrength = 0.08f;
    float g_postProcessVignetteRadiusX = 0.72f;
    float g_postProcessVignetteRadiusY = 0.72f;
    float g_postProcessVignetteSoftness = 0.70f;
    float g_postProcessPlayerLightCenterX = static_cast<float>(kVirtualScreenWidth) * 0.5f;
    float g_postProcessPlayerLightCenterY = static_cast<float>(kVirtualScreenHeight) * 0.5f;
    float g_postProcessPlayerLightStrength = 0.0f;
    float g_postProcessPlayerLightRadius = 120.0f;
    float g_postProcessPlayerLightSoftness = 170.0f;
    DarknessOverlayParams g_darknessOverlayParams = {};

    struct PostProcessParams
    {
        float param0;
        float param1;
        float param2;
        float param3;
        float param4;
        float param5;
        float param6;
        float param7;
        float param8;
        float param9;
        float param10;
        float param11;
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
        float lightColors[kMaxDarknessOverlayLights * 4];
        float lightShapeData[kMaxDarknessOverlayLights * 4];
    };

    void ResetDarknessOverlayShaderParams(DarknessOverlayShaderParams& buffer)
    {
        buffer = {};
    }

    void WriteDarknessOverlayLight(
        DarknessOverlayShaderParams& buffer,
        int lightIndex,
        const DarknessOverlayLight& light)
    {
        const int baseIndex = lightIndex * 4;
        buffer.lightParams[baseIndex + 0] = light.centerX;
        buffer.lightParams[baseIndex + 1] = light.centerY;
        buffer.lightParams[baseIndex + 2] = light.innerRadius;
        buffer.lightParams[baseIndex + 3] = (std::max)(light.outerRadius, light.innerRadius + 0.001f);
        buffer.lightColors[baseIndex + 0] = std::clamp(light.colorR, 0.0f, 1.0f);
        buffer.lightColors[baseIndex + 1] = std::clamp(light.colorG, 0.0f, 1.0f);
        buffer.lightColors[baseIndex + 2] = std::clamp(light.colorB, 0.0f, 1.0f);
        buffer.lightColors[baseIndex + 3] = std::clamp(light.intensity, 0.0f, 1.0f);
        buffer.lightShapeData[baseIndex + 0] = light.shapeType;
        buffer.lightShapeData[baseIndex + 1] = light.extentX;
        buffer.lightShapeData[baseIndex + 2] = light.extentY;
        buffer.lightShapeData[baseIndex + 3] = light.outerRadius;
    }

    void PopulateDarknessOverlayShaderParams(
        DarknessOverlayShaderParams& buffer,
        const DarknessOverlayParams& params)
    {
        buffer.screenWidth = static_cast<float>(kVirtualScreenWidth);
        buffer.screenHeight = static_cast<float>(kVirtualScreenHeight);
        buffer.pad0 = 0.0f;
        buffer.pad1 = 0.0f;
        buffer.viewLeft = params.viewLeft;
        buffer.viewTop = params.viewTop;
        buffer.viewRight = params.viewRight;
        buffer.viewBottom = params.viewBottom;
        buffer.darknessOpacity = std::clamp(params.darknessOpacity, 0.0f, 1.0f);
        buffer.lightCount = static_cast<float>(std::clamp(params.lightCount, 0, kMaxDarknessOverlayLights));
        buffer.pad2 = 0.0f;
        buffer.pad3 = 0.0f;
        buffer.colorR = std::clamp(params.colorR, 0.0f, 1.0f);
        buffer.colorG = std::clamp(params.colorG, 0.0f, 1.0f);
        buffer.colorB = std::clamp(params.colorB, 0.0f, 1.0f);
        buffer.pad4 = 0.0f;

        const int lightCount = std::clamp(params.lightCount, 0, kMaxDarknessOverlayLights);
        for (int lightIndex = 0; lightIndex < lightCount; ++lightIndex)
        {
            WriteDarknessOverlayLight(buffer, lightIndex, params.lights[lightIndex]);
        }
    }

    void UpdatePresentationRect();

    void RefreshOutputMetrics(HWND windowHandle)
    {
        int drawScreenWidth = 0;
        int drawScreenHeight = 0;
        GetDrawScreenSize(&drawScreenWidth, &drawScreenHeight);
        if (drawScreenWidth > 0 && drawScreenHeight > 0)
        {
            g_OutputWidth = (std::max)(1, drawScreenWidth);
            g_OutputHeight = (std::max)(1, drawScreenHeight);
            UpdatePresentationRect();
            return;
        }

        if (windowHandle != nullptr)
        {
            RECT clientRect = {};
            if (GetClientRect(windowHandle, &clientRect))
            {
                g_OutputWidth = (std::max)(1, static_cast<int>(clientRect.right - clientRect.left));
                g_OutputHeight = (std::max)(1, static_cast<int>(clientRect.bottom - clientRect.top));
                UpdatePresentationRect();
                return;
            }
        }

        g_OutputWidth = kVirtualScreenWidth;
        g_OutputHeight = kVirtualScreenHeight;
        UpdatePresentationRect();
    }

    void UpdatePresentationRect()
    {
        const float scale = std::min(
            static_cast<float>(g_OutputWidth) / static_cast<float>(kVirtualScreenWidth),
            static_cast<float>(g_OutputHeight) / static_cast<float>(kVirtualScreenHeight));
        const float clampedScale = std::max(scale, 0.0f);
        g_PresentationW = std::max(1, static_cast<int>(std::round(static_cast<float>(kVirtualScreenWidth) * clampedScale)));
        g_PresentationH = std::max(1, static_cast<int>(std::round(static_cast<float>(kVirtualScreenHeight) * clampedScale)));
        g_PresentationX = (g_OutputWidth - g_PresentationW) / 2;
        g_PresentationY = (g_OutputHeight - g_PresentationH) / 2;
    }

    void DrawPresentationGraph(int sourceHandle)
    {
        if (sourceHandle < 0)
        {
            return;
        }

        const int previousDrawMode = GetDrawMode();
        SetDrawMode(DX_DRAWMODE_NEAREST);
        DrawExtendGraph(
            g_PresentationX,
            g_PresentationY,
            g_PresentationX + g_PresentationW,
            g_PresentationY + g_PresentationH,
            sourceHandle,
            FALSE);
        SetDrawMode(previousDrawMode);
    }

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
        if (g_compositeRenderTarget >= 0)
        {
            DeleteGraph(g_compositeRenderTarget);
            g_compositeRenderTarget = -1;
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

        g_sceneRenderTarget = MakeScreen(kVirtualScreenWidth, kVirtualScreenHeight, TRUE);
        if (g_sceneRenderTarget < 0)
        {
            return;
        }

        g_compositeRenderTarget = MakeScreen(kVirtualScreenWidth, kVirtualScreenHeight, TRUE);
        if (g_compositeRenderTarget < 0)
        {
            return;
        }

        g_lightMaskRenderTarget = MakeScreen(kVirtualScreenWidth, kVirtualScreenHeight, TRUE);
        if (g_lightMaskRenderTarget < 0)
        {
            return;
        }

        g_lightBlurRenderTarget = MakeScreen(kVirtualScreenWidth, kVirtualScreenHeight, TRUE);
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

        vertices[1].pos = VGet(static_cast<float>(kVirtualScreenWidth), 0.0f, 0.0f);
        vertices[1].rhw = 1.0f;
        vertices[1].dif = white;
        vertices[1].spc = white;
        vertices[1].u = 1.0f;
        vertices[1].v = 0.0f;

        vertices[2].pos = VGet(0.0f, static_cast<float>(kVirtualScreenHeight), 0.0f);
        vertices[2].rhw = 1.0f;
        vertices[2].dif = white;
        vertices[2].spc = white;
        vertices[2].u = 0.0f;
        vertices[2].v = 1.0f;

        vertices[3].pos = VGet(static_cast<float>(kVirtualScreenWidth), static_cast<float>(kVirtualScreenHeight), 0.0f);
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
    SCREEN_WIDTH = kVirtualScreenWidth;
    SCREEN_HEIGHT = kVirtualScreenHeight;
    RefreshOutputMetrics(hWnd);
    g_BackBufferW = SCREEN_WIDTH;
    g_BackBufferH = SCREEN_HEIGHT;
    CreatePostProcessResources();
    SetDrawScreen(g_sceneRenderTarget >= 0 ? g_sceneRenderTarget : DX_SCREEN_BACK);
    ClearDrawScreen();
}

void DirectXFinalaize(void)
{
    ReleasePostProcessResources();
}

void DirectXResize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    g_OutputWidth = width;
    g_OutputHeight = height;
    UpdatePresentationRect();
}

void* DirectXGetDevice(void)
{
    return const_cast<void*>(GetUseDirect3D11Device());
}

void* DirectXGetDeviceContext(void)
{
    return const_cast<void*>(GetUseDirect3D11DeviceContext());
}

void Clear(void)
{
    ClearDrawScreen();
}

void DirectXBeginSceneRender(void)
{
    DirectXResetDarknessOverlay();
    g_sceneCompositedThisFrame = false;
    SetDrawScreen(g_sceneRenderTarget >= 0 ? g_sceneRenderTarget : DX_SCREEN_BACK);
    ClearDrawScreen();
}

void DirectXCompositeSceneToBackBuffer(float timeSeconds)
{
    if (g_sceneCompositedThisFrame)
    {
        SetDrawScreen(g_compositeRenderTarget >= 0 ? g_compositeRenderTarget : (g_sceneRenderTarget >= 0 ? g_sceneRenderTarget : DX_SCREEN_BACK));
        return;
    }

    if (g_sceneRenderTarget < 0)
    {
        SetDrawScreen(DX_SCREEN_BACK);
        g_sceneCompositedThisFrame = true;
        return;
    }

    if (!g_postProcessEnabled ||
        g_compositeRenderTarget < 0 ||
        !g_postProcessReady ||
        g_lightMaskRenderTarget < 0 ||
        g_lightBlurRenderTarget < 0 ||
        g_lightExtractPixelShader < 0 ||
        g_lightBlurVPixelShader < 0 ||
        g_lightBlurHPixelShader < 0 ||
        g_postProcessPixelShader < 0 ||
        g_postProcessConstantBuffer < 0)
    {
        if (g_compositeRenderTarget >= 0)
        {
            SetDrawScreen(g_compositeRenderTarget);
            ClearDrawScreen();
            DrawExtendGraph(0, 0, kVirtualScreenWidth, kVirtualScreenHeight, g_sceneRenderTarget, FALSE);
        }
        SetDrawScreen(g_compositeRenderTarget >= 0 ? g_compositeRenderTarget : g_sceneRenderTarget);
        g_sceneCompositedThisFrame = true;
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
        buffer->param0 = 1.0f / static_cast<float>(kVirtualScreenHeight);
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
        buffer->param0 = 1.0f / static_cast<float>(kVirtualScreenWidth);
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

    SetDrawScreen(g_compositeRenderTarget);
    ClearDrawScreen();
    if (buffer)
    {
        buffer->param0 = timeSeconds;
        buffer->param1 = g_postProcessVignetteStrength;
        buffer->param2 = 0.008f; // grain
        buffer->param3 = 0.52f; // light blend
        buffer->param4 = g_postProcessVignetteRadiusX;
        buffer->param5 = g_postProcessVignetteRadiusY;
        buffer->param6 = g_postProcessVignetteSoftness;
        buffer->param7 = g_postProcessPlayerLightCenterX;
        buffer->param8 = g_postProcessPlayerLightCenterY;
        buffer->param9 = g_postProcessPlayerLightStrength;
        buffer->param10 = g_postProcessPlayerLightRadius;
        buffer->param11 = g_postProcessPlayerLightSoftness;
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
    g_sceneCompositedThisFrame = true;
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

void DirectXSetPostProcessVignette(float strength, float radiusX, float radiusY, float softness)
{
    g_postProcessVignetteStrength = std::clamp(strength, 0.0f, 1.0f);
    g_postProcessVignetteRadiusX = std::max(0.001f, radiusX);
    g_postProcessVignetteRadiusY = std::max(0.001f, radiusY);
    g_postProcessVignetteSoftness = std::max(0.001f, softness);
}

void DirectXSetPostProcessPlayerLight(float centerX, float centerY, float strength, float radius, float softness)
{
    g_postProcessPlayerLightCenterX = std::clamp(centerX, 0.0f, static_cast<float>(kVirtualScreenWidth));
    g_postProcessPlayerLightCenterY = std::clamp(centerY, 0.0f, static_cast<float>(kVirtualScreenHeight));
    g_postProcessPlayerLightStrength = std::clamp(strength, 0.0f, 1.0f);
    g_postProcessPlayerLightRadius = std::max(0.001f, radius);
    g_postProcessPlayerLightSoftness = std::max(0.001f, softness);
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

    ResetDarknessOverlayShaderParams(*buffer);
    PopulateDarknessOverlayShaderParams(*buffer, g_darknessOverlayParams);
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
    RefreshOutputMetrics(GetMainWindowHandle());
    SetDrawScreen(DX_SCREEN_BACK);
    ClearDrawScreen();
    const int sourceHandle = g_compositeRenderTarget >= 0 ? g_compositeRenderTarget : g_sceneRenderTarget;
    DrawPresentationGraph(sourceHandle);
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

void DirectXGetOutputSize(int& w, int& h)
{
    w = g_OutputWidth;
    h = g_OutputHeight;
}

void DirectXGetPresentationRect(int& x, int& y, int& w, int& h)
{
    x = g_PresentationX;
    y = g_PresentationY;
    w = g_PresentationW;
    h = g_PresentationH;
}

int DirectXMapWindowToVirtualX(int x)
{
    if (g_PresentationW <= 0)
    {
        return 0;
    }

    const float normalized = static_cast<float>(x - g_PresentationX) / static_cast<float>(g_PresentationW);
    const float virtualX = normalized * static_cast<float>(SCREEN_WIDTH);
    return static_cast<int>(std::round(std::clamp(virtualX, 0.0f, static_cast<float>(SCREEN_WIDTH))));
}

int DirectXMapWindowToVirtualY(int y)
{
    if (g_PresentationH <= 0)
    {
        return 0;
    }

    const float normalized = static_cast<float>(y - g_PresentationY) / static_cast<float>(g_PresentationH);
    const float virtualY = normalized * static_cast<float>(SCREEN_HEIGHT);
    return static_cast<int>(std::round(std::clamp(virtualY, 0.0f, static_cast<float>(SCREEN_HEIGHT))));
}
