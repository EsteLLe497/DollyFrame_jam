#include "directX.h"

#include "DxLib.h"

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
    bool g_postProcessReady = false;
    bool g_postProcessEnabled = true;

    struct PostProcessParams
    {
        float param0;
        float param1;
        float param2;
        float param3;
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

        g_postProcessConstantBuffer = CreateShaderConstantBuffer(16);
        if (g_postProcessConstantBuffer < 0)
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
