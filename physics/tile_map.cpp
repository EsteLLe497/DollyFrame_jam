#include "pch.h"

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

bool ParseCsvHeader(const std::string& line, TileMapData& outData)
{
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] != '#')
    {
        return false;
    }

    const std::string header = Trim(trimmed.substr(1));
    constexpr const char* kTexturePrefix = "tileTexture=";
    if (header.rfind(kTexturePrefix, 0) == 0)
    {
        outData.tileTextureKey = Trim(header.substr(std::char_traits<char>::length(kTexturePrefix)));
        return true;
    }

    return false;
}

bool ParseCsvCell(const std::string& cell, int& outTileValue, char& outMarker, int& outMarkerParameter,char& outMarker2,int& outMarkerParameter2)
{
    outTileValue = 0;
    outMarker = '\0';
    outMarkerParameter = 0;
    outMarker2 = '\0';
    outMarkerParameter2 = 0;
    if (cell.empty())
    {
        return true;
    }

    const auto tryParseTileValue = [&](const std::string& token, int& outValue) -> bool
    {
        int parsedValue = 0;
        const auto parseResult = std::from_chars(
            token.data(),
            token.data() + token.size(),
            parsedValue);
        if (parseResult.ec == std::errc() && parseResult.ptr == token.data() + token.size())
        {
            outValue = parsedValue;
            return true;
        }

        if (token.size() == 1)
        {
            switch (static_cast<char>(std::toupper(static_cast<unsigned char>(token[0]))))
            {
            case 'N':
                outValue = TileMap::kPitTileValue;
                return true;
            default:
                break;
            }
        }

        return false;
    };

    if (tryParseTileValue(cell, outTileValue))
    {
        return true;
    }



    const auto tryParseMarker = [&](const std::string& token, char& outValue, int& outParameter) -> bool
    {
        if (token.empty())
        {
            return false;
        }

        const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
        const bool supportsParameter =
            std::isalpha(static_cast<unsigned char>(marker)) ||
            marker == '@' ||
            marker == '&' ||
            marker == '!' ||
            marker == '?' ||
			marker == '>' ||
            marker == '<' ||
            marker == '+' ||
            marker == '$' ||
            marker == '*' ||
            marker == '_';
        if (!supportsParameter)
        {
            return false;
        }

        int parameter = 0;
        if (token.size() > 1)
        {
            const std::string parameterToken = token.substr(1);
            const auto parseResult = std::from_chars(
                parameterToken.data(),
                parameterToken.data() + parameterToken.size(),
                parameter);
            if (parseResult.ec != std::errc() || parseResult.ptr != parameterToken.data() + parameterToken.size())
            {
                return false;
            }
        }

        outValue = marker;
        outParameter = parameter;
        return true;
    };

    const auto tryParseCompositeCell = [&](const std::string& leftToken, const std::string& rightToken) -> bool
    {
        const std::string left = Trim(leftToken);
        const std::string right = Trim(rightToken);
        int tileValue = 0;
        char markerValue = '\0';
        int markerParameter = 0;
        if (tryParseTileValue(left, tileValue) && tryParseMarker(right, markerValue, markerParameter))
        {
            outTileValue = tileValue;
            outMarker = markerValue;
            outMarkerParameter = markerParameter;
            return true;
        }
        if (tryParseMarker(left, markerValue, markerParameter) && tryParseTileValue(right, tileValue))
        {
            outTileValue = tileValue;
            outMarker = markerValue;
            outMarkerParameter = markerParameter;
            return true;
        }
        return false;
    };

    const auto tryParseCompositeCell2 = [&](const std::string& leftToken, const std::string& rightToken) -> bool
    {
        const std::string left = Trim(leftToken);
        const std::string right = Trim(rightToken);
        int tileValue = 0;
        char markerValue = '\0';
        int markerParameter = 0;
        char markerValue2 = '\0';
        int markerParameter2 = 0;
        if (tryParseMarker(left, markerValue, markerParameter) && tryParseMarker(right, markerValue2, markerParameter2))
        {
            outMarker = markerValue;
            outMarkerParameter = markerParameter;
            outMarker2 = markerValue2;
            outMarkerParameter2 = markerParameter2;
            return true;
        }
        if (tryParseMarker(left, markerValue, markerParameter) && tryParseTileValue(right, tileValue))
        {
            outTileValue = tileValue;
            outMarker = markerValue;
            outMarkerParameter = markerParameter;
            return true;
        }
       
        return false;
    };

    const size_t pipePos = cell.find('|');
    if (pipePos != std::string::npos)
    {
        if (tryParseCompositeCell(cell.substr(0, pipePos), cell.substr(pipePos + 1)))
        {
            return true;
        }
    }

    const size_t colonPos = cell.find(':');
    if (colonPos != std::string::npos)
    {
        if (tryParseCompositeCell(cell.substr(0, colonPos), cell.substr(colonPos + 1)))
        {
            return true;
        }
    }

	const size_t parenPos = cell.find('(');
    if(parenPos != std::string::npos)
    {
        if (tryParseCompositeCell2(cell.substr(0, parenPos), cell.substr(parenPos + 1)))
        {
            return true;
        }
	}

    size_t splitIndex = 0;
    while (splitIndex < cell.size() && (std::isdigit(static_cast<unsigned char>(cell[splitIndex])) || cell[splitIndex] == '-'))
    {
        ++splitIndex;
    }
    if (splitIndex > 0 && splitIndex < cell.size())
    {
        if (tryParseCompositeCell(cell.substr(0, splitIndex), cell.substr(splitIndex)))
        {
            return true;
        }
    }

    if (cell.size() > 5 && cell.rfind("tile=", 0) == 0)
    {
        const std::string tileToken = Trim(cell.substr(5));
        if (tryParseTileValue(tileToken, outTileValue))
        {
            return true;
        }
    }

    if (cell.size() == 1)
    {
        outMarker = static_cast<char>(std::toupper(static_cast<unsigned char>(cell[0])));
        return true;
    }

    int markerParameter = 0;
    if (tryParseMarker(cell, outMarker, markerParameter))
    {
        outMarkerParameter = markerParameter;
        return true;
    }

    return false;
}

bool IsConnectableTileValue(int tileValue)
{
    return tileValue == 1 ||
        tileValue == 2 ||
        tileValue == 3 ||
        tileValue == 4 ||
        tileValue == TileMap::kPitTileValue;
}

int SelectAutotileCell(const TileMapData& data, int column, int row)
{
    const auto isConnectable = [&](int testColumn, int testRow) -> bool
    {
        if (testColumn < 0 || testRow < 0 || testColumn >= data.width || testRow >= data.height)
        {
            return false;
        }

        const int tileValue = data.tiles[static_cast<size_t>(testRow * data.width + testColumn)];
        return IsConnectableTileValue(tileValue);
    };

    const bool up = isConnectable(column, row - 1);
    const bool down = isConnectable(column, row + 1);
    const bool left = isConnectable(column - 1, row);
    const bool right = isConnectable(column + 1, row);
    const int connectedCount = static_cast<int>(up) + static_cast<int>(down) + static_cast<int>(left) + static_cast<int>(right);

    if (connectedCount <= 0 || connectedCount == 4)
    {
        return 4;
    }

    if (connectedCount == 1)
    {
        if (up) return 7;
        if (down) return 1;
        if (left) return 5;
        if (right) return 3;
    }

    if (connectedCount == 2)
    {
        if (up && down)
        {
            return 4;
        }
        if (left && right)
        {
            return 4;
        }
        if (up && left)
        {
            return 8;
        }
        if (up && right)
        {
            return 6;
        }
        if (down && left)
        {
            return 2;
        }
        if (down && right)
        {
            return 0;
        }
    }

    if (connectedCount == 3)
    {
        if (!up) return 1;
        if (!down) return 7;
        if (!left) return 3;
        if (!right) return 5;
    }

    return 4;
}

std::string FormatCsvCell(int tileValue, char marker, int markerParameter, char marker2, int markerParameter2)
{
    constexpr int kCsvCellVisualWidth = 2;

    auto padCell = [](std::string value)
    {
        if (static_cast<int>(value.size()) < kCsvCellVisualWidth)
        {
            value.insert(value.begin(), static_cast<size_t>(kCsvCellVisualWidth - static_cast<int>(value.size())), ' ');
        }
        return value;
    };

    if (marker == '\0')
    {
        return padCell(std::to_string(tileValue));
    }

    std::string markerText(1, static_cast<char>(std::toupper(static_cast<unsigned char>(marker))));
    if (markerParameter != 0)
    {
        markerText += std::to_string(markerParameter);
    }

    if (marker2 != '\0')
    {
        markerText += "(";
        markerText += static_cast<char>(std::toupper(static_cast<unsigned char>(marker2)));
        if (markerParameter2 != 0)
        {
            markerText += std::to_string(markerParameter2);
        }
    }

    if (tileValue == 0)
    {
        return padCell(markerText);
    }

    return padCell(std::to_string(tileValue) + "|" + markerText);
}

void GetTileTint(int tileValue, float& r, float& g, float& b, float& a)
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
        a = 1.0f;
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
    case TileMap::kPitTileValue:
        r = 0.03f;
        g = 0.04f;
        b = 0.08f;
        break;
    case 11:
        r = 0.22f;
        g = 0.40f;
        b = 0.76f;
        break;
    default:
        r = 0.70f;
        g = 0.74f;
        b = 0.82f;
        break;
    }
}

void GetSquareTileTint(int tileValue, float& r, float& g, float& b, float& a)
{
    a = 1.0f;
    switch (tileValue)
    {
    case 4:
        r = 0.94f;
        g = 0.30f;
        b = 0.26f;
        break;
    case 5:
        r = 0.96f;
        g = 0.85f;
        b = 0.32f;
        break;
    case TileMap::kPitTileValue:
        r = 0.08f;
        g = 0.09f;
        b = 0.12f;
        break;
    default:
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        break;
    }
}

class TileMapCsvLoader
{
public:
    static bool Load(const std::string& path, float tileSize, TileMapData& outData)
    {
        outData = TileMapData{};
        outData.tileSize = tileSize;

        std::ifstream stream(path);
        if (!stream.is_open())
        {
            Logger::Error(std::string("TileMap failed to open CSV: ") + path);
            return false;
        }

        std::vector<std::vector<int>> rows;
        std::vector<std::vector<char>> markerRows;
        std::vector<std::vector<int>> markerParameterRows;
		std::vector<std::vector<char>> markerRows2;
		std::vector<std::vector<int>> markerParameterRows2;
        std::string line;
        while (std::getline(stream, line))
        {
            const std::string trimmedLine = Trim(line);
            if (trimmedLine.empty())
            {
                continue;
            }

            if (ParseCsvHeader(trimmedLine, outData))
            {
                continue;
            }
            if (!trimmedLine.empty() && trimmedLine[0] == '#')
            {
                continue;
            }

            std::vector<int> rowValues;
            std::vector<char> markerValues;
            std::vector<int> markerParameterValues;
			std::vector<char> markerValues2;
            std::vector<int> markerParameterValues2;
            std::stringstream lineStream(trimmedLine);
            std::string cell;
            while (std::getline(lineStream, cell, ','))
            {
                const std::string trimmedCell = Trim(cell);
                if (trimmedCell.empty())
                {
                    rowValues.push_back(0);
                    markerValues.push_back('\0');
                    markerParameterValues.push_back(0);
					markerValues2.push_back('\0');
					markerParameterValues2.push_back(0);
                    continue;
                }

                int parsedValue = 0;
                char parsedMarker = '\0';
                int parsedMarkerParameter = 0;
                char parsedMarker2 = '\0';
                int parsedMarkerParameter2 = 0;
                if (!ParseCsvCell(trimmedCell, parsedValue, parsedMarker, parsedMarkerParameter,parsedMarker2,parsedMarkerParameter2))
                {
                    parsedValue = 0;
                    parsedMarker = '\0';
                    parsedMarkerParameter = 0;
                    parsedMarker2 = '\0';
                    parsedMarkerParameter2 = 0;
                }
                rowValues.push_back(parsedValue);
                markerValues.push_back(parsedMarker);
                markerParameterValues.push_back(parsedMarkerParameter);
                markerValues2.push_back(parsedMarker2);
                markerParameterValues2.push_back(parsedMarkerParameter2);
            }

            if (!rowValues.empty())
            {
                rows.push_back(std::move(rowValues));
                markerRows.push_back(std::move(markerValues));
                markerParameterRows.push_back(std::move(markerParameterValues));
                markerRows2.push_back(std::move(markerValues2));
                markerParameterRows2.push_back(std::move(markerParameterValues2));
            }
        }

        if (rows.empty())
        {
            Logger::Error(std::string("TileMap CSV contained no rows: ") + path);
            return false;
        }

        for (const auto& row : rows)
        {
            outData.width = (std::max)(outData.width, static_cast<int>(row.size()));
        }
        outData.height = static_cast<int>(rows.size());
        outData.tiles.assign(static_cast<size_t>(outData.width * outData.height), 0);
        outData.markers.assign(static_cast<size_t>(outData.width * outData.height), '\0');
        outData.markerParameters.assign(static_cast<size_t>(outData.width * outData.height), 0);
        outData.markers2.assign(static_cast<size_t>(outData.width * outData.height), '\0');
        outData.markerParameters2.assign(static_cast<size_t>(outData.width * outData.height), 0);

        for (int row = 0; row < outData.height; ++row)
        {
            for (int column = 0; column < static_cast<int>(rows[row].size()); ++column)
            {
                outData.tiles[static_cast<size_t>(row * outData.width + column)] = rows[row][column];
                outData.markers[static_cast<size_t>(row * outData.width + column)] = markerRows[row][column];
                outData.markerParameters[static_cast<size_t>(row * outData.width + column)] = markerParameterRows[row][column];
                outData.markers2[static_cast<size_t>(row * outData.width + column)] = markerRows2[row][column];
                outData.markerParameters2[static_cast<size_t>(row * outData.width + column)] = markerParameterRows2[row][column];
            }
        }

        std::ostringstream message;
        message << "TileMap loaded from CSV: " << path
                << " (" << outData.width << "x" << outData.height
                << ", tileSize=" << std::fixed << std::setprecision(0) << outData.tileSize << ")";
        Logger::Info(message.str());
        return true;
    }
};

class TileMapRenderer
{
public:
    static void Draw(const TileMapData& data, int textureId, float originX, float originY, float scale, int tile2TextureId, int tile3TextureId)
    {
        if (textureId < 0 || data.tiles.empty() || data.width <= 0 || data.height <= 0)
        {
            return;
        }

        Shader_ResetStyle();
        for (int row = 0; row < data.height; ++row)
        {
            for (int column = 0; column < data.width; ++column)
            {
                const int tileValue = data.tiles[static_cast<size_t>(row * data.width + column)];
                if (tileValue <= 0)
                {
                    continue;
                }

                const float drawX = originX + static_cast<float>(column) * data.tileSize * scale;
                const float drawY = originY + static_cast<float>(row) * data.tileSize * scale;
                const TileTriangleShape triangle = TileMap::GetTriangleShape(tileValue);
                if (triangle.isTriangle)
                {
                    float r = 1.0f;
                    float g = 1.0f;
                    float b = 1.0f;
                    float a = 1.0f;
                    GetTileTint(tileValue, r, g, b, a);
                    const float drawWidth = static_cast<float>(triangle.widthTiles) * data.tileSize * scale;
                    const float drawHeight = static_cast<float>(triangle.heightTiles) * data.tileSize * scale;
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

                float r = 1.0f;
                float g = 1.0f;
                float b = 1.0f;
                float a = 1.0f;
                const bool useSpecialTile2Texture = tileValue == 2 && tile2TextureId >= 0;
                const bool useSpecialTile3Texture = tileValue == 3 && tile3TextureId >= 0;
                const bool isConnectableTile = IsConnectableTileValue(tileValue);
                if (!useSpecialTile2Texture && !useSpecialTile3Texture)
                {
                    GetSquareTileTint(tileValue, r, g, b, a);
                    // 地形タイル全体を暗くして、背景と足場のコントラストを作る。
                    if (isConnectableTile)
                    {
                        //このパラメータを変更することで地面の彩度変わります（下げると黒くなる）
                        constexpr float kTileBrightness = 0.35f;
                        r *= kTileBrightness;
                        g *= kTileBrightness;
                        b *= kTileBrightness;
                    }
                }
                Shader_SetTint(r, g, b, a);
                if (useSpecialTile2Texture || useSpecialTile3Texture)
                {
                    SpriteDraw(
                        useSpecialTile2Texture ? tile2TextureId : tile3TextureId,
                        drawX,
                        drawY,
                        data.tileSize * scale,
                        data.tileSize * scale,
                        0.0f,
                        0.0f,
                        1.0f,
                        1.0f);
                }
                else
                {
                    const int atlasCell = IsConnectableTileValue(tileValue)
                        ? SelectAutotileCell(data, column, row)
                        : 4;
                    const int atlasColumn = atlasCell % 3;
                    const int atlasRow = atlasCell / 3;
                    const float cellSize = 1.0f / 3.0f;
                    SpriteDraw(
                        textureId,
                        drawX,
                        drawY,
                        data.tileSize * scale,
                        data.tileSize * scale,
                        static_cast<float>(atlasColumn) * cellSize,
                        static_cast<float>(atlasRow) * cellSize,
                        cellSize,
                        cellSize);
                }
            }
        }
        Shader_ResetStyle();
    }
};
}

TileMap::TileMap()
{
}

bool TileMap::LoadFromCsv(const std::string& path, float tileSize)
{
    TileMapData loadedData;
    if (!TileMapCsvLoader::Load(path, tileSize, loadedData))
    {
        Clear();
        return false;
    }
    m_data = std::move(loadedData);
    return true;
}

bool TileMap::SaveToCsv(const std::string& path) const
{
    if (!IsLoaded())
    {
        Logger::Error("TileMap save failed: map data is empty");
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        Logger::Error(std::string("TileMap failed to save CSV: ") + path);
        return false;
    }

    if (!m_data.tileTextureKey.empty())
    {
        stream << "# tileTexture=" << m_data.tileTextureKey << "\n";
    }

    for (int row = 0; row < m_data.height; ++row)
    {
        for (int column = 0; column < m_data.width; ++column)
        {
            if (column > 0)
            {
                stream << ",";
            }

            const int tileValue = m_data.tiles[static_cast<size_t>(row * m_data.width + column)];
            const char marker = m_data.markers.empty()
                ? '\0'
                : m_data.markers[static_cast<size_t>(row * m_data.width + column)];
            const int markerParameter = m_data.markerParameters.empty()
                ? 0
                : m_data.markerParameters[static_cast<size_t>(row * m_data.width + column)];
            const char marker2 = m_data.markers2.empty()
                ? '\0'
                : m_data.markers2[static_cast<size_t>(row * m_data.width + column)];
            const int markerParameter2 = m_data.markerParameters2.empty()
                ? 0
                : m_data.markerParameters2[static_cast<size_t>(row * m_data.width + column)];

            stream << FormatCsvCell(tileValue, marker, markerParameter, marker2,markerParameter2);
        }
        if (row + 1 < m_data.height)
        {
            stream << "\n";
        }
    }

    Logger::Info(std::string("TileMap saved to CSV: ") + path);
    return true;
}

void TileMap::Clear()
{
    m_data = TileMapData{};
}

void TileMap::Draw(int textureId, float originX, float originY, float scale, int tile2TextureId, int tile3TextureId) const
{
    TileMapRenderer::Draw(m_data, textureId, originX, originY, scale, tile2TextureId, tile3TextureId);
}

int TileMap::GetWidth() const
{
    return m_data.width;
}

int TileMap::GetHeight() const
{
    return m_data.height;
}

float TileMap::GetTileSize() const
{
    return m_data.tileSize;
}

const std::string& TileMap::GetTileTextureKey() const
{
    return m_data.tileTextureKey;
}

void TileMap::SetTileTextureKey(const std::string& tileTextureKey)
{
    m_data.tileTextureKey = tileTextureKey;
}

bool TileMap::IsLoaded() const
{
    return !m_data.tiles.empty() && m_data.width > 0 && m_data.height > 0;
}

int TileMap::GetTile(int column, int row) const
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return 0;
    }

    return m_data.tiles[static_cast<size_t>(row * m_data.width + column)];
}

char TileMap::GetMarker(int column, int row) const
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return '\0';
    }

    if (m_data.markers.empty())
    {
        return '\0';
    }

    return m_data.markers[static_cast<size_t>(row * m_data.width + column)];
}

char TileMap::GetMarker2(int column, int row) const
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return '\0';
    }

    if (m_data.markers2.empty())
    {
        return '\0';
    }

    return m_data.markers2[static_cast<size_t>(row * m_data.width + column)];
}

int TileMap::GetMarkerParameter(int column, int row) const
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return 0;
    }

    if (m_data.markerParameters.empty())
    {
        return 0;
    }

    return m_data.markerParameters[static_cast<size_t>(row * m_data.width + column)];
}

int TileMap::GetMarkerParameter2(int column, int row) const
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return 0;
    }

    if (m_data.markerParameters2.empty())
    {
        return 0;
    }

    return m_data.markerParameters2[static_cast<size_t>(row * m_data.width + column)];
}

bool TileMap::SetTile(int column, int row, int tileValue)
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return false;
    }

    m_data.tiles[static_cast<size_t>(row * m_data.width + column)] = tileValue;
    return true;
}

bool TileMap::SetMarker(int column, int row, char markerValue)
{
    return SetMarker(column, row, markerValue, 0);
}

bool TileMap::SetMarker(int column, int row, char markerValue, int markerParameter)
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return false;
    }

    if (m_data.markers.empty())
    {
        m_data.markers.assign(static_cast<size_t>(m_data.width * m_data.height), '\0');
    }
    if (m_data.markerParameters.empty())
    {
        m_data.markerParameters.assign(static_cast<size_t>(m_data.width * m_data.height), 0);
    }

    m_data.markers[static_cast<size_t>(row * m_data.width + column)] =
        markerValue == '\0'
        ? '\0'
        : static_cast<char>(std::toupper(static_cast<unsigned char>(markerValue)));
    m_data.markerParameters[static_cast<size_t>(row * m_data.width + column)] =
        markerValue == '\0' ? 0 : markerParameter;
    return true;
}

bool TileMap::SetMarker2(int column, int row, char markerValue)
{
    return SetMarker2(column, row, markerValue, 0);
}

bool TileMap::SetMarker2(int column, int row, char markerValue, int markerParameter)
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return false;
    }

    if (m_data.markers2.empty())
    {
        m_data.markers2.assign(static_cast<size_t>(m_data.width * m_data.height), '\0');
    }
    if (m_data.markerParameters2.empty())
    {
        m_data.markerParameters2.assign(static_cast<size_t>(m_data.width * m_data.height), 0);
    }

    m_data.markers2[static_cast<size_t>(row * m_data.width + column)] =
        markerValue == '\0'
        ? '\0'
        : static_cast<char>(std::toupper(static_cast<unsigned char>(markerValue)));
    m_data.markerParameters2[static_cast<size_t>(row * m_data.width + column)] =
        markerValue == '\0' ? 0 : markerParameter;
    return true;
}

bool TileMap::SetMarkerParameter(int column, int row, int markerParameter)
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return false;
    }

    if (m_data.markers.empty())
    {
        return false;
    }
    if (m_data.markerParameters.empty())
    {
        m_data.markerParameters.assign(static_cast<size_t>(m_data.width * m_data.height), 0);
    }

    const size_t index = static_cast<size_t>(row * m_data.width + column);
    if (m_data.markers[index] == '\0')
    {
        return false;
    }

    m_data.markerParameters[index] = markerParameter;
    return true;
}

bool TileMap::SetMarkerParameter2(int column, int row, int markerParameter)
{
    if (column < 0 || row < 0 || column >= m_data.width || row >= m_data.height)
    {
        return false;
    }

    if (m_data.markers2.empty())
    {
        return false;
    }
    if (m_data.markerParameters2.empty())
    {
        m_data.markerParameters2.assign(static_cast<size_t>(m_data.width * m_data.height), 0);
    }

    const size_t index = static_cast<size_t>(row * m_data.width + column);
    if (m_data.markers2[index] == '\0')
    {
        return false;
    }

    m_data.markerParameters2[index] = markerParameter;
    return true;
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
    case 11:
        return TileTriangleShape{ true, 10, 6, true };
    default:
        return TileTriangleShape{};
    }
}
