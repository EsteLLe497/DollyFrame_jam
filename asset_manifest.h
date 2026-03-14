#pragma once

#include <string>
#include <unordered_map>

class ResourceManager;

class AssetManifest
{
public:
    void LoadDefaults(ResourceManager& resources);
    int GetTexture(const std::string& key) const;

private:
    std::unordered_map<std::string, int> m_textureIds;
};
