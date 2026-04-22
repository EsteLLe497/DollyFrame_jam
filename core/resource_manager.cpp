#include "resource_manager.h"

#include "texture.h"

ResourceManager::ResourceManager() = default;

void ResourceManager::Initialize(void* device)
{
    TextureInitialize(device);
    m_textureCache.clear();
}

void ResourceManager::Shutdown()
{
    m_textureCache.clear();
    TextureFinalize();
}

int ResourceManager::LoadTexture(const std::wstring& path)
{
    const auto found = m_textureCache.find(path);
    if (found != m_textureCache.end())
    {
        return found->second;
    }

    const int id = TextureLoad(path);
    if (id >= 0)
    {
        m_textureCache.emplace(path, id);
    }
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

//ss