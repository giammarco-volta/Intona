#pragma once

#include <cstdint>
#include <vector>
#include <list>


//----------
struct Chord
//----------
{
  enum EChordType : uint8_t
  {
    typeMajor = 0,    // Major
    typeMaj6,         // Major 6th
    typeMaj7,         // Major 7th
    typeMaj7f5,       // Major 7th flatted 5th
    typeSus4,         // Suspended 4th
    typeSus2,         // Suspended 2nd
    typeMaj7Sus4,     // Major 7th suspended 4th
    typeMinor,        // Minor
    typeMin6,         // Minor 6th
    typeMin7,         // Minor 7th
    typeMin7f5,       // Minor 7th flatted 5th (Half-diminished)
    typeMinMaj7,      // Minor major 7th
    typeDom7,         // Dominant 7th
    typeDom7f5,       // 7th flatted 5th
    typeDom7Sus4,     // 7th suspended 4th
    typeDim,          // Diminished
    typeDimMaj7,      // Diminished major 7th
    typeAug,          // Augmented
    typeAug7,         // Augmented 7th
    typeAugMaj7,      // Augmented major 7th
    typeMajNo3,       // Major w/o 3rd
    typeF5,           // Flatted 5th
    typeDim7,         // Diminished 7th
    typeAug6th,       // German augmented sixth

    NumOfChordTypes,
    typeNull = 0xFF     // Null chord
  };
  enum EChordTension9 : uint8_t
  {
    tensionVoid9 = 0,  // no tension 9
    tensionF9 = 1,  // Flatted 9th
    tensionN9 = 2,  // 9th
    tensionS9 = 3   // Sharped 9th
  };
  enum EChordTension11 : uint8_t
  {
    tensionVoid11 = 0,  // no tension 11    
    tensionN11 = 2,  // 11th  
    tensionS11 = 3   // Sharped 11th
  };
  enum EChordTension13 : uint8_t
  {
    tensionVoid13 = 0,  // no tension 13
    tensionF13 = 1,  // Flatted 13th
    tensionN13 = 2   // 13th
  };

  static void InitChordNames();

  bool IsNull() const { return type_ == typeNull; }
  void Reset() { type_ = typeNull; }

  void set(EChordType type, EChordTension9 t9, EChordTension11 t11, EChordTension13 t13)
  {
    type_ = type;
    t9_ = t9;
    t11_ = t11;
    t13_ = t13;
  }

  void SetTensions(EChordTension9 t9, EChordTension11 t11, EChordTension13 t13)
  {
    t9_ = t9;
    t11_ = t11;
    t13_ = t13;
  }

  std::list<int8_t> GetNotesIn5Cycles() const;

  const char* GetChordString() const
  {
    const uint8_t itype = static_cast<uint8_t>(type_);
    const uint8_t i9 = static_cast<uint8_t>(t9_);
    const uint8_t i11 = static_cast<uint8_t>(t11_);
    const uint8_t i13 = static_cast<uint8_t>(t13_);

    if (itype < 0 || itype >= Chord::NumOfChordTypes ||
      i9 < 0 || i9 >= 4 ||
      i11 < 0 || i11 >= 4 ||
      i13 < 0 || i13 >= 3)
    {
      return nullptr;
    }

    return sChordNames[itype][i9][i11][i13];
  }

  uint8_t root_ = 0;
  uint8_t bass_ = 0;
  EChordType type_ = typeNull;
  EChordTension9 t9_ = tensionVoid9;
  EChordTension11 t11_ = tensionVoid11;
  EChordTension13 t13_ = tensionVoid13;

  static const char* sChordNames
    [Chord::NumOfChordTypes]
    [4] // tension9:  0..3
    [4] // tension11: 0..3 (1 unused)
    [3]; // tension13: 0..2
};



constexpr uint16_t majorKeyMasks[12] =
{
  0b101010110101, // C major
  0b010101101011, // Db major
  0b101011010110, // D major
  0b010110101101, // Eb major
  0b101101011010, // E major
  0b011010110101, // F major
  0b110101101010, // F# major
  0b101011010101, // G major
  0b010110101011, // Ab major
  0b101101010110, // A major
  0b011010101101, // Bb major
  0b110101011010  // B major
};

constexpr uint16_t minorKeyMasks[12] =
{
  0b111010101101, // C minor
  0b110101011011, // Db minor
  0b101010110111, // D minor
  0b010101101111, // Eb minor
  0b101011011110, // E minor
  0b010110111101, // F minor
  0b101101111010, // F# minor
  0b011011110101, // G minor
  0b110111101010, // Ab minor
  0b101111010101, // A minor
  0b011110101011, // Bb minor
  0b111101010110  // B minor
};


constexpr uint16_t rotate12(uint16_t mask, int n)
{
  return static_cast<uint16_t>(((mask << n) | (mask >> (12 - n))) & 0x0FFF);
}
