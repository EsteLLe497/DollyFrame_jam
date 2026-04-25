#ifndef DIRECTX_H
#define DIRECTX_H

#include <windows.h>

void* DirectXGetSwapChain(void);

enum class BlendMode2D
{
    Alpha,
    Additive,
};

constexpr int kMaxDarknessOverlayLights = 16;

struct DarknessOverlayLight
{
    float centerX = 0.0f;
    float centerY = 0.0f;
    float shapeType = 0.0f;
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
    float extentX = 0.0f;
    float extentY = 0.0f;
    float intensity = 1.0f;
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;
};

struct DarknessOverlayParams
{
    bool enabled = false;
    int lightCount = 0;
    DarknessOverlayLight lights[kMaxDarknessOverlayLights] = {};
    float darknessOpacity = 0.0f;
    float viewLeft = 0.0f;
    float viewTop = 0.0f;
    float viewRight = 0.0f;
    float viewBottom = 0.0f;
    float colorR = 0.0f;
    float colorG = 0.0f;
    float colorB = 0.0f;
};

void DirectXInitialize(HWND hWnd);
void DirectXFinalaize(void);
void DirectXResize(int width, int height);

void* DirectXGetDevice(void);
void* DirectXGetDeviceContext(void);

void Clear(void);
void Present(void);
void DirectXSetBlendMode(BlendMode2D blendMode);
void DirectXBeginSceneRender(void);
void DirectXCompositeSceneToBackBuffer(float timeSeconds);
bool DirectXHasPostProcess(void);
void DirectXSetPostProcessEnabled(bool enabled);
void DirectXTogglePostProcess(void);
bool DirectXIsPostProcessEnabled(void);
bool DirectXHasDarknessOverlay(void);
void DirectXResetDarknessOverlay(void);
void DirectXSetDarknessOverlay(const DarknessOverlayParams& params);
void DirectXDrawDarknessOverlay(void);

extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;

void DirectXGetBackbufferSize(int& w, int& h);

#endif // !DIRECTX_H

