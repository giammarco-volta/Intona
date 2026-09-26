#pragma once

#include "TuningTypes.h"

#include <cstdint>
#include <optional>
#include <array>

namespace Intona::Tuning
{

void rebuildConfigMask(Config& config);

const Config& configForTuningCenter(
  const NtetMapping& mapping,
  int tuningCenter);

const Config* findConfigByValues(
  const NtetMapping& mapping,
  const std::array<int, 12>& values);

std::optional<int> spellingForPitchStep(
  int pitchStep,
  const Config& config,
  const NtetMapping& mapping);

bool isValueAllowedForKey(
  int keyIndex,
  int value,
  const Config& config,
  const NtetMapping& mapping,
  double globalOffsetCents);

std::optional<int> steppedValueForKey(
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
