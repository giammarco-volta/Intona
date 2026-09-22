#pragma once

#include "Config.hpp"

#include <cmath> // Essential header for std::round


//-------------------------------------------
inline double tetStepToCents(int step, int N)
//-------------------------------------------
{
  return 1200.0 * double(step) / double(N);
}

//-------------------------------------------------------------------
inline int fifthValueToNtetStep(int fifthValue, int fifthStep, int N)
//-------------------------------------------------------------------
{
  return mod(fifthValue * fifthStep, N);
}

//---------------------------------------------
inline double standard12TetCentsForKey(int key)
//---------------------------------------------
{
  return double(key) * 100.0;
}

inline double keyboardRawOffset(int key, const Config& config, int N, int fifthStep, double preferredOffset = 0)
{
  const auto step = [&](int value) { return mod(mod(value, N) * fifthStep, N); };
  const double cents = 1200.0 * step(config.valueForKey[key]) / N;
  double raw = cents - 100.0 * key;
  if (config.relativeKeyboard)
  {
    const int cStep = step(config.valueForKey[0]);
    double c = 1200.0 * cStep / N;
    // Keep the shared octave lift near the established output offset;
    // crossing the +/-600 boundary must not transpose eleven held notes.
    c += 1200 * std::round((preferredOffset - c) / 1200);
    return c + 1200.0 * mod(step(config.valueForKey[key])-cStep,N)/N - 100.0*key;
  }
  while (raw <= -600) raw += 1200;
  while (raw > 600) raw -= 1200;
  return raw;
}

//----------------------------------------------------------------------------------------------------------------------------
inline std::array<double, 12> computeDetuneTable(uint8_t N, uint8_t fifthStep, const Config& config, double globalOffsetCents)
//----------------------------------------------------------------------------------------------------------------------------
{
  std::array<double, 12> detunes;

  for (int key = 0; key < 12; ++key)
  {
    const double detune = keyboardRawOffset(key, config, N, fifthStep, globalOffsetCents) - globalOffsetCents;

    detunes[key] = detune;
    assert(detunes[key] >= -99.0 && detunes[key] <= 99.0);
  }

  return detunes;
}

//-------------------------------------------
inline uint16_t centsToMts14Bit(double cents)
//-------------------------------------------
{
  constexpr double RANGE = 100.0;

  double normalized = (cents / RANGE);

  int value = int(std::round(8192.0 + normalized * 8192.0));

  if (value < 0)
    value = 0;

  if (value > 16383)
    value = 16383;

  return uint16_t(value);
}

//---------------------------------------------------------------------------------------------------------------------------
inline std::array<uint16_t, 12> computeMtsTable(uint8_t N, uint8_t fifthStep, const Config& config, double globalOffsetCents)
//---------------------------------------------------------------------------------------------------------------------------
{
  const auto detunes = computeDetuneTable(N, fifthStep, config, globalOffsetCents);
  std::array<uint16_t, 12> mts;

  for (int i = 0; i < 12; ++i)
    mts[i] = centsToMts14Bit(detunes[i]);

  return mts;
}