#pragma once

#include "../Config.hpp"

#include <QString>

#include <cstdint>
#include <vector>

namespace Intona::Tuning
{

struct RetunedNote
{
  uint8_t midiNote;
  uint8_t velocity;
  int8_t oldValue;
  int8_t newValue;
};

struct ActiveNote
{
  uint8_t midiNote;
  uint8_t key;              // 0..11
  uint8_t velocity;
  int8_t interpretedValue;  // fifth-cycle value, e.g. E = 4, G# = 8
  uint32_t startMs;
};

struct KeyChoice
{
  int8_t tonic = 0;
  bool isMinor = false;
};

enum class ChordStructure : uint8_t
{
  None,
  Tertian,
  Quartal
};

struct AdaptiveChoice
{
  const Config* config = nullptr;
  std::vector<ActiveNote> resolvedNotes;
  std::vector<RetunedNote> notesToRetrigger;
  ConfigMask pressedMask5 = 0;

  int8_t keyTonic = Config::invalid;
  bool keyIsMinor = false;

  bool chordRootValid = false;
  int8_t chordRoot = 0;
  ChordStructure chordStructure = ChordStructure::None;
  QString chordName;
};

struct ChordRootAnalysis
{
  bool valid = false;
  int8_t root = 0;
  ChordStructure structure = ChordStructure::None;
  uint8_t holes = 0;
};

enum class AfterTouch : uint8_t
{
  stepUp,
  stepDown,
  off
};

struct ScaleKeyScore
{
  int8_t tonic = 0;
  bool isMinor = false;
  double fast = 0.0;
  double slow = 0.0;
};

} // namespace Intona::Tuning
