#include "pch.h"

#include "collision.h"

#include <cmath>

#include "components.h"
#include "entity.h"

namespace Collision
{
    bool Intersects(const Entity& lhs, const Entity& rhs)
    {
        const auto* left = lhs.GetComponent<TransformComponent>();
        const auto* right = rhs.GetComponent<TransformComponent>();
        if (!left || !right)
        {
            return false;
        }

        const float leftHalfW = (left->width * left->scale) * 0.5f;
        const float leftHalfH = (left->height * left->scale) * 0.5f;
        const float rightHalfW = (right->width * right->scale) * 0.5f;
        const float rightHalfH = (right->height * right->scale) * 0.5f;

        const float leftCenterX = left->x + leftHalfW;
        const float leftCenterY = left->y + leftHalfH;
        const float rightCenterX = right->x + rightHalfW;
        const float rightCenterY = right->y + rightHalfH;

        return std::fabs(leftCenterX - rightCenterX) <= (leftHalfW + rightHalfW)
            && std::fabs(leftCenterY - rightCenterY) <= (leftHalfH + rightHalfH);
    }
}
