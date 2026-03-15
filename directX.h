#ifndef DIRECTX_H
#define DIRECTX_H

#include <windows.h>

void* DirectXGetSwapChain(void);

enum class BlendMode2D
{
    Alpha,
    Additive,
};

void DirectXInitialize(HWND hWnd);
void DirectXFinalaize(void);
void DirectXResize(int width, int height);

void* DirectXGetDevice(void);
void* DirectXGetDeviceContext(void);

void Clear(void);
void Present(void);
void DirectXSetBlendMode(BlendMode2D blendMode);

extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;

void DirectXGetBackbufferSize(int& w, int& h);

#endif // !DIRECTX_H

