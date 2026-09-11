#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <tuple>

namespace Intona::Tuning
{

// Keep the selected pitches and their order, choosing only where the octave
// starts on the keyboard. Scores are in cents * EDO to make ties exact.
inline std::array<int8_t, 12> closestKeyboardMapping(
  const std::array<int8_t, 12>& values, int edo, int fifthStep)
{
  if (edo <= 0)
    return values;

  const auto step = [edo, fifthStep](int value)
  {
    const int pitch = (value * fifthStep) % edo;
    return pitch < 0 ? pitch + edo : pitch;
  };
  auto ordered = values;
  std::sort(ordered.begin(), ordered.end(), [&](int a, int b)
  {
    return std::make_pair(step(a), a) < std::make_pair(step(b), b);
  });

  using Score = std::tuple<int, int, int, int>;
  Score bestScore{std::numeric_limits<int>::max(), 0, 0, 0};
  auto best = values;
  const int octave = 1200 * edo;

  for (int rotation = 0; rotation < 12; ++rotation)
  {
    std::array<int8_t, 12> candidate;
    int total = 0;
    int maximum = 0;
    int cDistance = 0;
    for (int key = 0; key < 12; ++key)
    {
      candidate[key] = ordered[(key + rotation) % 12];
      const int raw = 1200 * step(candidate[key]) - 100 * key * edo;
      const int distance = std::min(std::abs(raw), octave - std::abs(raw));
      total += distance;
      maximum = std::max(maximum, distance);
      if (key == 0)
        cDistance = distance;
    }
    // Equal totals favour the smallest worst error, then C closest to zero.
    // The rotation index gives a deterministic final tie-break.
    const Score score{total, maximum, cDistance, rotation};
    if (score < bestScore)
    {
      bestScore = score;
      best = candidate;
    }
  }
  return best;
}

} // namespace Intona::Tuning
