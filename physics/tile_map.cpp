#include "tile_map.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "DxLib.h"
#include "logger.h"
#include "shader.h"
#include "sprite.h"

namespace
{
std::string Trim(const std::string& value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        return {};
    }

    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}
}

TileMap::TileMap()
    : m_width(0)
    , m_height(0)
    , m_tileSize(0.0f)
{
}

bool TileMap::LoadFromCsv(const std::string& path, float tileSize)
{
    Clear();
    m_tileSize = tileSize;

    std::ifstream stream(path);
    if (!stream.is_open())
    {
        Logger::Error(std::string("TileMap failed to open CSV: ") + path);
        return false;
    }

    std::vector<std::vector<int>> rows;
    std::string line;
    while (std::getline(stream, line))
    {
        const std::string trimmedLine = Trim(line);
        if (trimmedLine.empty())
        {
            continue;
        }

        std::vector<int> rowValues;
        std::stringstream lineStream(trimmedLine);
        std::string cell;
        while (std::getline(lineStream, cell, ','))
        {
            const std::string trimmedCell = Trim(cell);
            if (trimmedCell.empty())
            {
                rowValues.push_back(0);
                continue;
            }

            int parsedValue = 0;
            const auto parseResult = std::from_chars(
                trimmedCell.data(),
                trimmedCell.data() + trimmedCell.size(),
                parsedValue);
            rowValues.push_back(parseResult.ec == std::errc() ? parsedValue : 0);
        }

        if (!rowValues.empty())
        {
            rows.push_back(std::move(rowValues));
        }
    }

    if (rows.empty())
    {
        Logger::Error(std::string("TileMap CSV contained no rows: ") + path);
        return false;
    }

    for (const auto& row : rows)
    {
        m_width = (std::max)(m_width, static_cast<int>(row.size()));
    }
    m_height = static_cast<int>(rows.size());
    m_tiles.assign(static_cast<size_t>(m_width * m_height), 0);

    for (int row = 0; row < m_height; ++row)
    {
        for (int column = 0; column < static_cast<int>(rows[row].size()); ++column)
        {
            m_tiles[static_cast<size_t>(row * m_width + column)] = rows[row][column];
        }
    }

    std::ostringstream message;
    message << "TileMap loaded from CSV: " << path
            << " (" << m_width << "x" << m_height
            << ", tileSize=" << std::fixed << std::setprecision(0) << m_tileSize << ")";
    Logger::Info(message.str());
    return true;
}

void TileMap::Clear()
{
    m_tiles.clear();
    m_width = 0;
    m_height = 0;
    m_tileSize = 0.0f;
}

void TileMap::Draw(int textureId, float originX, float originY, float scale) const
{
    if (textureId < 0 || m_tiles.empty() || m_width <= 0 || m_height <= 0)
    {
        return;
    }

    Shader_ResetStyle();
    for (int row = 0; row < m_height; ++row)
    {
        for (int column = 0; column < m_width; ++column)
        {
            const int tileValue = GetTile(column, row);
            if (tileValue <= 0)
            {
                continue;
            }

            float r = 1.0f;
            float g = 1.0f;
            float b = 1.0f;
            float a = 1.0f;
            GetTileTint(tileValue, r, g, b, a);
            const float drawX = originX + static_cast<float>(column) * m_tileSize * scale;
            const float drawY = originY + static_cast<float>(row) * m_tileSize * scale;
            const float drawSize = m_tileSize * scale;
            if (tileValue == 6 || tileValue == 7)
            {
                const int color = GetColor(
                    static_cast<int>(std::round(r * 255.0f)),
                    static_cast<int>(std::round(g * 255.0f)),
                    static_cast<int>(std::round(b * 255.0f)));

              
                const float triSize = drawSize * 5.0f;
                const float offset = (triSize - drawSize) * 0.5f;  // 0.5 * 0.5 * drawSize = 0.25 * drawSize
                const float triX = drawX - offset;
                const float triY = drawY - offset;

                if (tileValue == 6)
                {
                    DrawTriangleAA(
                        triX,
                        triY + triSize,
                        triX + triSize,
                        triY + triSize,
                        triX + triSize,
                        triY,
                        color,
                        TRUE);
                }
                else
                {
                    DrawTriangleAA(
                        triX,
                        triY,
                        triX,
                        triY + triSize,
                        triX + triSize,
                        triY + triSize,
                        color,
                        TRUE);
                }
                continue;
            }
            Shader_SetTint(r, g, b, a);
            SpriteDraw(
                textureId,
                drawX,
                drawY,
                drawSize,
                drawSize,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
        }
    }
    Shader_ResetStyle();
}

int TileMap::GetWidth() const
{
    return m_width;
}

int TileMap::GetHeight() const
{
    return m_height;
}

float TileMap::GetTileSize() const
{
    return m_tileSize;
}

bool TileMap::IsLoaded() const
{
    return !m_tiles.empty() && m_width > 0 && m_height > 0;
}

int TileMap::GetTile(int column, int row) const
{
    if (column < 0 || row < 0 || column >= m_width || row >= m_height)
    {
        return 0;
    }

    return m_tiles[static_cast<size_t>(row * m_width + column)];
}

bool TileMap::IsSolid(int column, int row) const
{
    return GetTile(column, row) > 0;
}

void TileMap::GetTileTint(int tileValue, float& r, float& g, float& b, float& a)
{
    a = 1.0f;
    switch (tileValue)
    {
    case 1:
        r = 0.22f;
        g = 0.40f;
        b = 0.76f;
        break;
    case 2:
        r = 0.784f;
        g = 0.941f;
        b = 1.0f;
        break;
    case 3:
        r = 0.34f;
        g = 0.86f;
        b = 0.66f;
        break;
    case 4:
        r = 0.88f;
        g = 0.24f;
        b = 0.22f;
        break;
    case 5:
        r = 0.86f;
        g = 0.80f;
        b = 0.26f;
        break;
    case 6:
        r = 0.54f;
        g = 0.84f;
        b = 0.34f;
        break;
    case 7:
        r = 0.34f;
        g = 0.86f;
        b = 0.66f;
        break;
    default:
        r = 0.70f;
        g = 0.74f;
        b = 0.82f;
        break;
    }
}
