#pragma once

#include <string>
#include <unordered_map>

class ResourceManager;

class AssetManifest
{
public:
    void LoadDefaults(ResourceManager& resources);
    int GetTexture(const std::string& key) const;
    int getTextureByPath(const std::string& path) const;

private:
    ResourceManager* m_resources = nullptr;
    mutable std::unordered_map<std::string, int> m_textureIds;
    std::unordered_map<std::string, std::string> m_texturePaths;
};
