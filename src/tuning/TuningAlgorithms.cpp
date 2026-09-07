#include "TuningAlgorithms.h"

#include "../Chords.h"
#include <algorithm>
#include <iterator>

//#define CONSIDER_QUARTAL_CHORDS

namespace Intona::Tuning
{

namespace
{

bool pressedMaskContainsFifth(uint16_t pressedKeyMask, int16_t value)
{
  return (pressedKeyMask
    & (uint16_t{ 1 } << fifthToSemitone(value))) != 0;
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

      const int8_t lowerValue = config.valueForKey[a];
      const int8_t upperValue = config.valueForKey[b];
      const int8_t difference = upperValue - lowerValue;

      int8_t majorKeyTonic = 0;

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

      const int8_t relativeMinorTonic = majorKeyTonic + 3;
      const int8_t relativeMinorLeadingTone =
        relativeMinorTonic + 5;

      if (pressedMaskContainsFifth(
            pressedKeyMask,
            relativeMinorLeadingTone))
      {
        return KeyChoice{relativeMinorTonic, true};
      }

      return KeyChoice{majorKeyTonic, false};
    }
  }

  return std::nullopt;
}

bool isKeyCompatibleWithTuningCenter(
  int8_t tuningCenter,
  int8_t keyTonic,
  bool isMinor)
{
  const KeyChoice candidates[9] =
  {
    {tuningCenter,                          false},
    {tuningCenter,                          true},
    {static_cast<int8_t>(tuningCenter - 3), false},

    {static_cast<int8_t>(tuningCenter + 1), false},
    {static_cast<int8_t>(tuningCenter + 1), true},
    {static_cast<int8_t>(tuningCenter - 2), false},

    {static_cast<int8_t>(tuningCenter - 1), false},
    {static_cast<int8_t>(tuningCenter - 1), true},
    {static_cast<int8_t>(tuningCenter - 4), false}
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
    int16_t root = 0;
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
    int16_t value5 = 0;
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

      return int16_t{ 0 };
    };

  Candidate bestCandidate;
  bool found = false;

  auto evaluateStructure = [&](ChordStructure structure, int step)
    {
      for (uint8_t x = 0; x < 7; ++x)
      {
        if (!bitAtRepeatedBitmap(x))
          continue;

        const int16_t startValue5 = value5ForDegree7(x);

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
              const int16_t expectedValue5 = int16_t(startValue5 - k);
              const int16_t actualValue5 = value5ForDegree7(degree7);

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
          c.root = int16_t(startValue5 - 1);
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


} // namespace Intona::Tuning
