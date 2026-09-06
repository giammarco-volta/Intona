#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <QString>
#include <qassert.h>

//--------------------
struct MidiSettings
//--------------------
{
  QString midiInPort;
  QString midiOutPort;
  uint8_t midiInChannel = 0;
  bool outChnEnabled[16] = { false };
  uint8_t edo = 0;
};

// Generated NTET mappings.
// The global Config pool is indexed by tuning center in fifth-space.
// Pool tuning-center range: [-29, +34], inclusive.
// Config masks use the complete symbolic note range [-34, +40], inclusive (75 values).
// NtetMapping range rule:
//   odd  N -> [-(N-1)/2 + 3, +(N-1)/2 + 3]
//   even N -> [-N/2 + 3, +N/2 + 2]
// Key order: 0=C, 1=C#/Db, 2=D, ..., 11=B.
// valueForKey stores symbolic fifth-space values for the tuning center chromatic window [t-5, t+6].
// Config masks are symbolic too: they are not wrapped modulo the current N-EDO.

inline constexpr int8_t kConfigPoolMin = -29;
inline constexpr int8_t kConfigPoolMax = 34;
inline constexpr size_t kConfigPoolSize = 64;

inline constexpr int8_t kConfigMaskMin = kConfigPoolMin - 5;
inline constexpr int8_t kConfigMaskMax = kConfigPoolMax + 6;
inline constexpr size_t kConfigMaskSize = static_cast<size_t>(kConfigMaskMax - kConfigMaskMin + 1);

using ConfigMask = std::bitset<kConfigMaskSize>;

//-----------
struct Config
//-----------
{
  static constexpr int8_t invalid = -128;

  int8_t tuningCenter;
  std::array<int8_t, 12> valueForKey;
  ConfigMask mask;
};

//----------------
struct NtetMapping
//----------------
{
  uint8_t N;
  uint8_t fifthStep;
  int8_t minValue;
  int8_t maxValue;
  std::array<std::vector<int8_t>, 12> interpretationsForKey;

  const Config& getConfig(int8_t tuningCenter) const;
};

//--------------------------
inline int mod(int a, int b)
//--------------------------
{
  const int r = a % b;
  return r < 0 ? r + b : r;
}

//---------------------------------
inline int modInverse(int a, int m)
//---------------------------------
{
  a = mod(a, m);

  for (int x = 1; x < m; ++x)
    if ((a * x) % m == 1)
      return x;

  return 0; // errore: non invertibile
}

//---------------------------------------
inline uint8_t fifthToSemitone(int fifth)
//---------------------------------------
{
  // Maps positions in fifth-space to 12-TET semitone classes.
  //
  // Examples:
  //   0  -> C
  //   1  -> G
  //  -1  -> F
  //   2  -> D
  //  -2  -> Bb
  //
  // Enharmonic spellings differing by 7 fifths collapse to the same
  // semitone class:
  //
  //   Cb(-7), C(0), C#(+7) -> 0
  //   Db(-5), D(+2)        -> 2
  //
  // Formula:
  //   semitone = fifth * 7 mod 12

  int s = (fifth * 7) % 12;

  if (s < 0)
    s += 12;

  return static_cast<uint8_t>(s);
}

//---------------------------------
inline uint8_t mod7(int fifthValue)
//---------------------------------
{
  int x = (4 * fifthValue) % 7;
  return static_cast<uint8_t>(x < 0 ? x + 7 : x);
}

//---------------------
inline int mod12(int x)
//---------------------
{
  x %= 12;
  return x < 0 ? x + 12 : x;
}

//------------------------------------------
inline bool hasKey12(uint16_t mask, int key)
//------------------------------------------
{
  return (mask & (uint16_t{ 1 } << mod12(key))) != 0;
}

//------------------------------------------------------
template <typename type> inline uint8_t popcount(type x)
//------------------------------------------------------
{
  uint8_t c = 0;

  while (x)
  {
    c += x & 1;
    x >>= 1;
  }

  return c;
}

//--------------------------------------------------
inline bool areTwoAdjacentOrDistance2(uint16_t mask)
//--------------------------------------------------
{
  if (popcount(mask) != 2)
    return false;

  //se due bit sono adiacenti, ruotando di 1 uno va sopra l'altro
  //se distano 2, ruotando di 2 uno va sopra l'altro
    
    constexpr uint16_t all12 = 0x0FFF;

  // shift circolare di 1
  uint16_t rot1 = ((mask << 1) | (mask >> 11)) & all12;

  // shift circolare di 2
  uint16_t rot2 = ((mask << 2) | (mask >> 10)) & all12;

  return (mask & rot1) || (mask & rot2);
}

//---------------------------------------------------------------------------
inline int8_t wrapFifthsToMappingRange(int value, const NtetMapping& mapping)
//---------------------------------------------------------------------------
{
  return static_cast<int8_t>(mapping.minValue + mod(value - mapping.minValue, mapping.N));
}

//-----------------------------------------
inline ConfigMask valueToPoolBit(int value)
//-----------------------------------------
{
  Q_ASSERT(value >= kConfigMaskMin && value <= kConfigMaskMax);

  ConfigMask mask;
  mask.set(static_cast<size_t>(value - kConfigMaskMin));
  return mask;
}

//------------------------------------------
inline size_t valueToPoolBitIndex(int value)
//------------------------------------------
{
  Q_ASSERT(value >= kConfigMaskMin && value <= kConfigMaskMax);
  return static_cast<size_t>(value - kConfigMaskMin);
}

//--------------------------------------------------------------------------------
inline bool containsMask(const ConfigMask& container, const ConfigMask& contained)
//--------------------------------------------------------------------------------
{
  return (container & contained) == contained;
}

//------------------------------------------------------------------------
inline ConfigMask makeMaskFromValues(const std::array<int8_t, 12>& values)
//------------------------------------------------------------------------
{
  ConfigMask mask;

  for (const int8_t value : values)
  {
    Q_ASSERT(value >= kConfigMaskMin && value <= kConfigMaskMax);
    mask.set(static_cast<size_t>(value - kConfigMaskMin));
  }

  return mask;
}

//---------------------------------------------------------------------
inline ConfigMask makeMaskFromValues(const std::vector<int8_t>& values)
//---------------------------------------------------------------------
{
  ConfigMask mask;

  for (const int8_t value : values)
  {
    Q_ASSERT(value >= kConfigMaskMin && value <= kConfigMaskMax);
    mask.set(static_cast<size_t>(value - kConfigMaskMin));
  }

  return mask;
}

//------------------------------------------------------------------------------
inline bool isValidTuningCenter(int8_t tuningCenter, const NtetMapping& mapping)
//------------------------------------------------------------------------------
{
  return tuningCenter >= mapping.minValue && tuningCenter <= mapping.maxValue;
}

//-------------------------------------------------------------
inline const std::array<Config, kConfigPoolSize> configPool =
//-------------------------------------------------------------
{
  {
    Config{
      -29,
      std::array<int8_t, 12>{  -24,  -29,  -34,  -27,  -32,  -25,  -30,  -23,  -28,  -33,  -26,  -31 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -29,  -34,  -27,  -32,  -25,  -30,  -23,  -28,  -33,  -26,  -31 })
    },
    Config{
      -28,
      std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -32,  -25,  -30,  -23,  -28,  -33,  -26,  -31 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -32,  -25,  -30,  -23,  -28,  -33,  -26,  -31 })
    },
    Config{
      -27,
      std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -32,  -25,  -30,  -23,  -28,  -21,  -26,  -31 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -32,  -25,  -30,  -23,  -28,  -21,  -26,  -31 })
    },
    Config{
      -26,
      std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -20,  -25,  -30,  -23,  -28,  -21,  -26,  -31 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -20,  -25,  -30,  -23,  -28,  -21,  -26,  -31 })
    },
    Config{
      -25,
      std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -20,  -25,  -30,  -23,  -28,  -21,  -26,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -20,  -25,  -30,  -23,  -28,  -21,  -26,  -19 })
    },
    Config{
      -24,
      std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -20,  -25,  -18,  -23,  -28,  -21,  -26,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -29,  -22,  -27,  -20,  -25,  -18,  -23,  -28,  -21,  -26,  -19 })
    },
    Config{
      -23,
      std::array<int8_t, 12>{  -24,  -17,  -22,  -27,  -20,  -25,  -18,  -23,  -28,  -21,  -26,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -17,  -22,  -27,  -20,  -25,  -18,  -23,  -28,  -21,  -26,  -19 })
    },
    Config{
      -22,
      std::array<int8_t, 12>{  -24,  -17,  -22,  -27,  -20,  -25,  -18,  -23,  -16,  -21,  -26,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -17,  -22,  -27,  -20,  -25,  -18,  -23,  -16,  -21,  -26,  -19 })
    },
    Config{
      -21,
      std::array<int8_t, 12>{  -24,  -17,  -22,  -15,  -20,  -25,  -18,  -23,  -16,  -21,  -26,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -17,  -22,  -15,  -20,  -25,  -18,  -23,  -16,  -21,  -26,  -19 })
    },
    Config{
      -20,
      std::array<int8_t, 12>{  -24,  -17,  -22,  -15,  -20,  -25,  -18,  -23,  -16,  -21,  -14,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -17,  -22,  -15,  -20,  -25,  -18,  -23,  -16,  -21,  -14,  -19 })
    },
    Config{
      -19,
      std::array<int8_t, 12>{  -24,  -17,  -22,  -15,  -20,  -13,  -18,  -23,  -16,  -21,  -14,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -24,  -17,  -22,  -15,  -20,  -13,  -18,  -23,  -16,  -21,  -14,  -19 })
    },
    Config{
      -18,
      std::array<int8_t, 12>{  -12,  -17,  -22,  -15,  -20,  -13,  -18,  -23,  -16,  -21,  -14,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,  -17,  -22,  -15,  -20,  -13,  -18,  -23,  -16,  -21,  -14,  -19 })
    },
    Config{
      -17,
      std::array<int8_t, 12>{  -12,  -17,  -22,  -15,  -20,  -13,  -18,  -11,  -16,  -21,  -14,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,  -17,  -22,  -15,  -20,  -13,  -18,  -11,  -16,  -21,  -14,  -19 })
    },
    Config{
      -16,
      std::array<int8_t, 12>{  -12,  -17,  -10,  -15,  -20,  -13,  -18,  -11,  -16,  -21,  -14,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,  -17,  -10,  -15,  -20,  -13,  -18,  -11,  -16,  -21,  -14,  -19 })
    },
    Config{
      -15,
      std::array<int8_t, 12>{  -12,  -17,  -10,  -15,  -20,  -13,  -18,  -11,  -16,   -9,  -14,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,  -17,  -10,  -15,  -20,  -13,  -18,  -11,  -16,   -9,  -14,  -19 })
    },
    Config{
      -14,
      std::array<int8_t, 12>{  -12,  -17,  -10,  -15,   -8,  -13,  -18,  -11,  -16,   -9,  -14,  -19 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,  -17,  -10,  -15,   -8,  -13,  -18,  -11,  -16,   -9,  -14,  -19 })
    },
    Config{
      -13,
      std::array<int8_t, 12>{  -12,  -17,  -10,  -15,   -8,  -13,  -18,  -11,  -16,   -9,  -14,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,  -17,  -10,  -15,   -8,  -13,  -18,  -11,  -16,   -9,  -14,   -7 })
    },
    Config{
      -12,
      std::array<int8_t, 12>{  -12,  -17,  -10,  -15,   -8,  -13,   -6,  -11,  -16,   -9,  -14,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,  -17,  -10,  -15,   -8,  -13,   -6,  -11,  -16,   -9,  -14,   -7 })
    },
    Config{
      -11,
      std::array<int8_t, 12>{  -12,   -5,  -10,  -15,   -8,  -13,   -6,  -11,  -16,   -9,  -14,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,   -5,  -10,  -15,   -8,  -13,   -6,  -11,  -16,   -9,  -14,   -7 })
    },
    Config{
      -10,
      std::array<int8_t, 12>{  -12,   -5,  -10,  -15,   -8,  -13,   -6,  -11,   -4,   -9,  -14,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,   -5,  -10,  -15,   -8,  -13,   -6,  -11,   -4,   -9,  -14,   -7 })
    },
    Config{
      -9,
      std::array<int8_t, 12>{  -12,   -5,  -10,   -3,   -8,  -13,   -6,  -11,   -4,   -9,  -14,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,   -5,  -10,   -3,   -8,  -13,   -6,  -11,   -4,   -9,  -14,   -7 })
    },
    Config{
      -8,
      std::array<int8_t, 12>{  -12,   -5,  -10,   -3,   -8,  -13,   -6,  -11,   -4,   -9,   -2,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,   -5,  -10,   -3,   -8,  -13,   -6,  -11,   -4,   -9,   -2,   -7 })
    },
    Config{
      -7,
      std::array<int8_t, 12>{  -12,   -5,  -10,   -3,   -8,   -1,   -6,  -11,   -4,   -9,   -2,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{  -12,   -5,  -10,   -3,   -8,   -1,   -6,  -11,   -4,   -9,   -2,   -7 })
    },
    Config{
      -6,
      std::array<int8_t, 12>{    0,   -5,  -10,   -3,   -8,   -1,   -6,  -11,   -4,   -9,   -2,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,   -5,  -10,   -3,   -8,   -1,   -6,  -11,   -4,   -9,   -2,   -7 })
    },
    Config{
      -5,
      std::array<int8_t, 12>{    0,   -5,  -10,   -3,   -8,   -1,   -6,    1,   -4,   -9,   -2,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,   -5,  -10,   -3,   -8,   -1,   -6,    1,   -4,   -9,   -2,   -7 })
    },
    Config{
      -4,
      std::array<int8_t, 12>{    0,   -5,    2,   -3,   -8,   -1,   -6,    1,   -4,   -9,   -2,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,   -5,    2,   -3,   -8,   -1,   -6,    1,   -4,   -9,   -2,   -7 })
    },
    Config{
      -3,
      std::array<int8_t, 12>{    0,   -5,    2,   -3,   -8,   -1,   -6,    1,   -4,    3,   -2,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,   -5,    2,   -3,   -8,   -1,   -6,    1,   -4,    3,   -2,   -7 })
    },
    Config{
      -2,
      std::array<int8_t, 12>{    0,   -5,    2,   -3,    4,   -1,   -6,    1,   -4,    3,   -2,   -7 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,   -5,    2,   -3,    4,   -1,   -6,    1,   -4,    3,   -2,   -7 })
    },
    Config{
      -1,
      std::array<int8_t, 12>{    0,   -5,    2,   -3,    4,   -1,   -6,    1,   -4,    3,   -2,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,   -5,    2,   -3,    4,   -1,   -6,    1,   -4,    3,   -2,    5 })
    },
    Config{
      0,
      std::array<int8_t, 12>{    0,   -5,    2,   -3,    4,   -1,    6,    1,   -4,    3,   -2,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,   -5,    2,   -3,    4,   -1,    6,    1,   -4,    3,   -2,    5 })
    },
    Config{
      1,
      std::array<int8_t, 12>{    0,    7,    2,   -3,    4,   -1,    6,    1,   -4,    3,   -2,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,    7,    2,   -3,    4,   -1,    6,    1,   -4,    3,   -2,    5 })
    },
    Config{
      2,
      std::array<int8_t, 12>{    0,    7,    2,   -3,    4,   -1,    6,    1,    8,    3,   -2,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,    7,    2,   -3,    4,   -1,    6,    1,    8,    3,   -2,    5 })
    },
    Config{
      3,
      std::array<int8_t, 12>{    0,    7,    2,    9,    4,   -1,    6,    1,    8,    3,   -2,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,    7,    2,    9,    4,   -1,    6,    1,    8,    3,   -2,    5 })
    },
    Config{
      4,
      std::array<int8_t, 12>{    0,    7,    2,    9,    4,   -1,    6,    1,    8,    3,   10,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,    7,    2,    9,    4,   -1,    6,    1,    8,    3,   10,    5 })
    },
    Config{
      5,
      std::array<int8_t, 12>{    0,    7,    2,    9,    4,   11,    6,    1,    8,    3,   10,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{    0,    7,    2,    9,    4,   11,    6,    1,    8,    3,   10,    5 })
    },
    Config{
      6,
      std::array<int8_t, 12>{   12,    7,    2,    9,    4,   11,    6,    1,    8,    3,   10,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,    7,    2,    9,    4,   11,    6,    1,    8,    3,   10,    5 })
    },
    Config{
      7,
      std::array<int8_t, 12>{   12,    7,    2,    9,    4,   11,    6,   13,    8,    3,   10,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,    7,    2,    9,    4,   11,    6,   13,    8,    3,   10,    5 })
    },
    Config{
      8,
      std::array<int8_t, 12>{   12,    7,   14,    9,    4,   11,    6,   13,    8,    3,   10,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,    7,   14,    9,    4,   11,    6,   13,    8,    3,   10,    5 })
    },
    Config{
      9,
      std::array<int8_t, 12>{   12,    7,   14,    9,    4,   11,    6,   13,    8,   15,   10,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,    7,   14,    9,    4,   11,    6,   13,    8,   15,   10,    5 })
    },
    Config{
      10,
      std::array<int8_t, 12>{   12,    7,   14,    9,   16,   11,    6,   13,    8,   15,   10,    5 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,    7,   14,    9,   16,   11,    6,   13,    8,   15,   10,    5 })
    },
    Config{
      11,
      std::array<int8_t, 12>{   12,    7,   14,    9,   16,   11,    6,   13,    8,   15,   10,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,    7,   14,    9,   16,   11,    6,   13,    8,   15,   10,   17 })
    },
    Config{
      12,
      std::array<int8_t, 12>{   12,    7,   14,    9,   16,   11,   18,   13,    8,   15,   10,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,    7,   14,    9,   16,   11,   18,   13,    8,   15,   10,   17 })
    },
    Config{
      13,
      std::array<int8_t, 12>{   12,   19,   14,    9,   16,   11,   18,   13,    8,   15,   10,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,   19,   14,    9,   16,   11,   18,   13,    8,   15,   10,   17 })
    },
    Config{
      14,
      std::array<int8_t, 12>{   12,   19,   14,    9,   16,   11,   18,   13,   20,   15,   10,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,   19,   14,    9,   16,   11,   18,   13,   20,   15,   10,   17 })
    },
    Config{
      15,
      std::array<int8_t, 12>{   12,   19,   14,   21,   16,   11,   18,   13,   20,   15,   10,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,   19,   14,   21,   16,   11,   18,   13,   20,   15,   10,   17 })
    },
    Config{
      16,
      std::array<int8_t, 12>{   12,   19,   14,   21,   16,   11,   18,   13,   20,   15,   22,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,   19,   14,   21,   16,   11,   18,   13,   20,   15,   22,   17 })
    },
    Config{
      17,
      std::array<int8_t, 12>{   12,   19,   14,   21,   16,   23,   18,   13,   20,   15,   22,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   12,   19,   14,   21,   16,   23,   18,   13,   20,   15,   22,   17 })
    },
    Config{
      18,
      std::array<int8_t, 12>{   24,   19,   14,   21,   16,   23,   18,   13,   20,   15,   22,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   19,   14,   21,   16,   23,   18,   13,   20,   15,   22,   17 })
    },
    Config{
      19,
      std::array<int8_t, 12>{   24,   19,   14,   21,   16,   23,   18,   25,   20,   15,   22,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   19,   14,   21,   16,   23,   18,   25,   20,   15,   22,   17 })
    },
    Config{
      20,
      std::array<int8_t, 12>{   24,   19,   26,   21,   16,   23,   18,   25,   20,   15,   22,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   19,   26,   21,   16,   23,   18,   25,   20,   15,   22,   17 })
    },
    Config{
      21,
      std::array<int8_t, 12>{   24,   19,   26,   21,   16,   23,   18,   25,   20,   27,   22,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   19,   26,   21,   16,   23,   18,   25,   20,   27,   22,   17 })
    },
    Config{
      22,
      std::array<int8_t, 12>{   24,   19,   26,   21,   28,   23,   18,   25,   20,   27,   22,   17 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   19,   26,   21,   28,   23,   18,   25,   20,   27,   22,   17 })
    },
    Config{
      23,
      std::array<int8_t, 12>{   24,   19,   26,   21,   28,   23,   18,   25,   20,   27,   22,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   19,   26,   21,   28,   23,   18,   25,   20,   27,   22,   29 })
    },
    Config{
      24,
      std::array<int8_t, 12>{   24,   19,   26,   21,   28,   23,   30,   25,   20,   27,   22,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   19,   26,   21,   28,   23,   30,   25,   20,   27,   22,   29 })
    },
    Config{
      25,
      std::array<int8_t, 12>{   24,   31,   26,   21,   28,   23,   30,   25,   20,   27,   22,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   31,   26,   21,   28,   23,   30,   25,   20,   27,   22,   29 })
    },
    Config{
      26,
      std::array<int8_t, 12>{   24,   31,   26,   21,   28,   23,   30,   25,   32,   27,   22,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   31,   26,   21,   28,   23,   30,   25,   32,   27,   22,   29 })
    },
    Config{
      27,
      std::array<int8_t, 12>{   24,   31,   26,   33,   28,   23,   30,   25,   32,   27,   22,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   31,   26,   33,   28,   23,   30,   25,   32,   27,   22,   29 })
    },
    Config{
      28,
      std::array<int8_t, 12>{   24,   31,   26,   33,   28,   23,   30,   25,   32,   27,   34,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   31,   26,   33,   28,   23,   30,   25,   32,   27,   34,   29 })
    },
    Config{
      29,
      std::array<int8_t, 12>{   24,   31,   26,   33,   28,   35,   30,   25,   32,   27,   34,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   24,   31,   26,   33,   28,   35,   30,   25,   32,   27,   34,   29 })
    },
    Config{
      30,
      std::array<int8_t, 12>{   36,   31,   26,   33,   28,   35,   30,   25,   32,   27,   34,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   36,   31,   26,   33,   28,   35,   30,   25,   32,   27,   34,   29 })
    },
    Config{
      31,
      std::array<int8_t, 12>{   36,   31,   26,   33,   28,   35,   30,   37,   32,   27,   34,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   36,   31,   26,   33,   28,   35,   30,   37,   32,   27,   34,   29 })
    },
    Config{
      32,
      std::array<int8_t, 12>{   36,   31,   38,   33,   28,   35,   30,   37,   32,   27,   34,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   36,   31,   38,   33,   28,   35,   30,   37,   32,   27,   34,   29 })
    },
    Config{
      33,
      std::array<int8_t, 12>{   36,   31,   38,   33,   28,   35,   30,   37,   32,   39,   34,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   36,   31,   38,   33,   28,   35,   30,   37,   32,   39,   34,   29 })
    },
    Config{
      34,
      std::array<int8_t, 12>{   36,   31,   38,   33,   40,   35,   30,   37,   32,   39,   34,   29 },
      makeMaskFromValues(std::array<int8_t, 12>{   36,   31,   38,   33,   40,   35,   30,   37,   32,   39,   34,   29 })
    }
  }
};

//---------------------------------------------------------
inline const Config& NtetMapping::getConfig(int8_t tuningCenter) const
//---------------------------------------------------------
{
  Q_ASSERT(isValidTuningCenter(tuningCenter, *this));
  Q_ASSERT(tuningCenter >= kConfigPoolMin && tuningCenter <= kConfigPoolMax);
  return configPool[static_cast<size_t>(tuningCenter - kConfigPoolMin)];
}

//---------------------------------------------------
inline const std::vector<NtetMapping> kNtetMappings =
//---------------------------------------------------
{
    {
        17, 10, -5, 11,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -5, 0 }, // key 0
            std::vector<int8_t>{ -5, 7 }, // key 1
            std::vector<int8_t>{ -3, 2, 7 }, // key 2
            std::vector<int8_t>{ -3, 9 }, // key 3
            std::vector<int8_t>{ -1, 4, 9 }, // key 4
            std::vector<int8_t>{ -1, 11 }, // key 5
            std::vector<int8_t>{ 6, 11 }, // key 6
            std::vector<int8_t>{ -4, 1 }, // key 7
            std::vector<int8_t>{ -4, 8 }, // key 8
            std::vector<int8_t>{ -2, 3, 8 }, // key 9
            std::vector<int8_t>{ -2, 10 }, // key 10
            std::vector<int8_t>{ 0, 5, 10 }, // key 11
        }
    },
    {
        19, 11, -6, 12,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ 0, 12 }, // key 0
            std::vector<int8_t>{ -5, 7 }, // key 1
            std::vector<int8_t>{ -5, 2, 9 }, // key 2
            std::vector<int8_t>{ -3, 9 }, // key 3
            std::vector<int8_t>{ -3, 4, 11 }, // key 4
            std::vector<int8_t>{ -1, 11 }, // key 5
            std::vector<int8_t>{ -6, -1, 6 }, // key 6
            std::vector<int8_t>{ -6, 1, 8 }, // key 7
            std::vector<int8_t>{ -4, 8 }, // key 8
            std::vector<int8_t>{ -4, 3, 10 }, // key 9
            std::vector<int8_t>{ -2, 10 }, // key 10
            std::vector<int8_t>{ -2, 5, 12 }, // key 11
        }
    },
    {
        22, 13, -8, 13,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ 0, 10, 12 }, // key 0
            std::vector<int8_t>{ -5, -3, 7 }, // key 1
            std::vector<int8_t>{ -8, 2, 12 }, // key 2
            std::vector<int8_t>{ -3, 9 }, // key 3
            std::vector<int8_t>{ -8, -6, 4 }, // key 4
            std::vector<int8_t>{ -1, 9, 11 }, // key 5
            std::vector<int8_t>{ -6, -4, 6 }, // key 6
            std::vector<int8_t>{ 1, 11, 13 }, // key 7
            std::vector<int8_t>{ -4, 8 }, // key 8
            std::vector<int8_t>{ -7, 3, 13 }, // key 9
            std::vector<int8_t>{ -2, 10 }, // key 10
            std::vector<int8_t>{ -7, -5, 5 }, // key 11
        }
    },
    {
        26, 15, -10, 15,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ 0, 12, 14 }, // key 0
            std::vector<int8_t>{ -7, -5, 7 }, // key 1
            std::vector<int8_t>{ -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -5, -3, 9, 11 }, // key 3
            std::vector<int8_t>{ -10, -8, 4 }, // key 4
            std::vector<int8_t>{ -1, 11, 13 }, // key 5
            std::vector<int8_t>{ -8, -6, 6 }, // key 6
            std::vector<int8_t>{ 1, 13, 15 }, // key 7
            std::vector<int8_t>{ -6, -4, 8 }, // key 8
            std::vector<int8_t>{ -9, 3, 15 }, // key 9
            std::vector<int8_t>{ -2, 10, 12 }, // key 10
            std::vector<int8_t>{ -9, -7, 5 }, // key 11
        }
    },
    {
        27, 16, -10, 16,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ 0, 12, 15 }, // key 0
            std::vector<int8_t>{ -8, -5, 7 }, // key 1
            std::vector<int8_t>{ -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -6, -3, 9, 12 }, // key 3
            std::vector<int8_t>{ -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -1, 11, 14 }, // key 5
            std::vector<int8_t>{ -9, -6, 6 }, // key 6
            std::vector<int8_t>{ 1, 13, 16 }, // key 7
            std::vector<int8_t>{ -7, -4, 8 }, // key 8
            std::vector<int8_t>{ -9, 3, 15 }, // key 9
            std::vector<int8_t>{ -5, -2, 10, 13 }, // key 10
            std::vector<int8_t>{ -10, -7, 5 }, // key 11
        }
    },
    {
        29, 17, -11, 17,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ 0, 12, 17 }, // key 0
            std::vector<int8_t>{ -10, -5, 7 }, // key 1
            std::vector<int8_t>{ -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -8, -3, 9, 14 }, // key 3
            std::vector<int8_t>{ -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -6, -1, 11, 16 }, // key 5
            std::vector<int8_t>{ -11, -6, 6 }, // key 6
            std::vector<int8_t>{ -11, 1, 13 }, // key 7
            std::vector<int8_t>{ -9, -4, 8, 13 }, // key 8
            std::vector<int8_t>{ -9, 3, 15 }, // key 9
            std::vector<int8_t>{ -7, -2, 10, 15 }, // key 10
            std::vector<int8_t>{ -7, 5, 17 }, // key 11
        }
    },
    {
        31, 18, -12, 18,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, -7, 0, 12 }, // key 0
            std::vector<int8_t>{ -12, -5, 7, 14 }, // key 1
            std::vector<int8_t>{ -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -10, -3, 9, 16 }, // key 3
            std::vector<int8_t>{ -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -8, -1, 11, 18 }, // key 5
            std::vector<int8_t>{ -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -11, 1, 13 }, // key 7
            std::vector<int8_t>{ -11, -4, 8, 15 }, // key 8
            std::vector<int8_t>{ -9, 3, 15 }, // key 9
            std::vector<int8_t>{ -9, -2, 10, 17 }, // key 10
            std::vector<int8_t>{ -7, 5, 17 }, // key 11
        }
    },
    {
        32, 19, -13, 18,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, -8, 0, 12 }, // key 0
            std::vector<int8_t>{ -13, -5, 7, 15 }, // key 1
            std::vector<int8_t>{ -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -11, -3, 9, 17 }, // key 3
            std::vector<int8_t>{ -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -13, -9, -1, 11 }, // key 5
            std::vector<int8_t>{ -6, 6, 14, 18 }, // key 6
            std::vector<int8_t>{ -11, 1, 13 }, // key 7
            std::vector<int8_t>{ -12, -4, 8, 16 }, // key 8
            std::vector<int8_t>{ -9, 3, 15 }, // key 9
            std::vector<int8_t>{ -10, -2, 10, 18 }, // key 10
            std::vector<int8_t>{ -7, 5, 17 }, // key 11
        }
    },
    {
        33, 19, -13, 19,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, -9, 0, 12 }, // key 0
            std::vector<int8_t>{ -5, 7, 16, 19 }, // key 1
            std::vector<int8_t>{ -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -12, -3, 9, 18 }, // key 3
            std::vector<int8_t>{ -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -13, -10, -1, 11 }, // key 5
            std::vector<int8_t>{ -6, 6, 15, 18 }, // key 6
            std::vector<int8_t>{ -11, -8, 1, 13 }, // key 7
            std::vector<int8_t>{ -13, -4, 8, 17 }, // key 8
            std::vector<int8_t>{ -9, 3, 15 }, // key 9
            std::vector<int8_t>{ -11, -2, 10, 19 }, // key 10
            std::vector<int8_t>{ -7, 5, 17 }, // key 11
        }
    },
    {
        37, 22, -15, 21,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -13, -12, 0, 12 }, // key 0
            std::vector<int8_t>{ -5, 7, 19, 20 }, // key 1
            std::vector<int8_t>{ -11, -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -8, 4, 16, 17 }, // key 4
            std::vector<int8_t>{ -14, -13, -1, 11 }, // key 5
            std::vector<int8_t>{ -6, 6, 18, 19 }, // key 6
            std::vector<int8_t>{ -12, -11, 1, 13 }, // key 7
            std::vector<int8_t>{ -4, 8, 20, 21 }, // key 8
            std::vector<int8_t>{ -10, -9, 3, 15 }, // key 9
            std::vector<int8_t>{ -15, -14, -2, 10 }, // key 10
            std::vector<int8_t>{ -7, 5, 17, 18 }, // key 11
        }
    },
    {
        39, 23, -16, 22,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -15, -12, 0, 12 }, // key 0
            std::vector<int8_t>{ -5, 7, 19, 22 }, // key 1
            std::vector<int8_t>{ -13, -10, 2, 14 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -11, -8, 4, 16, 19 }, // key 4
            std::vector<int8_t>{ -16, -13, -1, 11 }, // key 5
            std::vector<int8_t>{ -6, 6, 18, 21 }, // key 6
            std::vector<int8_t>{ -14, -11, 1, 13 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -12, -9, 3, 15, 18 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -7, 5, 17, 20 }, // key 11
        }
    },
    {
        40, 23, -17, 22,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -16, -12, 0, 12 }, // key 0
            std::vector<int8_t>{ -17, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -14, -10, 2, 14, 18 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -12, -8, 4, 16, 20 }, // key 4
            std::vector<int8_t>{ -17, -13, -1, 11 }, // key 5
            std::vector<int8_t>{ -6, 6, 18, 22 }, // key 6
            std::vector<int8_t>{ -15, -11, 1, 13 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -13, -9, 3, 15, 19 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -7, 5, 17, 21 }, // key 11
        }
    },
    {
        41, 24, -17, 23,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -17, -12, 0, 12 }, // key 0
            std::vector<int8_t>{ -17, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -15, -10, 2, 14, 19 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -13, -8, 4, 16, 21 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -6, 6, 18, 23 }, // key 6
            std::vector<int8_t>{ -16, -11, 1, 13 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -14, -9, 3, 15, 20 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -12, -7, 5, 17, 22 }, // key 11
        }
    },
    {
        42, 25, -18, 23,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -18, -12, 0, 12 }, // key 0
            std::vector<int8_t>{ -17, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -16, -10, 2, 14, 20 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -14, -8, 4, 16, 22 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -17, -11, 1, 13, 19 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -15, -9, 3, 15, 21 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -13, -7, 5, 17, 23 }, // key 11
        }
    },
    {
        43, 25, -18, 24,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, 0, 12, 24 }, // key 0
            std::vector<int8_t>{ -17, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -17, -10, 2, 14, 21 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -15, -8, 4, 16, 23 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -13, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -18, -11, 1, 13, 20 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -16, -9, 3, 15, 22 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -14, -7, 5, 17, 24 }, // key 11
        }
    },
    {
        45, 26, -19, 25,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, 0, 12, 21, 24 }, // key 0
            std::vector<int8_t>{ -17, -14, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -19, -10, 2, 14, 23 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -17, -8, 4, 16, 25 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -15, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -11, 1, 13, 22, 25 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -18, -9, 3, 15, 24 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -19, -16, -7, 5, 17 }, // key 11
        }
    },
    {
        46, 27, -20, 25,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, 0, 12, 22, 24 }, // key 0
            std::vector<int8_t>{ -17, -15, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -20, -10, 2, 14, 24 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -20, -18, -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 21, 23 }, // key 5
            std::vector<int8_t>{ -18, -16, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -11, 1, 13, 23, 25 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -19, -9, 3, 15, 25 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -19, -17, -7, 5, 17 }, // key 11
        }
    },
    {
        47, 27, -20, 26,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, 0, 12, 23, 24 }, // key 0
            std::vector<int8_t>{ -17, -16, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -10, 2, 14, 25, 26 }, // key 2
            std::vector<int8_t>{ -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -20, -19, -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 22, 23 }, // key 5
            std::vector<int8_t>{ -18, -17, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -11, 1, 13, 24, 25 }, // key 7
            std::vector<int8_t>{ -16, -15, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -20, -9, 3, 15, 26 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -19, -18, -7, 5, 17 }, // key 11
        }
    },
    {
        49, 29, -21, 27,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, 0, 12, 24, 25 }, // key 0
            std::vector<int8_t>{ -18, -17, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -10, 2, 14, 26, 27 }, // key 2
            std::vector<int8_t>{ -16, -15, -3, 9, 21 }, // key 3
            std::vector<int8_t>{ -21, -20, -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 23, 24 }, // key 5
            std::vector<int8_t>{ -19, -18, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -11, 1, 13, 25, 26 }, // key 7
            std::vector<int8_t>{ -17, -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -21, -9, 3, 15, 27 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22, 23 }, // key 10
            std::vector<int8_t>{ -20, -19, -7, 5, 17 }, // key 11
        }
    },
    {
        50, 29, -22, 27,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, 0, 12, 24, 26 }, // key 0
            std::vector<int8_t>{ -19, -17, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -22, -10, 2, 14, 26 }, // key 2
            std::vector<int8_t>{ -17, -15, -3, 9, 21, 23 }, // key 3
            std::vector<int8_t>{ -22, -20, -8, 4, 16 }, // key 4
            std::vector<int8_t>{ -13, -1, 11, 23, 25 }, // key 5
            std::vector<int8_t>{ -20, -18, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -11, 1, 13, 25, 27 }, // key 7
            std::vector<int8_t>{ -18, -16, -4, 8, 20 }, // key 8
            std::vector<int8_t>{ -21, -9, 3, 15, 27 }, // key 9
            std::vector<int8_t>{ -14, -2, 10, 22, 24 }, // key 10
            std::vector<int8_t>{ -21, -19, -7, 5, 17 }, // key 11
        }
    },
    {
        53, 31, -23, 29,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -12, 0, 12, 24, 29 }, // key 0
            std::vector<int8_t>{ -22, -17, -5, 7, 19 }, // key 1
            std::vector<int8_t>{ -22, -10, 2, 14, 26 }, // key 2
            std::vector<int8_t>{ -20, -15, -3, 9, 21, 26 }, // key 3
            std::vector<int8_t>{ -20, -8, 4, 16, 28 }, // key 4
            std::vector<int8_t>{ -18, -13, -1, 11, 23, 28 }, // key 5
            std::vector<int8_t>{ -23, -18, -6, 6, 18 }, // key 6
            std::vector<int8_t>{ -23, -11, 1, 13, 25 }, // key 7
            std::vector<int8_t>{ -21, -16, -4, 8, 20, 25 }, // key 8
            std::vector<int8_t>{ -21, -9, 3, 15, 27 }, // key 9
            std::vector<int8_t>{ -19, -14, -2, 10, 22, 27 }, // key 10
            std::vector<int8_t>{ -19, -7, 5, 17, 29 }, // key 11
        }
    },
    {
        55, 32, -24, 30,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -24, -19, -12, 0, 12, 24 }, // key 0
            std::vector<int8_t>{ -24, -17, -5, 7, 19, 26 }, // key 1
            std::vector<int8_t>{ -22, -10, 2, 14, 26 }, // key 2
            std::vector<int8_t>{ -22, -15, -3, 9, 21, 28 }, // key 3
            std::vector<int8_t>{ -20, -8, 4, 16, 28 }, // key 4
            std::vector<int8_t>{ -20, -13, -1, 11, 23, 30 }, // key 5
            std::vector<int8_t>{ -18, -6, 6, 18, 30 }, // key 6
            std::vector<int8_t>{ -23, -11, 1, 13, 25 }, // key 7
            std::vector<int8_t>{ -23, -16, -4, 8, 20, 27 }, // key 8
            std::vector<int8_t>{ -21, -9, 3, 15, 27 }, // key 9
            std::vector<int8_t>{ -21, -14, -2, 10, 22, 29 }, // key 10
            std::vector<int8_t>{ -19, -7, 5, 17, 29 }, // key 11
        }
    },
    {
        56, 33, -25, 30,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -24, -20, -12, 0, 12, 24 }, // key 0
            std::vector<int8_t>{ -25, -17, -5, 7, 19, 27 }, // key 1
            std::vector<int8_t>{ -22, -10, 2, 14, 26 }, // key 2
            std::vector<int8_t>{ -23, -15, -3, 9, 21, 29 }, // key 3
            std::vector<int8_t>{ -20, -8, 4, 16, 28 }, // key 4
            std::vector<int8_t>{ -25, -21, -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -6, 6, 18, 26, 30 }, // key 6
            std::vector<int8_t>{ -23, -11, 1, 13, 25 }, // key 7
            std::vector<int8_t>{ -24, -16, -4, 8, 20, 28 }, // key 8
            std::vector<int8_t>{ -21, -9, 3, 15, 27 }, // key 9
            std::vector<int8_t>{ -22, -14, -2, 10, 22, 30 }, // key 10
            std::vector<int8_t>{ -19, -7, 5, 17, 29 }, // key 11
        }
    },
    {
        59, 34, -26, 32,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -24, -23, -12, 0, 12, 24 }, // key 0
            std::vector<int8_t>{ -17, -5, 7, 19, 30, 31 }, // key 1
            std::vector<int8_t>{ -22, -21, -10, 2, 14, 26 }, // key 2
            std::vector<int8_t>{ -26, -15, -3, 9, 21, 32 }, // key 3
            std::vector<int8_t>{ -20, -8, 4, 16, 28 }, // key 4
            std::vector<int8_t>{ -25, -24, -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -6, 6, 18, 29, 30 }, // key 6
            std::vector<int8_t>{ -23, -22, -11, 1, 13, 25 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20, 31, 32 }, // key 8
            std::vector<int8_t>{ -21, -9, 3, 15, 27 }, // key 9
            std::vector<int8_t>{ -26, -25, -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -19, -7, 5, 17, 28, 29 }, // key 11
        }
    },
    {
        61, 36, -27, 33,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -25, -24, -12, 0, 12, 24 }, // key 0
            std::vector<int8_t>{ -17, -5, 7, 19, 31, 32 }, // key 1
            std::vector<int8_t>{ -23, -22, -10, 2, 14, 26 }, // key 2
            std::vector<int8_t>{ -27, -15, -3, 9, 21, 33 }, // key 3
            std::vector<int8_t>{ -20, -8, 4, 16, 28, 29 }, // key 4
            std::vector<int8_t>{ -26, -25, -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -6, 6, 18, 30, 31 }, // key 6
            std::vector<int8_t>{ -24, -23, -11, 1, 13, 25 }, // key 7
            std::vector<int8_t>{ -16, -4, 8, 20, 32, 33 }, // key 8
            std::vector<int8_t>{ -22, -21, -9, 3, 15, 27 }, // key 9
            std::vector<int8_t>{ -27, -26, -14, -2, 10, 22 }, // key 10
            std::vector<int8_t>{ -19, -7, 5, 17, 29, 30 }, // key 11
        }
    },
    {
        63, 37, -28, 34,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -27, -24, -12, 0, 12, 24 }, // key 0
            std::vector<int8_t>{ -17, -5, 7, 19, 31, 34 }, // key 1
            std::vector<int8_t>{ -25, -22, -10, 2, 14, 26 }, // key 2
            std::vector<int8_t>{ -27, -15, -3, 9, 21, 33 }, // key 3
            std::vector<int8_t>{ -23, -20, -8, 4, 16, 28, 31 }, // key 4
            std::vector<int8_t>{ -28, -25, -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -6, 6, 18, 30, 33 }, // key 6
            std::vector<int8_t>{ -26, -23, -11, 1, 13, 25 }, // key 7
            std::vector<int8_t>{ -28, -16, -4, 8, 20, 32 }, // key 8
            std::vector<int8_t>{ -24, -21, -9, 3, 15, 27, 30 }, // key 9
            std::vector<int8_t>{ -26, -14, -2, 10, 22, 34 }, // key 10
            std::vector<int8_t>{ -19, -7, 5, 17, 29, 32 }, // key 11
        }
    },
    {
        64, 37, -29, 34,//N, fifthStep, minValue, maxValue
        {// interpretations for each key, in order from C to B
            std::vector<int8_t>{ -28, -24, -12, 0, 12, 24 }, // key 0
            std::vector<int8_t>{ -29, -17, -5, 7, 19, 31 }, // key 1
            std::vector<int8_t>{ -26, -22, -10, 2, 14, 26, 30 }, // key 2
            std::vector<int8_t>{ -27, -15, -3, 9, 21, 33 }, // key 3
            std::vector<int8_t>{ -24, -20, -8, 4, 16, 28, 32 }, // key 4
            std::vector<int8_t>{ -29, -25, -13, -1, 11, 23 }, // key 5
            std::vector<int8_t>{ -18, -6, 6, 18, 30, 34 }, // key 6
            std::vector<int8_t>{ -27, -23, -11, 1, 13, 25 }, // key 7
            std::vector<int8_t>{ -28, -16, -4, 8, 20, 32 }, // key 8
            std::vector<int8_t>{ -25, -21, -9, 3, 15, 27, 31 }, // key 9
            std::vector<int8_t>{ -26, -14, -2, 10, 22, 34 }, // key 10
            std::vector<int8_t>{ -19, -7, 5, 17, 29, 33 }, // key 11
        }
    }
};
