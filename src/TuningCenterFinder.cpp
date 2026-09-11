#include "TuningCenterFinder.h"
#include <algorithm>
#include <cstdlib>
#include <tuple>

namespace
{
// Major, natural minor, harmonic minor, melodic minor in fifth coordinates.
constexpr int scales[4][7] = {
  {0, 2, 4, -1, 1, 3, 5}, {0, 2, -3, -1, 1, -4, -2},
  {0, 2, -3, -1, 1, -4, 5}, {0, 2, -3, -1, 1, 3, 5}
};
bool keepsPivots(const Config& candidate, const Config& reference, uint16_t keys)
{
  for (int key = 0; key < 12; ++key)
    if (hasKey12(keys, key) && candidate.valueForKey[key] != reference.valueForKey[key])
      return false;
  return true;
}
std::vector<Chord> interpretations(const Chord& recognized)
{
  std::vector<Chord> result{recognized};
  if (recognized.t9_ != Chord::tensionVoid9 || recognized.t11_ != Chord::tensionVoid11
    || recognized.t13_ != Chord::tensionVoid13)
    return result;
  Chord alternative = recognized;
  switch (recognized.type_)
  {
  case Chord::typeDom7:
    alternative.type_ = Chord::typeAug6th;
    result.push_back(alternative);
    break;
  case Chord::typeDom7f5:
    alternative.type_ = Chord::typeFrench6th;
    result.push_back(alternative);
    break;
  case Chord::typeF5:
    alternative.type_ = Chord::typeMin6;
    alternative.root_ = (recognized.root_ + 9) % 12;
    alternative.omitRoot_ = true;
    result.push_back(alternative);
    break;
  case Chord::typeDim7:
  case Chord::typeAug:
    for (int shift = recognized.type_ == Chord::typeDim7 ? 3 : 4; shift < 12;
      shift += recognized.type_ == Chord::typeDim7 ? 3 : 4)
    {
      alternative.root_ = (recognized.root_ + shift) % 12;
      result.push_back(alternative);
    }
    break;
  default: break;
  }
  return result;
}
int contextPenalty(const Chord& chord, const Config& config, const HarmonicChordContext* previous)
{
  if (!previous || previous->notes.empty()) return 1;
  const int root = ChordRootValue(chord, config);
  if (chord.type_ == Chord::typeDim7)
    return previous->plainTriad && root == previous->root + 5 ? 0 : 1;
  const bool conventional = chord.type_ == Chord::typeDom7
    || chord.type_ == Chord::typeDom7f5 || chord.type_ == Chord::typeF5;
  const bool alternative = chord.type_ == Chord::typeAug6th
    || chord.type_ == Chord::typeFrench6th || chord.omitRoot_;
  if (!conventional && !alternative) return 1;
  const int anchor = chord.omitRoot_ ? root - 3 : root;
  const bool subdominantKey = FitsDiatonicKey(previous->notes, anchor - 1);
  const bool thirdKey = FitsDiatonicKey(previous->notes, anchor + 4);
  if (subdominantKey == thirdKey) return 1; // Both or neither: distance decides.
  return (conventional ? subdominantKey : thirdKey) ? 0 : 1;
}
} // namespace

int harmonicDistanceSum(const Config& candidate, const Config& reference, uint16_t keys)
{
  int distance = 0;
  for (int key = 0; key < 12; ++key)
    if (hasKey12(keys, key))
      distance += std::abs(int(candidate.valueForKey[key]) - int(reference.valueForKey[key]));
  return distance;
}
int ChordRootValue(const Chord& chord, const Config& config)
{
  // Rootless Am6 is anchored by its sounding third C, not the unused A key.
  return chord.omitRoot_ ? config.valueForKey[(chord.root_ + 3) % 12] + 3
    : config.valueForKey[chord.root_];
}
uint16_t ChordKeys(const Chord& chord)
{
  if (chord.IsNull() || chord.root_ >= 12) return 0;
  uint16_t keys = chord.omitRoot_ ? 0 : uint16_t{1} << chord.root_;
  for (int interval : chord.GetNotesIn5Cycles())
    keys |= uint16_t{1} << ((chord.root_ + fifthToSemitone(interval)) % 12);
  if (chord.bass_ < 12) keys |= uint16_t{1} << chord.bass_;
  return keys;
}
bool FitsDiatonicKey(const std::vector<int>& notes, int tonic)
{
  for (const auto& scale : scales)
    if (std::all_of(notes.begin(), notes.end(), [&](int note) {
      return std::find(std::begin(scale), std::end(scale), note - tonic) != std::end(scale);
    })) return true;
  return false;
}
const Config* FindClosestChordConfig(Chord& chord, const NtetMapping& mapping,
  const Config& reference, const HarmonicChordContext* previous, uint16_t pivotKeys)
{
  if (chord.IsNull() || chord.root_ >= 12) return nullptr;
  // Every alternative has the same sounding keys. Context precedes distance;
  // pivots remain hard constraints, even when a preferred reading is impossible.
  using Score = std::tuple<int, int, int, std::array<int8_t, 12>, int, int>;
  Score bestScore;
  const Config* best = nullptr;
  Chord bestChord = chord;
  for (const Chord& interpretation : interpretations(chord))
  {
    const uint16_t keys = ChordKeys(interpretation);
    const auto consider = [&](const Config& candidate)
    {
      if (!keepsPivots(candidate, reference, pivotKeys) || !TestChordConfig(interpretation, candidate)) return;
      const Score score{contextPenalty(interpretation, candidate, previous),
        harmonicDistanceSum(candidate, reference, keys),
        harmonicDistanceSum(candidate, reference, 0x0fff), candidate.valueForKey,
        ChordRootValue(interpretation, candidate), interpretation.type_};
      if (!best || score < bestScore)
      {
        best = &candidate;
        bestScore = score;
        bestChord = interpretation;
      }
    };
    consider(reference); // Includes explicit presets/custom mappings.
    for (int center = mapping.minValue; center <= mapping.maxValue; ++center)
      consider(mapping.getConfig(static_cast<int8_t>(center)));
  }
  if (best) chord = bestChord;
  return best;
}
const Config* FindConfig(Chord& chord, const std::vector<int8_t>& oldNotes,
  const NtetMapping& mapping, const Config& current, KeepOldNotes keep)
{
  uint16_t keys = 0;
  if (keep == KeepOldNotes::Yes)
    for (int note : oldNotes)
    {
      const auto key = std::find(current.valueForKey.begin(), current.valueForKey.end(), note);
      if (key == current.valueForKey.end()) return nullptr;
      keys |= uint16_t{1} << std::distance(current.valueForKey.begin(), key);
    }
  return FindClosestChordConfig(chord, mapping, current, nullptr, keys);
}
bool TestChordConfig(const Chord& chord, const Config& config)
{
  const ConfigMask mask = ChordMask(chord, config);
  if (mask.none() || !containsMask(config.mask, mask)) return false;
  const int root = ChordRootValue(chord, config);
  for (int interval : chord.GetNotesIn5Cycles())
  {
    const int key = (chord.root_ + fifthToSemitone(interval)) % 12;
    if (config.valueForKey[key] != root + interval) return false;
  }
  return true;
}
ConfigMask ChordMask(const Chord& chord, const Config& config)
{
  if (chord.IsNull() || chord.root_ >= 12) return {};
  const int root = ChordRootValue(chord, config);
  ConfigMask mask;
  if (!chord.omitRoot_)
  {
    if (root < kConfigMaskMin || root > kConfigMaskMax) return {};
    mask |= valueToPoolBit(root);
  }
  for (int interval : chord.GetNotesIn5Cycles())
  {
    const int note = root + interval;
    if (note < kConfigMaskMin || note > kConfigMaskMax) return {};
    mask |= valueToPoolBit(note);
  }
  return mask;
}
uint16_t ScaleKeys(int tonicKey, int form)
{
  uint16_t keys = 0;
  for (int interval : scales[form])
    keys |= uint16_t{1} << ((tonicKey + fifthToSemitone(interval)) % 12);
  return keys;
}
bool FitsScale(const Config& config, int tonicKey, int form)
{
  const int tonic = config.valueForKey[tonicKey];
  for (int interval : scales[form])
    if (config.valueForKey[(tonicKey + fifthToSemitone(interval)) % 12] != tonic + interval) return false;
  return true;
}
const Config* FindClosestScaleConfig(const NtetMapping& mapping, const Config& reference,
  uint16_t melodyKeys, uint16_t latestKeys, uint16_t pivotKeys, int8_t& tonic, bool& minor)
{
  if (popcount(melodyKeys) < 3 || !latestKeys) return nullptr;
  using Score = std::tuple<int, int, int, std::array<int8_t, 12>, int, int, int>;
  Score bestScore;
  const Config* best = nullptr;
  for (int key = 0; key < 12; ++key)
    for (int form = 0; form < 4; ++form)
    {
      const uint16_t scaleKeys = ScaleKeys(key, form);
      const int coverage = popcount<uint16_t>(melodyKeys & scaleKeys);
      if (coverage < 3 || (latestKeys & ~scaleKeys)) continue;
      const auto consider = [&](const Config& candidate)
      {
        if (!FitsScale(candidate, key, form) || !keepsPivots(candidate, reference, pivotKeys)) return;
        const Score score{-coverage, harmonicDistanceSum(candidate, reference, melodyKeys),
          harmonicDistanceSum(candidate, reference, 0x0fff), candidate.valueForKey,
          std::abs(int(candidate.valueForKey[key])), form, candidate.valueForKey[key]};
        if (!best || score < bestScore)
        {
          best = &candidate;
          bestScore = score;
          tonic = candidate.valueForKey[key];
          minor = form != 0;
        }
      };
      consider(reference);
      for (int center = mapping.minValue; center <= mapping.maxValue; ++center)
        consider(mapping.getConfig(static_cast<int8_t>(center)));
    }
  return best;
}
