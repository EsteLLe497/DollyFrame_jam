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

// タイル描画を行う画面上の矩形範囲。
struct TileMapViewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct TileMapData
{
    std::vector<int> tiles;
    std::vector<char> markers;
    std::vector<int> markerParameters;
	std::vector<char> markers2;
	std::vector<int> markerParameters2;
    std::string tileTextureKey;
    int width = 0;
    int height = 0;
    float tileSize = 0.0f;
};

class TileMap
{
public:
    static constexpr int kPitTileValue = 10;
    // 複数セルへ広がる特殊タイルの最大描画サイズ。
    static constexpr int kMaxDrawWidthTiles = 10;
    static constexpr int kMaxDrawHeightTiles = 6;

    TileMap();

    bool LoadFromCsv(const std::string& path, float tileSize);
    bool SaveToCsv(const std::string& path) const;
    void Clear();

    // ビューポート内に見えるタイルだけを描画する。
    void Draw(
        int textureId,
        float originX,
        float originY,
        const TileMapViewport& viewport,
        float scale = 1.0f,
        int tile2TextureId = -1,
        int tile3TextureId = -1,
        int tile4TextureId = -1) const;

    int GetWidth() const;
    int GetHeight() const;
    float GetTileSize() const;
    const std::string& GetTileTextureKey() const;
    void SetTileTextureKey(const std::string& tileTextureKey);
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
