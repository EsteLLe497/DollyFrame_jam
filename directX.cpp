#include "directX.h"

#pragma comment(lib, "d3d11.lib")

int SCREEN_WIDTH = 1280;
int SCREEN_HEIGHT = 720;

static int g_BackBufferW = 0;
static int g_BackBufferH = 0;

static ID3D11Device* g_Device = nullptr;
static ID3D11DeviceContext* g_DeviceContext = nullptr;
static IDXGISwapChain* g_SwapChain = nullptr;
static ID3D11Texture2D* g_DepthStencilTexture = nullptr;
static ID3D11RenderTargetView* g_RenderTargetView = nullptr;
static ID3D11DepthStencilView* g_DepthStencilView = nullptr;
static ID3D11RasterizerState* g_RasterizerState = nullptr;
static ID3D11BlendState* g_BlendStateAlpha = nullptr;
static ID3D11BlendState* g_BlendStateAdditive = nullptr;
static ID3D11DepthStencilState* g_DepthStencilStateDepthDisable = nullptr;

static void ApplyBlendState(BlendMode2D blendMode)
{
    ID3D11BlendState* blendState = blendMode == BlendMode2D::Additive ? g_BlendStateAdditive : g_BlendStateAlpha;
    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_DeviceContext->OMSetBlendState(blendState, blendFactor, 0xffffffff);
}

static void ReleaseRenderTargets()
{
    SAFE_RELEASE(g_DepthStencilView);
    SAFE_RELEASE(g_DepthStencilTexture);
    SAFE_RELEASE(g_RenderTargetView);
}

static bool CreateRenderTargets()
{
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_RenderTargetView);
    backBuffer->Release();
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = static_cast<UINT>(g_BackBufferW);
    depthDesc.Height = static_cast<UINT>(g_BackBufferH);
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = g_Device->CreateTexture2D(&depthDesc, nullptr, &g_DepthStencilTexture);
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_Device->CreateDepthStencilView(g_DepthStencilTexture, nullptr, &g_DepthStencilView);
    if (FAILED(hr))
    {
        return false;
    }

    g_DeviceContext->OMSetRenderTargets(1, &g_RenderTargetView, g_DepthStencilView);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<FLOAT>(g_BackBufferW);
    viewport.Height = static_cast<FLOAT>(g_BackBufferH);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    g_DeviceContext->RSSetViewports(1, &viewport);
    return true;
}

void DirectXGetBackbufferSize(int& w, int& h)
{
    w = g_BackBufferW;
    h = g_BackBufferH;
}

void DirectXInitialize(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    SCREEN_WIDTH = rc.right - rc.left;
    SCREEN_HEIGHT = rc.bottom - rc.top;
    g_BackBufferW = SCREEN_WIDTH;
    g_BackBufferH = SCREEN_HEIGHT;

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = static_cast<UINT>(g_BackBufferW);
    sd.BufferDesc.Height = static_cast<UINT>(g_BackBufferH);
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &g_SwapChain,
        &g_Device,
        &featureLevel,
        &g_DeviceContext);

    if (FAILED(hr))
    {
        MessageBoxA(hWnd, "Failed to initialize Direct3D 11.", "Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hWnd);
        return;
    }

    if (!CreateRenderTargets())
    {
        MessageBoxA(hWnd, "Failed to create Direct3D render targets.", "Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hWnd);
        return;
    }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;
    g_Device->CreateRasterizerState(&rd, &g_RasterizerState);
    g_DeviceContext->RSSetState(g_RasterizerState);

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_Device->CreateBlendState(&bd, &g_BlendStateAlpha);

    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    g_Device->CreateBlendState(&bd, &g_BlendStateAdditive);
    ApplyBlendState(BlendMode2D::Alpha);

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    g_Device->CreateDepthStencilState(&dsd, &g_DepthStencilStateDepthDisable);
    g_DeviceContext->OMSetDepthStencilState(g_DepthStencilStateDepthDisable, 0);
}

void DirectXResize(int width, int height)
{
    if (!g_SwapChain || width <= 0 || height <= 0)
    {
        return;
    }

    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;
    g_BackBufferW = width;
    g_BackBufferH = height;

    g_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    ReleaseRenderTargets();
    g_SwapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE);
    CreateRenderTargets();
}

void DirectXFinalaize(void)
{
    ReleaseRenderTargets();
    SAFE_RELEASE(g_DepthStencilStateDepthDisable);
    SAFE_RELEASE(g_BlendStateAdditive);
    SAFE_RELEASE(g_BlendStateAlpha);
    SAFE_RELEASE(g_RasterizerState);
    SAFE_RELEASE(g_SwapChain);
    SAFE_RELEASE(g_DeviceContext);
    SAFE_RELEASE(g_Device);
}

ID3D11Device* DirectXGetDevice(void)
{
    return g_Device;
}

ID3D11DeviceContext* DirectXGetDeviceContext(void)
{
    return g_DeviceContext;
}

IDXGISwapChain* DirectXGetSwapChain(void)
{
    return g_SwapChain;
}

void Clear(void)
{
    const float clearColor[4] = { 0.08f, 0.09f, 0.12f, 1.0f };
    g_DeviceContext->ClearRenderTargetView(g_RenderTargetView, clearColor);
    g_DeviceContext->ClearDepthStencilView(g_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Present(void)
{
    g_SwapChain->Present(1, 0);
}

void DirectXSetBlendMode(BlendMode2D blendMode)
{
    if (!g_DeviceContext)
    {
        return;
    }

    ApplyBlendState(blendMode);
}
