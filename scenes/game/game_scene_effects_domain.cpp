#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>

using namespace game_scene_detail;

namespace
{
    constexpr float kBarrelDebrisGravity = 980.0f;
}

void GameScene::UpdateEffects(float deltaTime)
{
    for (auto& particle : m_effects.barrelDebris)
    {
        particle.life = std::max(0.0f, particle.life - deltaTime);
        particle.x += particle.velocityX * deltaTime;
        particle.y += particle.velocityY * deltaTime;
        particle.velocityY += kBarrelDebrisGravity * deltaTime;
        particle.rotation += particle.rotationSpeed * deltaTime;
    }
    for (auto& spark : m_effects.laserSparks)
    {
        spark.life = std::max(0.0f, spark.life - deltaTime);
        spark.x += spark.velocityX * deltaTime;
        spark.y += spark.velocityY * deltaTime;
        spark.velocityY += kBarrelDebrisGravity * deltaTime * spark.gravityScale;
    }

    m_effects.barrelDebris.erase(
        std::remove_if(
            m_effects.barrelDebris.begin(),
            m_effects.barrelDebris.end(),
            [](const BarrelDebrisParticle& particle)
            {
                return particle.life <= 0.0f;
            }),
        m_effects.barrelDebris.end());
    m_effects.laserSparks.erase(
        std::remove_if(
            m_effects.laserSparks.begin(),
            m_effects.laserSparks.end(),
            [](const LaserSparkParticle& spark)
            {
                return spark.life <= 0.0f;
            }),
        m_effects.laserSparks.end());
}

