#include "TuningCenterFinder.h"

#include <cstdlib>
#include <climits>
#include <QDebug>

static constexpr std::array<int8_t, 4> kGermanAug6Intervals5 =
{
  0,   // root
  4,   // major third
  1,   // perfect fifth
  10   // augmented sixth, enharmonic to minor seventh
};

//---------------------------------------------------------------------------------
ConfigMask makeTransposedMask(int8_t root5, const std::array<int8_t, 4>& intervals)
//---------------------------------------------------------------------------------
{
  ConfigMask mask;

  for (int8_t interval : intervals)
    mask |= valueToPoolBit(root5 + interval);

  return mask;
}

//------------------------------------------------------------------------
bool keepsOldNotes(const Config& cfg, const std::vector<int8_t>& oldNotes)
//------------------------------------------------------------------------
{
  for (const auto& n : oldNotes)
    if (!containsMask(cfg.mask, valueToPoolBit(n)))
      return false;

  return true;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
const Config* FindConfig(const Chord& chord, const std::vector<int8_t>& oldNotes, const NtetMapping& m, const Config& currentCfg, KeepOldNotes keep)
//--------------------------------------------------------------------------------------------------------------------------------------------------
{
  if (chord.IsNull() || chord.root_ >= 12)
    return nullptr;

  int bestDistance = INT_MAX;
  const Config* bestConfig = nullptr;

  for (int8_t root5 : m.interpretationsForKey[chord.root_])
  {
    if (fifthToSemitone(root5) != chord.root_)
      continue;

    // Special case: dominant seventh may be interpreted as German augmented sixth.
    // If the current config already contains that spelling, do not change tuning center.
    if (chord.type_ == Chord::typeDom7 && chord.t9_ == Chord::tensionVoid9 && chord.t11_ == Chord::tensionVoid11 && chord.t13_ == Chord::tensionVoid13)
    {
      const ConfigMask aug6Bitmap = makeTransposedMask(root5, kGermanAug6Intervals5);

      if (containsMask(currentCfg.mask, aug6Bitmap))
      {
        const_cast<Chord&>(chord).set(Chord::typeAug6th, Chord::tensionVoid9, Chord::tensionVoid11, Chord::tensionVoid13);
        return &currentCfg;
      }
    }

    ConfigMask bitmap = ChordMask(chord, currentCfg);

    for (int tc = m.minValue; tc <= m.maxValue; ++tc)
    {
      const Config& cfg = m.getConfig(tc);

      if (!containsMask(cfg.mask, bitmap))
        continue;

      if (keep == KeepOldNotes::Yes && !keepsOldNotes(cfg, oldNotes))
        continue;

      const int distance = std::abs(int(cfg.tuningCenter) - int(currentCfg.tuningCenter));

      if (!bestConfig
        || distance < bestDistance
        || (distance == bestDistance
          && std::abs(int(cfg.tuningCenter)) < std::abs(int(bestConfig->tuningCenter))))
      {
        bestConfig = &cfg;
        bestDistance = distance;
      }
    }
  }

  static int c = 0;
  if (!bestConfig)
    qDebug() << ++c << chord.root_ << chord.GetChordString() << "current tc = " << currentCfg.tuningCenter << "No config found!!!!";

  return bestConfig;
}

//----------------------------------------------------------------
bool TestChordConfig(const Chord& chord, const Config& currentCfg)
//----------------------------------------------------------------
{
  ConfigMask bitmap = ChordMask(chord, currentCfg);
  return containsMask(currentCfg.mask, bitmap);
}

//----------------------------------------------------------------
ConfigMask ChordMask(const Chord& chord, const Config& currentCfg)
//----------------------------------------------------------------
{
  const int8_t root5 = currentCfg.valueForKey[chord.root_];
  const std::list<int8_t> notes5 = chord.GetNotesIn5Cycles();

  ConfigMask bitmap = valueToPoolBit(root5); // include the root itself

  for (const auto& n : notes5)
  {
    const int8_t note5 = root5 + n;
    bitmap |= valueToPoolBit(note5);
  }

  return bitmap;
}
