#pragma once

class fixedCameraRange
{
private:
    float startX = 0.0f;
    float startY = 0.0f;
    float endX = 0.0f;
    float endY = 0.0f;

    float cameraX = 0.0f;
    float cameraY = 0.0f;

    bool followPlayer = false;
    int cameraNum = -1;

    float zoomWidth = 1920.0f;
    float zoomHeight = 1080.0f;

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    int offsetTilesX = 0;
    int offsetTilesY = 0;

public:
    fixedCameraRange() = default;

    //固定カメラ範囲
    void SetStartTiles(int startCol, int startRow, float tileSize);
    void SetEndTiles(int endCol, int endRow, float tileSize);

    //固定カメラ位置
    void SetFixedCameraPosition(float x, float y);

    // プレイヤー追従
    void SetFollowPlayer(bool follow);
    bool IsFollowPlayer() const { return followPlayer; }

    //カメラ番号（大→優先）
    void SetCameraNum(int num);
    int GetCameraNum() const { return cameraNum; }

    void SetZoomWidth(float width);
    float GetZoomWidth() const { return zoomWidth; }
    void SetZoomHeight(float height);
    float GetZoomHeight() const { return zoomHeight; }

    bool IsInRange(float px, float py) const;
    float GetCameraX() const { return cameraX; }
    float GetCameraY() const { return cameraY; }
    float GetStartX() const { return startX; }
    float GetEndX() const { return endX; }
    float GetWidth() const;

    void SetOffsetX(int tiles, float tileSize);
    float GetOffsetX() const { return offsetX; }
    void SetOffsetY(int tiles, float tileSize);
    float GetOffsetY() const { return offsetY; }
};

