#pragma once

#include <string>
#include <vector>

struct TileTriangleShape
{
    bool isTriangle = false;
    int widthTiles = 1;
    int heightTiles = 1;
    bool risesRight = false;
};

struct TileMapData
{
    std::vector<int> tiles;
    std::vector<char> markers;
    std::vector<int> markerParameters;
	std::vector<char> markers2;
	std::vector<int> markerParameters2;
    int width = 0;
    int height = 0;
    float tileSize = 0.0f;
};

class TileMap
{
public:
    static constexpr int kPitTileValue = 10;

    TileMap();

    bool LoadFromCsv(const std::string& path, float tileSize);
    bool SaveToCsv(const std::string& path) const;
    void Clear();

    void Draw(int textureId, float originX, float originY, float scale = 1.0f) const;

    int GetWidth() const;
    int GetHeight() const;
    float GetTileSize() const;
    bool IsLoaded() const;
    int GetTile(int column, int row) const;
    char GetMarker(int column, int row) const;
    int GetMarkerParameter(int column, int row) const;
    bool SetTile(int column, int row, int tileValue);
    bool SetMarker(int column, int row, char markerValue);
    bool SetMarker(int column, int row, char markerValue, int markerParameter);
    bool SetMarkerParameter(int column, int row, int markerParameter);
    char GetMarker2(int column, int row) const;
    int GetMarkerParameter2(int column, int row) const;
    bool SetMarker2(int column, int row, char markerValue);
    bool SetMarker2(int column, int row, char markerValue, int markerParameter);
    bool SetMarkerParameter2(int column, int row, int markerParameter);
    bool IsSolid(int column, int row) const;
    static TileTriangleShape GetTriangleShape(int tileValue);

private:
    TileMapData m_data;
};
