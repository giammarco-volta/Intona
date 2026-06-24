#pragma once

#include <stdint.h>
#include <bitset>
#include <vector>
#include <utility>
#include <list>

#include "Chords.h"

//------------
class CKeysMap
//------------
{
public:
  CKeysMap();
  virtual ~CKeysMap() {}

  void  Reset();

  void onNoteOn(uint8_t key, uint8_t velocity, uint32_t timeMs);
  void onNoteOff(uint8_t key, uint8_t velocity, uint32_t timeMs);
  uint16_t  CalcLeftAndMap() const;
  bool  IsEmpty() const;

  uint8_t  GetLowestNote();
  uint8_t  GetHighestNote();
  uint8_t  GetPressedNote();
  uint8_t  GetKey(int i) { return noteList_[i].first; }
  uint32_t GetTime(int i) { return noteList_[i].second; }
  bool  ChordIsChanged();
  void  SetExcludedKey(int key);
  bool  NotesOutOfChord();

  void  SetLowerChordNote(int note) { lastLowerChordNote_ = note; }
  uint8_t   GetLowerChordNote() { return lastLowerChordNote_; }
  void  ClearChordStatus();
  void  SaveStatus();

  inline void MapKey(uint8_t byKey);

private:
  std::bitset<128> keysMap_;
  std::vector<std::pair<uint8_t, uint32_t>> noteList_;

  // for Chord status
  std::bitset<128> bckKeysMap_;   // used by Backup
  int lastLowerChordNote_;
  int excludedKey_;
};

//---------------------------
inline void CKeysMap::Reset()
//---------------------------
{
  keysMap_.reset();
  excludedKey_ = 0xffff;
}

//-----------------------------------------
inline void CKeysMap::MapKey(uint8_t byKey)
//-----------------------------------------
{
  keysMap_.set(byKey);
}

//---------------------------------------
inline uint8_t CKeysMap::GetPressedNote()
//---------------------------------------
{
  return noteList_.size();
}

//--------------------------------------
inline uint8_t CKeysMap::GetLowestNote()
//--------------------------------------
{
  if (noteList_.empty())
    return -1;

  return noteList_.front().first;
}

//---------------------------------------
inline uint8_t CKeysMap::GetHighestNote()
//---------------------------------------
{
  if (noteList_.empty())
    return -1;

  return noteList_.back().first;
}

//-----------------------------------
inline bool CKeysMap::IsEmpty() const
//-----------------------------------
{
  return !keysMap_.any();
}

//--------------------------------------------
inline void  CKeysMap::SetExcludedKey(int key)
//--------------------------------------------
{
  if (excludedKey_ > key)
    excludedKey_ = key;
}

//----------------
struct SChordVoice
//----------------
{
  Chord::EChordType basicChordType; // basic chord ID
  uint8_t           bassDegree;     // bass note degree of the voicing (0..11)
  uint16_t          voice;          // mandatory notes                     
  uint16_t          inhibitNote;    // inhibit notes
  uint16_t          availTension;   // available tension notes
  bool              forceBassInv;   // force bass inversion
};

//-------------------
class ChordRecognizer
//-------------------
{
public:
  void onNoteOn(uint8_t key, uint8_t velocity, uint32_t timeMs) { m_oKeysMap.onNoteOn(key, velocity, timeMs); }
  void onNoteOff(uint8_t key, uint8_t velocity, uint32_t timeMs) { m_oKeysMap.onNoteOff(key, velocity, timeMs); }
  Chord Recognize();

private:
  Chord RecognizeChord(uint16_t wChordNotes, uint8_t iLowestNoteNumber, const SChordVoice* ptChordVoices);
  Chord SlashChord(uint8_t iLNoteNum, uint8_t iHNoteNum);

private:
  CKeysMap m_oKeysMap;
};

