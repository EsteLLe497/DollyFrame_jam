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

class TileMap
{
public:
    TileMap();

    bool LoadFromCsv(const std::string& path, float tileSize);
    void Clear();

    void Draw(int textureId, float originX, float originY, float scale = 1.0f) const;

    int GetWidth() const;
    int GetHeight() const;
    float GetTileSize() const;
    bool IsLoaded() const;
    int GetTile(int column, int row) const;
    bool IsSolid(int column, int row) const;
    static TileTriangleShape GetTriangleShape(int tileValue);

private:
    static void GetTileTint(int tileValue, float& r, float& g, float& b, float& a);

    std::vector<int> m_tiles;
    int m_width;
    int m_height;
    float m_tileSize;
};
