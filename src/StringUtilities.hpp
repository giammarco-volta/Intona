#pragma once

#include <QString>
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


