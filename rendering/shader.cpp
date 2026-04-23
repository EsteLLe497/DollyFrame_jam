#include "shader.h"

#include <algorithm>

namespace
{
    ShaderEffect2D g_effect = ShaderEffect2D::Normal;
    float g_tintR = 1.0f;
    float g_tintG = 1.0f;
    float g_tintB = 1.0f;
    float g_tintA = 1.0f;
    ShaderBlendMode2D g_blendMode = ShaderBlendMode2D::Alpha;
    float g_uvScrollU = 0.0f;
    float g_uvScrollV = 0.0f;
    float g_distortionStrengthU = 0.0f;
    float g_distortionStrengthV = 0.0f;
    float g_distortionTime = 0.0f;
    float g_distortionTintStrength = 0.0f;
    float g_parallaxBackSpeed = 0.0f;
    float g_parallaxFrontSpeed = 0.0f;
    float g_parallaxMixRatio = 0.0f;
    float g_parallaxTime = 0.0f;
}

ShaderSupportLevel Shader_GetEffectSupport(ShaderEffect2D effect)
{
    switch (effect)
    {
    case ShaderEffect2D::Normal:
    case ShaderEffect2D::Grayscale:
    case ShaderEffect2D::Outline:
    case ShaderEffect2D::Flash:
    case ShaderEffect2D::UVScroll:
    case ShaderEffect2D::PaletteSwap:
    case ShaderEffect2D::Posterize:
    case ShaderEffect2D::Pixelate:
    case ShaderEffect2D::RimLight:
    case ShaderEffect2D::GradientMap:
        return ShaderSupportLevel::Supported;

    case ShaderEffect2D::Dissolve:
    case ShaderEffect2D::MaskClip:
    case ShaderEffect2D::Distortion:
    case ShaderEffect2D::ChromaticAberration:
    case ShaderEffect2D::Glitch:
    case ShaderEffect2D::Wave:
    case ShaderEffect2D::NoiseReveal:
    case ShaderEffect2D::HeatOverlay:
    case ShaderEffect2D::Parallax:
        return ShaderSupportLevel::Approximate;

    case ShaderEffect2D::NormalMapLighting:
        return ShaderSupportLevel::Unsupported;
    }

    return ShaderSupportLevel::Unsupported;
}

const char* Shader_GetEffectSupportText(ShaderEffect2D effect)
{
    switch (Shader_GetEffectSupport(effect))
    {
    case ShaderSupportLevel::Supported:
        return "Supported";
    case ShaderSupportLevel::Approximate:
        return "Approximate";
    case ShaderSupportLevel::Unsupported:
        return "Unsupported";
    }

    return "Unsupported";
}

bool Shader_Initialize(void* pDevice, void* pContext)
{
    static_cast<void>(pDevice);
    static_cast<void>(pContext);
    Shader_ResetStyle();
    return true;
}

void Shader_Finalize()
{
}

void Shader_SetTint(float r, float g, float b, float a)
{
    g_tintR = r;
    g_tintG = g;
    g_tintB = b;
    g_tintA = a;
}

void Shader_SetEffect(ShaderEffect2D effect)
{
    g_effect = effect;
}

void Shader_SetBlendMode(ShaderBlendMode2D blendMode)
{
    g_blendMode = blendMode;
}

void Shader_SetOutline(float r, float g, float b, float a, float thickness)
{
    static_cast<void>(thickness);
    g_effect = ShaderEffect2D::Outline;
    Shader_SetTint(r, g, b, a);
}

void Shader_SetFlash(float r, float g, float b, float a, float intensity)
{
    g_effect = ShaderEffect2D::Flash;
    const float boost = std::clamp(intensity, 0.0f, 1.0f);
    Shader_SetTint(
        std::clamp(r * (0.5f + boost), 0.0f, 1.0f),
        std::clamp(g * (0.5f + boost), 0.0f, 1.0f),
        std::clamp(b * (0.5f + boost), 0.0f, 1.0f),
        a);
}

void Shader_SetUVScroll(float scrollU, float scrollV)
{
    g_effect = ShaderEffect2D::UVScroll;
    g_uvScrollU = scrollU;
    g_uvScrollV = scrollV;
}

void Shader_SetDissolve(float threshold, float edgeWidth, float edgeR, float edgeG, float edgeB, float edgeA)
{
    g_effect = ShaderEffect2D::Dissolve;
    static_cast<void>(threshold);
    static_cast<void>(edgeWidth);
    Shader_SetTint(edgeR, edgeG, edgeB, edgeA);
}

void Shader_SetMaskClip(float threshold, float softness)
{
    g_effect = ShaderEffect2D::MaskClip;
    static_cast<void>(threshold);
    static_cast<void>(softness);
}

void Shader_SetDistortion(float strengthU, float strengthV, float time, float tintStrength)
{
    g_effect = ShaderEffect2D::Distortion;
    g_distortionStrengthU = strengthU;
    g_distortionStrengthV = strengthV;
    g_distortionTime = time;
    g_distortionTintStrength = tintStrength;
}

void Shader_SetPaletteSwap(
    float sourceR, float sourceG, float sourceB, float sourceA,
    float targetR, float targetG, float targetB, float targetA,
    float threshold)
{
    g_effect = ShaderEffect2D::PaletteSwap;
    static_cast<void>(sourceR);
    static_cast<void>(sourceG);
    static_cast<void>(sourceB);
    static_cast<void>(sourceA);
    static_cast<void>(threshold);
    Shader_SetTint(targetR, targetG, targetB, targetA);
}

void Shader_SetPosterize(float levels, float contrast)
{
    g_effect = ShaderEffect2D::Posterize;
    static_cast<void>(levels);
    static_cast<void>(contrast);
}

void Shader_SetChromaticAberration(float amount, float time)
{
    g_effect = ShaderEffect2D::ChromaticAberration;
    static_cast<void>(amount);
    static_cast<void>(time);
}

void Shader_SetGlitch(float amount, float time, float scanlineStrength)
{
    g_effect = ShaderEffect2D::Glitch;
    static_cast<void>(amount);
    static_cast<void>(time);
    static_cast<void>(scanlineStrength);
}

void Shader_SetPixelate(float pixelWidth, float pixelHeight)
{
    g_effect = ShaderEffect2D::Pixelate;
    static_cast<void>(pixelWidth);
    static_cast<void>(pixelHeight);
}

void Shader_SetWave(float amplitudeU, float amplitudeV, float frequency, float time)
{
    g_effect = ShaderEffect2D::Wave;
    static_cast<void>(amplitudeU);
    static_cast<void>(amplitudeV);
    static_cast<void>(frequency);
    static_cast<void>(time);
}

void Shader_SetRimLight(float r, float g, float b, float a, float power)
{
    static_cast<void>(power);
    g_effect = ShaderEffect2D::RimLight;
    Shader_SetTint(r, g, b, a);
}

void Shader_SetGradientMap(
    float darkR, float darkG, float darkB, float darkA,
    float brightR, float brightG, float brightB, float brightA,
    float contrast)
{
    g_effect = ShaderEffect2D::GradientMap;
    static_cast<void>(darkR);
    static_cast<void>(darkG);
    static_cast<void>(darkB);
    static_cast<void>(darkA);
    static_cast<void>(contrast);
    Shader_SetTint(brightR, brightG, brightB, brightA);
}

void Shader_SetNoiseReveal(float threshold, float edgeWidth, float time, float edgeR, float edgeG, float edgeB, float edgeA)
{
    g_effect = ShaderEffect2D::NoiseReveal;
    static_cast<void>(threshold);
    static_cast<void>(edgeWidth);
    static_cast<void>(time);
    Shader_SetTint(edgeR, edgeG, edgeB, edgeA);
}

void Shader_SetHeatOverlay(float intensity, float pulseTime)
{
    g_effect = ShaderEffect2D::HeatOverlay;
    static_cast<void>(intensity);
    static_cast<void>(pulseTime);
}

void Shader_SetParallax(float backSpeed, float frontSpeed, float mixRatio, float time)
{
    g_effect = ShaderEffect2D::Parallax;
    g_parallaxBackSpeed = backSpeed;
    g_parallaxFrontSpeed = frontSpeed;
    g_parallaxMixRatio = mixRatio;
    g_parallaxTime = time;
}

void Shader_SetNormalMapLighting(float lightX, float lightY, float lightZ, float ambient, float intensity)
{
    g_effect = ShaderEffect2D::NormalMapLighting;
    static_cast<void>(lightX);
    static_cast<void>(lightY);
    static_cast<void>(lightZ);
    static_cast<void>(ambient);
    static_cast<void>(intensity);
}

void Shader_SetAuxTexture(int textureID)
{
    static_cast<void>(textureID);
}

void Shader_ResetStyle()
{
    g_effect = ShaderEffect2D::Normal;
    g_tintR = 1.0f;
    g_tintG = 1.0f;
    g_tintB = 1.0f;
    g_tintA = 1.0f;
    g_blendMode = ShaderBlendMode2D::Alpha;
    g_uvScrollU = 0.0f;
    g_uvScrollV = 0.0f;
    g_distortionStrengthU = 0.0f;
    g_distortionStrengthV = 0.0f;
    g_distortionTime = 0.0f;
    g_distortionTintStrength = 0.0f;
    g_parallaxBackSpeed = 0.0f;
    g_parallaxFrontSpeed = 0.0f;
    g_parallaxMixRatio = 0.0f;
    g_parallaxTime = 0.0f;
}

ShaderEffect2D Shader_GetCurrentEffect()
{
    return g_effect;
}

ShaderBlendMode2D Shader_GetCurrentBlendMode()
{
    return g_blendMode;
}

void Shader_GetTintBytes(int& r, int& g, int& b, int& a)
{
    r = static_cast<int>(std::clamp(g_tintR, 0.0f, 1.0f) * 255.0f);
    g = static_cast<int>(std::clamp(g_tintG, 0.0f, 1.0f) * 255.0f);
    b = static_cast<int>(std::clamp(g_tintB, 0.0f, 1.0f) * 255.0f);
    a = static_cast<int>(std::clamp(g_tintA, 0.0f, 1.0f) * 255.0f);
}

void Shader_GetUVScroll(float& scrollU, float& scrollV)
{
    scrollU = g_uvScrollU;
    scrollV = g_uvScrollV;
}

void Shader_GetDistortion(float& strengthU, float& strengthV, float& time, float& tintStrength)
{
    strengthU = g_distortionStrengthU;
    strengthV = g_distortionStrengthV;
    time = g_distortionTime;
    tintStrength = g_distortionTintStrength;
}

void Shader_GetParallax(float& backSpeed, float& frontSpeed, float& mixRatio, float& time)
{
    backSpeed = g_parallaxBackSpeed;
    frontSpeed = g_parallaxFrontSpeed;
    mixRatio = g_parallaxMixRatio;
    time = g_parallaxTime;
}
