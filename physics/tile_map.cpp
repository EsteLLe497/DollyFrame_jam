#include "tile_map.h"

#include <algorithm>
#include <cctype>
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

bool ParseCsvCell(const std::string& cell, int& outTileValue, char& outMarker)
{
    outTileValue = 0;
    outMarker = '\0';
    if (cell.empty())
    {
        return true;
    }

    int parsedValue = 0;
    const auto parseResult = std::from_chars(
        cell.data(),
        cell.data() + cell.size(),
        parsedValue);
    if (parseResult.ec == std::errc() && parseResult.ptr == cell.data() + cell.size())
    {
        outTileValue = parsedValue;
        return true;
    }

    if (cell.size() == 1)
    {
        outMarker = static_cast<char>(std::toupper(static_cast<unsigned char>(cell[0])));
        return true;
    }

    return false;
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
    std::vector<std::vector<char>> markerRows;
    std::string line;
    while (std::getline(stream, line))
    {
        const std::string trimmedLine = Trim(line);
        if (trimmedLine.empty())
        {
            continue;
        }

        std::vector<int> rowValues;
        std::vector<char> markerValues;
        std::stringstream lineStream(trimmedLine);
        std::string cell;
        while (std::getline(lineStream, cell, ','))
        {
            const std::string trimmedCell = Trim(cell);
            if (trimmedCell.empty())
            {
                rowValues.push_back(0);
                markerValues.push_back('\0');
                continue;
            }

            int parsedValue = 0;
            char parsedMarker = '\0';
            if (!ParseCsvCell(trimmedCell, parsedValue, parsedMarker))
            {
                parsedValue = 0;
                parsedMarker = '\0';
            }
            rowValues.push_back(parsedValue);
            markerValues.push_back(parsedMarker);
        }

        if (!rowValues.empty())
        {
            rows.push_back(std::move(rowValues));
            markerRows.push_back(std::move(markerValues));
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
    m_markers.assign(static_cast<size_t>(m_width * m_height), '\0');

    for (int row = 0; row < m_height; ++row)
    {
        for (int column = 0; column < static_cast<int>(rows[row].size()); ++column)
        {
            m_tiles[static_cast<size_t>(row * m_width + column)] = rows[row][column];
            m_markers[static_cast<size_t>(row * m_width + column)] = markerRows[row][column];
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
    m_markers.clear();
    m_width = 0;
    m_height = 0;
    m_tileSize = 0.0f;
}

void TileMap::Draw(int textureId, float originX, float originY, float scale) const
{
    if (textureId < 0  ||m_tiles.empty()||  m_width <= 0 || m_height <= 0)
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
            const TileTriangleShape triangle = GetTriangleShape(tileValue);
            if (triangle.isTriangle)
            {
                const float drawWidth = static_cast<float>(triangle.widthTiles) * m_tileSize * scale;
                const float drawHeight = static_cast<float>(triangle.heightTiles) * m_tileSize * scale;
                const int color = GetColor(
                    static_cast<int>(std::round(r * 255.0f)),
                    static_cast<int>(std::round(g * 255.0f)),
                    static_cast<int>(std::round(b * 255.0f)));
                if (triangle.risesRight)
                {
                    DrawTriangleAA(
                        drawX,
                        drawY + drawHeight,
                        drawX + drawWidth,
                        drawY + drawHeight,
                        drawX + drawWidth,
                        drawY,
                        color,
                        TRUE);
                }
                else
                {
                    DrawTriangleAA(
                        drawX,
                        drawY,
                        drawX,
                        drawY + drawHeight,
                        drawX + drawWidth,
                        drawY + drawHeight,
                        color,
                        TRUE);
                }
                continue;
            }
            const float drawSize = m_tileSize * scale;
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

char TileMap::GetMarker(int column, int row) const
{
    if (column < 0 || row < 0 || column >= m_width || row >= m_height)
    {
        return '\0';
    }

    if (m_markers.empty())
    {
        return '\0';
    }

    return m_markers[static_cast<size_t>(row * m_width + column)];
}

bool TileMap::IsSolid(int column, int row) const
{
    return GetTile(column, row) > 0;
}

TileTriangleShape TileMap::GetTriangleShape(int tileValue)
{
    switch (tileValue)
    {
    case 6:
        return TileTriangleShape{ true, 1, 1, true };
    case 7:
        return TileTriangleShape{ true, 1, 1, false };
    case 8:
        return TileTriangleShape{ true, 2, 2, true };
    case 9:
        return TileTriangleShape{ true, 5, 5, false };
    default:
        return TileTriangleShape{};
    }
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
        r = 1.0f;
        g = 0.0f;
        b = 0.0f;
        a = 0.0f;
        break;
    case 5:
        r = 0.86f;
        g = 0.80f;
        b = 0.26f;
        break;
    case 6:
        r = 0.22f;
        g = 0.40f;
        b = 0.76f;
        break;
    case 7:
        r = 0.34f;
        g = 0.86f;
        b = 0.66f;
        break;
    case 8:
        r = 0.22f;
        g = 0.40f;
        b = 0.76f;
        break;
    case 9:
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
