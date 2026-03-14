#pragma once

#include <string>
#include <vector>

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

private:
    static void GetTileTint(int tileValue, float& r, float& g, float& b, float& a);

    std::vector<int> m_tiles;
    int m_width;
    int m_height;
    float m_tileSize;
};
