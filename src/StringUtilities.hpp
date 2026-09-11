#pragma once

#include <QString>
#include "tuning/NoteNaming.h"
#include "CentsUtilities.hpp"

//------------------------------------
static QString accidentalString(int n)
//------------------------------------
{
#if (0)
  if (n == 0)  return "";
  if (n == 1)  return "♯";
  if (n == 2)  return "𝄪";   // double sharp
  if (n == -1) return "♭";
  if (n == -2) return "♭♭";

  if (n > 0)
    return QString::number(n) + "♯";

  return QString::number(-n) + "♭";
#else
  if (n == 0)  return "";
  if (n == 1)  return "#";
  if (n == 2)  return "×";   // double sharp
  if (n == -1) return "b";
  if (n == -2) return "bb";

  if (n > 0)
    return QString::number(n) + "#";

  return QString::number(-n) + "b";
#endif
}

//------------------------------------------------
static QString noteNameFromFifths(int fifthsFromC)
//------------------------------------------------
{
  // Cycle of fifths:
  // 0=C, 1=G, 2=D, 3=A, 4=E, 5=B, 6=F
  static const std::array<QString, 7> names = { "C", "G", "D", "A", "E", "B", "F" };

  // Natural notes in fifth-space:
  // C=0, D=2, E=4, F=-1, G=1, A=3, B=5
  static const std::array<int, 7> naturalFifths = { 0, 1, 2, 3, 4, 5, -1 };

  const int idx = mod(fifthsFromC, 7);

  const QString baseName = names[idx];
  const int naturalValue = naturalFifths[idx];

  const int accidentals = (fifthsFromC - naturalValue) / 7;

  return baseName + accidentalString(accidentals);
}



// Preserve the symbolic fifth value; this conversion only affects its label.
static QString displayNoteName(
  int fifthsFromC,
  const NtetMapping& mapping,
  Intona::Tuning::NoteNamingMode mode,
  int tuningCenter = Config::invalid)
{
  if (mode == Intona::Tuning::NoteNamingMode::Fifths)
    return noteNameFromFifths(fifthsFromC);

  const bool isSelectedDegree = tuningCenter != Config::invalid
    && fifthsFromC >= tuningCenter - 5 && fifthsFromC <= tuningCenter + 6;
  const auto spelling = isSelectedDegree
    ? Intona::Tuning::relativeNoteSpelling(
        fifthsFromC, tuningCenter, mapping.N, mapping.fifthStep)
    : Intona::Tuning::limitedNoteSpelling(
        fifthsFromC, mapping.N, mapping.fifthStep);
  return noteNameFromFifths(spelling.fifths)
    + QString(std::abs(spelling.stepOffset),
              spelling.stepOffset < 0 ? QLatin1Char('-') : QLatin1Char('+'));
}
