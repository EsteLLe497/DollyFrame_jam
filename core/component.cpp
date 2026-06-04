#include "pch.h"

#include "component.h"

#include <cassert>

#include "entity.h"

void MonoBehaviour::OnAttach(GameObject& owner)
{
    m_owner = &owner;
}

void MonoBehaviour::Awake()
{
}

void MonoBehaviour::Start()
{
}

void MonoBehaviour::OnEnable()
{
}

void MonoBehaviour::OnDisable()
{
}

void MonoBehaviour::Update(float)
{
}

void MonoBehaviour::Draw()
{
}

void MonoBehaviour::DrawDebugUI()
{
}

bool MonoBehaviour::IsEnabled() const
{
    return m_enabled;
}

bool MonoBehaviour::IsActiveAndEnabled() const
{
    return m_enabled && m_owner != nullptr && m_owner->IsActive();
}

void MonoBehaviour::SetEnabled(bool enabled)
{
    if (m_enabled == enabled)
    {
        return;
    }

    const bool wasActiveAndEnabled = IsActiveAndEnabled();
    m_enabled = enabled;
    const bool isActiveAndEnabled = IsActiveAndEnabled();

    if (wasActiveAndEnabled == isActiveAndEnabled)
    {
        return;
    }

    if (isActiveAndEnabled)
    {
        OnEnable();
    }
    else
    {
        OnDisable();
    }
}

GameObject& MonoBehaviour::GetGameObject()
{
    assert(m_owner != nullptr);
    return *m_owner;
}

const GameObject& MonoBehaviour::GetGameObject() const
{
    assert(m_owner != nullptr);
    return *m_owner;
}
