#include "Chords.h"
#include <array>
#include <utility>

// Generated from sm_aCompleteTypes: ASCII chord suffix table
const char* Chord::sChordNames
[Chord::NumOfChordTypes]
[4] // tension9:  0..3
[4] // tension11: 0..3 (1 unused)
[3] // tension13: 0..2
= {};

void Chord::InitChordNames()
{
  // typeMajor
  sChordNames[Chord::typeMajor][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "";
  sChordNames[Chord::typeMajor][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "(add9)";
  sChordNames[Chord::typeMajor][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionVoid13] = "(add#9)";
  sChordNames[Chord::typeMajor][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "(addb9)";
  sChordNames[Chord::typeMajor][Chord::tensionS9][Chord::tensionN11][Chord::tensionVoid13] = "(add#9/11)";
  sChordNames[Chord::typeMajor][Chord::tensionF9][Chord::tensionS11][Chord::tensionVoid13] = "(addb9/#11)";
  sChordNames[Chord::typeMajor][Chord::tensionN9][Chord::tensionS11][Chord::tensionVoid13] = "(add9/#11)";
  sChordNames[Chord::typeMajor][Chord::tensionS9][Chord::tensionS11][Chord::tensionVoid13] = "(add#9/#11)";
  sChordNames[Chord::typeMajor][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionVoid13] = "(add11)";
  sChordNames[Chord::typeMajor][Chord::tensionVoid9][Chord::tensionS11][Chord::tensionVoid13] = "(add#11)";
  // typeMaj6
  sChordNames[Chord::typeMaj6][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "6";
  sChordNames[Chord::typeMaj6][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "6/9";
  sChordNames[Chord::typeMaj6][Chord::tensionN9][Chord::tensionS11][Chord::tensionVoid13] = "6/9(#11)";
  sChordNames[Chord::typeMaj6][Chord::tensionVoid9][Chord::tensionS11][Chord::tensionVoid13] = "6(#11)";
  // typeMaj7
  sChordNames[Chord::typeMaj7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7";
  sChordNames[Chord::typeMaj7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "M9";
  sChordNames[Chord::typeMaj7][Chord::tensionN9][Chord::tensionS11][Chord::tensionVoid13] = "M9(#11)";
  sChordNames[Chord::typeMaj7][Chord::tensionVoid9][Chord::tensionS11][Chord::tensionVoid13] = "M7(#11)";
  sChordNames[Chord::typeMaj7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "M13";
  sChordNames[Chord::typeMaj7][Chord::tensionN9][Chord::tensionS11][Chord::tensionN13] = "M13(#11)";
  // typeMaj7f5
  sChordNames[Chord::typeMaj7f5][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7(b5)";
  sChordNames[Chord::typeMaj7f5][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "M9(b5)";
  sChordNames[Chord::typeMaj7f5][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "M13(b5)";
  // typeSus4
  sChordNames[Chord::typeSus4][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "sus";
  sChordNames[Chord::typeSus4][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "sus(add9)";
  sChordNames[Chord::typeSus4][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "sus(addb9)";
  // typeSus2
  sChordNames[Chord::typeSus2][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "sus2";
  // typeMaj7Sus4
  sChordNames[Chord::typeMaj7Sus4][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7sus";
  sChordNames[Chord::typeMaj7Sus4][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "M9sus";
  sChordNames[Chord::typeMaj7Sus4][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7sus(#9)";
  sChordNames[Chord::typeMaj7Sus4][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7sus(b9)";
  sChordNames[Chord::typeMaj7Sus4][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionN13] = "M7sus(add13)";
  sChordNames[Chord::typeMaj7Sus4][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "M13sus";
  sChordNames[Chord::typeMaj7Sus4][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionN13] = "M13sus(b9)";
  // typeMinor
  sChordNames[Chord::typeMinor][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "m";
  sChordNames[Chord::typeMinor][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "m(add9)";
  sChordNames[Chord::typeMinor][Chord::tensionN9][Chord::tensionN11][Chord::tensionVoid13] = "m(add9/11)";
  sChordNames[Chord::typeMinor][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionVoid13] = "m(add11)";
  // typeMin6
  sChordNames[Chord::typeMin6][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "m6";
  sChordNames[Chord::typeMin6][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "m6/9";
  sChordNames[Chord::typeMin6][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionVoid13] = "m6(add11)";
  sChordNames[Chord::typeMin6][Chord::tensionN9][Chord::tensionN11][Chord::tensionVoid13] = "m6/9(add11)";
  // typeMin7
  sChordNames[Chord::typeMin7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "m7";
  sChordNames[Chord::typeMin7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "m9";
  sChordNames[Chord::typeMin7][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionVoid13] = "m7(add11)";
  sChordNames[Chord::typeMin7][Chord::tensionN9][Chord::tensionN11][Chord::tensionVoid13] = "m11";
  sChordNames[Chord::typeMin7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionN13] = "m7(add13)";
  sChordNames[Chord::typeMin7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "m9(add13)";
  sChordNames[Chord::typeMin7][Chord::tensionN9][Chord::tensionN11][Chord::tensionN13] = "m13";
  sChordNames[Chord::typeMin7][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionN13] = "m7(add11/13)";
  // typeMin7f5
  sChordNames[Chord::typeMin7f5][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "m7(b5)";
  sChordNames[Chord::typeMin7f5][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "m9(b5)";
  sChordNames[Chord::typeMin7f5][Chord::tensionN9][Chord::tensionN11][Chord::tensionVoid13] = "m11(b5)";
  sChordNames[Chord::typeMin7f5][Chord::tensionN9][Chord::tensionN11][Chord::tensionN13] = "m13(b5)";
  sChordNames[Chord::typeMin7f5][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "m9(b5/13)";
  sChordNames[Chord::typeMin7f5][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionN13] = "m7(b5/11/13)";
  sChordNames[Chord::typeMin7f5][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionVoid13] = "m7(b5/11)";
  sChordNames[Chord::typeMin7f5][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionN13] = "m7(b5/13)";
  // typeMinMaj7
  sChordNames[Chord::typeMinMaj7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "m(M7)";
  sChordNames[Chord::typeMinMaj7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "m9(M7)";
  sChordNames[Chord::typeMinMaj7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionN13] = "mM7(add13)";
  sChordNames[Chord::typeMinMaj7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "m6/9(M7)";
  // typeDom7
  sChordNames[Chord::typeDom7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "7";
  sChordNames[Chord::typeDom7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "9";
  sChordNames[Chord::typeDom7][Chord::tensionN9][Chord::tensionS11][Chord::tensionVoid13] = "9(#11)";
  sChordNames[Chord::typeDom7][Chord::tensionS9][Chord::tensionS11][Chord::tensionVoid13] = "7(#9/#11)";
  sChordNames[Chord::typeDom7][Chord::tensionVoid9][Chord::tensionS11][Chord::tensionVoid13] = "7(#11)";
  sChordNames[Chord::typeDom7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "13";
  sChordNames[Chord::typeDom7][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionN13] = "13(#9)";
  sChordNames[Chord::typeDom7][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(b9)";
  sChordNames[Chord::typeDom7][Chord::tensionF9][Chord::tensionS11][Chord::tensionVoid13] = "7(b9/#11)";
  sChordNames[Chord::typeDom7][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(#9)";
  sChordNames[Chord::typeDom7][Chord::tensionN9][Chord::tensionS11][Chord::tensionN13] = "13(#11)";
  sChordNames[Chord::typeDom7][Chord::tensionVoid9][Chord::tensionS11][Chord::tensionN13] = "7(#11/13)";
  sChordNames[Chord::typeDom7][Chord::tensionS9][Chord::tensionS11][Chord::tensionN13] = "13(#9/#11)";
  sChordNames[Chord::typeDom7][Chord::tensionF9][Chord::tensionS11][Chord::tensionN13] = "13(b9/#11)";
  sChordNames[Chord::typeDom7][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionN13] = "13(b9)";
  sChordNames[Chord::typeDom7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionN13] = "7(add13)";
  sChordNames[Chord::typeDom7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionF13] = "7(addb13)";
  // typeDom7f5
  sChordNames[Chord::typeDom7f5][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(b5)";
  sChordNames[Chord::typeDom7f5][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionN13] = "7(b5/13)";
  sChordNames[Chord::typeDom7f5][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "9(b5)";
  sChordNames[Chord::typeDom7f5][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(b5/b9)";
  sChordNames[Chord::typeDom7f5][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(b5/#9)";
  sChordNames[Chord::typeDom7f5][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionN13] = "13(b5)";
  sChordNames[Chord::typeDom7f5][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionN13] = "13(b5/b9)";
  sChordNames[Chord::typeDom7f5][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionN13] = "13(b5/#9)";
  // typeDom7Sus4
  sChordNames[Chord::typeDom7Sus4][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "7sus";
  sChordNames[Chord::typeDom7Sus4][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "9sus";
  sChordNames[Chord::typeDom7Sus4][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "7sus(b9)";
  // typeDim
  sChordNames[Chord::typeDim][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "dim";
  sChordNames[Chord::typeDim][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "dim(add9)";
  sChordNames[Chord::typeDim][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "dim(addb9)";
  // typeDim7
  sChordNames[Chord::typeDim7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "dim7";
  // typeDimMaj7
  sChordNames[Chord::typeDimMaj7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "dim(M7)";
  sChordNames[Chord::typeDimMaj7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "dim(M7/9)";
  sChordNames[Chord::typeDimMaj7][Chord::tensionN9][Chord::tensionN11][Chord::tensionVoid13] = "dim(M7/9/11)";
  sChordNames[Chord::typeDimMaj7][Chord::tensionVoid9][Chord::tensionN11][Chord::tensionVoid13] = "dim(M7/11)";
  // typeAug
  sChordNames[Chord::typeAug][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "(#5)";
  sChordNames[Chord::typeAug][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "#5(add9)";
  sChordNames[Chord::typeAug][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionVoid13] = "#5(add#9)";
  // typeAug7
  sChordNames[Chord::typeAug7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(#5)";
  sChordNames[Chord::typeAug7][Chord::tensionVoid9][Chord::tensionS11][Chord::tensionVoid13] = "7(#5/#11)";
  sChordNames[Chord::typeAug7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "9(#5)";
  sChordNames[Chord::typeAug7][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(#5/#9)";
  sChordNames[Chord::typeAug7][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "7(#5/b9)";
  sChordNames[Chord::typeAug7][Chord::tensionF9][Chord::tensionS11][Chord::tensionVoid13] = "7alt(b9)";
  sChordNames[Chord::typeAug7][Chord::tensionS9][Chord::tensionS11][Chord::tensionVoid13] = "7alt(#9)";
  sChordNames[Chord::typeAug7][Chord::tensionN9][Chord::tensionS11][Chord::tensionVoid13] = "9(#5/#11)";
  // typeAugMaj7
  sChordNames[Chord::typeAugMaj7][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7(#5)";
  sChordNames[Chord::typeAugMaj7][Chord::tensionN9][Chord::tensionVoid11][Chord::tensionVoid13] = "M9(#5)";
  sChordNames[Chord::typeAugMaj7][Chord::tensionF9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7(#5/b9)";
  sChordNames[Chord::typeAugMaj7][Chord::tensionS9][Chord::tensionVoid11][Chord::tensionVoid13] = "M7(#5/#9)";
  sChordNames[Chord::typeAugMaj7][Chord::tensionVoid9][Chord::tensionS11][Chord::tensionVoid13] = "M7(#5/#11)";
  sChordNames[Chord::typeAugMaj7][Chord::tensionN9][Chord::tensionS11][Chord::tensionVoid13] = "M9(#11)";
  sChordNames[Chord::typeAugMaj7][Chord::tensionS9][Chord::tensionS11][Chord::tensionVoid13] = "M7alt(#9)";
  sChordNames[Chord::typeAugMaj7][Chord::tensionF9][Chord::tensionS11][Chord::tensionVoid13] = "M7alt(b9)";
  // typeMajNo3
  sChordNames[Chord::typeMajNo3][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "(1+5)";
  // typeF5
  sChordNames[Chord::typeF5][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "(b5)";
  // typeAug6th
  sChordNames[Chord::typeAug6th][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "(#6)";
  sChordNames[Chord::typeFrench6th][Chord::tensionVoid9][Chord::tensionVoid11][Chord::tensionVoid13] = "Fr(#6)";
}

static constexpr int8_t unison( 0);
static constexpr int8_t min2nd(-5);
static constexpr int8_t maj2nd( 2);
static constexpr int8_t aug2nd( 9);
static constexpr int8_t min3rd(-3);
static constexpr int8_t maj3rd( 4);
static constexpr int8_t prf4th(-1);
static constexpr int8_t aug4th( 6);
static constexpr int8_t dim5th(-6);
static constexpr int8_t prf5th( 1);
static constexpr int8_t aug5th( 8);
static constexpr int8_t min6th(-4);
static constexpr int8_t maj6th( 3);
static constexpr int8_t dim7th(-9);
static constexpr int8_t aug6th(10);
static constexpr int8_t min7th(-2);
static constexpr int8_t maj7th( 5);

//--------------------------------------------------------------------------------
const std::array<std::list<int8_t>, Chord::NumOfChordTypes> kChordNotesIn5Cycles =
//--------------------------------------------------------------------------------
{
  std::list<int8_t>{ maj3rd, prf5th         },//typeMajor = 0,    Major
  std::list<int8_t>{ maj3rd, prf5th, maj6th },//typeMaj6,         Major 6th
  std::list<int8_t>{ maj3rd, prf5th, maj7th },//typeMaj7,         Major 7th
  std::list<int8_t>{ maj3rd, dim5th, maj7th },//typeMaj7f5,       Major 7th flatted 5th
  std::list<int8_t>{ prf4th, prf5th         },//typeSus4,         Suspended 4th
  std::list<int8_t>{ maj2nd, prf5th         },//typeSus2,         Suspended 2nd
  std::list<int8_t>{ prf4th, prf5th, maj7th },//typeMaj7Sus4,     Major 7th suspended 4th
  std::list<int8_t>{ min3rd, prf5th         },//typeMinor,        Minor
  std::list<int8_t>{ min3rd, prf5th, maj6th },//typeMin6,         Minor 6th
  std::list<int8_t>{ min3rd, prf5th, min7th },//typeMin7,         Minor 7th
  std::list<int8_t>{ min3rd, dim5th, min7th },//typeMin7f5,       Minor 7th flatted 5th (Half-diminished)
  std::list<int8_t>{ min3rd, prf5th, maj7th },//typeMinMaj7,      Minor major 7th
  std::list<int8_t>{ maj3rd, prf5th, min7th },//typeDom7,         Dominant 7th
  std::list<int8_t>{ maj3rd, dim5th, min7th },//typeDom7f5,       7th flatted 5th
  std::list<int8_t>{ prf4th, prf5th, min7th },//typeDom7Sus4,     7th suspended 4th
  std::list<int8_t>{ min3rd, dim5th         },//typeDim,          Diminished
  std::list<int8_t>{ min3rd, dim5th, maj7th },//typeDimMaj7,      Diminished major 7th
  std::list<int8_t>{ maj3rd, aug5th         },//typeAug,          Augmented
  std::list<int8_t>{ maj3rd, aug5th, min7th },//typeAug7,         Augmented 7th
  std::list<int8_t>{ maj3rd, aug5th, maj7th },//typeAugMaj7,      Augmented major 7th
  std::list<int8_t>{ prf5th                 },//typeMajNo3,       Major w/o 3rd
  std::list<int8_t>{ maj3rd, dim5th         },//typeF5,           Flatted 5th
  std::list<int8_t>{ min3rd, dim5th, dim7th },//typeDim7,         Diminished 7th
  std::list<int8_t>{ maj3rd, prf5th, aug6th },//typeAug6th,       German augmented sixth
  std::list<int8_t>{ maj3rd, aug4th, aug6th },//typeFrench6th,    French augmented sixth
};

const std::array<int8_t, 4> kTens9th{ unison, min2nd, maj2nd, aug2nd };
const std::array<int8_t, 4> kTens11th{ unison, unison, prf4th, aug4th };//pos 1 never used
const std::array<int8_t, 3> kTens13th{ unison, min6th, maj6th };

//------------------------------------------------
std::list<int8_t> Chord::GetNotesIn5Cycles() const
//------------------------------------------------
{
  if (type_ == typeNull)
    return std::list<int8_t>();

  std::list<int8_t> ret = kChordNotesIn5Cycles[type_];

  if (t9_ != tensionVoid9)
    ret.push_back(kTens9th[t9_]);

  if (t11_ != tensionVoid11)
    ret.push_back(kTens11th[t11_]);

  if (t13_ != tensionVoid13)
    ret.push_back(kTens13th[t13_]);

  return ret;
}
