#pragma once

#include <string>
#include <unordered_map>

struct ID3D11Device;

class ResourceManager
{
public:
    ResourceManager();
    ~ResourceManager() = default;

    void Initialize(ID3D11Device* device);
    void Shutdown();

    int LoadTexture(const std::wstring& path);
    int CreateSolidTexture(int width, int height, unsigned int rgba);
    int CreateCheckerboardTexture(int width, int height, unsigned int rgbaA, unsigned int rgbaB, int cellSize);
    int CreateDiscTexture(int width, int height, unsigned int rgbaInner, unsigned int rgbaOuter, float innerRatio);

private:
    std::unordered_map<std::wstring, int> m_textureCache;
};
