#pragma once
#include "../Config.hpp"
#include <optional>

namespace Intona::Tuning {
// A fixed anchor preserves the eleven common assignments across one fifth.
std::optional<int> relativeKeyboardAnchor(const Config& config);
Config relativeKeyboardConfig(int center, int anchor);
} // namespace Intona::Tuning
