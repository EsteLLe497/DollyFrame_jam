#include "game_scene_internal.h"

using namespace game_scene_detail;

namespace
{
    bool UsesSolidCollision(const Entity& entity)
    {
        if (const auto* layer = entity.GetComponent<PhotoCopyLayerComponent>())
        {
            return layer->layer == PhotoCopyLayer::Foreground;
        }
        return true;
    }
}

bool GameScene::IsSolidTile(int column, int row) const
{
    const int tile = m_tileMap.GetTile(column, row);
    return tile == 1 || tile == 2 || tile == 3 || tile == 4;
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

bool GameScene::IsGoalTile(int column, int row) const
{
    return m_tileMap.GetTile(column, row) == 5;
}

bool GameScene::IsStandingOnGround(const TransformComponent& transform) const
{
    float photoSourceX = 0.0f;
    float photoSourceY = 0.0f;
    float photoSourceWidth = 0.0f;
    float photoSourceHeight = 0.0f;
    if (GetEntityBoundsByTag("PhotoSource", photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight))
    {
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

    std::vector<TransformComponent> photoBoxes;
    GetPhotoBoxBounds(photoBoxes);
    for (const auto& photoBox : photoBoxes)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float playerBottom = transform.y + height;
        const float playerLeft = transform.x + 6.0f;
        const float playerRight = transform.x + width - 6.0f;
        const float boxTop = photoBox.y;
        const float boxLeft = photoBox.x;
        const float boxRight = photoBox.x + photoBox.width * photoBox.scale;
        const bool horizontallyOverlapping = playerRight > boxLeft && playerLeft < boxRight;
        if (horizontallyOverlapping && std::fabs(playerBottom - boxTop) <= kSurfaceContactEpsilon)
        {
            return true;
        }
    }

    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const float footY = transform.y + transform.height * transform.scale + 2.0f;
    const int row = static_cast<int>(footY / tileSize);
    const int columnStart = static_cast<int>((transform.x + 6.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 6.0f) / tileSize);
    for (int column = columnStart; column <= columnEnd; ++column)
    {
        if (IsSolidTile(column, row) || IsPlatformTile(column, row))
        {
            return true;
        }
    }
    return false;
}

bool GameScene::TrySnapToGround(TransformComponent& transform, float maxSnapDistance) const
{
    float photoSourceX = 0.0f;
    float photoSourceY = 0.0f;
    float photoSourceWidth = 0.0f;
    float photoSourceHeight = 0.0f;
    if (GetEntityBoundsByTag("PhotoSource", photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight))
    {
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

    std::vector<TransformComponent> photoBoxes;
    GetPhotoBoxBounds(photoBoxes);
    for (const auto& photoBox : photoBoxes)
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float left = transform.x + 6.0f;
        const float right = transform.x + width - 6.0f;
        const float photoBoxWidth = photoBox.width * photoBox.scale;
        const bool horizontallyOverlapping = right > photoBox.x && left < photoBox.x + photoBoxWidth;
        if (horizontallyOverlapping)
        {
            const float candidateY = photoBox.y - height;
            if (candidateY >= transform.y - 0.5f && (candidateY - transform.y) <= maxSnapDistance)
            {
                transform.y = candidateY;
                return true;
            }
        }
    }

    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const float height = transform.height * transform.scale;
    const float bottom = transform.y + height;
    const int columnStart = static_cast<int>((transform.x + 6.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 6.0f) / tileSize);
    const int rowStart = static_cast<int>(bottom / tileSize);
    const int rowEnd = static_cast<int>((bottom + maxSnapDistance) / tileSize);

    float nearestGroundY = 0.0f;
    bool foundGround = false;
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (!IsSolidTile(column, row))
            {
                continue;
            }

            const float candidateY = static_cast<float>(row) * tileSize - height;
            if (candidateY < transform.y - 0.5f)
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

    if ((nearestGroundY - transform.y) > maxSnapDistance)
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

        TransformComponent rect(transform->x, transform->y, transform->width, transform->height);
        rect.scale = transform->scale;
        bounds.push_back(rect);
    }
}

bool GameScene::FindSpawnPosition(float desiredX, float objectWidth, float objectHeight, float& outX, float& outY) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float mapWidth = GetMapPixelWidth();
    const float desiredCenterX = desiredX + objectWidth * 0.5f;
    const int centerColumn = static_cast<int>(desiredCenterX / tileSize);
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

            for (int supportRow = 0; supportRow < m_tileMap.GetHeight(); ++supportRow)
            {
                bool hasSupport = false;
                for (int supportColumn = leftColumn; supportColumn <= rightColumn; ++supportColumn)
                {
                    if (IsSolidTile(supportColumn, supportRow))
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
                        if (!IsSolidTile(testColumn, testRow))
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
                if (!IsSolidTile(column, row))
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

            if (HasTag(*entity, "PhotoBox"))
            {
                continue;
            }

            const auto* transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
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
