#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "DxLib.h"

#include <imgui.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26495)
#endif
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <sol/sol.hpp>
#include <tracy/Tracy.hpp>

#include "build_config.h"
