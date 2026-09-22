#include "../CentsUtilities.hpp"
#include "TuningAlgorithms.h"

#include "../Chords.h"
#include <algorithm>
#include <iterator>
#include <limits>

//#define CONSIDER_QUARTAL_CHORDS

namespace Intona::Tuning
{

namespace
{

bool pressedMaskContainsFifth(
  uint16_t pressedKeyMask, int value, const Config& config)
{
  for (int key = 0; key < 12; ++key)
    if (config.valueForKey[key] == value && hasKey12(pressedKeyMask, key))
      return true;
  return false;
}

} // namespace

void rebuildConfigMask(Config& config)
{
  config.mask = 0;

  for (int key = 0; key < 12; ++key)
    config.mask |= valueToPoolBit(config.valueForKey[key]);
}

std::optional<KeyChoice> inferKeyFromDominantSignature(
  uint16_t pressedKeyMask,
  const Config& config)
{
  for (int a = 0; a < 12; ++a)
  {
    if (!hasKey12(pressedKeyMask, a))
      continue;

    for (int b = a + 1; b < 12; ++b)
    {
      if (!hasKey12(pressedKeyMask, b))
        continue;

      if ((b - a) != 6)
        continue;

      const int lowerValue = config.valueForKey[a];
      const int upperValue = config.valueForKey[b];
      const int difference = upperValue - lowerValue;

      int majorKeyTonic = 0;

      if (difference == -6)
      {
        majorKeyTonic = lowerValue - 5;
      }
      else if (difference == 6)
      {
        majorKeyTonic = upperValue - 5;
      }
      else
      {
        continue;
      }

      const int relativeMinorTonic = majorKeyTonic + 3;
      const int relativeMinorLeadingTone =
        relativeMinorTonic + 5;

      if (pressedMaskContainsFifth(
            pressedKeyMask,
            relativeMinorLeadingTone, config))
      {
        return KeyChoice{relativeMinorTonic, true};
      }

      return KeyChoice{majorKeyTonic, false};
    }
  }

  return std::nullopt;
}

bool isKeyCompatibleWithTuningCenter(
  int tuningCenter,
  int keyTonic,
  bool isMinor)
{
  const KeyChoice candidates[9] =
  {
    {tuningCenter,                          false},
    {tuningCenter,                          true},
    {static_cast<int>(tuningCenter - 3), false},

    {static_cast<int>(tuningCenter + 1), false},
    {static_cast<int>(tuningCenter + 1), true},
    {static_cast<int>(tuningCenter - 2), false},

    {static_cast<int>(tuningCenter - 1), false},
    {static_cast<int>(tuningCenter - 1), true},
    {static_cast<int>(tuningCenter - 4), false}
  };

  for (const auto& candidate : candidates)
  {
    if (candidate.tonic == keyTonic
      && candidate.isMinor == isMinor)
    {
      return true;
    }
  }

  return false;
}

//-------------------------------------
static int holeWeight(int holePosition)
//-------------------------------------
{
  // holePosition:
  //   0 = missing third / missing upper note of first fourth
  //   1 = missing fifth / second fourth level
  //   2 = missing seventh / third fourth level
  //   3 = missing ninth / higher level
  //
  // Lower holes are structurally much more important.

  static constexpr int weights[] =
  {
     10, // position 0
      6, // position 1
      8, // position 2
      5, // position 3
      4, // position 4
      3, // position 5
      2  // position 6
  };

  if (holePosition < 0)
    return 0;

  if (holePosition >= int(std::size(weights)))
    return 1;

  return weights[holePosition];
}

//---------------------------------------------------------------------------
ChordRootAnalysis inferChordRootByStack(const std::vector<ActiveNote>& notes)
//---------------------------------------------------------------------------
{
  struct Candidate
  {
    int root = 0;
    ChordStructure structure = ChordStructure::None;
    uint8_t holes = 0;
    int holeCost = 0;
    uint8_t span = 0;
  };

  ChordRootAnalysis result;

  if (notes.size() < 3)
    return result;

  uint8_t bitmap7 = 0;

  struct RootMap
  {
    uint8_t degree7 = 0;
    int value5 = 0;
  };

  std::vector<RootMap> roots;

  for (const auto& n : notes)
  {
    const uint8_t d7 = mod7(n.interpretedValue);

    bitmap7 |= uint8_t{ 1 } << d7;

    auto it = std::find_if(
      roots.begin(),
      roots.end(),
      [&](const RootMap& r)
      {
        return r.degree7 == d7;
      });

    if (it == roots.end())
      roots.push_back({ d7, n.interpretedValue });
  }

  const int noteCount = popcount(bitmap7);

  if (noteCount < 3)
    return result;

  auto structurePriority = [](ChordStructure s)
    {
      switch (s)
      {
      case ChordStructure::Tertian: return 0;
      case ChordStructure::Quartal: return 1;
      default: return 2;
      }
    };

  auto isBetter = [&](const Candidate& a, const Candidate& b)
    {
      if (a.holeCost != b.holeCost)
        return a.holeCost < b.holeCost;

      if (a.holes != b.holes)
        return a.holes < b.holes;

      if (a.span != b.span)
        return a.span < b.span;

      return structurePriority(a.structure) < structurePriority(b.structure);
    };

  auto bitAtRepeatedBitmap = [&](int pos)
    {
      return (bitmap7 & (uint8_t{ 1 } << (pos % 7))) != 0;
    };

  auto value5ForDegree7 = [&](uint8_t degree7)
    {
      for (const auto& r : roots)
      {
        if (r.degree7 == degree7)
          return r.value5;
      }

      return int{ 0 };
    };

  Candidate bestCandidate;
  bool found = false;

  auto evaluateStructure = [&](ChordStructure structure, int step)
    {
      for (uint8_t x = 0; x < 7; ++x)
      {
        if (!bitAtRepeatedBitmap(x))
          continue;

        const int startValue5 = value5ForDegree7(x);

        int ones = 0;
        int holes = 0;
        int holeCost = 0;
        int span = 0;
        bool valid = true;

        for (int k = 0; k < 7; ++k)
        {
          const int pos = int(x) + k * step;
          const uint8_t degree7 = uint8_t(pos % 7);

          if (bitAtRepeatedBitmap(pos))
          {
            if (structure == ChordStructure::Quartal)
            {
              const int expectedValue5 = int(startValue5 - k);
              const int actualValue5 = value5ForDegree7(degree7);

              if (actualValue5 != expectedValue5)
              {
                valid = false;
                break;
              }
            }

            ++ones;

            if (ones == noteCount)
            {
              span = pos - int(x);
              break;
            }
          }
          else
          {
            ++holes;
            holeCost += holeWeight(k - 1);
          }
        }

        if (!valid || ones != noteCount)
          continue;

        Candidate c;

        if (structure == ChordStructure::Quartal)
        {
          // In a stack of perfect fourths, the perceived chord root is taken
          // as the upper note of the first fourth.
          //
          // Examples:
          //   G-C-F       -> root C
          //   B-E-A-D-G   -> root E
          c.root = int(startValue5 - 1);
        }
        else
        {
          c.root = startValue5;
        }

        c.structure = structure;
        c.holes = uint8_t(holes);
        c.holeCost = holeCost;
        c.span = uint8_t(span);

        if (!found || isBetter(c, bestCandidate))
        {
          bestCandidate = c;
          found = true;
        }
      }
    };

  evaluateStructure(ChordStructure::Tertian, 2);
#ifdef CONSIDER_QUARTAL_CHORDS
  evaluateStructure(ChordStructure::Quartal, 3);
#endif

  if (!found)
    return result;

  result.valid = true;
  result.root = bestCandidate.root;
  result.structure = bestCandidate.structure;
  result.holes = bestCandidate.holes;

  return result;
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
