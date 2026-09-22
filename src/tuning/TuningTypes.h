#pragma once

#include "../Config.hpp"

#include <QString>

#include <cstdint>
#include <vector>
#include <array>

namespace Intona::Tuning
{

struct RetunedNote
{
  uint8_t midiNote;
  uint8_t velocity;
  int oldValue;
  int newValue;
};

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
  uint32_t startMs;
  uint64_t generation = 0;
  uint32_t minimumDurationMs = 0;
  double startTime = 0;
};

struct KeyChoice
{
  int tonic = 0;
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

  int keyTonic = Config::invalid;
  bool keyIsMinor = false;

  bool chordRootValid = false;
  int chordRoot = 0;
  ChordStructure chordStructure = ChordStructure::None;
  bool chordNameValid = false;
  QString chordSuffix;
  int chordBass = Config::invalid;
};

struct ChordRootAnalysis
{
  bool valid = false;
  int root = 0;
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
  int tonic = 0;
  bool isMinor = false;
  double fast = 0.0;
  double slow = 0.0;
};

} // namespace Intona::Tuning
