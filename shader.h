#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

enum class ShaderEffect2D
{
    Normal,
    Grayscale,
    Outline,
    Flash,
    UVScroll,
    Dissolve,
    MaskClip,
    Distortion,
    PaletteSwap,
    Posterize,
    ChromaticAberration,
    Glitch,
    Pixelate,
    Wave,
    RimLight,
    GradientMap,
    NoiseReveal,
    HeatOverlay,
    Parallax,
    NormalMapLighting,
};

enum class ShaderBlendMode2D
{
    Alpha,
    Additive,
};

bool Shader_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_Finalize();
void Shader_SetMatrix(const DirectX::XMMATRIX& matrix);
void Shader_Begin();
void Shader_SetTint(float r, float g, float b, float a);
void Shader_SetEffect(ShaderEffect2D effect);
void Shader_SetBlendMode(ShaderBlendMode2D blendMode);
void Shader_SetOutline(float r, float g, float b, float a, float thickness);
void Shader_SetFlash(float r, float g, float b, float a, float intensity);
void Shader_SetUVScroll(float scrollU, float scrollV);
void Shader_SetDissolve(float threshold, float edgeWidth, float edgeR, float edgeG, float edgeB, float edgeA);
void Shader_SetMaskClip(float threshold, float softness);
void Shader_SetDistortion(float strengthU, float strengthV, float time, float tintStrength);
void Shader_SetPaletteSwap(
    float sourceR, float sourceG, float sourceB, float sourceA,
    float targetR, float targetG, float targetB, float targetA,
    float threshold);
void Shader_SetPosterize(float levels, float contrast);
void Shader_SetChromaticAberration(float amount, float time);
void Shader_SetGlitch(float amount, float time, float scanlineStrength);
void Shader_SetPixelate(float pixelWidth, float pixelHeight);
void Shader_SetWave(float amplitudeU, float amplitudeV, float frequency, float time);
void Shader_SetRimLight(float r, float g, float b, float a, float power);
void Shader_SetGradientMap(
    float darkR, float darkG, float darkB, float darkA,
    float brightR, float brightG, float brightB, float brightA,
    float contrast);
void Shader_SetNoiseReveal(float threshold, float edgeWidth, float time, float edgeR, float edgeG, float edgeB, float edgeA);
void Shader_SetHeatOverlay(float intensity, float pulseTime);
void Shader_SetParallax(float backSpeed, float frontSpeed, float mixRatio, float time);
void Shader_SetNormalMapLighting(float lightX, float lightY, float lightZ, float ambient, float intensity);
void Shader_SetAuxTexture(int textureID);
void Shader_BindSpriteTextures(int primaryTextureID);
void Shader_SetTextureSize(float width, float height);
void Shader_ResetStyle();
