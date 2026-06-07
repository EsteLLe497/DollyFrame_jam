# Unity-style migration

This project is moving from the old ECS-facing names to Unity-style names.

## Canonical names

- `GameObject` is the canonical name for scene objects.
- `MonoBehaviour` is the canonical base class for attachable behavior.
- `Entity` remains as a compatibility alias for `GameObject`.
- `Component` remains as a compatibility alias for `MonoBehaviour`.

New gameplay code should include `game_object.h` and prefer:

```cpp
class PlayerControllerComponent final : public MonoBehaviour
{
public:
    void Awake() override;
    void Start() override;
    void Update(float deltaTime) override;
};
```

## Migration order

1. Replace behavior classes first: anything overriding `Update`, `Awake`, `Start`, `OnEnable`, or `OnDisable`.
2. Replace function signatures from `Entity&` / `Entity*` to `GameObject&` / `GameObject*` one subsystem at a time.
3. Replace containers such as `std::vector<std::unique_ptr<Entity>>` with `std::vector<std::unique_ptr<GameObject>>`.
4. Replace remaining data-only component inheritance from `Component` to `MonoBehaviour` once behavior code is stable.
5. Remove compatibility aliases only after `rg "\bEntity\b|\bComponent\b"` shows no project-owned uses outside the alias header.

## Compatibility rule

Do not reintroduce `class Entity;` or `class Component;` forward declarations.
Use `game_object_fwd.h` when a header only needs pointers or references.
