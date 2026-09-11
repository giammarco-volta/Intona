#pragma once

#include "Config.hpp"
#include "Chords.h"

enum class KeepOldNotes : uint8_t { No, Yes };

extern const Config* FindConfig(Chord& chord, const std::vector<int8_t>& oldNotes, const NtetMapping& m, const Config& currentCfg, KeepOldNotes keep);
extern bool TestChordConfig(const Chord& chord, const Config& currentCfg);
extern ConfigMask ChordMask(const Chord& chord, const Config& currentCfg);

// Sum of absolute fifth-space changes on the specified physical keys.
// Dividing by the fixed key count gives the mean harmonic distance.
int harmonicDistanceSum(const Config& candidate, const Config& reference, uint16_t keys);
struct HarmonicChordContext
{
  int root = Config::invalid;
  bool plainTriad = false;
  std::vector<int> notes;
};

int ChordRootValue(const Chord& chord, const Config& config);
uint16_t ChordKeys(const Chord& chord);
bool FitsDiatonicKey(const std::vector<int>& notes, int tonic);
const Config* FindClosestChordConfig(Chord& chord, const NtetMapping& mapping,
  const Config& reference, const HarmonicChordContext* previous = nullptr,
  uint16_t pivotKeys = 0);

// Exact scale degrees on physical keys; neither function reads tuningCenter.
bool FitsScale(const Config& config, int tonicKey, int form);
uint16_t ScaleKeys(int tonicKey, int form);
const Config* FindClosestScaleConfig(const NtetMapping& mapping, const Config& reference,
  uint16_t melodyKeys, uint16_t latestKeys, uint16_t pivotKeys,
  int8_t& tonic, bool& minor);
