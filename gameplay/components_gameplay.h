#pragma once

#include "game_object.h"

class EventBus;

class HealthComponent final : public MonoBehaviour
{
public:
    explicit HealthComponent(int maxHealth);

    void DrawDebugUI() override;
    void ApplyDamage(int amount);
    void SetCurrentHealth(int value);
    void RestoreToFull();
    int GetCurrentHealth() const;
    int GetMaxHealth() const;
    bool IsDead() const;

private:
    int m_maxHealth;
    int m_currentHealth;
};

class DamageCooldownComponent final : public MonoBehaviour
{
public:
    explicit DamageCooldownComponent(float cooldownSeconds);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;
    bool CanTakeDamage() const;
    void Trigger();
    void SetRemainingSeconds(float seconds);
    float GetCooldownSeconds() const;
    float GetRemainingSeconds() const;

private:
    float m_cooldownSeconds;
    float m_remainingSeconds;
};

class PlayerControllerComponent final : public MonoBehaviour
{
public:
    explicit PlayerControllerComponent(EventBus& eventBus);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;

private:
    EventBus* m_eventBus;
};
