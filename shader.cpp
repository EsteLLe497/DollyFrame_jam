#include "shader.h"

#include <fstream>
#include <vector>

#include "directX.h"
#include "texture.h"

using namespace DirectX;

static ID3D11VertexShader* g_pVertexShader = nullptr;
static ID3D11InputLayout* g_pInputLayout = nullptr;
static ID3D11Buffer* g_pVSConstantBuffer = nullptr;
static ID3D11PixelShader* g_pPixelShaders[20] = {};
static ID3D11Buffer* g_pPSColorBuffer = nullptr;
static ID3D11Buffer* g_pPSEffectBuffer = nullptr;
static ID3D11SamplerState* g_SamplerState = nullptr;
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

struct PS_COLOR
{
    XMFLOAT4 tint;
};

struct PS_EFFECT
{
    XMINT4 modeAndFlags;
    XMFLOAT4 outlineColor;
    XMFLOAT4 textureInfo;
    XMFLOAT4 effectParams;
    XMFLOAT4 secondaryColor;
    XMFLOAT4 tertiaryColor;
};

static ShaderBlendMode2D g_blendMode = ShaderBlendMode2D::Alpha;
static int g_auxTextureID = -1;
static PS_EFFECT g_effectState = {
    XMINT4(static_cast<int>(ShaderEffect2D::Normal), 0, 0, 0),
    XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f),
    XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f),
    XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
    XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
    XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)
};

static bool LoadBinaryFile(const char* path, std::vector<unsigned char>& outData)
{
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs)
    {
        return false;
    }

    const std::streamsize size = ifs.tellg();
    if (size <= 0)
    {
        return false;
    }

    outData.resize(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    return ifs.read(reinterpret_cast<char*>(outData.data()), size).good();
}

static bool LoadPixelShaderFromBinary(ID3D11Device* device, const char* path, ID3D11PixelShader** outShader)
{
    std::vector<unsigned char> binary;
    if (!LoadBinaryFile(path, binary))
    {
        MessageBoxA(nullptr, path, "Failed to load pixel shader", MB_OK | MB_ICONERROR);
        return false;
    }

    const HRESULT hr = device->CreatePixelShader(binary.data(), binary.size(), nullptr, outShader);
    return SUCCEEDED(hr);
}

static void UpdateEffectState()
{
    g_pContext->UpdateSubresource(g_pPSEffectBuffer, 0, nullptr, &g_effectState, 0, 0);
}

bool Shader_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    if (!pDevice || !pContext)
    {
        return false;
    }

    g_pDevice = pDevice;
    g_pContext = pContext;

    std::vector<unsigned char> vsBinary;
    if (!LoadBinaryFile("shaderVertex2D.cso", vsBinary))
    {
        MessageBoxA(nullptr, "Failed to load shaderVertex2D.cso", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    HRESULT hr = g_pDevice->CreateVertexShader(vsBinary.data(), vsBinary.size(), nullptr, &g_pVertexShader);
    if (FAILED(hr))
    {
        return false;
    }

    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vsBinary.data(), vsBinary.size(), &g_pInputLayout);
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_BUFFER_DESC vsBufferDesc{};
    vsBufferDesc.ByteWidth = sizeof(XMMATRIX);
    vsBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = g_pDevice->CreateBuffer(&vsBufferDesc, nullptr, &g_pVSConstantBuffer);
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_BUFFER_DESC psBufferDesc{};
    psBufferDesc.ByteWidth = sizeof(PS_COLOR);
    psBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    psBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    hr = g_pDevice->CreateBuffer(&psBufferDesc, nullptr, &g_pPSColorBuffer);
    if (FAILED(hr))
    {
        return false;
    }

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);

    D3D11_BUFFER_DESC psEffectDesc{};
    psEffectDesc.ByteWidth = sizeof(PS_EFFECT);
    psEffectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    psEffectDesc.Usage = D3D11_USAGE_DEFAULT;
    hr = g_pDevice->CreateBuffer(&psEffectDesc, nullptr, &g_pPSEffectBuffer);
    if (FAILED(hr))
    {
        return false;
    }

    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Normal.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Normal)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Grayscale.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Grayscale)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Outline.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Outline)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Flash.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Flash)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_UVScroll.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::UVScroll)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Dissolve.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Dissolve)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_MaskClip.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::MaskClip)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Distortion.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Distortion)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_PaletteSwap.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::PaletteSwap)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Posterize.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Posterize)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_ChromaticAberration.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::ChromaticAberration)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Glitch.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Glitch)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Pixelate.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Pixelate)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Wave.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Wave)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_RimLight.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::RimLight)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_GradientMap.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::GradientMap)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_NoiseReveal.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::NoiseReveal)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_HeatOverlay.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::HeatOverlay)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_Parallax.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::Parallax)]))
    {
        return false;
    }
    if (!LoadPixelShaderFromBinary(g_pDevice, "shaderPixel2D_NormalMapLighting.cso", &g_pPixelShaders[static_cast<int>(ShaderEffect2D::NormalMapLighting)]))
    {
        return false;
    }

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_pDevice->CreateSamplerState(&samplerDesc, &g_SamplerState);
    if (FAILED(hr))
    {
        return false;
    }

    g_pContext->PSSetSamplers(0, 1, &g_SamplerState);
    Shader_ResetStyle();
    return true;
}

void Shader_Finalize()
{
    SAFE_RELEASE(g_SamplerState);
    SAFE_RELEASE(g_pPSEffectBuffer);
    SAFE_RELEASE(g_pPSColorBuffer);
    for (auto& pixelShader : g_pPixelShaders)
    {
        SAFE_RELEASE(pixelShader);
    }
    SAFE_RELEASE(g_pVSConstantBuffer);
    SAFE_RELEASE(g_pInputLayout);
    SAFE_RELEASE(g_pVertexShader);
}

void Shader_SetMatrix(const XMMATRIX& matrix)
{
    XMFLOAT4X4 transpose{};
    XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
    g_pContext->UpdateSubresource(g_pVSConstantBuffer, 0, nullptr, &transpose, 0, 0);
}

void Shader_Begin()
{
    g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pContext->PSSetShader(g_pPixelShaders[g_effectState.modeAndFlags.x], nullptr, 0);
    g_pContext->IASetInputLayout(g_pInputLayout);
    g_pContext->VSSetConstantBuffers(0, 1, &g_pVSConstantBuffer);
    g_pContext->PSSetConstantBuffers(1, 1, &g_pPSColorBuffer);
    g_pContext->PSSetConstantBuffers(2, 1, &g_pPSEffectBuffer);
    DirectXSetBlendMode(g_blendMode == ShaderBlendMode2D::Additive ? BlendMode2D::Additive : BlendMode2D::Alpha);
}

void Shader_SetTint(float r, float g, float b, float a)
{
    const PS_COLOR color = { XMFLOAT4(r, g, b, a) };
    g_pContext->UpdateSubresource(g_pPSColorBuffer, 0, nullptr, &color, 0, 0);
}

void Shader_SetEffect(ShaderEffect2D effect)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(effect), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetBlendMode(ShaderBlendMode2D blendMode)
{
    g_blendMode = blendMode;
}

void Shader_SetOutline(float r, float g, float b, float a, float thickness)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Outline), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(r, g, b, a);
    g_effectState.textureInfo.z = thickness;
    g_effectState.effectParams = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetFlash(float r, float g, float b, float a, float intensity)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Flash), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(r, g, b, a);
    g_effectState.effectParams = XMFLOAT4(intensity, 0.0f, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetUVScroll(float scrollU, float scrollV)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::UVScroll), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(scrollU, scrollV, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetDissolve(float threshold, float edgeWidth, float edgeR, float edgeG, float edgeB, float edgeA)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Dissolve), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(edgeR, edgeG, edgeB, edgeA);
    g_effectState.effectParams = XMFLOAT4(threshold, edgeWidth, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetMaskClip(float threshold, float softness)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::MaskClip), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(threshold, softness, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetDistortion(float strengthU, float strengthV, float time, float tintStrength)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Distortion), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(strengthU, strengthV, time, tintStrength);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetPaletteSwap(
    float sourceR, float sourceG, float sourceB, float sourceA,
    float targetR, float targetG, float targetB, float targetA,
    float threshold)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::PaletteSwap), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(sourceR, sourceG, sourceB, sourceA);
    g_effectState.secondaryColor = XMFLOAT4(targetR, targetG, targetB, targetA);
    g_effectState.effectParams = XMFLOAT4(threshold, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetPosterize(float levels, float contrast)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Posterize), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(levels, contrast, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetChromaticAberration(float amount, float time)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::ChromaticAberration), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(amount, time, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetGlitch(float amount, float time, float scanlineStrength)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Glitch), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(amount, time, scanlineStrength, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetPixelate(float pixelWidth, float pixelHeight)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Pixelate), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(pixelWidth, pixelHeight, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetWave(float amplitudeU, float amplitudeV, float frequency, float time)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Wave), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(amplitudeU, amplitudeV, frequency, time);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetRimLight(float r, float g, float b, float a, float power)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::RimLight), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(r, g, b, a);
    g_effectState.effectParams = XMFLOAT4(power, 0.0f, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetGradientMap(
    float darkR, float darkG, float darkB, float darkA,
    float brightR, float brightG, float brightB, float brightA,
    float contrast)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::GradientMap), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(darkR, darkG, darkB, darkA);
    g_effectState.secondaryColor = XMFLOAT4(brightR, brightG, brightB, brightA);
    g_effectState.effectParams = XMFLOAT4(contrast, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetNoiseReveal(float threshold, float edgeWidth, float time, float edgeR, float edgeG, float edgeB, float edgeA)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::NoiseReveal), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(edgeR, edgeG, edgeB, edgeA);
    g_effectState.effectParams = XMFLOAT4(threshold, edgeWidth, time, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetHeatOverlay(float intensity, float pulseTime)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::HeatOverlay), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(intensity, pulseTime, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetParallax(float backSpeed, float frontSpeed, float mixRatio, float time)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::Parallax), 0, 0, 0);
    g_effectState.effectParams = XMFLOAT4(backSpeed, frontSpeed, mixRatio, time);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetNormalMapLighting(float lightX, float lightY, float lightZ, float ambient, float intensity)
{
    g_effectState.modeAndFlags = XMINT4(static_cast<int>(ShaderEffect2D::NormalMapLighting), 0, 0, 0);
    g_effectState.outlineColor = XMFLOAT4(lightX, lightY, lightZ, 1.0f);
    g_effectState.effectParams = XMFLOAT4(ambient, intensity, 0.0f, 0.0f);
    g_effectState.secondaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    g_effectState.tertiaryColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    UpdateEffectState();
}

void Shader_SetAuxTexture(int textureID)
{
    g_auxTextureID = textureID;
}

void Shader_BindSpriteTextures(int primaryTextureID)
{
    ID3D11ShaderResourceView* primary = GetTexture(primaryTextureID);
    ID3D11ShaderResourceView* secondary = GetTexture(g_auxTextureID);
    g_pContext->PSSetShaderResources(0, 1, &primary);
    g_pContext->PSSetShaderResources(1, 1, &secondary);
}

void Shader_SetTextureSize(float width, float height)
{
    if (g_effectState.textureInfo.z <= 0.0f)
    {
        g_effectState.textureInfo.z = 1.0f;
    }
    g_effectState.textureInfo.x = width > 0.0f ? 1.0f / width : 1.0f;
    g_effectState.textureInfo.y = height > 0.0f ? 1.0f / height : 1.0f;
    UpdateEffectState();
}

void Shader_ResetStyle()
{
    g_effectState = {
        XMINT4(static_cast<int>(ShaderEffect2D::Normal), 0, 0, 0),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)
    };
    g_blendMode = ShaderBlendMode2D::Alpha;
    g_auxTextureID = -1;
    UpdateEffectState();
}
