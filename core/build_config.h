#pragma once

namespace build_config
{
#if defined(_DEBUG)
    inline constexpr bool kDebugFeaturesEnabled = true;
#else
    inline constexpr bool kDebugFeaturesEnabled = false;
#endif
}
