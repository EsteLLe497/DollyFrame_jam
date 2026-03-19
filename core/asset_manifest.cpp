#include "asset_manifest.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "logger.h"
#include "resource_manager.h"

namespace
{
    unsigned int ParseHexColor(const std::string& value)
    {
        return static_cast<unsigned int>(std::stoul(value, nullptr, 16));
    }

    std::wstring ToWideString(const std::string& value)
    {
        return std::wstring(value.begin(), value.end());
    }
}

void AssetManifest::LoadDefaults(ResourceManager& resources)
{
    m_textureIds.clear();

    std::ifstream ifs("assets/manifest.json");
    if (!ifs)
    {
        Logger::Warn("assets/manifest.json not found. Falling back to built-in defaults.");
        m_textureIds.emplace("white", resources.CreateSolidTexture(1, 1, 0xFFFFFFFF));
        m_textureIds.emplace("player", resources.CreateCheckerboardTexture(256, 256, 0xFF4EC9B0, 0xFF1B3340, 32));
        m_textureIds.emplace("target", resources.CreateCheckerboardTexture(192, 192, 0xFFE08A2E, 0xFF5A2615, 24));
        m_textureIds.emplace("hazard", resources.CreateCheckerboardTexture(128, 128, 0xFFE33F33, 0xFF4E0A07, 16));
        m_textureIds.emplace("enemy", resources.CreateCheckerboardTexture(144, 144, 0xFF7E63E6, 0xFF1D143A, 18));
        return;
    }

    nlohmann::json root;
    ifs >> root;

    const auto& textures = root["textures"];
    for (auto it = textures.begin(); it != textures.end(); ++it)
    {
        const std::string key = it.key();
        const auto& desc = it.value();
        const std::string type = desc.value("type", "");

        if (type == "solid")
        {
            m_textureIds.emplace(
                key,
                resources.CreateSolidTexture(
                    desc.value("width", 1),
                    desc.value("height", 1),
                    ParseHexColor(desc.value("rgba", "FFFFFFFF"))));
        }
        else if (type == "checkerboard")
        {
            m_textureIds.emplace(
                key,
                resources.CreateCheckerboardTexture(
                    desc.value("width", 1),
                    desc.value("height", 1),
                    ParseHexColor(desc.value("rgbaA", "FFFFFFFF")),
                    ParseHexColor(desc.value("rgbaB", "FF000000")),
                    desc.value("cellSize", 8)));
        }
        else if (type == "file")
        {
            const std::string path = desc.value("path", "");
            if (!path.empty())
            {
                m_textureIds.emplace(key, resources.LoadTexture(ToWideString(path)));
            }
        }
    }

    Logger::Info("Asset manifest loaded from JSON");
}

int AssetManifest::GetTexture(const std::string& key) const
{
    const auto found = m_textureIds.find(key);
    if (found == m_textureIds.end())
    {
        return -1;
    }

    return found->second;
}
