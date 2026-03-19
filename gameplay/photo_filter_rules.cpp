#include "photo_filter_rules.h"

#include <cmath>

#include "entity.h"

namespace
{
    struct TintValues
    {
        float r;
        float g;
        float b;
        float a;
    };

    TintValues GetCapturedTargetTint(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            return { 1.0f, 0.28f, 0.10f, 0.95f };
        case PhotoFilterTheme::Cold:
            return { 0.54f, 0.82f, 1.0f, 0.96f };
        case PhotoFilterTheme::Invert:
            return { 0.60f, 0.96f, 0.72f, 0.96f };
        case PhotoFilterTheme::Sepia:
            return { 0.76f, 0.58f, 0.34f, 0.96f };
        case PhotoFilterTheme::None:
        default:
            return { 0.16f, 0.34f, 0.38f, 0.55f };
        }
    }

    TintValues GetPhotoBoxTint(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            return { 1.0f, 0.34f, 0.12f, 1.0f };
        case PhotoFilterTheme::Cold:
            return { 0.76f, 0.90f, 1.0f, 1.0f };
        case PhotoFilterTheme::Invert:
            return { 0.62f, 0.62f, 0.64f, 1.0f };
        case PhotoFilterTheme::Sepia:
            return { 0.76f, 0.58f, 0.34f, 1.0f };
        case PhotoFilterTheme::None:
        default:
            return { 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    struct ThemeTransform
    {
        PhotoCopyRole role;
        PhotoCopyLayer layer;
        TintValues tint;
    };

    ThemeTransform GetPhotoBoxTransform(PhotoFilterTheme theme, PhotoCopyOrigin origin, PhotoCopyRole currentRole, PhotoCopyLayer currentLayer)
    {
        ThemeTransform transform{ currentRole, currentLayer, GetPhotoBoxTint(theme) };
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            transform.role = PhotoCopyRole::Hazard;
            transform.layer = PhotoCopyLayer::Foreground;
            break;
        case PhotoFilterTheme::Cold:
            transform.role = PhotoCopyRole::Solid;
            transform.layer = PhotoCopyLayer::Foreground;
            break;
        case PhotoFilterTheme::Invert:
            transform.role = origin == PhotoCopyOrigin::Enemy ? PhotoCopyRole::Ally : PhotoCopyRole::Solid;
            transform.layer = PhotoCopyLayer::Foreground;
            break;
        case PhotoFilterTheme::Sepia:
            transform.role = PhotoCopyRole::Solid;
            transform.layer = PhotoCopyLayer::Foreground;
            break;
        case PhotoFilterTheme::None:
        default:
            break;
        }

        return transform;
    }
}

const char* GetPhotoFilterThemeLabel(PhotoFilterTheme theme)
{
    switch (theme)
    {
    case PhotoFilterTheme::Hot:
        return "Hot";
    case PhotoFilterTheme::Cold:
        return "Cold";
    case PhotoFilterTheme::Invert:
        return "Invert";
    case PhotoFilterTheme::Sepia:
        return "Sepia";
    case PhotoFilterTheme::None:
    default:
        return "None";
    }
}

const char* GetPhotoFilterThemeEffectText(PhotoFilterTheme theme)
{
    switch (theme)
    {
    case PhotoFilterTheme::Hot:
        return "Makes copies dangerous";
    case PhotoFilterTheme::Cold:
        return "Freezes copies into footing";
    case PhotoFilterTheme::Invert:
        return "Enemy shots become allies";
    case PhotoFilterTheme::Sepia:
        return "Rewinds the target in time";
    case PhotoFilterTheme::None:
    default:
        return "Keeps captured properties";
    }
}

void GetPhotoFilterThemeOverlayColor(PhotoFilterTheme theme, float& r, float& g, float& b)
{
    switch (theme)
    {
    case PhotoFilterTheme::Hot:
        r = 1.0f;
        g = 0.20f;
        b = 0.08f;
        break;
    case PhotoFilterTheme::Cold:
        r = 0.14f;
        g = 0.56f;
        b = 1.0f;
        break;
    case PhotoFilterTheme::Invert:
        r = 0.86f;
        g = 0.86f;
        b = 0.90f;
        break;
    case PhotoFilterTheme::Sepia:
        r = 0.78f;
        g = 0.60f;
        b = 0.36f;
        break;
    case PhotoFilterTheme::None:
    default:
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        break;
    }
}

void GetPhotoFilterThemePreviewOutlineColor(PhotoFilterTheme theme, float& r, float& g, float& b)
{
    switch (theme)
    {
    case PhotoFilterTheme::Hot:
        r = 1.0f;
        g = 0.42f;
        b = 0.18f;
        break;
    case PhotoFilterTheme::Cold:
        r = 0.76f;
        g = 0.94f;
        b = 1.0f;
        break;
    case PhotoFilterTheme::Invert:
        r = 0.86f;
        g = 0.86f;
        b = 0.92f;
        break;
    case PhotoFilterTheme::Sepia:
        r = 0.90f;
        g = 0.72f;
        b = 0.42f;
        break;
    case PhotoFilterTheme::None:
    default:
        r = 0.32f;
        g = 0.92f;
        b = 1.0f;
        break;
    }
}

PhotoFilterTheme GetNextPhotoFilterTheme(PhotoFilterTheme current)
{
    switch (current)
    {
    case PhotoFilterTheme::None:
        return PhotoFilterTheme::Hot;
    case PhotoFilterTheme::Hot:
        return PhotoFilterTheme::Cold;
    case PhotoFilterTheme::Cold:
        return PhotoFilterTheme::Invert;
    case PhotoFilterTheme::Invert:
        return PhotoFilterTheme::Sepia;
    case PhotoFilterTheme::Sepia:
    default:
        return PhotoFilterTheme::None;
    }
}

const char* GetPhotoCaptureLogMessage(PhotoFilterTheme theme)
{
    switch (theme)
    {
    case PhotoFilterTheme::Hot:
        return "Captured framed objects with Hot filter";
    case PhotoFilterTheme::Cold:
        return "Captured framed objects with Cold filter";
    case PhotoFilterTheme::Invert:
        return "Captured framed objects with Invert filter";
    case PhotoFilterTheme::Sepia:
        return "Captured framed objects with Sepia filter";
    case PhotoFilterTheme::None:
    default:
        return "Captured framed objects with no filter";
    }
}

void ApplyPhotoFilterThemeToPreviewItem(
    PhotoFilterTheme theme,
    PhotoCopyOrigin origin,
    PhotoCopyRole& role,
    PhotoCopyLayer& layer,
    float& tintR,
    float& tintG,
    float& tintB,
    float& tintA)
{
    if (theme == PhotoFilterTheme::None)
    {
        return;
    }

    const ThemeTransform transform = GetPhotoBoxTransform(theme, origin, role, layer);
    role = transform.role;
    layer = transform.layer;
    tintR = transform.tint.r;
    tintG = transform.tint.g;
    tintB = transform.tint.b;
    tintA = transform.tint.a;
}

bool ApplyPhotoFilterToCapturedTarget(Entity& target, PhotoFilterTheme theme)
{
    bool changed = false;

    if (auto* tint = target.GetComponent<TintComponent>())
    {
        const TintValues nextTint = GetCapturedTargetTint(theme);
        tint->r = nextTint.r;
        tint->g = nextTint.g;
        tint->b = nextTint.b;
        tint->a = nextTint.a;
        changed = true;
    }

    if (auto* enemy = target.GetComponent<EnemyComponent>())
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            enemy->MarkDefeated();
            changed = true;
            break;
        case PhotoFilterTheme::Cold:
            if (auto* mover = target.GetComponent<EnemyMoverComponent>())
            {
                mover->SetFrozen(true);
            }
            changed = true;
            break;
        case PhotoFilterTheme::Invert:
            enemy->SetEnabled(false);
            if (auto* mover = target.GetComponent<EnemyMoverComponent>())
            {
                mover->SetFrozen(true);
            }
            changed = true;
            break;
        case PhotoFilterTheme::Sepia:
            enemy->Restore();
            if (auto* mover = target.GetComponent<EnemyMoverComponent>())
            {
                mover->SetFrozen(false);
                mover->Rewind(1.5f);
            }
            changed = true;
            break;
        case PhotoFilterTheme::None:
        default:
            break;
        }
    }

    if (auto* gimmick = target.GetComponent<GimmickComponent>())
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            if (gimmick->GetType() == GimmickType::PhotoSource || gimmick->GetType() == GimmickType::Pickup)
            {
                gimmick->SetEnabled(false);
                changed = true;
            }
            break;
        case PhotoFilterTheme::Cold:
            if (gimmick->GetType() == GimmickType::Hazard)
            {
                gimmick->SetEnabled(false);
                changed = true;
            }
            break;
        case PhotoFilterTheme::Sepia:
            gimmick->Restore();
            changed = true;
            break;
        case PhotoFilterTheme::Invert:
        case PhotoFilterTheme::None:
        default:
            break;
        }
    }

    return changed;
}

bool ApplyPhotoFilterToPhotoBox(Entity& photoBox, PhotoFilterTheme theme)
{
    auto* role = photoBox.GetComponent<PhotoCopyRoleComponent>();
    auto* layer = photoBox.GetComponent<PhotoCopyLayerComponent>();
    auto* tint = photoBox.GetComponent<TintComponent>();
    auto* effect = photoBox.GetComponent<PhotoCopyEffectComponent>();
    const auto* origin = photoBox.GetComponent<PhotoCopyOriginComponent>();
    if (!role || !layer || !tint || !effect)
    {
        return false;
    }

    if (layer->layer == PhotoCopyLayer::Background || layer->layer == PhotoCopyLayer::Shadow)
    {
        effect->SetTheme(PhotoFilterTheme::None);
        return false;
    }

    if (theme == PhotoFilterTheme::None)
    {
        const bool changed = effect->GetTheme() != PhotoFilterTheme::None;
        effect->SetTheme(PhotoFilterTheme::None);
        return changed;
    }

    const ThemeTransform transform = GetPhotoBoxTransform(
        theme,
        origin ? origin->origin : PhotoCopyOrigin::Generic,
        role->role,
        layer->layer);
    const PhotoCopyRole nextRole = transform.role;
    const PhotoCopyLayer nextLayer = transform.layer;
    const TintValues nextTint = transform.tint;

    const bool changed =
        role->role != nextRole ||
        layer->layer != nextLayer ||
        effect->GetTheme() != theme ||
        std::fabs(tint->r - nextTint.r) > 0.001f ||
        std::fabs(tint->g - nextTint.g) > 0.001f ||
        std::fabs(tint->b - nextTint.b) > 0.001f ||
        std::fabs(tint->a - nextTint.a) > 0.001f;

    role->role = nextRole;
    layer->layer = nextLayer;
    effect->SetTheme(theme);
    tint->r = nextTint.r;
    tint->g = nextTint.g;
    tint->b = nextTint.b;
    tint->a = nextTint.a;
    return changed;
}
