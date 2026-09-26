#include "../CentsUtilities.hpp"
#include "TuningAlgorithms.h"

#include <algorithm>
#include <iterator>
#include <limits>


namespace Intona::Tuning
{

void rebuildConfigMask(Config& config)
{
  config.mask = 0;

  for (int key = 0; key < 12; ++key)
    config.mask |= valueToPoolBit(config.valueForKey[key]);
}

const Config& configForTuningCenter(
  const NtetMapping& mapping,
  int tuningCenter)
{
  const int normalizedCenter =
    wrapFifthsToMappingRange(tuningCenter, mapping);

  return mapping.getConfig(normalizedCenter);
}

const Config* findConfigByValues(
  const NtetMapping& mapping,
  const std::array<int, 12>& values)
{
  for (int center = mapping.minValue;
       center <= mapping.maxValue;
       ++center)
  {
    const Config& config =
      mapping.getConfig(static_cast<int>(center));

    if (config.valueForKey == values)
      return &config;
  }

  return nullptr;
}

namespace
{

double valueToCentsInOctave(
  int value,
  const NtetMapping& mapping)
{
  const int step =
    mod(value * mapping.fifthStep, mapping.N);

  return 1200.0 * double(step) / double(mapping.N);
}

double rawOffsetForKey(
  int key,
  int value,
  const NtetMapping& mapping)
{
  const double cents =
    valueToCentsInOctave(value, mapping);

  double offset = cents - 100.0 * key;

  while (offset > 600.0)
    offset -= 1200.0;

  while (offset <= -600.0)
    offset += 1200.0;

  return offset;
}

} // namespace

std::optional<double> findGlobalOffsetCents(
  const Config& config,
  const NtetMapping& mapping,
  double preferredOffset)
{
  constexpr double limit = 99.0;

  // Keep the common offset within the MIDI coarse/fine tuning range.
  // Relative mappings may travel beyond an octave, unlike the legacy pool.
  double lowerBound = -6300.0;
  double upperBound = 6300.0;

  for (int key = 0; key < 12; ++key)
  {
    const double rawOffset = keyboardRawOffset(key, config, mapping.N, mapping.fifthStep, preferredOffset);

    lowerBound = std::max(
      lowerBound,
      rawOffset - limit);

    upperBound = std::min(
      upperBound,
      rawOffset + limit);
  }

  if (lowerBound > upperBound)
    return std::nullopt;

  return std::clamp(
    preferredOffset,
    lowerBound,
    upperBound);
}

std::optional<int> spellingForPitchStep(
  int pitchStep,
  const Config& config,
  const NtetMapping& mapping)
{
  if (mapping.N == 0)
    return std::nullopt;

  pitchStep = mod(pitchStep, mapping.N);

  // Conserva la grafia esatta già presente nella configurazione.
  for (const int value : config.valueForKey)
  {
    if (mod(mod(value, mapping.N) * mapping.fifthStep, mapping.N)
      == pitchStep)
    {
      return value;
    }
  }

  const int referenceValue =
    config.tuningCenter != Config::invalid
      ? config.tuningCenter
      : 0;

  if (config.relativeKeyboard)
  {
    const int residue = mod(pitchStep * modInverse(mapping.fifthStep, mapping.N), mapping.N);
    const int lower = referenceValue - mod(referenceValue - residue, mapping.N);
    return referenceValue - lower <= lower + mapping.N - referenceValue ? lower : lower + mapping.N;
  }
  int bestValue = 0;
  int bestDistance = std::numeric_limits<int>::max();
  bool found = false;

  for (int value = kConfigMaskMin;
       value <= kConfigMaskMax;
       ++value)
  {
    if (mod(value * mapping.fifthStep, mapping.N)
      != pitchStep)
    {
      continue;
    }

    const int distance =
      std::abs(value - referenceValue);

    if (distance < bestDistance)
    {
      bestValue = value;
      bestDistance = distance;
      found = true;
    }
  }

  if (!found)
    return std::nullopt;

  return static_cast<int>(bestValue);
}

bool isValueAllowedForKey(
  int keyIndex,
  int value,
  const Config& config,
  const NtetMapping& mapping,
  double globalOffsetCents)
{
  constexpr double limit = 99.0;

  if (keyIndex < 0 || keyIndex >= 12)
    return false;

  if (config.relativeKeyboard)
  {
    Config candidate = config;
    candidate.valueForKey[keyIndex] = value;
    const double oldCents = 100.0*keyIndex + keyboardRawOffset(keyIndex, config, mapping.N, mapping.fifthStep, globalOffsetCents);
    const double newCents = 100.0*keyIndex + keyboardRawOffset(keyIndex, candidate, mapping.N, mapping.fifthStep, globalOffsetCents);
    if (std::abs(newCents-oldCents) > 1200.0/mapping.N + 1e-7) return false;
    for (int key=0;key<12;++key)
    {
      const double raw = keyboardRawOffset(key,candidate,mapping.N,mapping.fifthStep,globalOffsetCents);
      if (raw-globalOffsetCents < -99 || raw-globalOffsetCents > 99) return false;
      if (key && 100*key+raw <= 100*(key-1)+keyboardRawOffset(key-1,candidate,mapping.N,mapping.fifthStep,globalOffsetCents)) return false;
    }
    return true;
  }
  if (value < kConfigMaskMin || value > kConfigMaskMax)
    return false;

  const double rawOffset =
    rawOffsetForKey(keyIndex, value, mapping);

  // Mantiene il vincolo usato dalla precedente tabella
  // allowedValuesPerKey_.
  if (rawOffset < -limit || rawOffset > limit)
    return false;

  const double candidateDetune =
    rawOffset - globalOffsetCents;

  if (candidateDetune < -limit
    || candidateDetune > limit)
  {
    return false;
  }

  const auto absoluteKeyboardCents =
    [&](int key, int keyValue)
    {
      return 100.0 * double(key)
        + rawOffsetForKey(key, keyValue, mapping);
    };

  const double candidateCents =
    absoluteKeyboardCents(keyIndex, value);

  const int previousKey = (keyIndex + 11) % 12;
  const int nextKey = (keyIndex + 1) % 12;

  const int previousValue =
    config.valueForKey[previousKey];

  const int nextValue =
    config.valueForKey[nextKey];

  if (previousValue != Config::invalid)
  {
    double previousCents =
      absoluteKeyboardCents(previousKey, previousValue);

    if (previousKey > keyIndex)
      previousCents -= 1200.0;

    if (candidateCents <= previousCents)
      return false;
  }

  if (nextValue != Config::invalid)
  {
    double nextCents =
      absoluteKeyboardCents(nextKey, nextValue);

    if (nextKey < keyIndex)
      nextCents += 1200.0;

    if (candidateCents >= nextCents)
      return false;
  }

  return true;
}

std::optional<int> steppedValueForKey(
  int keyIndex,
  int direction,
  const Config& config,
  const NtetMapping& mapping,
  double globalOffsetCents)
{
  if (keyIndex < 0 || keyIndex >= 12)
    return std::nullopt;

  if (direction != -1 && direction != 1)
    return std::nullopt;

  const int currentValue =
    config.valueForKey[keyIndex];

  if (!config.relativeKeyboard && (currentValue < kConfigMaskMin
    || currentValue > kConfigMaskMax))
  {
    return std::nullopt;
  }

  const int currentPitch =
    mod(currentValue * mapping.fifthStep, mapping.N);

  const int targetPitch =
    mod(currentPitch + direction, mapping.N);

  const auto targetValue =
    spellingForPitchStep(
      targetPitch,
      config,
      mapping);

  if (!targetValue)
    return std::nullopt;

  if (!isValueAllowedForKey(
        keyIndex,
        *targetValue,
        config,
        mapping,
        globalOffsetCents))
  {
    return std::nullopt;
  }

  return targetValue;
}

} // namespace Intona::Tuning
