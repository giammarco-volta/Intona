#include "TuningAlgorithms.h"

#include "../Chords.h"

namespace Intona::Tuning
{

namespace
{

bool pressedMaskContainsFifth(uint16_t pressedKeyMask, int16_t value)
{
  return (pressedKeyMask
    & (uint16_t{ 1 } << fifthToSemitone(value))) != 0;
}

} // namespace

void rebuildConfigMask(Config& config)
{
  config.mask = 0;

  for (int key = 0; key < 12; ++key)
    config.mask |= valueToPoolBit(config.valueForKey[key]);
}

std::optional<KeyChoice> inferKeyFromDominantSignature(
  uint16_t pressedKeyMask,
  const Config& config)
{
  for (int a = 0; a < 12; ++a)
  {
    if (!hasKey12(pressedKeyMask, a))
      continue;

    for (int b = a + 1; b < 12; ++b)
    {
      if (!hasKey12(pressedKeyMask, b))
        continue;

      if ((b - a) != 6)
        continue;

      const int8_t lowerValue = config.valueForKey[a];
      const int8_t upperValue = config.valueForKey[b];
      const int8_t difference = upperValue - lowerValue;

      int8_t majorKeyTonic = 0;

      if (difference == -6)
      {
        majorKeyTonic = lowerValue - 5;
      }
      else if (difference == 6)
      {
        majorKeyTonic = upperValue - 5;
      }
      else
      {
        continue;
      }

      const int8_t relativeMinorTonic = majorKeyTonic + 3;
      const int8_t relativeMinorLeadingTone =
        relativeMinorTonic + 5;

      if (pressedMaskContainsFifth(
            pressedKeyMask,
            relativeMinorLeadingTone))
      {
        return KeyChoice{relativeMinorTonic, true};
      }

      return KeyChoice{majorKeyTonic, false};
    }
  }

  return std::nullopt;
}

bool isKeyCompatibleWithTuningCenter(
  int8_t tuningCenter,
  int8_t keyTonic,
  bool isMinor)
{
  const KeyChoice candidates[9] =
  {
    {tuningCenter,                          false},
    {tuningCenter,                          true},
    {static_cast<int8_t>(tuningCenter - 3), false},

    {static_cast<int8_t>(tuningCenter + 1), false},
    {static_cast<int8_t>(tuningCenter + 1), true},
    {static_cast<int8_t>(tuningCenter - 2), false},

    {static_cast<int8_t>(tuningCenter - 1), false},
    {static_cast<int8_t>(tuningCenter - 1), true},
    {static_cast<int8_t>(tuningCenter - 4), false}
  };

  for (const auto& candidate : candidates)
  {
    if (candidate.tonic == keyTonic
      && candidate.isMinor == isMinor)
    {
      return true;
    }
  }

  return false;
}

} // namespace Intona::Tuning
