#pragma once

#include "TuningTypes.h"

#include <cstdint>
#include <optional>
#include <array>

namespace Intona::Tuning
{

void rebuildConfigMask(Config& config);

std::optional<KeyChoice> inferKeyFromDominantSignature(
  uint16_t pressedKeyMask,
  const Config& config);

bool isKeyCompatibleWithTuningCenter(
  int8_t tuningCenter,
  int8_t keyTonic,
  bool isMinor);

ChordRootAnalysis inferChordRootByStack(
  const std::vector<ActiveNote>& notes);

const Config& configForTuningCenter(
  const NtetMapping& mapping,
  int8_t tuningCenter);

const Config* findConfigByValues(
  const NtetMapping& mapping,
  const std::array<int8_t, 12>& values);

std::optional<int8_t> spellingForPitchStep(
  int pitchStep,
  const Config& config,
  const NtetMapping& mapping);

bool isValueAllowedForKey(
  int keyIndex,
  int value,
  const Config& config,
  const NtetMapping& mapping,
  double globalOffsetCents);

std::optional<int8_t> steppedValueForKey(
  int keyIndex,
  int direction,
  const Config& config,
  const NtetMapping& mapping,
  double globalOffsetCents);

std::optional<double> findGlobalOffsetCents(
  const Config& config,
  const NtetMapping& mapping,
  double preferredOffset);

} // namespace Intona::Tuning
