#pragma once

#include "TuningTypes.h"

#include <cstdint>
#include <optional>

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

} // namespace Intona::Tuning
