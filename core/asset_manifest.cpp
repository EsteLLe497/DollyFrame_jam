#include "pch.h"

#include "asset_manifest.h"

#include <fstream>
#include <iomanip>
#include <sstream>

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
    m_resources = &resources;
    m_textureIds.clear();
    m_texturePaths.clear();

    std::ifstream ifs("assets/manifest.json");
    if (!ifs)
    {
        Logger::Warn("assets/manifest.json not found. Falling back to built-in defaults.");
        m_textureIds.reserve(5);
        resources.ReserveTextureCache(5);
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
    if (textures.is_object())
    {
        m_textureIds.reserve(textures.size());
        resources.ReserveTextureCache(textures.size());
    }
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
                m_texturePaths.emplace(key, path);
            }
        }
        else if (type == "file_sequence")
        {
            const std::string pathPrefix = desc.value("pathPrefix", "");
            const std::string pathSuffix = desc.value("pathSuffix", "");
            const int start = desc.value("start", 0);
            const int count = desc.value("count", 0);
            const int digits = desc.value("digits", 3);
            for (int i = 0; i < count; ++i)
            {
                std::ostringstream number;
                number << std::setw(digits) << std::setfill('0') << (start + i);
                const std::string textureKey = key + "_" + number.str();
                const std::string path = pathPrefix + number.str() + pathSuffix;
                m_texturePaths.emplace(textureKey, path);
            }
        }
    }

    Logger::Info("Asset manifest loaded from JSON");
}

int AssetManifest::GetTexture(const std::string& key) const
{
    const auto found = m_textureIds.find(key);
    if (found != m_textureIds.end())
    {
        return found->second;
    }

    const auto path = m_texturePaths.find(key);
    if (path == m_texturePaths.end() || m_resources == nullptr)
    {
        return -1;
    }

    const int textureId = m_resources->LoadTexture(ToWideString(path->second));
    if (textureId >= 0)
    {
        m_textureIds.emplace(key, textureId);
    }
    return textureId;
}
