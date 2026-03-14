#include "texture.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "DirectXTex.h"
#include "directX.h"

#pragma comment(lib, "windowscodecs.lib")
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex_Debug.lib")
#else
#pragma comment(lib, "DirectXTex_Release.lib")
#endif

namespace
{
    struct TextureEntry
    {
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0;
        int height = 0;
    };

    ID3D11Device* g_Device = nullptr;
    std::vector<TextureEntry> g_Textures;

    int RegisterTexture(ID3D11ShaderResourceView* srv, int width, int height)
    {
        g_Textures.push_back({ srv, width, height });
        return static_cast<int>(g_Textures.size() - 1);
    }

    int CreateTextureFromMemory(int width, int height, const std::vector<unsigned int>& pixels)
    {
        if (!g_Device || width <= 0 || height <= 0 || pixels.empty())
        {
            return -1;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = pixels.data();
        initData.SysMemPitch = static_cast<UINT>(width * sizeof(unsigned int));

        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = g_Device->CreateTexture2D(&desc, &initData, &texture);
        if (FAILED(hr))
        {
            return -1;
        }

        ID3D11ShaderResourceView* srv = nullptr;
        hr = g_Device->CreateShaderResourceView(texture, nullptr, &srv);
        texture->Release();
        if (FAILED(hr))
        {
            return -1;
        }

        return RegisterTexture(srv, width, height);
    }
}

void TextureInitialize(ID3D11Device* device)
{
    g_Device = device;
    g_Textures.clear();
}

int TextureLoad(const std::wstring& texture_filename)
{
    if (!g_Device)
    {
        return -1;
    }

    DirectX::TexMetadata metadata{};
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromWICFile(texture_filename.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, image);
    if (FAILED(hr))
    {
        return -1;
    }

    ID3D11ShaderResourceView* srv = nullptr;
    hr = DirectX::CreateShaderResourceView(g_Device, image.GetImages(), image.GetImageCount(), metadata, &srv);
    if (FAILED(hr))
    {
        return -1;
    }

    return RegisterTexture(srv, static_cast<int>(metadata.width), static_cast<int>(metadata.height));
}

int TextureCreateSolidColor(int width, int height, unsigned int rgba)
{
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), rgba);
    return CreateTextureFromMemory(width, height, pixels);
}

int TextureCreateCheckerboard(int width, int height, unsigned int rgbaA, unsigned int rgbaB, int cellSize)
{
    if (cellSize <= 0)
    {
        return -1;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const bool useA = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = useA ? rgbaA : rgbaB;
        }
    }

    return CreateTextureFromMemory(width, height, pixels);
}

int TextureCreateDisc(int width, int height, unsigned int rgbaInner, unsigned int rgbaOuter, float innerRatio)
{
    if (width <= 0 || height <= 0)
    {
        return -1;
    }

    const float safeInnerRatio = std::clamp(innerRatio, 0.0f, 1.0f);
    const float centerX = (static_cast<float>(width) - 1.0f) * 0.5f;
    const float centerY = (static_cast<float>(height) - 1.0f) * 0.5f;
    const float radius = static_cast<float>((width < height ? width : height)) * 0.5f;
    const float innerRadius = radius * safeInnerRatio;

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), rgbaOuter);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= innerRadius)
            {
                pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = rgbaInner;
            }
        }
    }

    return CreateTextureFromMemory(width, height, pixels);
}

ID3D11ShaderResourceView* GetTexture(int id)
{
    if (id < 0 || id >= static_cast<int>(g_Textures.size()))
    {
        return nullptr;
    }

    return g_Textures[static_cast<size_t>(id)].srv;
}

int TextureGetWidth(int id)
{
    if (id < 0 || id >= static_cast<int>(g_Textures.size()))
    {
        return 0;
    }

    return g_Textures[static_cast<size_t>(id)].width;
}

int TextureGetHeight(int id)
{
    if (id < 0 || id >= static_cast<int>(g_Textures.size()))
    {
        return 0;
    }

    return g_Textures[static_cast<size_t>(id)].height;
}

void TextureFinalize(void)
{
    for (auto& texture : g_Textures)
    {
        SAFE_RELEASE(texture.srv);
    }
    g_Textures.clear();
    g_Device = nullptr;
}
