#pragma once

#include "../Config.hpp"


#include <cstdint>
#include <vector>
#include <array>

namespace Intona::Tuning
{

struct TuningPreset
{
  std::array<int, 12> values{};
  int tuningCenter = Config::invalid;
  double globalOffsetCents = 0.0;
  bool relativeKeyboard = false;
};

struct ActiveNote
{
  uint8_t midiNote;
  uint8_t key;              // 0..11
  uint8_t velocity;
  int interpretedValue;  // fifth-cycle value, e.g. E = 4, G# = 8
  uint64_t generation = 0;
};

enum class AfterTouch : uint8_t
{
  stepUp,
  stepDown,
  off
};

} // namespace Intona::Tuning
