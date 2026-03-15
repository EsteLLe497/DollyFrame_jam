#include "directX.h"

#include "DxLib.h"

int SCREEN_WIDTH = 1280;
int SCREEN_HEIGHT = 720;

namespace
{
    int g_BackBufferW = SCREEN_WIDTH;
    int g_BackBufferH = SCREEN_HEIGHT;
    BlendMode2D g_blendMode = BlendMode2D::Alpha;
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
    SetDrawScreen(DX_SCREEN_BACK);
    ClearDrawScreen();
}

void DirectXFinalaize(void)
{
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

void Present(void)
{
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
