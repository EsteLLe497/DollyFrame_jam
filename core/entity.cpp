#include "pch.h"

#include "entity.h"

void GameObject::SetActive(bool active)
{
    if (m_activeSelf == active)
    {
        return;
    }

    m_activeSelf = active;
    for (const auto& component : m_components)
    {
        if (!component->IsEnabled())
        {
            continue;
        }

        if (m_activeSelf)
        {
            component->OnEnable();
        }
        else
        {
            component->OnDisable();
        }
    }
}

bool GameObject::IsActive() const
{
    return m_activeSelf;
}

void GameObject::Update(float deltaTime)
{
    if (!m_activeSelf)
    {
        return;
    }

    for (const auto& component : m_components)
    {
        if (!component->IsEnabled())
        {
            continue;
        }

        if (!component->m_started)
        {
            component->m_started = true;
            component->Start();
        }

        component->Update(deltaTime);
    }
}

void GameObject::Draw()
{
    if (!m_activeSelf)
    {
        return;
    }

    for (const auto& component : m_components)
    {
        if (!component->IsEnabled())
        {
            continue;
        }

        component->Draw();
    }
}

void GameObject::DrawDebugUI()
{
    if (!m_activeSelf)
    {
        return;
    }

    for (const auto& component : m_components)
    {
        if (!component->IsEnabled())
        {
            continue;
        }

        component->DrawDebugUI();
    }
}
