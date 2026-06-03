#include "pch.h"

#include "resource_manager.h"

#include "logger.h"
#include "texture.h"

namespace
{
    std::string ToLogPath(const std::wstring& path)
    {
        std::string result;
        result.reserve(path.size());
        for (const wchar_t ch : path)
        {
            result.push_back(ch >= 0 && ch <= 0x7F ? static_cast<char>(ch) : '?');
        }
        return result;
    }
}

ResourceManager::ResourceManager() = default;

void ResourceManager::Initialize(void* device)
{
    TextureInitialize(device);
    m_textureCache.clear();
    m_textureCache.reserve(32);
}

void ResourceManager::Shutdown()
{
    m_textureCache.clear();
    TextureFinalize();
}

void ResourceManager::ReserveTextureCache(size_t count)
{
    m_textureCache.reserve(count);
}

int ResourceManager::LoadTexture(const std::wstring& path)
{
    const auto [found, inserted] = m_textureCache.try_emplace(path, -1);
    if (!inserted)
    {
        return found->second;
    }

    const int id = TextureLoad(path);
    if (id >= 0)
    {
        found->second = id;
        return id;
    }

    Logger::Warn("Failed to load texture: " + ToLogPath(path));
    m_textureCache.erase(found);
    return id;
}

int ResourceManager::CreateSolidTexture(int width, int height, unsigned int rgba)
{
    return TextureCreateSolidColor(width, height, rgba);
}

int ResourceManager::CreateCheckerboardTexture(int width, int height, unsigned int rgbaA, unsigned int rgbaB, int cellSize)
{
    return TextureCreateCheckerboard(width, height, rgbaA, rgbaB, cellSize);
}

int ResourceManager::CreateDiscTexture(int width, int height, unsigned int rgbaInner, unsigned int rgbaOuter, float innerRatio)
{
    return TextureCreateDisc(width, height, rgbaInner, rgbaOuter, innerRatio);
}
