#include "RelativeKeyboard.h"

namespace Intona::Tuning {

std::optional<int> relativeKeyboardAnchor(const Config& config)
{
  if (config.tuningCenter == Config::invalid) return std::nullopt;
  for (int anchor = 0; anchor < 12; ++anchor)
    if (relativeKeyboardConfig(config.tuningCenter, anchor).valueForKey == config.valueForKey)
      return anchor;
  return std::nullopt;
}

Config relativeKeyboardConfig(int center, int anchor)
{
  Config result{};
  result.tuningCenter = center;
  result.relativeKeyboard = true;
  for (int i = -5; i <= 6; ++i)
  {
    const int value = center + i;
    result.valueForKey[mod(anchor + 7 * mod(value, 12), 12)] = value;
    if (value >= kConfigMaskMin && value <= kConfigMaskMax)
      result.mask |= valueToPoolBit(value);
  }
  return result;
}

} // namespace Intona::Tuning
