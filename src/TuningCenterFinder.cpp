#include "TuningCenterFinder.h"

#include <algorithm>
#include <cstdlib>
#include <climits>

namespace
{

bool keepsOldNotes(const Config& candidate, const Config& current,
  const std::vector<int8_t>& oldNotes)
{
  for (const int8_t note : oldNotes)
  {
    const auto key = std::find(current.valueForKey.begin(), current.valueForKey.end(), note);
    if (key == current.valueForKey.end()
      || candidate.valueForKey[std::distance(current.valueForKey.begin(), key)] != note)
    {
      return false;
    }
  }
  return true;
}

} // namespace

const Config* FindConfig(Chord& chord, const std::vector<int8_t>& oldNotes,
  const NtetMapping& mapping, const Config& current, KeepOldNotes keep)
{
  if (chord.IsNull() || chord.root_ >= 12)
    return nullptr;

  // Preserve the existing German augmented-sixth interpretation, using the
  // actual MIDI root key rather than the note's former 12-EDO spelling.
  if (chord.type_ == Chord::typeDom7 && chord.t9_ == Chord::tensionVoid9
    && chord.t11_ == Chord::tensionVoid11 && chord.t13_ == Chord::tensionVoid13)
  {
    const int root = current.valueForKey[chord.root_];
    bool matches = true;
    for (const int interval : {4, 1, 10})
    {
      const int key = (chord.root_ + fifthToSemitone(interval)) % 12;
      matches &= current.valueForKey[key] == root + interval;
    }
    if (matches)
    {
      chord.set(Chord::typeAug6th, Chord::tensionVoid9,
        Chord::tensionVoid11, Chord::tensionVoid13);
      return &current;
    }
  }

  int bestDistance = INT_MAX;
  const Config* best = nullptr;
  for (int center = mapping.minValue; center <= mapping.maxValue; ++center)
  {
    const Config& candidate = mapping.getConfig(static_cast<int8_t>(center));
    if (!TestChordConfig(chord, candidate))
      continue;
    if (keep == KeepOldNotes::Yes && !keepsOldNotes(candidate, current, oldNotes))
      continue;

    const int distance = std::abs(int(candidate.tuningCenter) - int(current.tuningCenter));
    if (!best || distance < bestDistance
      || (distance == bestDistance
        && std::abs(int(candidate.tuningCenter)) < std::abs(int(best->tuningCenter))))
    {
      best = &candidate;
      bestDistance = distance;
    }
  }
  return best;
}

bool TestChordConfig(const Chord& chord, const Config& config)
{
  const ConfigMask mask = ChordMask(chord, config);
  if (mask.none() || !containsMask(config.mask, mask))
    return false;

  const int root = config.valueForKey[chord.root_];
  for (const int interval : chord.GetNotesIn5Cycles())
  {
    // Here fifthToSemitone describes an interval, not an absolute note name.
    const int key = (chord.root_ + fifthToSemitone(interval)) % 12;
    if (config.valueForKey[key] != root + interval)
      return false;
  }
  return true;
}

ConfigMask ChordMask(const Chord& chord, const Config& config)
{
  if (chord.IsNull() || chord.root_ >= 12)
    return {};
  const int root = config.valueForKey[chord.root_];
  if (root < kConfigMaskMin || root > kConfigMaskMax)
    return {};

  ConfigMask mask = valueToPoolBit(root);
  for (const int interval : chord.GetNotesIn5Cycles())
  {
    const int note = root + interval;
    if (note < kConfigMaskMin || note > kConfigMaskMax)
      return {};
    mask |= valueToPoolBit(note);
  }
  return mask;
}
