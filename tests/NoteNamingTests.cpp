#include "StringUtilities.hpp"
#include "tuning/TuningController.h"
#include "tuning/TuningViewModel.h"
#include "midi/MidiController.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QElapsedTimer>
#include <QVariantMap>
#include <iostream>
#include <stdexcept>

using namespace Intona::Tuning;

void runKeyboardMappingTests(const QString& temporarySettingsFile);

static void require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

static void verifySpelling(int value, const NtetMapping& mapping)
{
  const auto result = limitedNoteSpelling(value, mapping.N, mapping.fifthStep);
  require(std::abs(accidentalCount(result.fifths)) <= 2, "Accidental limit");
  require(mod(result.fifths * mapping.fifthStep + result.stepOffset, mapping.N)
    == mod(value * mapping.fifthStep, mapping.N), "Pitch must be preserved");

  if (std::abs(accidentalCount(value)) <= 2)
  {
    require(result.fifths == value && result.stepOffset == 0, "Preserve allowed spelling");
    return;
  }

  // Independent oracle: enumerate letters/alterations and neighbouring octaves.
  const int naturals[] = {0, 2, 4, -1, 1, 3, 5};
  int bestDistance = mapping.N;
  int fewestAccidentals = 99;
  const int target = mod(value * mapping.fifthStep, mapping.N);
  for (int natural : naturals)
    for (int accidental = -2; accidental <= 2; ++accidental)
      for (int octave = -1; octave <= 1; ++octave)
      {
        const int pitch = mod((natural + 7 * accidental) * mapping.fifthStep, mapping.N)
          + octave * mapping.N;
        const int distance = std::abs(target - pitch);
        if (distance < bestDistance)
        {
          bestDistance = distance;
          fewestAccidentals = std::abs(accidental);
        }
        else if (distance == bestDistance)
          fewestAccidentals = std::min(fewestAccidentals, std::abs(accidental));
      }
  require(std::abs(result.stepOffset) == bestDistance, "Nearest anchor");
  require(std::abs(accidentalCount(result.fifths)) == fewestAccidentals, "Fewest accidentals at equal distance");
}

static void verifyRelativeNames(int center, const NtetMapping& mapping)
{
  // Independent diatonic oracle: transpose letter, semitone and octave in
  // conventional notation, then compare with the fifth-space implementation.
  const int naturals[] = {0, 2, 4, -1, 1, 3, 5}; // C D E F G A B
  const int semitones[] = {0, 2, 4, 5, 7, 9, 11};
  const int degrees[] = {0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6};
  const int intervals[] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};
  const auto root = limitedNoteSpelling(center, mapping.N, mapping.fifthStep);
  int rootLetter = 0;
  while (mod(naturals[rootLetter], 7) != mod(root.fifths, 7)) ++rootLetter;
  const int rootAccidentals = (root.fifths - naturals[rootLetter]) / 7;
  for (int interval = 0; interval < 12; ++interval)
  {
    const int degree = rootLetter + degrees[interval];
    const int letter = degree % 7;
    const int accidentals = semitones[rootLetter] + rootAccidentals + interval
      - semitones[letter] - 12 * (degree / 7);
    const int expectedFifths = naturals[letter] + 7 * accidentals;
    const int value = center + intervals[interval];
    const auto result = relativeNoteSpelling(value, center, mapping.N, mapping.fifthStep);
    require(result.fifths == expectedFifths, "Exact diatonic degree and interval quality");
    require(result.stepOffset == root.stepOffset, "Every degree inherits the center step modifier");
    require(mod(result.fifths * mapping.fifthStep + result.stepOffset, mapping.N)
      == mod(value * mapping.fifthStep, mapping.N), "Relative spelling preserves EDO pitch");
    const QString expected = noteNameFromFifths(expectedFifths)
      + QString(std::abs(root.stepOffset), root.stepOffset < 0 ? QLatin1Char('-') : QLatin1Char('+'));
    require(displayNoteName(value, mapping, NoteNamingMode::LimitedAccidentals, center)
      == expected, "Contextual formatted name");
    require(displayNoteName(value, mapping, NoteNamingMode::Fifths, center)
      == noteNameFromFifths(value), "Original fifth spelling remains unchanged");
  }
  for (const int value : {center - 6, center + 7})
    require(displayNoteName(value, mapping, NoteNamingMode::LimitedAccidentals, center)
      == displayNoteName(value, mapping, NoteNamingMode::LimitedAccidentals),
      "Unselected notes keep independent simplification");
}

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir settingsDir;
  require(settingsDir.isValid(), "Temporary settings directory");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
  QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir.path());
  QSettings probe(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  require(probe.fileName().startsWith(settingsDir.path()), "Settings must be isolated before tests run");

  try
  {
    int edo31 = -1, edo53 = -1;
    int checked = 0;
    for (int index = 0; index < int(kNtetMappings.size()); ++index)
    {
      const auto& mapping = kNtetMappings[index];
      if (mapping.N == 31)
        edo31 = index;
      if (mapping.N == 53)
        edo53 = index;
      for (int value = kConfigMaskMin; value <= kConfigMaskMax; ++value)
      {
        verifySpelling(value, mapping);
        require(displayNoteName(value, mapping, NoteNamingMode::Fifths)
          == noteNameFromFifths(value), "Legacy mode regression");
        ++checked;
      }
    }
    require(edo31 >= 0, "31-EDO available");
    require(edo53 >= 0, "53-EDO available");
    const auto& mapping31 = kNtetMappings[edo31];
    require(displayNoteName(12, mapping31, NoteNamingMode::LimitedAccidentals) == "B#", "31-EDO B#");
    require(displayNoteName(26, mapping31, NoteNamingMode::LimitedAccidentals) == "Db", "31-EDO triple sharp");
    require(displayNoteName(33, mapping31, NoteNamingMode::LimitedAccidentals) == "D", "31-EDO quadruple sharp");
    require(displayNoteName(-20, mapping31, NoteNamingMode::LimitedAccidentals) == "E#", "31-EDO triple flat");
    const auto tie = limitedNoteSpelling(26, 53, 31);
    require(tie.fifths == 14 && tie.stepOffset == 1, "53-EDO tie follows original fifth spelling");
    const auto below = limitedNoteSpelling(-22, 53, 31);
    require(below.stepOffset == -1, "Negative step modifier");
    require(displayNoteName(21, mapping31, NoteNamingMode::LimitedAccidentals, 17)
      == "C3#", "A double sharp requires C triple sharp as its major third");
    require(displayNoteName(30, kNtetMappings[edo53], NoteNamingMode::LimitedAccidentals, 26)
      == QString::fromUtf8("E×+"), "Respell all degrees relative to C double sharp plus");
    // One step below C across the octave boundary; also covers repeated signs.
    const auto boundary = limitedNoteSpelling(53, 1200, 701);
    require(boundary.stepOffset < 0, "Octave boundary direction");
    bool multipleSteps = false;
    for (int value = -34; value <= 40; ++value)
      multipleSteps |= std::abs(limitedNoteSpelling(value, 1200, 701).stepOffset) > 1;
    require(multipleSteps, "General EDO repeated modifiers");

    runKeyboardMappingTests(probe.fileName());

    MidiController midi;
    TuningController controller(&midi);
    require(controller.noteNamingMode() == 0, "Default mode");
    int notifications = 0;
    QObject::connect(&controller, &TuningController::tuningStateChanged,
      [&notifications]() { ++notifications; });
    controller.setNoteNamingMode(1);
    require(notifications == 1, "Mode change notifies");
    controller.setNoteNamingMode(1);
    controller.setNoteNamingMode(-1);
    controller.setNoteNamingMode(2);
    require(notifications == 1, "No-op and invalid mode do not notify");
    TuningController restored(&midi);
    require(restored.noteNamingMode() == 1, "Persisted mode restored");

    int centers = 0;
    for (int index = 0; index < int(kNtetMappings.size()); ++index)
    {
      controller.setEdoIndex(index);
      const auto& mapping = kNtetMappings[index];
      for (int center = mapping.minValue; center <= mapping.maxValue; ++center)
      {
        verifyRelativeNames(center, mapping);
        controller.selectTuningCenter(center);
        controller.setNoteNamingMode(0);
        const auto legacy = controller.uiSnapshot();
        controller.setNoteNamingMode(1);
        const auto limited = controller.uiSnapshot();
        require(limited.noteNamingMode == 1, "Snapshot mode");
        require(legacy.keyValues == limited.keyValues
          && legacy.tuningCenter == limited.tuningCenter
          && legacy.canRaiseKeys == limited.canRaiseKeys
          && legacy.canLowerKeys == limited.canLowerKeys
          && legacy.pressedKeys == limited.pressedKeys
          && legacy.currentPresetIndex == limited.currentPresetIndex,
          "Naming must not change tuning or interaction state");
        for (int key = 0; key < limited.keyValues.size(); ++key)
        {
          const int value = limited.keyValues[key].toInt();
          verifySpelling(value, mapping);
          require(limited.keyNames[key] == displayNoteName(value, mapping, NoteNamingMode::LimitedAccidentals, center), "Keyboard labels");
          require(legacy.keyNames[key] == noteNameFromFifths(value), "Legacy keyboard labels");
        }
        require(legacy.circleEntries.size() == limited.circleEntries.size(), "Circle entry count");
        for (int i = 0; i < limited.circleEntries.size(); ++i)
        {
          auto oldEntry = legacy.circleEntries[i].toMap();
          auto newEntry = limited.circleEntries[i].toMap();
          const int value = newEntry["value"].toInt();
          require(newEntry["name"].toString() == displayNoteName(value, mapping, NoteNamingMode::LimitedAccidentals, center), "Circle labels");
          const int key = newEntry["keyIndex"].toInt();
          if (newEntry["selected"].toBool())
            require(newEntry["name"].toString() == limited.keyNames[key], "Selected circle and keyboard names agree");
          oldEntry.remove("name");
          newEntry.remove("name");
          require(oldEntry == newEntry, "Circle identity, cents and selection preserved");
        }
        controller.setNoteNamingMode(0);
        require(controller.keyNames() == legacy.keyNames, "Switch back restores original spelling");
        ++centers;
      }
    }

    controller.setEdoIndex(edo53);
    controller.selectTuningCenter(kNtetMappings[edo53].maxValue);
    controller.captureCurrentPreset();
    const auto originalPreset = controller.presetEntries();
    controller.setNoteNamingMode(1);
    const auto limitedPreset = controller.presetEntries();
    require(originalPreset != limitedPreset, "Preset labels update");
    const auto originalValues = controller.keyValues();
    const auto originalNames = controller.keyNames();
    require(limitedPreset[0].toMap()["text"].toString() == originalNames.join("  "),
      "Preset labels match its selected center");
    controller.selectTuningCenter(0);
    require(controller.presetEntries() == limitedPreset, "Preset spelling uses its own center, not the active center");
    controller.applyPreset(0);
    require(controller.keyValues() == originalValues, "Preset pitches preserved");
    require(controller.keyNames() == originalNames, "Preset contextual names restored");
    controller.setNoteNamingMode(0);
    require(controller.presetEntries() == originalPreset, "Legacy preset labels restored");

    // Exercise the same queued setter / worker snapshot route used by QML.
    auto* worker = new TuningController(&midi);
    TuningViewModel model(worker);
    QThread thread;
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    thread.start();
    model.setNoteNamingMode(1);
    QElapsedTimer timer;
    timer.start();
    while (model.noteNamingMode() != 1 && timer.elapsed() < 3000)
    {
      QCoreApplication::processEvents();
      QThread::msleep(1);
    }
    const bool modelUpdated = model.noteNamingMode() == 1;
    thread.quit();
    thread.wait();
    require(modelUpdated, "Queued view model update");

    std::cout << "PASS: " << checked << " symbolic spellings, " << centers
      << " tuning centers with all 12 interval spellings, persistence, presets and queued QML view model.\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
