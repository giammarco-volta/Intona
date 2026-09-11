#include "Config.hpp"
#include "CentsUtilities.hpp"
#include "Chords.h"
#include "TuningCenterFinder.h"
#include "tuning/TuningAlgorithms.h"
#include "tuning/TuningController.h"
#include "midi/MidiController.h"

#include <QSettings>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <tuple>

using namespace Intona::Tuning;

namespace
{
void check(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

double distanceToKey(int value, int key, const NtetMapping& mapping)
{
  const double cents = 1200.0 * mod(value * mapping.fifthStep, mapping.N) / mapping.N;
  return std::abs(std::remainder(cents - 100.0 * key, 1200.0));
}

auto score(const std::array<int8_t, 12>& values, const NtetMapping& mapping)
{
  double total = 0, worst = 0;
  for (int key = 0; key < 12; ++key)
  {
    const double error = distanceToKey(values[key], key, mapping);
    total += error;
    worst = std::max(worst, error);
  }
  return std::make_tuple(total, worst, distanceToKey(values[0], 0, mapping));
}

void verifyConfiguration(const Config& config, const NtetMapping& mapping)
{
  auto selected = config.valueForKey;
  std::sort(selected.begin(), selected.end());
  for (int i = 0; i < 12; ++i)
    check(selected[i] == config.tuningCenter - 5 + i, "Selected 12 notes changed");
  check(config.mask == makeMaskFromValues(config.valueForKey), "Symbolic mask changed");

  // Independent oracle: all octave rotations in floating-point cents.
  std::sort(selected.begin(), selected.end(), [&](int a, int b)
  {
    return mod(a * mapping.fifthStep, mapping.N) < mod(b * mapping.fifthStep, mapping.N);
  });
  double minimum = 1e9, minimumWorst = 1e9, minimumC = 1e9;
  bool ordered = false;
  for (int start = 0; start < 12; ++start)
  {
    std::array<int8_t, 12> assignment;
    for (int key = 0; key < 12; ++key) assignment[key] = selected[(start + key) % 12];
    ordered |= assignment == config.valueForKey;
    const auto [total, worst, c] = score(assignment, mapping);
    if (total < minimum - 1e-7)
    {
      minimum = total; minimumWorst = worst; minimumC = c;
    }
    else if (std::abs(total - minimum) < 1e-7)
    {
      if (worst < minimumWorst - 1e-7) { minimumWorst = worst; minimumC = c; }
      else if (std::abs(worst - minimumWorst) < 1e-7) minimumC = std::min(minimumC, c);
    }
  }
  const auto [actual, worst, c] = score(config.valueForKey, mapping);
  check(ordered, "Keyboard must preserve cyclic pitch order");
  check(std::abs(actual - minimum) < 1e-7, "Total deviation is not minimal");
  check(std::abs(worst - minimumWorst) < 1e-7, "Worst-deviation tie break");
  check(std::abs(c - minimumC) < 1e-7, "C-distance tie break");
  const auto& legacy = configPool[config.tuningCenter - kConfigPoolMin];
  check(actual <= std::get<0>(score(legacy.valueForKey, mapping)) + 1e-7, "Regression against old mapping");
  check(closestKeyboardMapping(config.valueForKey, mapping.N, mapping.fifthStep) == config.valueForKey,
    "Mapping must be independent of previous assignment");
  const auto offset = findGlobalOffsetCents(config, mapping, 0.0);
  check(offset.has_value(), "MIDI tuning range must be feasible");
  const auto detunes = computeDetuneTable(mapping.N, mapping.fifthStep, config, *offset);
  const auto mts = computeMtsTable(mapping.N, mapping.fifthStep, config, *offset);
  for (int key = 0; key < 12; ++key)
  {
    const double intended = 1200.0 * mod(config.valueForKey[key] * mapping.fifthStep, mapping.N) / mapping.N;
    check(std::abs(std::remainder(100.0 * key + detunes[key] + *offset - intended, 1200.0)) < 1e-7,
      "Offset compensation must preserve target pitch");
    const double decoded = (double(mts[key]) / 8192.0 - 1.0) * 100.0;
    check(std::abs(decoded - detunes[key]) <= 100.0 / 16384.0 + 1e-7, "MTS encoding round trip");
  }
}
} // namespace

void runKeyboardMappingTests(const QString& temporarySettingsFile)
{
  int centers = 0, changed = 0, chordChecks = 0, index31 = -1;
  for (int index = 0; index < int(kNtetMappings.size()); ++index)
  {
    const auto& mapping = kNtetMappings[index];
    if (mapping.N == 31) index31 = index;
    for (int center = mapping.minValue; center <= mapping.maxValue; ++center)
    {
      const Config& config = mapping.getConfig(static_cast<int8_t>(center));
      verifyConfiguration(config, mapping);
      check(&config == &configForTuningCenter(mapping, static_cast<int8_t>(center)), "Manual selection must use optimal mapping");
      check(findConfigByValues(mapping, config.valueForKey) == &config, "Config lookup after reordering");
      ++centers;
      changed += config.valueForKey != configPool[center - kConfigPoolMin].valueForKey;
      uint16_t dominantMask = 0;
      for (int key = 0; key < 12; ++key)
      {
        const int value = config.valueForKey[key];
        if (value == center - 4 || value == center + 2 || value == center + 5)
          dominantMask |= uint16_t{1} << key;
      }
      const auto inferred = inferKeyFromDominantSignature(dominantMask, config);
      check(inferred && inferred->tonic == center && inferred->isMinor,
        "Leading-tone detection must follow the actual assigned key");
      for (int key = 0; key < 12; ++key)
      {
        const int root = config.valueForKey[key];
        if (config.valueForKey[(key + 4) % 12] == root + 4
          && config.valueForKey[(key + 7) % 12] == root + 1
          && config.valueForKey[(key + 10) % 12] == root + 10)
        {
          Chord dominant;
          dominant.root_ = key; dominant.type_ = Chord::typeDom7;
          check(FindConfig(dominant, {}, mapping, config, KeepOldNotes::Yes) == &config
            && dominant.type_ == Chord::typeAug6th, "Preserve augmented-sixth interpretation");
        }
        const bool triad = config.valueForKey[(key + 4) % 12] == root + 4
          && config.valueForKey[(key + 7) % 12] == root + 1;
        Chord chord;
        chord.root_ = key; chord.type_ = Chord::typeMajor;
        check(TestChordConfig(chord, config) == triad, "Chord matching must use actual root key");
        if (triad)
        {
          const auto* found = FindConfig(chord, {static_cast<int8_t>(root)}, mapping, config, KeepOldNotes::Yes);
          check(found == &config, "Adaptive lookup should retain an already compatible center");
          ++chordChecks;
        }
      }
    }
  }
  check(index31 >= 0, "31-EDO required");
  const auto& edo31 = kNtetMappings[index31];
  const Config& example = edo31.getConfig(18);
  check(example.valueForKey[0] == 19, "31-EDO center 18 must assign 38.71 cents to C");
  check(std::abs(std::get<0>(score(example.valueForKey, edo31)) - 483.8709677419355) < 1e-7,
    "31-EDO expected aggregate error");
  check(example.valueForKey[5] == 18, "31-EDO center 18 must map to F");

  // A pre-fix preset must keep its explicit assignments until center selection.
  // The caller supplies the file inside its QTemporaryDir, never the registry.
  QSettings settings(temporarySettingsFile, QSettings::IniFormat);
  check(QSettings::defaultFormat() == QSettings::IniFormat, "Controllers must also use INI format");
  QSettings controllerSettings(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  check(controllerSettings.fileName() == settings.fileName(), "Controller and fixture must use the same temporary file");
  settings.setValue("status/edoIndex", index31);
  const Config& legacy = configPool[18 - kConfigPoolMin];
  Chord remappedChord;
  remappedChord.root_ = 5;
  remappedChord.type_ = Chord::typeMajor;
  const auto* unrestricted = FindConfig(remappedChord, {24}, edo31, legacy, KeepOldNotes::No);
  check(unrestricted && TestChordConfig(remappedChord, *unrestricted),
    "Adaptive lookup must match the optimized physical root assignment");
  const auto harmonicScore = [&](const Config& candidate) {
    int local = 0, global = 0;
    for (int key = 0; key < 12; ++key)
    {
      const int distance = std::abs(candidate.valueForKey[key] - legacy.valueForKey[key]);
      global += distance;
      if (key == 5 || key == 9 || key == 0) local += distance;
    }
    return std::make_tuple(local, global, candidate.valueForKey);
  };
  for (int center = edo31.minValue; center <= edo31.maxValue; ++center)
  {
    const auto& candidate = edo31.getConfig(static_cast<int8_t>(center));
    if (TestChordConfig(remappedChord, candidate))
      check(harmonicScore(*unrestricted) <= harmonicScore(candidate),
        "Adaptive lookup minimizes harmonic distance rather than center distance");
  }
  const auto* preserving = FindConfig(remappedChord, {24}, edo31, legacy, KeepOldNotes::Yes);
  check(!preserving || preserving->valueForKey[0] == 24,
    "Keeping a held note means keeping it on the same physical key");
  const auto offset = findGlobalOffsetCents(legacy, edo31, 0.0);
  check(offset.has_value(), "Legacy preset feasible");
  QVariantList savedValues;
  for (int value : legacy.valueForKey) savedValues.append(value);
  settings.setValue("tuningPresetsV2/31/count", 1);
  settings.setValue("tuningPresetsV2/31/preset0/values", savedValues);
  settings.setValue("tuningPresetsV2/31/preset0/tuningCenter", 18);
  settings.setValue("tuningPresetsV2/31/preset0/globalOffsetCents", *offset);
  {
    MidiController midi;
    TuningController controller(&midi);
    controller.applyPreset(0);
    check(controller.keyValues() == savedValues, "Old preset must retain saved mapping");
    controller.selectTuningCenter(18);
    check(controller.keyValues()[0].toInt() == 19, "Selecting center after preset must optimize mapping");
    check(controller.currentPresetIndex() == -1, "Selecting center clears preset selection");
    controller.setNoteNamingMode(1);
    check(controller.keyValues()[0].toInt() == 19, "Naming mode must not change mapping");
  }
  settings.clear();
  settings.sync();
  std::cout << "PASS: " << centers << " optimal keyboard mappings (" << changed
    << " corrected), " << chordChecks << " adaptive chord cases, MIDI encoding and old presets.\n";
}
