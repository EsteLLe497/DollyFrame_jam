#pragma once

class Entity;

namespace Collision
{
    bool Intersects(const Entity& lhs, const Entity& rhs);
}
