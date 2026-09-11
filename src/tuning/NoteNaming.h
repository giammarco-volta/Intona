#pragma once

#include <cstdlib>
#include <tuple>

namespace Intona::Tuning
{

enum class NoteNamingMode
{
  Fifths = 0,
  LimitedAccidentals = 1
};

struct NoteSpelling
{
  int fifths;
  int stepOffset = 0;
};

inline int accidentalCount(int fifths)
{
  // The natural notes occupy the consecutive fifth positions F=-1 through B=5.
  const int index = ((fifths + 1) % 7 + 7) % 7;
  return (fifths + 1 - index) / 7;
}

inline NoteSpelling limitedNoteSpelling(int fifths, int edo, int fifthStep)
{
  if (std::abs(accidentalCount(fifths)) <= 2 || edo <= 0)
    return {fifths, 0};

  NoteSpelling best{0, 0};
  using Score = std::tuple<int, int, int, int>;
  Score bestScore{edo + 1, 0, 0, 0};

  // Fbb=-15 through B##=19 cover all 35 permitted anchor spellings.
  // Search pitch classes, independently of the current tuning-center window.
  for (int anchor = -15; anchor <= 19; ++anchor)
  {
    int offset = ((fifths - anchor) * fifthStep) % edo;
    if (offset < 0)
      offset += edo;
    if (2 * offset > edo)
      offset -= edo;

    const Score score{
      std::abs(offset),
      std::abs(accidentalCount(anchor)),
      std::abs(anchor - fifths),
      anchor};
    if (score < bestScore)
    {
      bestScore = score;
      best = {anchor, offset};
    }
  }

  return best;
}

// Transpose the chosen center spelling, rather than simplifying each degree
// independently. The chromatic window is the fifth offsets [-5, +6]:
// unison, m2, M2, m3, M3, P4, A4, P5, m6, M6, m7, M7 are respectively
// 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5.
// A step modifier on the center is shared by every transposed degree.
inline NoteSpelling relativeNoteSpelling(
  int fifths, int center, int edo, int fifthStep)
{
  const auto root = limitedNoteSpelling(center, edo, fifthStep);
  return {root.fifths + fifths - center, root.stepOffset};
}

} // namespace Intona::Tuning
