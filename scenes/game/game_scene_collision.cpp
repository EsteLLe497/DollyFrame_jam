#include "game_scene_internal.h"
#include "photo_system_bridge.h"

using namespace game_scene_detail;

namespace
{
    constexpr int kGroundProbeCount = 3;

    constexpr int kMaxTriangleSpanTiles = 5;

    struct CollisionPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    using CollisionPolygon = std::vector<CollisionPoint>;

    void BuildRotatedRectPolygon(float left, float top, float width, float height, float rotation, CollisionPolygon& outPolygon)
    {
        outPolygon.resize(4);
        outPolygon[0] = { left, top };
        outPolygon[1] = { left + width, top };
        outPolygon[2] = { left + width, top + height };
        outPolygon[3] = { left, top + height };

        const float centerX = left + width * 0.5f;
        const float centerY = top + height * 0.5f;
        for (CollisionPoint& point : outPolygon)
        {
            RotatePoint(centerX, centerY, rotation, point.x, point.y);
        }
    }

    void BuildTrianglePolygon(
        float left,
        float top,
        float width,
        float height,
        bool risesRight,
        bool flipX,
        float rotation,
        CollisionPolygon& outPolygon)
    {
        outPolygon.resize(3);
        const bool finalRisesRight = flipX ? !risesRight : risesRight;
        if (finalRisesRight)
        {
            outPolygon[0] = { left, top + height };
            outPolygon[1] = { left + width, top + height };
            outPolygon[2] = { left + width, top };
        }
        else
        {
            outPolygon[0] = { left, top };
            outPolygon[1] = { left, top + height };
            outPolygon[2] = { left + width, top + height };
        }

        const float centerX = left + width * 0.5f;
        const float centerY = top + height * 0.5f;
        for (CollisionPoint& point : outPolygon)
        {
            RotatePoint(centerX, centerY, rotation, point.x, point.y);
        }
    }

    void BuildDamagePlatformSpikePolygon(
        const TransformComponent& transform,
        int tileSpan,
        int spikeIndex,
        CollisionPolygon& outPolygon)
    {
        const int spikeCount = (std::max)(1, tileSpan);
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float spikeWidth = width / static_cast<float>(spikeCount);
        const float left = transform.x + static_cast<float>(spikeIndex) * spikeWidth;
        const float top = transform.y;
        const float centerX = transform.x + width * 0.5f;
        const float centerY = transform.y + height * 0.5f;

        outPolygon.resize(3);
        outPolygon[0] = { left, top + height * 0.5f };
        outPolygon[1] = { left + spikeWidth, top + height * 0.5f };
        outPolygon[2] = { left + spikeWidth * 0.5f, top };

        for (CollisionPoint& point : outPolygon)
        {
            RotatePoint(centerX, centerY, transform.rotation, point.x, point.y);
        }
    }

    void BuildDamagePlatformHazardBandPolygon(
        const TransformComponent& transform,
        CollisionPolygon& outPolygon)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        BuildRotatedRectPolygon(
            transform.x,
            transform.y,
            width,
            height * 0.5f,
            transform.rotation,
            outPolygon);
    }

    void WorldToDamagePlatformLocal(
        const TransformComponent& transform,
        float worldX,
        float worldY,
        float& outLocalX,
        float& outLocalY)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float centerX = transform.x + width * 0.5f;
        const float centerY = transform.y + height * 0.5f;
        const float localX = worldX - centerX;
        const float localY = worldY - centerY;
        const float cosTheta = std::cos(-transform.rotation);
        const float sinTheta = std::sin(-transform.rotation);
        outLocalX = width * 0.5f + (localX * cosTheta - localY * sinTheta);
        outLocalY = height * 0.5f + (localX * sinTheta + localY * cosTheta);
    }

    bool IsPointOnDamagePlatformSpike(
        const TransformComponent& transform,
        int tileSpan,
        float worldX,
        float worldY,
        float tolerance)
    {
        const int spikeCount = (std::max)(1, tileSpan);
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float spikeBandHeight = height * 0.5f;
        float localX = 0.0f;
        float localY = 0.0f;
        WorldToDamagePlatformLocal(transform, worldX, worldY, localX, localY);

        if (localX < -tolerance || localX > width + tolerance ||
            localY < -tolerance || localY > spikeBandHeight + tolerance)
        {
            return false;
        }

        const float spikeWidth = width / static_cast<float>(spikeCount);
        for (int spikeIndex = 0; spikeIndex < spikeCount; ++spikeIndex)
        {
            const float left = static_cast<float>(spikeIndex) * spikeWidth;
            const float right = left + spikeWidth;
            if (localX < left - tolerance || localX > right + tolerance)
            {
                continue;
            }

            const float mid = left + spikeWidth * 0.5f;
            const float halfWidth = spikeWidth * 0.5f;
            const float normalizedDistance = std::min(1.0f, std::fabs(localX - mid) / (std::max)(halfWidth, 0.0001f));
            const float surfaceY = spikeBandHeight * normalizedDistance;
            if (localY >= surfaceY - tolerance && localY <= spikeBandHeight + tolerance)
            {
                return true;
            }
        }

        return false;
    }

    bool IsPointInDamagePlatformHazardBand(
        const TransformComponent& transform,
        float worldX,
        float worldY,
        float tolerance)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        float localX = 0.0f;
        float localY = 0.0f;
        WorldToDamagePlatformLocal(transform, worldX, worldY, localX, localY);
        return localX >= -tolerance &&
            localX <= width + tolerance &&
            localY >= -tolerance &&
            localY <= height * 0.5f + tolerance;
    }

    void BuildImageOutlinePolygon(
        const TransformComponent& transform,
        const ImageOutlineColliderComponent& collider,
        CollisionPolygon& outPolygon)
    {
        const auto& normalizedOutline = collider.GetNormalizedOutline();
        outPolygon.clear();
        outPolygon.reserve(normalizedOutline.size());

        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        for (const b2Vec2& point : normalizedOutline)
        {
            outPolygon.push_back({
                transform.x + point.x * width,
                transform.y + point.y * height
            });
        }

        const float centerX = transform.x + width * 0.5f;
        const float centerY = transform.y + height * 0.5f;
        for (CollisionPoint& point : outPolygon)
        {
            RotatePoint(centerX, centerY, transform.rotation, point.x, point.y);
        }
    }

    bool BuildEntityCollisionPolygon(const Entity& entity, CollisionPolygon& outPolygon)
    {
        const auto* transform = entity.GetComponent<TransformComponent>();
        if (!transform)
        {
            return false;
        }

        if (const auto* imageCollider = entity.GetComponent<ImageOutlineColliderComponent>())
        {
            if (!imageCollider->GetNormalizedOutline().empty())
            {
                BuildImageOutlinePolygon(*transform, *imageCollider, outPolygon);
                return true;
            }
        }

        const auto* tileValue = entity.GetComponent<PhotoCopyTileValueComponent>();
        const auto* sprite = entity.GetComponent<SpriteRenderComponent>();
        const float width = transform->width * transform->scale;
        const float height = transform->height * transform->scale;
        if (tileValue)
        {
            const TileTriangleShape triangle = TileMap::GetTriangleShape(tileValue->tileValue);
            if (triangle.isTriangle)
            {
                BuildTrianglePolygon(
                    transform->x,
                    transform->y,
                    width,
                    height,
                    triangle.risesRight,
                    sprite ? sprite->GetFlipX() : false,
                    transform->rotation,
                    outPolygon);
                return true;
            }
        }

        BuildRotatedRectPolygon(transform->x, transform->y, width, height, transform->rotation, outPolygon);
        return true;
    }

    void GetPolygonAabb(const CollisionPolygon& polygon, float& left, float& top, float& right, float& bottom)
    {
        left = polygon.front().x;
        right = polygon.front().x;
        top = polygon.front().y;
        bottom = polygon.front().y;
        for (const CollisionPoint& point : polygon)
        {
            left = (std::min)(left, point.x);
            right = (std::max)(right, point.x);
            top = (std::min)(top, point.y);
            bottom = (std::max)(bottom, point.y);
        }
    }

    void ProjectPolygonOntoAxis(const CollisionPolygon& polygon, float axisX, float axisY, float& outMin, float& outMax)
    {
        outMin = polygon.front().x * axisX + polygon.front().y * axisY;
        outMax = outMin;
        for (size_t index = 1; index < polygon.size(); ++index)
        {
            const float projection = polygon[index].x * axisX + polygon[index].y * axisY;
            outMin = (std::min)(outMin, projection);
            outMax = (std::max)(outMax, projection);
        }
    }

    float Cross2D(const CollisionPoint& a, const CollisionPoint& b, const CollisionPoint& c)
    {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    bool OnSegment(const CollisionPoint& a, const CollisionPoint& b, const CollisionPoint& point)
    {
        return point.x >= (std::min)(a.x, b.x) - 0.01f &&
            point.x <= (std::max)(a.x, b.x) + 0.01f &&
            point.y >= (std::min)(a.y, b.y) - 0.01f &&
            point.y <= (std::max)(a.y, b.y) + 0.01f;
    }

    bool SegmentsIntersect(
        const CollisionPoint& a0,
        const CollisionPoint& a1,
        const CollisionPoint& b0,
        const CollisionPoint& b1)
    {
        const float d1 = Cross2D(a0, a1, b0);
        const float d2 = Cross2D(a0, a1, b1);
        const float d3 = Cross2D(b0, b1, a0);
        const float d4 = Cross2D(b0, b1, a1);

        const bool straddlesA = (d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f);
        const bool straddlesB = (d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f);
        if (straddlesA && straddlesB)
        {
            return true;
        }

        if (std::fabs(d1) <= 0.01f && OnSegment(a0, a1, b0)) return true;
        if (std::fabs(d2) <= 0.01f && OnSegment(a0, a1, b1)) return true;
        if (std::fabs(d3) <= 0.01f && OnSegment(b0, b1, a0)) return true;
        if (std::fabs(d4) <= 0.01f && OnSegment(b0, b1, a1)) return true;
        return false;
    }

    bool IsPointInsidePolygon(const CollisionPolygon& polygon, const CollisionPoint& point)
    {
        bool inside = false;
        for (size_t index = 0, last = polygon.size() - 1; index < polygon.size(); last = index++)
        {
            const CollisionPoint& a = polygon[index];
            const CollisionPoint& b = polygon[last];
            const bool intersectsY = (a.y > point.y) != (b.y > point.y);
            if (!intersectsY)
            {
                continue;
            }

            const float edgeX = (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 0.00001f) + a.x;
            if (point.x < edgeX)
            {
                inside = !inside;
            }
        }
        return inside;
    }

    bool PolygonsIntersect(const CollisionPolygon& a, const CollisionPolygon& b)
    {
        if (a.empty() || b.empty())
        {
            return false;
        }

        for (size_t aIndex = 0; aIndex < a.size(); ++aIndex)
        {
            const CollisionPoint& a0 = a[aIndex];
            const CollisionPoint& a1 = a[(aIndex + 1) % a.size()];
            for (size_t bIndex = 0; bIndex < b.size(); ++bIndex)
            {
                const CollisionPoint& b0 = b[bIndex];
                const CollisionPoint& b1 = b[(bIndex + 1) % b.size()];
                if (SegmentsIntersect(a0, a1, b0, b1))
                {
                    return true;
                }
            }
        }

        return IsPointInsidePolygon(a, b.front()) || IsPointInsidePolygon(b, a.front());
    }

    bool TryIntersectVerticalLineSegment(float lineX, const CollisionPoint& a, const CollisionPoint& b, float& outY)
    {
        const float minX = (std::min)(a.x, b.x);
        const float maxX = (std::max)(a.x, b.x);
        if (lineX < minX - 0.01f || lineX > maxX + 0.01f)
        {
            return false;
        }

        if (std::fabs(a.x - b.x) <= 0.01f)
        {
            outY = (std::min)(a.y, b.y);
            return true;
        }

        const float t = (lineX - a.x) / (b.x - a.x);
        if (t < -0.01f || t > 1.01f)
        {
            return false;
        }

        outY = a.y + (b.y - a.y) * t;
        return true;
    }

    bool TryGetPolygonSurfaceY(const CollisionPolygon& polygon, float worldX, float& outSurfaceY)
    {
        bool found = false;
        float bestY = 0.0f;
        for (size_t index = 0; index < polygon.size(); ++index)
        {
            float edgeY = 0.0f;
            if (!TryIntersectVerticalLineSegment(worldX, polygon[index], polygon[(index + 1) % polygon.size()], edgeY))
            {
                continue;
            }

            if (!found || edgeY < bestY)
            {
                bestY = edgeY;
                found = true;
            }
        }

        if (!found)
        {
            return false;
        }

        outSurfaceY = bestY;
        return true;
    }

    bool TryInferPolygonSurfaceDirection(
        const CollisionPolygon& polygon,
        float minX,
        float maxX,
        bool& outRisesRight)
    {
        const float width = maxX - minX;
        if (width <= 1.0f)
        {
            return false;
        }

        const float sampleInset = std::min(width * 0.25f, 8.0f);
        const float leftSampleX = minX + sampleInset;
        const float rightSampleX = maxX - sampleInset;
        if (rightSampleX <= leftSampleX)
        {
            return false;
        }

        float leftY = 0.0f;
        float rightY = 0.0f;
        if (!TryGetPolygonSurfaceY(polygon, leftSampleX, leftY) ||
            !TryGetPolygonSurfaceY(polygon, rightSampleX, rightY) ||
            std::fabs(leftY - rightY) <= 1.0f)
        {
            return false;
        }

        outRisesRight = rightY < leftY;
        return true;
    }

    bool UsesSolidCollision(const Entity& entity)
    {
        if (const auto* layer = entity.GetComponent<PhotoCopyLayerComponent>())
        {
            return layer->layer == PhotoCopyLayer::Foreground;
        }
        return true;
    }

    bool IsSolidPolygonEntity(const Entity& entity)
    {
        if (entity.GetComponent<ImageOutlineColliderComponent>())
        {
            return true;
        }

        return HasTag(entity, kTagPhotoBox) && UsesSolidCollision(entity);
    }

    bool IsSlopeTileValue(int tile)
    {
        return TileMap::GetTriangleShape(tile).isTriangle;
    }

    bool DoesTriangleOccupyCell(
        int originColumn,
        int originRow,
        const TileTriangleShape& triangle,
        float tileSize,
        int column,
        int row)
    {
        if (!triangle.isTriangle ||
            column < originColumn ||
            row < originRow ||
            column >= originColumn + triangle.widthTiles ||
            row >= originRow + triangle.heightTiles)
        {
            return false;
        }

        const float localLeft = static_cast<float>(column - originColumn) * tileSize;
        const float localRight = localLeft + tileSize;
        const float localBottom = static_cast<float>(row - originRow + 1) * tileSize;
        const float width = static_cast<float>(triangle.widthTiles) * tileSize;
        const float height = static_cast<float>(triangle.heightTiles) * tileSize;
        const float minSurfaceY = triangle.risesRight
            ? height - (localRight / width) * height
            : (localLeft / width) * height;
        return localBottom > minSurfaceY;
    }

    bool TryComputeSlopeSurfaceY(float left, float top, float width, float height, bool risesRight, float worldX, float& outSurfaceY)
    {
        if (width <= 0.0f || height <= 0.0f || worldX < left || worldX > left + width)
        {
            return false;
        }

        const float localX = std::clamp(worldX - left, 0.0f, width);
        const float normalizedX = width > 0.0f ? localX / width : 0.0f;
        outSurfaceY = risesRight
            ? top + height - normalizedX * height
            : top + normalizedX * height;
        return true;
    }

    bool TryGetSlopeSurfaceY(
        const TileMap& tileMap,
        int column,
        int row,
        float worldX,
        float& outSurfaceY,
        TileTriangleShape* outTriangle = nullptr)
    {
        const float tileSize = tileMap.GetTileSize();
        bool foundSurface = false;
        float bestSurfaceY = 0.0f;
        TileTriangleShape bestTriangle{};
        const int originColumnStart = (std::max)(0, column - (kMaxTriangleSpanTiles - 1));
        const int originRowStart = (std::max)(0, row - (kMaxTriangleSpanTiles - 1));
        for (int originRow = originRowStart; originRow <= row; ++originRow)
        {
            for (int originColumn = originColumnStart; originColumn <= column; ++originColumn)
            {
                const TileTriangleShape triangle = TileMap::GetTriangleShape(tileMap.GetTile(originColumn, originRow));
                if (!triangle.isTriangle)
                {
                    continue;
                }

                if (!DoesTriangleOccupyCell(originColumn, originRow, triangle, tileSize, column, row))
                {
                    continue;
                }

                float surfaceY = 0.0f;
                if (!TryComputeSlopeSurfaceY(
                        static_cast<float>(originColumn) * tileSize,
                        static_cast<float>(originRow) * tileSize,
                        static_cast<float>(triangle.widthTiles) * tileSize,
                        static_cast<float>(triangle.heightTiles) * tileSize,
                        triangle.risesRight,
                        worldX,
                        surfaceY))
                {
                    continue;
                }

                if (!foundSurface || surfaceY < bestSurfaceY)
                {
                    bestSurfaceY = surfaceY;
                    bestTriangle = triangle;
                    foundSurface = true;
                }
            }
        }

        if (!foundSurface)
        {
            return false;
        }

        outSurfaceY = bestSurfaceY;
        if (outTriangle)
        {
            *outTriangle = bestTriangle;
        }
        return true;
    }

    bool TryGetPhotoBoxSlopeSurfaceY(const Entity& entity, float worldX, float& outSurfaceY)
    {
        CollisionPolygon polygon;
        if (!BuildEntityCollisionPolygon(entity, polygon))
        {
            return false;
        }
        return TryGetPolygonSurfaceY(polygon, worldX, outSurfaceY);
    }

    void GetGroundProbeXs(const TransformComponent& transform, float outProbeXs[kGroundProbeCount])
    {
        const float width = transform.width * transform.scale;
        outProbeXs[0] = transform.x + 6.0f;
        outProbeXs[1] = transform.x + width * 0.5f;
        outProbeXs[2] = transform.x + width - 6.0f;
    }
}

bool GameScene::IsSolidTile(int column, int row) const
{
    const int tile = m_tileMap.GetTile(column, row);
    return tile == 1 || tile == 2 || tile == 3 || tile == 4;
}

bool GameScene::IsSlopeTile(int column, int row) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const int originColumnStart = (std::max)(0, column - (kMaxTriangleSpanTiles - 1));
    const int originRowStart = (std::max)(0, row - (kMaxTriangleSpanTiles - 1));
    for (int originRow = originRowStart; originRow <= row; ++originRow)
    {
        for (int originColumn = originColumnStart; originColumn <= column; ++originColumn)
        {
            const TileTriangleShape triangle = TileMap::GetTriangleShape(m_tileMap.GetTile(originColumn, originRow));
            if (!triangle.isTriangle)
            {
                continue;
            }

            if (DoesTriangleOccupyCell(originColumn, originRow, triangle, tileSize, column, row))
            {
                return true;
            }
        }
    }

    return false;
}

bool GameScene::IsTileBlockingFromLeft(int column, int row) const
{
    if (IsSolidTile(column, row))
    {
        return true;
    }

    const float tileSize = m_tileMap.GetTileSize();
    const int originColumnStart = (std::max)(0, column - (kMaxTriangleSpanTiles - 1));
    const int originRowStart = (std::max)(0, row - (kMaxTriangleSpanTiles - 1));
    for (int originRow = originRowStart; originRow <= row; ++originRow)
    {
        for (int originColumn = originColumnStart; originColumn <= column; ++originColumn)
        {
            const TileTriangleShape triangle = TileMap::GetTriangleShape(m_tileMap.GetTile(originColumn, originRow));
            if (!triangle.isTriangle ||
                triangle.risesRight ||
                !DoesTriangleOccupyCell(originColumn, originRow, triangle, tileSize, column, row))
            {
                continue;
            }

            if (column == originColumn)
            {
                return true;
            }
        }
    }

    return false;
}

bool GameScene::IsTileBlockingFromRight(int column, int row) const
{
    if (IsSolidTile(column, row))
    {
        return true;
    }

    const float tileSize = m_tileMap.GetTileSize();
    const int originColumnStart = (std::max)(0, column - (kMaxTriangleSpanTiles - 1));
    const int originRowStart = (std::max)(0, row - (kMaxTriangleSpanTiles - 1));
    for (int originRow = originRowStart; originRow <= row; ++originRow)
    {
        for (int originColumn = originColumnStart; originColumn <= column; ++originColumn)
        {
            const TileTriangleShape triangle = TileMap::GetTriangleShape(m_tileMap.GetTile(originColumn, originRow));
            if (!triangle.isTriangle ||
                !triangle.risesRight ||
                !DoesTriangleOccupyCell(originColumn, originRow, triangle, tileSize, column, row))
            {
                continue;
            }

            if (column == originColumn + triangle.widthTiles - 1)
            {
                return true;
            }
        }
    }

    return false;
}

bool GameScene::IsPlatformTile(int column, int row) const
{
    static_cast<void>(column);
    static_cast<void>(row);
    return false;
}

bool GameScene::IsHazardTile(int column, int row) const
{
    return m_tileMap.GetTile(column, row) == 4;
}

bool GameScene::IsPitTile(int column, int row) const
{
    return m_tileMap.GetTile(column, row) == TileMap::kPitTileValue;
}

bool GameScene::IsGoalTile(int column, int row) const
{
    return m_tileMap.GetTile(column, row) == 5;
}

bool GameScene::IsStandingOnGround(const TransformComponent& transform) const
{
    std::vector<TransformComponent> groundPlatforms;
    GetGroundPlatformBounds(groundPlatforms);
    for (const auto& photoSourceBounds : groundPlatforms)
    {
        const float photoSourceX = photoSourceBounds.x;
        const float photoSourceY = photoSourceBounds.y;
        const float photoSourceWidth = photoSourceBounds.width * photoSourceBounds.scale;
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float playerBottom = transform.y + height;
        const float playerLeft = transform.x + 6.0f;
        const float playerRight = transform.x + width - 6.0f;
        const float sourceTop = photoSourceY;
        const float sourceLeft = photoSourceX;
        const float sourceRight = photoSourceX + photoSourceWidth;
        const bool horizontallyOverlapping = playerRight > sourceLeft && playerLeft < sourceRight;
        if (horizontallyOverlapping && std::fabs(playerBottom - sourceTop) <= kSurfaceContactEpsilon)
        {
            return true;
        }
    }

    // ?????]??????? PhotoBox / ?^?C??????
    for (const auto& entity : m_entities)
    {
        if (!entity || !IsSolidPolygonEntity(*entity))
        {
            continue;
        }

        if (!entity->GetComponent<TransformComponent>())
        {
            continue;
        }

        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float playerBottom = transform.y + height;
        const float playerLeft = transform.x + 6.0f;
        const float playerRight = transform.x + width - 6.0f;
        CollisionPolygon polygon;
        if (!BuildEntityCollisionPolygon(*entity, polygon))
        {
            continue;
        }

        float boxLeft = 0.0f;
        float boxTop = 0.0f;
        float boxRight = 0.0f;
        float boxBottom = 0.0f;
        GetPolygonAabb(polygon, boxLeft, boxTop, boxRight, boxBottom);
        static_cast<void>(boxBottom);
        const bool horizontallyOverlapping = playerRight > boxLeft && playerLeft < boxRight;
        if (horizontallyOverlapping && std::fabs(playerBottom - boxTop) <= kSurfaceContactEpsilon)
        {
            return true;
        }

        if (horizontallyOverlapping)
        {
            float probeXs[kGroundProbeCount]{};
            GetGroundProbeXs(transform, probeXs);
            for (float probeX : probeXs)
            {
                float photoSlopeSurfaceY = 0.0f;
                if (TryGetPhotoBoxSlopeSurfaceY(*entity, probeX, photoSlopeSurfaceY) &&
                    std::fabs(playerBottom - photoSlopeSurfaceY) <= kSurfaceContactEpsilon + 2.0f)
                {
                    return true;
                }
            }
        }
    }

    // ?^?C??????i??X?????j
    const float tileSize = m_tileMap.GetTileSize();
    const float width2 = transform.width * transform.scale;
    const float footY = transform.y + transform.height * transform.scale + 2.0f;
    const int rowStart = std::max(0, static_cast<int>((footY - 4.0f) / tileSize));
    const int rowEnd = std::min(m_tileMap.GetHeight() - 1, static_cast<int>(footY / tileSize));
    const int columnStart = static_cast<int>((transform.x + 6.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width2 - 6.0f) / tileSize);
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (IsSolidTile(column, row) || IsPlatformTile(column, row))
            {
                return true;
            }

            float probeXs[kGroundProbeCount]{};
            GetGroundProbeXs(transform, probeXs);
            for (float probeX : probeXs)
            {
                float slopeSurfaceY = 0.0f;
                if (TryGetSlopeSurfaceY(m_tileMap, column, row, probeX, slopeSurfaceY) &&
                    std::fabs((transform.y + transform.height * transform.scale) - slopeSurfaceY) <= kSurfaceContactEpsilon + 2.0f)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

bool GameScene::TrySnapToGroundUsingPlatforms(
    TransformComponent& transform,
    float maxSnapDistance,
    const std::vector<TransformComponent>& groundPlatforms) const
{
    for (const auto& photoSourceBounds : groundPlatforms)
    {
        const float photoSourceX = photoSourceBounds.x;
        const float photoSourceY = photoSourceBounds.y;
        const float photoSourceWidth = photoSourceBounds.width * photoSourceBounds.scale;
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float left = transform.x + 6.0f;
        const float right = transform.x + width - 6.0f;
        const bool horizontallyOverlapping = right > photoSourceX && left < photoSourceX + photoSourceWidth;
        if (horizontallyOverlapping)
        {
            const float candidateY = photoSourceY - height;
            if (candidateY >= transform.y - 0.5f && (candidateY - transform.y) <= maxSnapDistance)
            {
                transform.y = candidateY;
                return true;
            }
        }
    }

    // ?????]??????? PhotoBox ?x?[?X??z???t??????
    for (const auto& entity : m_entities)
    {
        if (!entity || !IsSolidPolygonEntity(*entity))
        {
            continue;
        }

        if (!entity->GetComponent<TransformComponent>())
        {
            continue;
        }

        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float left = transform.x + 6.0f;
        const float right = transform.x + width - 6.0f;
        CollisionPolygon polygon;
        if (!BuildEntityCollisionPolygon(*entity, polygon))
        {
            continue;
        }

        float photoBoxLeft = 0.0f;
        float photoBoxTop = 0.0f;
        float photoBoxRight = 0.0f;
        float photoBoxBottom = 0.0f;
        GetPolygonAabb(polygon, photoBoxLeft, photoBoxTop, photoBoxRight, photoBoxBottom);
        static_cast<void>(photoBoxBottom);
        const bool horizontallyOverlapping = right > photoBoxLeft && left < photoBoxRight;
        if (horizontallyOverlapping)
        {
            float candidateY = photoBoxTop - height;
            float probeXs[kGroundProbeCount]{};
            GetGroundProbeXs(transform, probeXs);
            bool foundSlopeProbe = false;
            for (float probeX : probeXs)
            {
                float photoSlopeSurfaceY = 0.0f;
                if (!TryGetPhotoBoxSlopeSurfaceY(*entity, probeX, photoSlopeSurfaceY))
                {
                    continue;
                }

                const float slopeCandidateY = photoSlopeSurfaceY - height;
                if (!foundSlopeProbe)
                {
                    candidateY = slopeCandidateY;
                    foundSlopeProbe = true;
                    continue;
                }

                if (slopeCandidateY < candidateY)
                {
                    candidateY = slopeCandidateY;
                }
            }

            if (candidateY >= transform.y - maxSnapDistance && std::fabs(candidateY - transform.y) <= maxSnapDistance)
            {
                transform.y = candidateY;
                return true;
            }
        }
    }

    // ?^?C???x?[?X??z???t???i??X?????j
    const float tileSize = m_tileMap.GetTileSize();
    const float width2 = transform.width * transform.scale;
    const float height2 = transform.height * transform.scale;
    const float bottom = transform.y + height2;
    const int columnStart = static_cast<int>((transform.x + 6.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width2 - 6.0f) / tileSize);
    const int rowStart = std::max(0, static_cast<int>((bottom - maxSnapDistance) / tileSize));
    const int rowEnd = std::min(
        m_tileMap.GetHeight() - 1,
        static_cast<int>((bottom + maxSnapDistance) / tileSize));

    float nearestGroundY = 0.0f;
    bool foundGround = false;
    float probeXs[kGroundProbeCount]{};
    GetGroundProbeXs(transform, probeXs);
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
            {
                continue;
            }

            if (IsSlopeTile(column, row))
            {
                bool hasSlopeCandidate = false;
                float slopeGroundY = 0.0f;
                for (float probeX : probeXs)
                {
                    float slopeSurfaceY = 0.0f;
                    TileTriangleShape triangle{};
                    if (!TryGetSlopeSurfaceY(m_tileMap, column, row, probeX, slopeSurfaceY, &triangle))
                    {
                        continue;
                    }

                    const float candidateY = slopeSurfaceY - height2;
                    if (candidateY < transform.y - maxSnapDistance)
                    {
                        continue;
                    }

                    if (!hasSlopeCandidate)
                    {
                        slopeGroundY = candidateY;
                        hasSlopeCandidate = true;
                        continue;
                    }

                    if ((triangle.risesRight && candidateY < slopeGroundY) ||
                        (!triangle.risesRight && candidateY > slopeGroundY))
                    {
                        slopeGroundY = candidateY;
                    }
                }

                if (hasSlopeCandidate && (!foundGround || slopeGroundY < nearestGroundY))
                {
                    nearestGroundY = slopeGroundY;
                    foundGround = true;
                }
                continue;
            }

            const float candidateY = static_cast<float>(row) * tileSize - height2;
            if (candidateY < transform.y - maxSnapDistance)
            {
                continue;
            }

            if (!foundGround || candidateY < nearestGroundY)
            {
                nearestGroundY = candidateY;
                foundGround = true;
            }
        }
    }

    if (!foundGround)
    {
        return false;
    }

    if (std::fabs(nearestGroundY - transform.y) > maxSnapDistance)
    {
        return false;
    }

    transform.y = nearestGroundY;
    return true;
}

bool GameScene::IntersectsHazardTile(const TransformComponent& transform) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const int columnStart = static_cast<int>((transform.x + 8.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 8.0f) / tileSize);
    const int footRow = static_cast<int>((transform.y + transform.height * transform.scale + 2.0f) / tileSize);
    for (int column = columnStart; column <= columnEnd; ++column)
    {
        if (IsHazardTile(column, footRow))
        {
            return true;
        }
    }
    return false;
}

bool GameScene::TrySnapToGround(TransformComponent& transform, float maxSnapDistance) const
{
    std::vector<TransformComponent> groundPlatforms;
    GetGroundPlatformBounds(groundPlatforms);
    return TrySnapToGroundUsingPlatforms(transform, maxSnapDistance, groundPlatforms);
}

bool GameScene::IntersectsPitTile(const TransformComponent& transform) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const int columnStart = static_cast<int>((transform.x + 8.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 8.0f) / tileSize);
    const int footRow = static_cast<int>((transform.y + transform.height * transform.scale + 2.0f) / tileSize);
    for (int column = columnStart; column <= columnEnd; ++column)
    {
        if (IsPitTile(column, footRow))
        {
            return true;
        }
    }
    return false;
}

bool GameScene::IntersectsGoalTile(const TransformComponent& transform) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const float height = transform.height * transform.scale;
    const int columnStart = static_cast<int>((transform.x + 8.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 8.0f) / tileSize);
    const int rowStart = static_cast<int>((transform.y + 8.0f) / tileSize);
    const int rowEnd = static_cast<int>((transform.y + height - 8.0f) / tileSize);
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (IsGoalTile(column, row))
            {
                return true;
            }
        }
    }
    return false;
}

bool GameScene::IntersectsEntity(const Entity& a, const Entity& b) const
{
    const auto* transformA = a.GetComponent<TransformComponent>();
    const auto* transformB = b.GetComponent<TransformComponent>();
    if (!transformA || !transformB)
    {
        return false;
    }

    return IntersectsRect(*transformA, *transformB);
}

bool GameScene::IntersectsHazardEntity(const Entity& player, const Entity& hazard) const
{
    const auto* hazardTransform = hazard.GetComponent<TransformComponent>();
    const auto* playerTransform = player.GetComponent<TransformComponent>();
    const auto* damagePlatform = hazard.GetComponent<DamagePlatformComponent>();
    const auto* tileValue = hazard.GetComponent<PhotoCopyTileValueComponent>();
    const auto* imageCollider = hazard.GetComponent<ImageOutlineColliderComponent>();
    if (hazardTransform && playerTransform && damagePlatform)
    {
        CollisionPolygon playerPolygon;
        BuildRotatedRectPolygon(
            playerTransform->x,
            playerTransform->y,
            playerTransform->width * playerTransform->scale,
            playerTransform->height * playerTransform->scale,
            playerTransform->rotation,
            playerPolygon);

        CollisionPolygon hazardBandPolygon;
        BuildDamagePlatformHazardBandPolygon(*hazardTransform, hazardBandPolygon);
        if (PolygonsIntersect(playerPolygon, hazardBandPolygon))
        {
            return true;
        }

        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float footSampleY = playerTransform->y + playerHeight + 2.0f;
        const float footSampleXs[] =
        {
            playerTransform->x + 6.0f,
            playerTransform->x + playerWidth * 0.5f,
            playerTransform->x + playerWidth - 6.0f
        };
        for (const float footSampleX : footSampleXs)
        {
            if (IsPointInDamagePlatformHazardBand(*hazardTransform, footSampleX, footSampleY, 3.0f))
            {
                return true;
            }
        }
        return false;
    }

    if (!IntersectsEntity(player, hazard))
    {
        return false;
    }

    if (!hazardTransform || !playerTransform || !tileValue)
    {
        if (!hazardTransform || !playerTransform || !imageCollider)
        {
            return true;
        }
    }

    const TileTriangleShape triangle = tileValue ? TileMap::GetTriangleShape(tileValue->tileValue) : TileTriangleShape{};
    if ((!triangle.isTriangle && !imageCollider) || !m_player.grounded)
    {
        return true;
    }

    CollisionPolygon hazardPolygon;
    if (!BuildEntityCollisionPolygon(hazard, hazardPolygon))
    {
        return true;
    }

    const float playerWidth = playerTransform->width * playerTransform->scale;
    const float playerHeight = playerTransform->height * playerTransform->scale;
    const float playerBottom = playerTransform->y + playerHeight;
    const float playerTop = playerTransform->y;
    const float probeXs[] =
    {
        playerTransform->x + 6.0f,
        playerTransform->x + playerWidth * 0.5f,
        playerTransform->x + playerWidth - 6.0f,
    };

    for (const float probeX : probeXs)
    {
        float surfaceY = 0.0f;
        if (!TryGetPolygonSurfaceY(hazardPolygon, probeX, surfaceY))
        {
            continue;
        }

        if (playerTop < surfaceY - 2.0f &&
            std::fabs(playerBottom - surfaceY) <= kSurfaceContactEpsilon + 3.0f)
        {
            return false;
        }
    }

    return true;
}

bool GameScene::IntersectsSolidPhotoBox(const TransformComponent& transform) const
{
    CollisionPolygon candidatePolygon;
    BuildRotatedRectPolygon(
        transform.x,
        transform.y,
        transform.width * transform.scale,
        transform.height * transform.scale,
        transform.rotation,
        candidatePolygon);

    for (const auto& entity : m_entities)
    {
        if (!entity || !IsSolidPolygonEntity(*entity))
        {
            continue;
        }

        CollisionPolygon photoPolygon;
        if (!BuildEntityCollisionPolygon(*entity, photoPolygon))
        {
            continue;
        }

        if (PolygonsIntersect(candidatePolygon, photoPolygon))
        {
            return true;
        }
    }

    return false;
}

bool GameScene::IntersectsSolidPhotoBoxForMovement(const TransformComponent& transform) const
{
    CollisionPolygon candidatePolygon;
    BuildRotatedRectPolygon(
        transform.x,
        transform.y,
        transform.width * transform.scale,
        transform.height * transform.scale,
        transform.rotation,
        candidatePolygon);

    const float width = transform.width * transform.scale;
    const float height = transform.height * transform.scale;
    const float candidateBottom = transform.y + height;
    const float candidateLeft = transform.x + 6.0f;
    const float candidateRight = transform.x + width - 6.0f;
    float probeXs[kGroundProbeCount]{};
    GetGroundProbeXs(transform, probeXs);

    for (const auto& entity : m_entities)
    {
        if (!entity || !IsSolidPolygonEntity(*entity))
        {
            continue;
        }

        CollisionPolygon photoPolygon;
        if (!BuildEntityCollisionPolygon(*entity, photoPolygon))
        {
            continue;
        }

        if (!PolygonsIntersect(candidatePolygon, photoPolygon))
        {
            continue;
        }

        float boxLeft = 0.0f;
        float boxTop = 0.0f;
        float boxRight = 0.0f;
        float boxBottom = 0.0f;
        GetPolygonAabb(photoPolygon, boxLeft, boxTop, boxRight, boxBottom);
        static_cast<void>(boxBottom);

        const bool horizontallyOverlapping = candidateRight > boxLeft && candidateLeft < boxRight;
        bool restingOnTopSurface = horizontallyOverlapping &&
            std::fabs(candidateBottom - boxTop) <= kSurfaceContactEpsilon;

        if (!restingOnTopSurface && horizontallyOverlapping)
        {
            for (float probeX : probeXs)
            {
                float surfaceY = 0.0f;
                if (!TryGetPhotoBoxSlopeSurfaceY(*entity, probeX, surfaceY))
                {
                    continue;
                }

                if (std::fabs(candidateBottom - surfaceY) <= kSurfaceContactEpsilon + 2.0f)
                {
                    restingOnTopSurface = true;
                    break;
                }
            }
        }

        if (!restingOnTopSurface)
        {
            return true;
        }
    }

    return false;
}

bool GameScene::GetEntityBoundsByTag(const char* tag, float& x, float& y, float& width, float& height) const
{
    Entity* entity = FindEntityByTag(tag);
    if (!entity)
    {
        return false;
    }

    const auto* transform = entity->GetComponent<TransformComponent>();
    if (!transform)
    {
        return false;
    }

    x = transform->x;
    y = transform->y;
    width = transform->width * transform->scale;
    height = transform->height * transform->scale;
    return true;
}

void GameScene::GetEntityBoundsByTag(const char* tag, std::vector<TransformComponent>& bounds) const
{
    bounds.clear();
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, tag))
        {
            continue;
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform)
        {
            continue;
        }

        TransformComponent rect(transform->x, transform->y, transform->width, transform->height);
        rect.scale = transform->scale;
        bounds.push_back(rect);
    }
}

bool GameScene::IsGroundPlatformEntity(const Entity& entity) const
{
    return HasTag(entity, kTagPhotoSource) ||
        HasTag(entity, kTagBatterySwitch) ||
        HasTag(entity, kTagElevator) ||
        HasTag(entity, kTagLaserSwitch) ||
        HasTag(entity, kTagShutter) ||
        HasTag(entity, kTagLaserTurret);
}

void GameScene::GetGroundPlatformBounds(std::vector<TransformComponent>& bounds) const
{
    bounds.clear();
    for (const auto& entity : m_entities)
    {
        if (!entity || !IsGroundPlatformEntity(*entity))
        {
            continue;
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform)
        {
            continue;
        }

        TransformComponent rect(transform->x, transform->y, transform->width, transform->height);
        rect.scale = transform->scale;
        bounds.push_back(rect);
    }
}

void GameScene::GetPhotoBoxBounds(std::vector<TransformComponent>& bounds) const
{
    bounds.clear();
    for (const auto& entity : m_entities)
    {
        if (!entity || !IsSolidPolygonEntity(*entity))
        {
            continue;
        }
        CollisionPolygon polygon;
        if (!BuildEntityCollisionPolygon(*entity, polygon))
        {
            continue;
        }

        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        GetPolygonAabb(polygon, left, top, right, bottom);
        TransformComponent rect(left, top, right - left, bottom - top);
        rect.scale = 1.0f;
        bounds.push_back(rect);
    }
}

bool GameScene::FindSpawnPosition(float desiredX, float objectWidth, float objectHeight, float& outX, float& outY) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float mapWidth = GetMapPixelWidth();
    const float desiredCenterX = desiredX + objectWidth * 0.5f;
    const int centerColumn = static_cast<int>(desiredCenterX / tileSize);
    const int desiredSupportRow = std::clamp(
        static_cast<int>((outY + objectHeight) / tileSize),
        0,
        m_tileMap.GetHeight() - 1);
    const int maxOffset = 6;

    for (int offset = 0; offset <= maxOffset; ++offset)
    {
        const int candidates[2] = { centerColumn + offset, centerColumn - offset };
        for (int i = 0; i < 2; ++i)
        {
            const int column = candidates[i];
            if (column < 0 || column >= m_tileMap.GetWidth())
            {
                continue;
            }

            const float columnCenterX = (static_cast<float>(column) + 0.5f) * tileSize;
            const float candidateX = std::clamp(columnCenterX - objectWidth * 0.5f, 0.0f, std::max(0.0f, mapWidth - objectWidth));
            const int leftColumn = std::max(0, static_cast<int>(candidateX / tileSize));
            const int rightColumn = std::min(
                m_tileMap.GetWidth() - 1,
                static_cast<int>((candidateX + objectWidth - 1.0f) / tileSize));

            for (int rowOffset = 0; rowOffset < m_tileMap.GetHeight(); ++rowOffset)
            {
                const int supportCandidates[2] = { desiredSupportRow + rowOffset, desiredSupportRow - rowOffset };
                for (int rowIndex = 0; rowIndex < 2; ++rowIndex)
                {
                    const int supportRow = supportCandidates[rowIndex];
                    if (supportRow < 0 || supportRow >= m_tileMap.GetHeight())
                    {
                        continue;
                    }
                    if (rowOffset == 0 && rowIndex == 1)
                    {
                        continue;
                    }

                    bool hasSupport = false;
                    for (int supportColumn = leftColumn; supportColumn <= rightColumn; ++supportColumn)
                    {
                        if (IsSolidTile(supportColumn, supportRow) || IsSlopeTile(supportColumn, supportRow))
                        {
                            hasSupport = true;
                            break;
                        }
                    }

                    if (!hasSupport)
                    {
                        continue;
                    }

                    const float candidateY = static_cast<float>(supportRow) * tileSize - objectHeight;
                    const int topRow = std::max(0, static_cast<int>(candidateY / tileSize));
                    const int bottomRow = std::min(
                        m_tileMap.GetHeight() - 1,
                        static_cast<int>((candidateY + objectHeight - 1.0f) / tileSize));

                    bool intersectsSolid = false;
                    for (int testRow = topRow; testRow <= bottomRow && !intersectsSolid; ++testRow)
                    {
                        for (int testColumn = leftColumn; testColumn <= rightColumn; ++testColumn)
                        {
                            if (!IsSolidTile(testColumn, testRow) && !IsSlopeTile(testColumn, testRow))
                            {
                                continue;
                            }

                            const float tileX = static_cast<float>(testColumn) * tileSize;
                            const float tileY = static_cast<float>(testRow) * tileSize;
                            if (candidateX < tileX + tileSize &&
                                candidateX + objectWidth > tileX &&
                                candidateY < tileY + tileSize &&
                                candidateY + objectHeight > tileY)
                            {
                                intersectsSolid = true;
                                break;
                            }
                        }
                    }

                    if (!intersectsSolid)
                    {
                        outX = candidateX;
                        outY = candidateY;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool GameScene::IsPhotoPlacementValid(float x, float y, float width, float height) const
{
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    if (x < 0.0f || y < 0.0f || x + width > mapWidth || y + height > mapHeight)
    {
        return false;
    }

    const float tileSize = m_tileMap.GetTileSize();
    auto buildCandidatePolygon = [](const TransformComponent& candidate, CollisionPolygon& outPolygon)
    {
        BuildRotatedRectPolygon(
            candidate.x,
            candidate.y,
            candidate.width * candidate.scale,
            candidate.height * candidate.scale,
            candidate.rotation,
            outPolygon);
    };
    // Forbidden target: Enemy. Disabled enemies are excluded from overlap checks.
    auto intersectsEnemy = [&](const TransformComponent& candidate) -> bool
    {
        CollisionPolygon candidatePolygon;
        buildCandidatePolygon(candidate, candidatePolygon);
        for (const auto& entity : m_entities)
        {
            if (!entity)
            {
                continue;
            }

            const bool isEnemyEntity = HasTag(*entity, kTagEnemy) || entity->GetComponent<EnemyComponent>() != nullptr;
            if (!isEnemyEntity)
            {
                continue;
            }

            if (const auto* enemy = entity->GetComponent<EnemyComponent>())
            {
                if (!enemy->IsEnabled())
                {
                    continue;
                }
            }

            const auto* transform = entity->GetComponent<TransformComponent>();
            CollisionPolygon entityPolygon;
            if (transform && BuildEntityCollisionPolygon(*entity, entityPolygon) && PolygonsIntersect(candidatePolygon, entityPolygon))
            {
                return true;
            }
        }

        return false;
    };

    // Forbidden target: Floor. Check solid/slope tiles and solid pasted photo boxes.
    auto intersectsFloorObject = [&](const TransformComponent& candidate) -> bool
    {
        CollisionPolygon candidatePolygon;
        buildCandidatePolygon(candidate, candidatePolygon);
        const int leftColumn = std::max(0, static_cast<int>(candidate.x / tileSize));
        const int rightColumn = std::min(
            m_tileMap.GetWidth() - 1,
            static_cast<int>((candidate.x + candidate.width * candidate.scale - 1.0f) / tileSize));
        const int topRow = std::max(0, static_cast<int>(candidate.y / tileSize));
        const int bottomRow = std::min(
            m_tileMap.GetHeight() - 1,
            static_cast<int>((candidate.y + candidate.height * candidate.scale - 1.0f) / tileSize));

        for (int row = topRow; row <= bottomRow; ++row)
        {
            for (int column = leftColumn; column <= rightColumn; ++column)
            {
                if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                {
                    continue;
                }

                TransformComponent tileRect(
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize,
                    tileSize,
                    tileSize);
                CollisionPolygon tilePolygon;
                BuildRotatedRectPolygon(tileRect.x, tileRect.y, tileRect.width, tileRect.height, 0.0f, tilePolygon);
                if (PolygonsIntersect(candidatePolygon, tilePolygon))
                {
                    return true;
                }
            }
        }

        return IntersectsSolidPhotoBox(candidate);
    };

    constexpr std::array<PhotoPlacementForbiddenTarget, 2> kPlacementForbiddenTargets = {
        PhotoPlacementForbiddenTarget::Floor,
        PhotoPlacementForbiddenTarget::Enemy,
    };

    auto violatesForbiddenTarget = [&](const TransformComponent& candidate, PhotoPlacementForbiddenTarget target) -> bool
    {
        switch (target)
        {
        case PhotoPlacementForbiddenTarget::Floor:
            return intersectsFloorObject(candidate);
        case PhotoPlacementForbiddenTarget::Enemy:
            return intersectsEnemy(candidate);
        case PhotoPlacementForbiddenTarget::None:
        default:
            return false;
        }
    };

    auto violatesPlacementRule = [&](const TransformComponent& candidate, PhotoPlacementRuleGroup group) -> bool
    {
        const auto* ruleDefinition = FindPhotoPlacementRuleDefinition(group);
        const std::uint8_t forbiddenMask = ruleDefinition
            ? ruleDefinition->forbiddenMask
            : GetPlacementForbiddenMask(PhotoPlacementRuleGroup::Group1);
        for (const PhotoPlacementForbiddenTarget target : kPlacementForbiddenTargets)
        {
            if (!HasPlacementForbiddenTarget(forbiddenMask, target))
            {
                continue;
            }

            if (violatesForbiddenTarget(candidate, target))
            {
                return true;
            }
        }

        return false;
    };

    if (m_photo.capture.items.empty())
    {
        // Backward-compatible fallback: treat as Group1 when no captured items exist.
        TransformComponent candidate(x, y, width, height);
        return !violatesPlacementRule(candidate, PhotoPlacementRuleGroup::Group1);
    }

    float placementWidth = 0.0f;
    float placementHeight = 0.0f;
    const std::vector<CapturedPhotoItem> placementItems = photo_system_bridge::BuildPlacementItemsBridge(
        m_photo.capture,
        m_photo.placement,
        m_whiteTexture,
        placementWidth,
        placementHeight);

    for (const auto& item : placementItems)
    {
        TransformComponent candidate(x + item.relativeX, y + item.relativeY, item.width, item.height);
        candidate.rotation = item.rotation;
        if (violatesPlacementRule(candidate, item.placementRuleGroup))
        {
            return false;
        }
    }

    return true;
}

float GameScene::GetMapPixelWidth() const
{
    return static_cast<float>(m_tileMap.GetWidth()) * m_tileMap.GetTileSize();
}

float GameScene::GetMapPixelHeight() const
{
    return static_cast<float>(m_tileMap.GetHeight()) * m_tileMap.GetTileSize();
}

// 3/21 added: snap enemies to ground.
bool GameScene::SnapEnemyToGround(TransformComponent& transform) const
{
    return TrySnapToGround(transform, 48.0f);
}
