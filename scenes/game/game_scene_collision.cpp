#include "game_scene_internal.h"

using namespace game_scene_detail;

namespace
{
    constexpr int kMaxTriangleSpanTiles = 5;

    struct CollisionPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    using CollisionPolygon = std::vector<CollisionPoint>;

    void RotatePoint(float centerX, float centerY, float rotation, float& x, float& y)
    {
        if (std::fabs(rotation) <= 0.0001f)
        {
            return;
        }

        const float localX = x - centerX;
        const float localY = y - centerY;
        const float cosTheta = std::cos(rotation);
        const float sinTheta = std::sin(rotation);
        x = centerX + (localX * cosTheta - localY * sinTheta);
        y = centerY + (localX * sinTheta + localY * cosTheta);
    }

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

    bool BuildPhotoBoxCollisionPolygon(const Entity& entity, CollisionPolygon& outPolygon)
    {
        const auto* transform = entity.GetComponent<TransformComponent>();
        if (!transform)
        {
            return false;
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

    bool PolygonsIntersect(const CollisionPolygon& a, const CollisionPolygon& b)
    {
        const CollisionPolygon* polygons[2] = { &a, &b };
        for (const CollisionPolygon* polygon : polygons)
        {
            for (size_t index = 0; index < polygon->size(); ++index)
            {
                const CollisionPoint& p0 = (*polygon)[index];
                const CollisionPoint& p1 = (*polygon)[(index + 1) % polygon->size()];
                const float edgeX = p1.x - p0.x;
                const float edgeY = p1.y - p0.y;
                const float axisX = -edgeY;
                const float axisY = edgeX;
                const float axisLength = std::sqrt(axisX * axisX + axisY * axisY);
                if (axisLength <= 0.0001f)
                {
                    continue;
                }

                const float normalizedAxisX = axisX / axisLength;
                const float normalizedAxisY = axisY / axisLength;
                float minA = 0.0f;
                float maxA = 0.0f;
                float minB = 0.0f;
                float maxB = 0.0f;
                ProjectPolygonOntoAxis(a, normalizedAxisX, normalizedAxisY, minA, maxA);
                ProjectPolygonOntoAxis(b, normalizedAxisX, normalizedAxisY, minB, maxB);
                if (maxA < minB || maxB < minA)
                {
                    return false;
                }
            }
        }

        return true;
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

    bool UsesSolidCollision(const Entity& entity)
    {
        if (const auto* layer = entity.GetComponent<PhotoCopyLayerComponent>())
        {
            return layer->layer == PhotoCopyLayer::Foreground;
        }
        return true;
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
        if (!BuildPhotoBoxCollisionPolygon(entity, polygon))
        {
            return false;
        }
        return TryGetPolygonSurfaceY(polygon, worldX, outSurfaceY);
    }

    void GetGroundProbeXs(const TransformComponent& transform, float outProbeXs[3])
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
    std::vector<TransformComponent> photoSources;
    GetEntityBoundsByTag("PhotoSource", photoSources);
    for (const auto& photoSourceBounds : photoSources)
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
        if (!entity || !HasTag(*entity, "PhotoBox") || !UsesSolidCollision(*entity))
        {
            continue;
        }

        const auto* photoBoxTransform = entity->GetComponent<TransformComponent>();
        if (!photoBoxTransform)
        {
            continue;
        }

        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float playerBottom = transform.y + height;
        const float playerLeft = transform.x + 6.0f;
        const float playerRight = transform.x + width - 6.0f;
        CollisionPolygon polygon;
        if (!BuildPhotoBoxCollisionPolygon(*entity, polygon))
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
            float probeXs[3]{};
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

            float probeXs[3]{};
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

bool GameScene::TrySnapToGround(TransformComponent& transform, float maxSnapDistance) const
{
    std::vector<TransformComponent> photoSources;
    GetEntityBoundsByTag("PhotoSource", photoSources);
    for (const auto& photoSourceBounds : photoSources)
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
        if (!entity || !HasTag(*entity, "PhotoBox") || !UsesSolidCollision(*entity))
        {
            continue;
        }

        const auto* photoBoxTransform = entity->GetComponent<TransformComponent>();
        if (!photoBoxTransform)
        {
            continue;
        }

        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float left = transform.x + 6.0f;
        const float right = transform.x + width - 6.0f;
        CollisionPolygon polygon;
        if (!BuildPhotoBoxCollisionPolygon(*entity, polygon))
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
            float probeXs[3]{};
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
    float probeXs[3]{};
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
        if (!entity || !HasTag(*entity, "PhotoBox") || !UsesSolidCollision(*entity))
        {
            continue;
        }

        CollisionPolygon photoPolygon;
        if (!BuildPhotoBoxCollisionPolygon(*entity, photoPolygon))
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

void GameScene::GetPhotoBoxBounds(std::vector<TransformComponent>& bounds) const
{
    bounds.clear();
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }
        if (!UsesSolidCollision(*entity))
        {
            continue;
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform)
        {
            continue;
        }
        CollisionPolygon polygon;
        if (!BuildPhotoBoxCollisionPolygon(*entity, polygon))
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
    const bool usesWorldCollision = m_photo.placement.layer == PhotoCopyLayer::Foreground;
    auto intersectsWorld = [&](const TransformComponent& candidate) -> bool
    {
        if (!usesWorldCollision)
        {
            for (const auto& entity : m_entities)
            {
                if (!entity || !HasTag(*entity, "Player"))
                {
                    continue;
                }

                const auto* transform = entity->GetComponent<TransformComponent>();
                if (transform && IntersectsRect(candidate, *transform))
                {
                    return true;
                }
            }

            return false;
        }

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
                if (IntersectsRect(candidate, tileRect))
                {
                    return true;
                }
            }
        }

        for (const auto& entity : m_entities)
        {
            if (!entity)
            {
                continue;
            }

            const auto* transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                continue;
            }

            if (HasTag(*entity, "PhotoBox"))
            {
                if (UsesSolidCollision(*entity) && IntersectsSolidPhotoBox(candidate))
                {
                    return true;
                }
                continue;
            }

            if (!HasTag(*entity, "Player") && !HasTag(*entity, "PhotoSource") && !HasTag(*entity, "Goal"))
            {
                continue;
            }

            if (IntersectsRect(candidate, *transform))
            {
                return true;
            }
        }

        return false;
    };

    if (m_photo.capture.items.empty())
    {
        TransformComponent candidate(x, y, width, height);
        return !intersectsWorld(candidate);
    }

    for (const auto& item : m_photo.capture.items)
    {
        TransformComponent candidate(x + item.relativeX, y + item.relativeY, item.width, item.height);
        if (intersectsWorld(candidate))
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

// 3/21追加：敵の地面スナップ(田之上俊)
bool GameScene::SnapEnemyToGround(TransformComponent& transform) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float enemyWidth = transform.width * transform.scale;
    const float enemyHeight = transform.height * transform.scale;
    const float bottom = transform.y + enemyHeight;
    const int columnStart = static_cast<int>((transform.x + 4.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + enemyWidth - 4.0f) / tileSize);
    const int rowStart = std::max(0, static_cast<int>((bottom - 8.0f) / tileSize));
    const int rowEnd = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bottom + 48.0f) / tileSize));

    float nearestGroundY = transform.y;
    float nearestDistance = 0.0f;
    bool foundGround = false;
    const float probeXs[3] = {
        transform.x + 4.0f,
        transform.x + enemyWidth * 0.5f,
        transform.x + enemyWidth - 4.0f
    };

    auto considerCandidate = [&](float candidateY)
    {
        const float distance = std::fabs(candidateY - transform.y);
        if (!foundGround || distance < nearestDistance)
        {
            nearestGroundY = candidateY;
            nearestDistance = distance;
            foundGround = true;
        }
    };

    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (IsSolidTile(column, row))
            {
                considerCandidate(static_cast<float>(row) * tileSize - enemyHeight);
                continue;
            }

            for (float probeX : probeXs)
            {
                float slopeSurfaceY = 0.0f;
                if (TryGetSlopeSurfaceYShared(m_tileMap, column, row, probeX, slopeSurfaceY))
                {
                    considerCandidate(slopeSurfaceY - enemyHeight);
                }
            }
        }
    }

    if (foundGround && nearestDistance <= 48.0f)
    {
        transform.y = nearestGroundY;
        return true;
    }
    return false;
}
