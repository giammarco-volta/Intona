#include "ChordRecognizer.h"
#include <algorithm>


const int8_t kInterval3rd[Chord::NumOfChordTypes] =
{
   4,     // typeMajor
   4,     // typeMaj6
   4,     // typeMaj7
   4,     // typeMaj7f5
  -1,     // typeSus4        !!! CAUTION !!!
  -1,     // typeSus2        !!! CAUTION !!!
  -1,     // typeMaj7Sus4    !!! CAUTION !!!
   3,     // typeMinor
   3,     // typeMin6
   3,     // typeMin7
   3,     // typeMin7f5
   3,     // typeMinMaj7
   4,     // typeDom7
   4,     // typeDom7f5
  -1,     // typeDom7Sus4    !!! CAUTION !!!
   3,     // typeDim
   3,     // typeDimMaj7
   4,     // typeAug
   4,     // typeAug7
   4,     // typeAugMaj7
  -1,     // typeMajNo3      !!! CAUTION !!!
   4,     // typeF5
   3,     // typeDim7
   4,     // typeAug6th
   4,     // typeFrench6th
};

//----------------------------------------------------------------------------------
// CKeysMap: constructor
//----------------------------------------------------------------------------------
CKeysMap::CKeysMap()
{
  Reset();
  ClearChordStatus();
}

//---------------------------------------------------------------------
void CKeysMap::onNoteOn(uint8_t key, uint8_t velocity, uint32_t timeMs)
//---------------------------------------------------------------------
{
  keysMap_.set(key);

  noteList_.push_back({ key, timeMs });

  std::sort(noteList_.begin(), noteList_.end(),
    [](const auto& a, const auto& b)
    {
      return a.first < b.first;
    });
}

//----------------------------------------------------------------------
void CKeysMap::onNoteOff(uint8_t key, uint8_t velocity, uint32_t timeMs)
//----------------------------------------------------------------------
{
  keysMap_.reset(key);

  noteList_.erase(
    std::remove_if(
      noteList_.begin(),
      noteList_.end(),
      [key](const auto& p)
      {
        return p.first == key;
      }),
    noteList_.end());

  if (GetPressedNote() == 0)
    ClearChordStatus();
}

//-------------------------------------------------------------------------
// CalcLeftAndMap
//----------------------------------------------------------------------------------
uint16_t CKeysMap::CalcLeftAndMap() const
{
  static constexpr int kNumKeys = 128;
  static constexpr int kOctave = 12;

  int lowestNote = -1;

  // Find lowest pressed key
  for (int note = 0; note < kNumKeys; ++note)
  {
    if (keysMap_.test(note))
    {
      lowestNote = note;
      break;
    }
  }

  if (lowestNote < 0)
    return static_cast<uint16_t>(-1); // No key pressed

  uint16_t result = 0;

  // Build one-octave map relative to the lowest note
  for (int note = lowestNote; note < kNumKeys; ++note)
  {
    if (keysMap_.test(note))
    {
      int relativePitchClass = (note - lowestNote) % kOctave;
      result |= static_cast<uint16_t>(1u << relativePitchClass);
    }
  }

  // Octave information: same key class exactly one octave above lowest note
  if (lowestNote + kOctave < kNumKeys)
  {
    if (keysMap_.test(lowestNote + kOctave))
      result |= 0x1000;
  }

  return result;
}

//----------------------------------------------------------------------------
bool CKeysMap::ChordIsChanged()
{
  if (excludedKey_ != 0xffff)
    for (int k = excludedKey_; k < 128; k++)
      if (bckKeysMap_[k >> 5])
        bckKeysMap_.reset(k);

  return bckKeysMap_ != keysMap_;
}

//----------------------------------------------------------------------------
void CKeysMap::ClearChordStatus()
{
  keysMap_ = bckKeysMap_;
  lastLowerChordNote_ = -1;
  excludedKey_ = 0xffff;
}

//----------------------------------------------------------------------------
void CKeysMap::SaveStatus()
{
  bckKeysMap_ = keysMap_;
}

//-----------------------------------------------------------------------------
bool CKeysMap::NotesOutOfChord()
{
  uint8_t numNotes = GetPressedNote();

  for (uint8_t i = 0; i < numNotes; i++)
    if (GetKey(i) < lastLowerChordNote_ + 16)
      return false;

  return true;
}

enum ETensionNotes : uint16_t
{
  f9  = 0x0002,	// Flatted 9th
  n9  = 0x0004,	// 9th
  s9  = 0x0008,	// Sharped 9th
  n11 = 0x0020,	// 11th
  s11 = 0x0040,	// Sharped 11th
  f13 = 0x0100,	// Flatted 13th
  n13 = 0x0200	// 13th
};

const SChordVoice voicesTable[] =
{
  //  byBasicChordID
  //  |                    byBassDegree
  //  |                    |         wVoice
  //  |                    |         |             wInihibitNote
  //  |                    |         |             |            wAvailTension
  //  |                    |         |             |            |                                bForceBassInv
  //  |                    |         |             |            |                                |
    { Chord::typeMin7,     3,        0x0881,       0x077e,     n9,                               false },  // L.1   (new minor 7/9 - RL)
    { Chord::typeDom7,    10,        0x0845,       0x07ba,     n13,                              false },  // L.2   (new Major 7/13 - RL)
    { Chord::typeMajor,    5,        0x0885,       0x077a,     0,                                true  },  // L.3   (new Major w/4th on Root)
    { Chord::typeMajor,    2,        0x0425,       0x0bda,     0,                                true  },  // L.4   (new Major w/2nd on Root)
    { Chord::typeMaj6,     4,        0x0421,       0x0bde,     n9,                               false },  // L.5   (new Major 6/9 - RL)
    { Chord::typeDom7,     4,        0x0441,       0x0bbe,     n9,                               false },  // L.6   (new Major 7/9 - RL)
    { Chord::typeMajor,   10,        0x0005,       0x0ffa,     0,                                true  },  // L.7   (new Major w/7th on Root)
    { Chord::typeDom7,    10,        0x0841,       0x07be,     n13,                              false },  // L.8   (new Major 7/13 - RL)
    { Chord::typeMaj7,     0,        0x0a91,       0x056e,     n13,                              false },  // L.9   (new Major 7/13)
    { Chord::typeDom7,     0,        0x0591,       0x0a6e,     f13,                              false },  // L.10  (new Dom 7/13b)
    { Chord::typeMin7,     2,        0x0023,       0x0fdc,     n9,                               false },  // L.11  (new min7/9 - RL - riv 1)
    { Chord::typeMin7,    10,        0x0031,       0x0fce,     n9,                               false },  // L.12  (new min7/9 - RL - riv 3)
    { Chord::typeMaj7,     0,        0x0891,       0x0200,     n9 + s11,                         false },  // L.13
    { Chord::typeMaj7,     4,        0x0189,       0x0020,     n9 + s11,                         false },  // L.14
    { Chord::typeMaj7,     7,        0x0231,       0x0506,     n9 + s11,                         false },  // L.15
    { Chord::typeMaj7,    11,        0x0123,       0x0400,     n9 + s11,                         false },  // L.16
    { Chord::typeMin6,     0,        0x0289,       0x0c00,     n9 + n11,                         false },  // L.17
    { Chord::typeDom7,     7,        0x0229,       0x0400,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.18
    { Chord::typeDom7,    10,        0x0245,       0x0510,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.19
    { Chord::typeMaj6,     0,        0x0281,       0x0c02,     n9 + s11,                         false },  // L.20  (edit omitted 3rd)
    { Chord::typeMaj6,     0,        0x0285,       0x0c00,     n9 + s11,                         false },  // L.21
    { Chord::typeMin7,     0,        0x0489,       0x0150,     n9 + n11 + n13,                   false },  // L.22  (edit added n13)
    { Chord::typeMin7,     7,        0x0129,       0x0000,     n9 + n11,                         false },  // L.23
    { Chord::typeMin7,    10,        0x0225,       0x0c00,     n9 + n11,                         false },  // L.24
    { Chord::typeMinMaj7,  0,        0x0889,       0x0020,     n9 + n13,                         false },  // L.25
    { Chord::typeMinMaj7,  3,        0x0311,       0x0000,     n9,                               false },  // L.26
    { Chord::typeMinMaj7,  7,        0x0131,       0x0000,     n9 + n13,                         false },  // L.27
    { Chord::typeMin7f5,   0,        0x0449,       0x0190,     n9 + n11 + n13,                   false },  // L.28
    { Chord::typeMin7f5,   6,        0x0251,       0x0406,     n9 + n11 + n13,                   false },  // L.29
    { Chord::typeMin7f5,  10,        0x0125,       0x0a40,     n9 + n11,                         false },  // L.30
    { Chord::typeMaj7f5,   0,        0x0841,       0x0188,     n9 + n13,                         false },  // L.31
    { Chord::typeDom7Sus4, 0,        0x04a1,       0x0018,     f9 + n9,                          false },  // L.32
    { Chord::typeSus4,     0,        0x00a1,       0x0a18,     f9 + n9,                          false },  // L.33
    { Chord::typeDom7Sus4, 7,        0x0429,       0x0300,     0,                                false },  // L.34
    { Chord::typeMaj7Sus4, 0,        0x08a1,       0x0010,     f9 + n9 + s9 + n13,               false },  // L.35
    { Chord::typeMaj7Sus4, 5,        0x00c5,       0x0418,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.36
    { Chord::typeMaj7Sus4, 7,        0x0431,       0x0080,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.37
    { Chord::typeDom7,     4,        0x0461,       0x0000,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.38
    { Chord::typeDom7f5,   0,        0x0451,       0x0180,     f9 + n9 + s9 + f13 + n13,         false },  // L.39
    { Chord::typeDom7f5,   4,        0x0145,       0x0018,     f9 + n9 + s9 + f13 + n13,         false },  // L.40
    { Chord::typeDom7,     0,        0x0613,       0x0000,     f9 + n9 + s9 + s11 + n13,         false },  // L.41
    { Chord::typeDom7,     0,        0x0651,       0x0000,     f9 + n9 + s9 + s11 + n13,         false },  // L.42
    { Chord::typeDom7,     0,        0x0411,       0x0100,     f9 + n9 + s9 + s11 + n13,         false },  // L.43
    { Chord::typeDom7,     4,        0x0141,       0x0012,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.44
    { Chord::typeAugMaj7,  0,        0x0911,       0x0020,     f9 + n9 + s9 + s11,               false },  // L.45
    { Chord::typeAugMaj7,  4,        0x0191,       0x0400,     f9 + n9 + s9 + s11,               false },  // L.46
    { Chord::typeAugMaj7, 11,        0x0223,       0x0400,     f9 + n9 + s9 + s11,               false },  // L.47
    { Chord::typeMaj7,     4,        0x0181,       0x0424,     n9 + s11,                         false },  // L.48
    { Chord::typeMaj7f5,   4,        0x0185,       0x0000,     0,                                false },  // L.49
    { Chord::typeMaj7,    11,        0x0023,       0x0400,     n9 + s11,                         false },  // L.50
    { Chord::typeMin7,     0,        0x0409,       0x0340,     n9 + n11,                         false },  // L.51
    { Chord::typeMin7,     3,        0x0281,       0x0466,     n9 + n11,                         false },  // L.52
    { Chord::typeMin7,    10,        0x0025,       0x0c48,     n9 + n11,                         false },  // L.53
    { Chord::typeMajor,    3,        0x0213,       0x0000,     0,                                false },  // L.54
    { Chord::typeMinMaj7,  0,        0x0809,       0x0360,     n9,                               false },  // L.55
    { Chord::typeMinMaj7,  3,        0x0301,       0x0068,     n9,                               false },  // L.56
    { Chord::typeMinMaj7, 11,        0x0013,       0x0480,     n9,                               false },  // L.57
    { Chord::typeMajor,    0,        0x0091,       0x0e00,     f9 + n9 + s9 + n11 + s11,         false },  // L.58
    { Chord::typeMajor,    4,        0x0109,       0x0050,     f9 + n9 + s9 + n11 + s11,         false },  // L.59
    { Chord::typeMajor,    7,        0x0221,       0x0c00,     f9 + n9 + s9 + n11 + s11,         false },  // L.60
    { Chord::typeAug7,     0,        0x0501,       0x0020,     f9 + n9 + s9 + s11,               false },  // L.61
    { Chord::typeAug7,     4,        0x0151,       0x0000,     s9,                               false },  // L.62
    { Chord::typeMajor,    4,        0x0101,       0x0efe,     0,                                true  },  // L.63  (new Major/3rd bass)
    { Chord::typeAug,      0,        0x0101,       0x0ca2,     n9 + s9 + s11,                    false },  // L.64
    { Chord::typeMinor,    0,        0x0089,       0x0040,     n9 + n11,                         false },  // L.65
    { Chord::typeMinor,    3,        0x0211,       0x0004,     n9 + n11,                         false },  // L.66
    { Chord::typeMinor,    7,        0x0121,       0x0802,     n9 + n11,                         false },  // L.67
    { Chord::typeMaj6,     0,        0x0211,       0x0c00,     n9 + s11,                         false },  // L.68
    { Chord::typeDom7Sus4, 0,        0x0421,       0x0058,     f9 + n9,                          false },  // L.69
    { Chord::typeSus2,     0,        0x0085,       0x0c40,     0,                                false },  // L.70
    { Chord::typeDimMaj7,  0,        0x0849,       0x0000,     n9 + n11 + f13,                   false },  // L.71
    { Chord::typeDim7,     0,        0x0249,       0x0db6,     0,                                false },  // L.72  (new dim7)
    { Chord::typeDim,      0,        0x0049,       0x0490,     f9 + n9,                          false },  // L.73
    { Chord::typeDim,      6,        0x0241,       0x0404,     f9 + n9,                          false },  // L.74
    { Chord::typeAug7,    10,        0x0445,       0x0bba,     0,                                false },  // L.75  (edit old was Dom7f5)
    { Chord::typeDom7,     0,        0x0645,       0x0000,     f9 + n9 + s9 + s11 + n13,         false },  // L.76
    { Chord::typeDom7,     0,        0x0445,       0x0000,     f9 + n9 + s9 + s11,               false },  // L.77
    { Chord::typeDom7,    10,        0x0045,       0x0808,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.78
    { Chord::typeDom7,     0,        0x0443,       0x0000,     f9 + n9 + s9 + s11 + n13,         false },  // L.79
    { Chord::typeDom7,     0,        0x0603,       0x0000,     f9,                               false },  // L.80
    { Chord::typeDom7,     0,        0x0401,       0x0008,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.81
    { Chord::typeDom7,     0,        0x04c9,       0x0000,     f9 + n9 + s9 + s11 + n13,         false },  // L.82
    { Chord::typeDim,      3,        0x0209,       0x0000,     f9 + n9,                          false },  // L.83
    { Chord::typeMaj7Sus4, 0,        0x0821,       0x0000,     n9 + n13,                         false },  // L.84
    { Chord::typeMaj7Sus4,11,        0x0043,       0x0000,     n9 + n13,                         false },  // L.85
    { Chord::typeAugMaj7,  0,        0x0901,       0x0000,     n9,                               false },  // L.86
    { Chord::typeNull,     0,        0x0007,       0x0ff8,     0,                                false },  // L.87  (new set typeNull by manual chord cancel)
    { Chord::typeMaj7,     0,        0x0801,       0x0000,     n9 + s11 + n13,                   false },  // L.88
    { Chord::typeDom7,    10,        0x0005,       0x0098,     f9 + n9 + s9 + s11 + f13 + n13,   false },  // L.89
    { Chord::typeMaj7,    11,        0x0003,       0x0000,     n9 + s11,                         false },  // L.90
    { Chord::typeMinor,    0,        0x0009,       0x0010,     n9 + n11,                         false },  // L.91
    { Chord::typeMinor,    3,        0x0201,       0x0dfe,     0,                                true  },  // L.92  (edit minor/3rd bass)
    { Chord::typeF5,       0,        0x0051,       0x0fae,     0,                                false },  // L.93  (new 5b)
    { Chord::typeMajor,    0,        0x0011,       0x0000,     f9 + n9 + s9 + n11 + s11,         false },  // L.94
    { Chord::typeMajNo3,   0,        0x0081,       0x0f7e,     0,                                false },  // L.95
    { Chord::typeMajor,    0,        0x0081,       0x0000,     n9 + n11 + s11,                   false },  // L.96
    { Chord::typeMajor,    7,        0x0021,       0x0fde,     0,                                true  },  // L.97  (edit Major/5th bass)
    { Chord::typeMajor,    0,        0x0001,       0x0000,     n9 + n11 + s11,                   false },  // L.99
      
    { Chord::typeNull,     0,        0x0000,       0x0000,     0,                                false },  //
};

const SChordVoice slashChordVoicesTable[] =
{
  //  byBasicChordID
  //  |                    byBassDegree
  //  |                    |  wVoice
  //  |                    |  |                  wInihibitNote
  //  |                    |  |                  |       wAvailTension
  //  |                    |  |                  |       |  bForceBassInv
  //  |                    |  |                  |       |  |
    { Chord::typeMajor,    0, 0x0091, (uint16_t)~0x0091, 0, false },  // L. 1
    { Chord::typeMajor,    4, 0x0109, (uint16_t)~0x0109, 0, false },  // L. 2
    { Chord::typeMajor,    7, 0x0221, (uint16_t)~0x0221, 0, false },  // L. 3
    { Chord::typeMaj6,     0, 0x0291, (uint16_t)~0x0291, 0, false },  // L. 4
    { Chord::typeSus2,     0, 0x0085, (uint16_t)~0x0085, 0, false },  // L. 7
    { Chord::typeMaj7Sus4, 0, 0x08a1, (uint16_t)~0x08a1, 0, false },  // L. 8
    { Chord::typeMaj7Sus4, 5, 0x00c5, (uint16_t)~0x00c5, 0, false },  // L. 9
    { Chord::typeMaj7Sus4, 7, 0x0431, (uint16_t)~0x0431, 0, false },  // L.10
    { Chord::typeMinor,    0, 0x0089, (uint16_t)~0x0089, 0, false },  // L.11
    { Chord::typeMinor,    3, 0x0211, (uint16_t)~0x0211, 0, false },  // L.12
    { Chord::typeMinor,    7, 0x0121, (uint16_t)~0x0121, 0, false },  // L.13
    { Chord::typeMin6,     0, 0x0289, (uint16_t)~0x0289, 0, false },  // L.14
    { Chord::typeMinMaj7,  0, 0x0889, (uint16_t)~0x0889, 0, false },  // L.15
    { Chord::typeMinMaj7,  3, 0x0311, (uint16_t)~0x0311, 0, false },  // L.16
    { Chord::typeMinMaj7,  7, 0x0131, (uint16_t)~0x0131, 0, false },  // L.17
    { Chord::typeMinMaj7, 11, 0x0113, (uint16_t)~0x0113, 0, false },  // L.18
    { Chord::typeMaj7,     0, 0x0891, (uint16_t)~0x0891, 0, false },  // L.19
    { Chord::typeMaj7,     4, 0x0189, (uint16_t)~0x0189, 0, false },  // L.20
    { Chord::typeMaj7,     7, 0x0231, (uint16_t)~0x0231, 0, false },  // L.21
    { Chord::typeMaj7,    11, 0x0123, (uint16_t)~0x0123, 0, false },  // L.22
    { Chord::typeMin7,     0, 0x0489, (uint16_t)~0x0489, 0, false },  // L.23
    { Chord::typeMin7,     7, 0x0129, (uint16_t)~0x0129, 0, false },  // L.24
    { Chord::typeMin7,    10, 0x0225, (uint16_t)~0x0225, 0, false },  // L.25
    { Chord::typeDom7,     0, 0x0491, (uint16_t)~0x0491, 0, false },  // L.26
    { Chord::typeDom7,     4, 0x0149, (uint16_t)~0x0149, 0, false },  // L.27
    { Chord::typeDom7,     7, 0x0229, (uint16_t)~0x0229, 0, false },  // L.28
    { Chord::typeDom7,    10, 0x0245, (uint16_t)~0x0245, 0, false },  // L.29
    { Chord::typeSus4,     0, 0x00a1, (uint16_t)~0x00a1, 0, false },  // L.30
    { Chord::typeMin7f5,   0, 0x0449, (uint16_t)~0x0449, 0, false },  // L.31
    { Chord::typeMin7f5,   6, 0x0251, (uint16_t)~0x0251, 0, false },  // L.32
    { Chord::typeMin7f5,  10, 0x0125, (uint16_t)~0x0125, 0, false },  // L.33
    { Chord::typeDom7f5,   0, 0x0451, (uint16_t)~0x0451, 0, false },  // L.34
    { Chord::typeDom7f5,   4, 0x0145, (uint16_t)~0x0145, 0, false },  // L.35
    { Chord::typeDom7Sus4, 0, 0x04a1, (uint16_t)~0x04a1, 0, false },  // L.36
    { Chord::typeDom7Sus4, 7, 0x0429, (uint16_t)~0x0429, 0, false },  // L.37
    { Chord::typeDim,      0, 0x0049, (uint16_t)~0x0049, 0, false },  // L.38
    { Chord::typeDim,      3, 0x0241, (uint16_t)~0x0241, 0, false },  // L.39
    { Chord::typeDim,      6, 0x0209, (uint16_t)~0x0209, 0, false },  // L.40
    { Chord::typeDimMaj7,  0, 0x0849, (uint16_t)~0x0849, 0, false },  // L.41
    { Chord::typeAug,      0, 0x0111, (uint16_t)~0x0111, 0, false },  // L.42
    { Chord::typeAug7,     0, 0x0511, (uint16_t)~0x0511, 0, false },  // L.43
    { Chord::typeAug7,     4, 0x0151, (uint16_t)~0x0151, 0, false },  // L.44
    { Chord::typeAug7,    10, 0x0445, (uint16_t)~0x0445, 0, false },  // L.45
    { Chord::typeAugMaj7,  0, 0x0911, (uint16_t)~0x0911, 0, false },  // L.46
    { Chord::typeAugMaj7,  4, 0x0191, (uint16_t)~0x0191, 0, false },  // L.47
    { Chord::typeAugMaj7, 11, 0x0223, (uint16_t)~0x0223, 0, false },  // L.48
    { Chord::typeF5,       0, 0x0051, (uint16_t)~0x0051, 0, false },  // L.49
    { Chord::typeDim7,     0, 0x0249, (uint16_t)~0x0249, 0, false },  // L.50
                                                            
    { Chord::typeNull,     0, 0x0000, (uint16_t)0x0000,  0, false }
};

//----------------------------------------------------------------------------------------------------------------------
Chord ChordRecognizer::RecognizeChord(uint16_t wChordNotes, uint8_t iLowestNoteNumber, const SChordVoice* ptChordVoices)
//----------------------------------------------------------------------------------------------------------------------
{
  if (wChordNotes == 0xFFFF)
  {
    return Chord(); // No Chord notes
  }

  Chord oNewChord; // Null chord
  bool bOctaveOnLowestNote = (wChordNotes & 0x1000) != 0;
  wChordNotes &= 0x0FFF; // Remove octave bit

  // Calculate chord type
  // to do: must be optimized (sequential search is too much expensive !!!

  const SChordVoice* ptChordVoice = ptChordVoices;

  for (; ptChordVoice->voice; ptChordVoice++)
  {
    // check if include inhibit note
    if (ptChordVoice->inhibitNote & wChordNotes)
      continue;
    // check if include all necessary voices
    if ((ptChordVoice->voice & wChordNotes) != ptChordVoice->voice)
      continue;
    oNewChord.type_ = ptChordVoice->basicChordType;
    break;
  }

  if (oNewChord.type_ != Chord::typeNull)
  {
    // Convert input chord bit into pattern of C key
    // NOTE: this operation is a ROL on 12 bits. After this operation the root note
    //       is placed in the least significant bit of 'wChordNotes'.
    uint32_t dwTemp = ((uint32_t)wChordNotes) << ptChordVoice->bassDegree;
    dwTemp |= dwTemp >> 12;
    wChordNotes = (uint16_t)(dwTemp & 0x0FFF);

    // Calculate tension
    Chord::EChordTension9  eTension9;
    Chord::EChordTension11 eTension11;
    Chord::EChordTension13 eTension13;
    wChordNotes &= ptChordVoice->availTension;

    // tension 13th
    if (wChordNotes & n13) wChordNotes &= ~f13;
    eTension13 = (Chord::EChordTension13)((wChordNotes >> 8) & 3);

    // tension 11th
    if (wChordNotes & s11)
    {
      wChordNotes &= ~n11;
      eTension11 = Chord::tensionS11;
    }
    else
      eTension11 = (wChordNotes & n11) ? Chord::tensionN11 : Chord::tensionVoid11;

    // tension 9th
    if (wChordNotes & s9)
    {
      wChordNotes &= ~(n9 + f9);
      eTension9 = Chord::tensionS9;
    }
    else
    {
      if (wChordNotes & n9) wChordNotes &= ~f9;
      eTension9 = (Chord::EChordTension9)((wChordNotes >> 1) & 3);
    }

    oNewChord.SetTensions(eTension9, eTension11, eTension13);

    const int16_t lowestPc = static_cast<int>(iLowestNoteNumber) % 12;

    // Bass = nota più bassa normalizzata
    uint8_t byBass = static_cast<uint8_t>(lowestPc);

    // Root = bass - inversion degree, modulo 12
    int16_t rootPc = lowestPc - static_cast<int16_t>(ptChordVoice->bassDegree);

    while (rootPc < 0)
      rootPc += 12;

    rootPc %= 12;

    oNewChord.root_ = static_cast<uint8_t>(rootPc);

    // Set bass if required, i.e. no bass inversion
    if (!ptChordVoice->forceBassInv && !bOctaveOnLowestNote)
      byBass = static_cast<uint8_t>(rootPc);

    oNewChord.bass_ = byBass;
  }

  return oNewChord;  // Return recognized chord by value
}

//---------------------------------------------------------------------
Chord ChordRecognizer::SlashChord(uint8_t iLNoteNum, uint8_t iHNoteNum)
//---------------------------------------------------------------------
{
  Chord tChordSlash;

  static constexpr uint8_t kiMaxSxNotes = 2;
  static constexpr uint8_t kiMinDxNotes = 3;
  static constexpr uint8_t kiMinLeftRightDist = 12;

  int iPressedNotes = m_oKeysMap.GetPressedNote();
  if (iLNoteNum > 0 && iLNoteNum <= kiMaxSxNotes && iHNoteNum >= kiMinDxNotes)
  {
    const uint8_t kBass = m_oKeysMap.GetKey(iLNoteNum - 1);
    const uint8_t kFirstDxNote = m_oKeysMap.GetKey(iLNoteNum);

    if (((iLNoteNum == 1) || (kBass - m_oKeysMap.GetKey(0) == 12)) && kFirstDxNote - kBass >= kiMinLeftRightDist)
    {
      // Recognized right hand chord in simplified mode.
      CKeysMap oKeysMap;
      oKeysMap.Reset();
      oKeysMap.MapKey(kFirstDxNote);

      for (uint8_t i = iLNoteNum + 1; i < iLNoteNum + iHNoteNum && i < iPressedNotes; i++)
        oKeysMap.MapKey(m_oKeysMap.GetKey(i));

      // get chord
      oKeysMap.SetLowerChordNote(kFirstDxNote);

      uint16_t wChordNotes = oKeysMap.CalcLeftAndMap();
      tChordSlash = RecognizeChord(wChordNotes, kFirstDxNote, slashChordVoicesTable);

      Chord::EChordType eType = tChordSlash.type_;
      if (eType != Chord::typeNull)
      {
        const uint8_t kRootBassInterval = (tChordSlash.root_ + 12 - (kBass % 12)) % 12;
        const int8_t kChordThird = kInterval3rd[tChordSlash.type_];
        if (((kChordThird == 4) &&  (kRootBassInterval == 3)                              )   //Minor third below a major chord
         || ((kChordThird == 3) && ((kRootBassInterval == 3)  || (kRootBassInterval == 4))))  //Minor or major third below a minor chord
          //Case of left note a third below the right hand chord:
          //it is a common case of chord comping, where the right hand plays the chord without the root,
          //which is played alone with the left hand.
          tChordSlash.Reset();
        else
          tChordSlash.bass_ = kBass % 12;
      }
    }
  }
  return tChordSlash;
}

//--------------------------------
Chord ChordRecognizer::Recognize()
//--------------------------------
{
  Chord& tLastChord = lastChord_;

  const int kiDeltaTime = 70;
  int iLnote, iHnote;

  static constexpr int P = 17;  // > 12
  static constexpr int Q = 10;  // < 12

  int i;
  int iCL = 0;  // concentration respect Lowest Note
  int iCH = 0;  // concentration respect Highest Note
  int iL = m_oKeysMap.GetLowestNote();;   // lowest note
  int iH = m_oKeysMap.GetHighestNote();   // highest note
  int iZ = (iH - iL) >> 1;                  // middle note: between 0 and (iH-iL)
  int iXLH;    // upper limit for L side
  int iXHL;    // lower limit for H side

  int iLNoteNum = 0;
  int iHNoteNum = 0;
  uint32_t iMediumTime = 0;

  // find range for L(lower) side (0,iXLH) and for H(higher) side (iXHL, iH-iL)
  uint8_t numNotes = m_oKeysMap.GetPressedNote();
  if (numNotes == 0)
    return Chord(); // null chord

  // find medium time of pressed notes
  for (i = 0; i < numNotes; i++)  // exclude highest Note
  {
    iMediumTime += m_oKeysMap.GetTime(i);
  }
  iMediumTime /= numNotes; // medium time of pressed notes

  if (iH - iL < 12)
  {
    iLnote = iL;
    iHnote = iH;
  }
  else
  {
    for (i = 0; i < numNotes; i++)
    {
      uint8_t iKey = m_oKeysMap.GetKey(i);
      iCL += iKey - iL;
      iCH += iH - iKey;
    }
    if (iCH > iCL)  // concentration on L side is higher
    {
      iXLH = std::max(iZ, P);
      iXHL = std::max(iZ, Q);
    }
    else            // concentration on H side is higher
    {
      iXLH = std::min(iZ, iH - iL - Q);
      iXHL = std::min(iZ, iH - iL - P);

      if (iXLH < 0)
        iXHL = 0;
      if (iXHL < 0)
        iXHL = 0;
    }

    // find number of notes in L and H range
    for (i = 0; i < numNotes; i++)  // exclude highest Note
    {
      uint8_t iKey = m_oKeysMap.GetKey(i);
      if (iKey < iXLH + iL)
        iLNoteNum++;
      if (iKey >= iXHL + iL)
        iHNoteNum++;
    }

    if (iLNoteNum < 3 && iHNoteNum < 3)
    {
      if (m_oKeysMap.NotesOutOfChord())
      {
        m_oKeysMap.ClearChordStatus();
      }
      else
      {
        return tLastChord;
      }
      return Chord(); // null chord
    }

    // find total range for chord recognize
    if (iLNoteNum >= 3)
      iLnote = iL;
    else
      iLnote = iL + iXHL;

    if (iHNoteNum >= 3)
      iHnote = iH;
    else
      iHnote = iL + iXLH;
  }

  uint32_t dwMediumTime = 0;
  uint8_t iInsNote = 0; // include note number
  uint8_t iExcNote = 0; // exclude note number

  if (m_oKeysMap.GetLowerChordNote() < 0)
    m_oKeysMap.SetLowerChordNote(iLnote);

  // calculate note range map
  m_oKeysMap.Reset();

  iL = 128;
  for (i = 0; i < numNotes; i++)
  {
    uint8_t iKey = m_oKeysMap.GetKey(i);

    uint32_t iNoteTime = m_oKeysMap.GetTime(i);
    if (iKey > m_oKeysMap.GetLowerChordNote() + 16)
      if (iNoteTime - iMediumTime > kiDeltaTime)
      {
        m_oKeysMap.SetExcludedKey(iLnote + 16);
        continue;
      }

    if (iKey >= iLnote && iKey <= iHnote)
    {
      if (iL > iKey)
        iL = iKey;
      m_oKeysMap.MapKey(iKey);

      iInsNote++;
    }
    else
      if (iL == 128)
        iExcNote++;
  }

  // include simultaneus note of lower side
  if (iExcNote)
  {
    for (i = 0; i < iExcNote; i++)
    {
      uint8_t iKey = m_oKeysMap.GetKey(i);
      if (iL > iKey)
        iL = iKey;
      m_oKeysMap.MapKey(iKey);
    }
  }

  bool bChordIsChanged = m_oKeysMap.ChordIsChanged();

  m_oKeysMap.SaveStatus();

  if (bChordIsChanged)
  {
    Chord tChord = SlashChord(iLNoteNum, iHNoteNum);
    if (!tChord.IsNull())
    {
      tLastChord = tChord;
    }
    else
    {
      m_oKeysMap.SetLowerChordNote(iLnote);

      uint16_t wChordNotes = m_oKeysMap.CalcLeftAndMap();

      tLastChord = RecognizeChord(wChordNotes, iL, voicesTable);
    }
  }

  return tLastChord;
}
