#include "entity.h"

void Entity::Update(float deltaTime)
{
    for (const auto& component : m_components)
    {
        component->Update(deltaTime);
    }
}

void Entity::Draw()
{
    for (const auto& component : m_components)
    {
        component->Draw();
    }
}

void Entity::DrawDebugUI()
{
    for (const auto& component : m_components)
    {
        component->DrawDebugUI();
    }
}
