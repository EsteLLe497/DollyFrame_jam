#include "pch.h"

#include "texture.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "DxLib.h"

namespace
{
    struct TextureEntry
    {
        int graphHandle = -1;
        int width = 0;
        int height = 0;
    };

    std::vector<TextureEntry> g_Textures;

    std::string ToUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
        {
            return {};
        }

        std::string result(static_cast<size_t>(size - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
    }

    int RegisterTexture(int graphHandle, int width, int height)
    {
        if (graphHandle < 0)
        {
            return -1;
        }

        g_Textures.push_back({ graphHandle, width, height });
        return static_cast<int>(g_Textures.size() - 1);
    }

    int CreateTextureFromMemory(int width, int height, const std::vector<unsigned int>& pixels)
    {
        if (width <= 0 || height <= 0 || pixels.empty())
        {
            return -1;
        }

        const int softImage = MakeARGB8ColorSoftImage(width, height);
        if (softImage < 0)
        {
            return -1;
        }

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const unsigned int rgba = pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
                const int a = static_cast<int>((rgba >> 24) & 0xff);
                const int r = static_cast<int>((rgba >> 16) & 0xff);
                const int g = static_cast<int>((rgba >> 8) & 0xff);
                const int b = static_cast<int>(rgba & 0xff);
                DrawPixelSoftImage(softImage, x, y, r, g, b, a);
            }
        }

        const int graphHandle = CreateGraphFromSoftImage(softImage);
        DeleteSoftImage(softImage);
        return RegisterTexture(graphHandle, width, height);
    }
}

void TextureInitialize(void* device)
{
    static_cast<void>(device);
    g_Textures.clear();
}

int TextureLoad(const std::wstring& texture_filename)
{
    const std::string path = ToUtf8(texture_filename);
    if (path.empty())
    {
        return -1;
    }

    const int graphHandle = LoadGraph(path.c_str());
    if (graphHandle < 0)
    {
        return -1;
    }

    int width = 0;
    int height = 0;
    GetGraphSize(graphHandle, &width, &height);
    return RegisterTexture(graphHandle, width, height);
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

void* GetTexture(int id)
{
    static_cast<void>(id);
    return nullptr;
}

int TextureGetGraphHandle(int id)
{
    if (id < 0 || id >= static_cast<int>(g_Textures.size()))
    {
        return -1;
    }

    return g_Textures[static_cast<size_t>(id)].graphHandle;
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
        if (texture.graphHandle >= 0)
        {
            DeleteGraph(texture.graphHandle);
            texture.graphHandle = -1;
        }
    }
    g_Textures.clear();
}
