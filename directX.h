#ifndef DIRECTX_H
#define DIRECTX_H

#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgi.h>

IDXGISwapChain* DirectXGetSwapChain(void);
using namespace DirectX;

enum class BlendMode2D
{
    Alpha,
    Additive,
};

#define SAFE_RELEASE(o) if(o){(o)->Release(); o = NULL;}

void DirectXInitialize(HWND hWnd);
void DirectXFinalaize(void);
void DirectXResize(int width, int height);

ID3D11Device* DirectXGetDevice(void);
ID3D11DeviceContext* DirectXGetDeviceContext(void);

void Clear(void);
void Present(void);
void DirectXSetBlendMode(BlendMode2D blendMode);

extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;

void DirectXGetBackbufferSize(int& w, int& h);

#endif // !DIRECTX_H

