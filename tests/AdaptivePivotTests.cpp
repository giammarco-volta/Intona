#include "tuning/TuningController.h"
#include "tuning/TuningAlgorithms.h"
#include "TuningCenterFinder.h"
#include "midi/MidiController.h"
#include "IMidiOut.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QThread>
#include <QSettings>
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <tuple>

using namespace Intona::Tuning;
namespace {
void check(bool ok, const char* message)
{
  if (!ok) throw std::runtime_error(message);
}
void waitMs(int milliseconds)
{
  QElapsedTimer elapsed;
  elapsed.start();
  while (elapsed.elapsed() < milliseconds)
  {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  QCoreApplication::processEvents();
}
struct Message { int status, note, velocity; };
class RecordingOutput : public IMidiOut
{
public:
  std::vector<Message> messages;
  QStringList listOutputs() const override { throw std::runtime_error("No hardware enumeration in tests"); }
  bool open(int) override { throw std::runtime_error("No physical MIDI ports in tests"); }
  void close() override {}
  bool sendShort(uint8_t status, uint8_t data1, uint8_t data2) override
  { messages.push_back({status, data1, data2}); return true; }
  bool sendSysEx(const std::vector<uint8_t>&) override
  { messages.push_back({0xf0, 0, 0}); return true; }
};
int edoIndex(int edo)
{
  for (int i = 0; i < int(kNtetMappings.size()); ++i)
    if (kNtetMappings[i].N == edo) return i;
  throw std::runtime_error("Missing EDO");
}
using HarmonicState = std::tuple<QVariantList, int, QString, QString, int>;
HarmonicState state(const TuningController& controller)
{
  return {controller.keyValues(), controller.tuningCenter(), controller.keyDescription(),
    controller.chordDescription(), controller.currentPresetIndex()};
}
struct Session
{
  RecordingOutput* output = new RecordingOutput;
  MidiController midi{std::unique_ptr<IMidiOut>(output), 0x0005};
  TuningController controller{&midi};
  std::map<int, int> held;
  std::map<int, uint32_t> starts;
  uint32_t time = 1000;
  int divisions;
  Session(int edo = 31, int threshold = 3) : divisions(edo)
  {
    controller.setEdoIndex(edoIndex(edo));
    controller.selectTuningCenter(0);
    controller.setAdaptingEnabled(true);
    controller.setDirtyNoteThresholdMs(threshold);
    controller.setNoteNamingMode(0);
    output->messages.clear();
  }
  QTimer* timer() { return controller.findChild<QTimer*>(); }
  void on(int note)
  {
    const auto before = state(controller);
    output->messages.clear();
    starts[note] = ++time;
    const int velocity = 80 + note % 12;
    midi.midiNoteOnReceived(note, velocity, time);
    held[note] = velocity;
    check(state(controller) == before, "Note On must not evaluate harmonic state before timeout");
    check(output->messages.size() == 2, "Each new note sounds immediately on the two enabled channels");
    for (const auto& msg : output->messages)
      check((msg.status == 0x90 || msg.status == 0x92) && msg.note == note && msg.velocity == velocity,
        "New Note On must not retune or retrigger any other note");
  }
  void off(int note, bool dirty = false)
  {
    const auto before = state(controller);
    output->messages.clear();
    time = dirty ? starts[note] + 1 : std::max(time + 1, starts[note] + 2000);
    midi.midiNoteOffReceived(note, 0, time);
    held.erase(note);
    check(state(controller) == before, "Note Off must preserve every harmonic decision");
    check(output->messages.size() == 2, "Note Off must only release the requested note");
    for (const auto& msg : output->messages)
      check((msg.status == 0x80 || msg.status == 0x82) && msg.note == note,
        "No retuning or retrigger on release");
  }
  void finish()
  {
    const auto before = controller.keyValues();
    const bool wasActive = timer()->isActive();
    int notifications = 0;
    const auto connection = QObject::connect(&controller, &TuningController::tuningStateChanged,
      [&notifications]() { ++notifications; });
    output->messages.clear();
    QElapsedTimer elapsed;
    elapsed.start();
    while (timer()->isActive() && elapsed.elapsed() < 2500) waitMs(1);
    QObject::disconnect(connection);
    check(!timer()->isActive(), "Validation timer must expire");
    check(notifications == (wasActive ? 1 : 0), "Exactly one evaluation notification per stable group");
    const auto after = controller.keyValues();
    const int fifth = kNtetMappings[edoIndex(divisions)].fifthStep;
    int expected = 0;
    for (const auto& [note, velocity] : held)
    {
      const int delta = after[note % 12].toInt() - before[note % 12].toInt();
      if (mod(delta * fifth, divisions) == 0) continue;
      expected += 2;
      for (int channel : {0, 2})
      {
        int off = -1, tuning = -1, restart = -1;
        for (int i = 0; i < int(output->messages.size()); ++i)
        {
          const auto& msg = output->messages[i];
          if (msg.status == 0x80 + channel && msg.note == note) off = i;
          if (msg.status == 0xf0) tuning = i;
          if (msg.status == 0x90 + channel && msg.note == note)
          { restart = i; check(msg.velocity == velocity, "Retrigger preserves velocity"); }
        }
        check(off >= 0 && off < tuning && tuning < restart, "At timeout: NoteOff, tuning, NoteOn ordering");
      }
    }
    int offs = 0, ons = 0;
    for (const auto& msg : output->messages)
    {
      offs += (msg.status & 0xf0) == 0x80;
      ons += (msg.status & 0xf0) == 0x90;
    }
    check(offs == expected && ons == expected, "Only actually retuned sounding notes may restart");
  }
};

using Score = std::tuple<int, int, std::array<int8_t, 12>>;
Score score(const Config& config, const Config& reference, int root, bool minor)
{
  int chordDistance = 0, total = 0;
  for (int key = 0; key < 12; ++key)
  {
    const int d = std::abs(config.valueForKey[key] - reference.valueForKey[key]);
    total += d;
    if (key == root || key == (root + (minor ? 3 : 4)) % 12 || key == (root + 7) % 12)
      chordDistance += d;
  }
  return {chordDistance, total, config.valueForKey};
}

bool matches(const Config& config, int root, bool minor)
{
  const int value = config.valueForKey[root];
  return config.valueForKey[(root + (minor ? 3 : 4)) % 12] == value + (minor ? -3 : 4)
    && config.valueForKey[(root + 7) % 12] == value + 1;
}
} // namespace

void runHarmonyInterpretationTests()
{
  const auto& mapping = kNtetMappings[edoIndex(31)];
  const HarmonicChordContext fMajor{-1, true, {-1, 3, 0}};
  const HarmonicChordContext eMajor{4, true, {4, 8, 5}};
  const HarmonicChordContext aMinor{3, true, {3, 0, 4}};
  const HarmonicChordContext cMajor{0, true, {0, 4, 1}};
  const auto chord = [](Chord::EChordType type, int root) {
    Chord result; result.type_ = type; result.root_ = result.bass_ = root; return result;
  };
  check(FitsDiatonicKey(aMinor.notes, -1) && FitsDiatonicKey(aMinor.notes, 4),
    "A minor belongs to both F major and E minor: context must not force a choice");
  for (auto type : {Chord::typeDom7, Chord::typeDom7f5, Chord::typeF5})
  {
    const auto alternative = type == Chord::typeDom7 ? Chord::typeAug6th
      : type == Chord::typeDom7f5 ? Chord::typeFrench6th : Chord::typeMin6;
    Chord fromF = chord(type, 0);
    const auto* f = FindClosestChordConfig(fromF, mapping, mapping.getConfig(5), &fMajor);
    check(f && fromF.type_ == type && ChordRootValue(fromF, *f) == 0,
      "Previous F harmony selects conventional reading even against reference distance");
    Chord fromE = chord(type, 0);
    const auto* e = FindClosestChordConfig(fromE, mapping, mapping.getConfig(-1), &eMajor);
    check(e && fromE.type_ == alternative && ChordRootValue(fromE, *e) == (type == Chord::typeF5 ? 3 : 0),
      "Previous E harmony selects augmented sixth/rootless minor sixth");
    check(e->valueForKey[0] == 0 && e->valueForKey[4] == 4,
      "Alternatives preserve C and E spellings");
    if (type != Chord::typeDom7)
      check(e->valueForKey[6] == 6, "French sixth and rootless Am6 require F#, not Gb");
    if (type != Chord::typeF5)
      check(e->valueForKey[10] == 10, "Augmented sixth requires A#, not Bb");
    Chord noHistory = chord(type, 0), ambiguous = noHistory;
    const auto* nearest = FindClosestChordConfig(noHistory, mapping, mapping.getConfig(0));
    const auto* shared = FindClosestChordConfig(ambiguous, mapping, mapping.getConfig(0), &aMinor);
    check(nearest && shared && nearest->valueForKey == shared->valueForKey
      && noHistory.type_ == ambiguous.type_, "Shared-key previous chord falls back to distance");
    Chord pinned = chord(type, 0);
    const auto* constrained = FindClosestChordConfig(pinned, mapping, *f, &eMajor, ChordKeys(fromF));
    check(constrained && pinned.type_ == type, "Pivots override incompatible harmonic context");
  }
  {
    Config custom = mapping.getConfig(0);
    custom.valueForKey[9] = -9; // The omitted root's key has an unrelated spelling.
    rebuildConfigMask(custom);
    Chord rootless = chord(Chord::typeF5, 0);
    const auto* result = FindClosestChordConfig(rootless, mapping, custom);
    check(result == &custom && rootless.omitRoot_ && ChordRootValue(rootless, *result) == 3,
      "Rootless Am6 derives its virtual root from C; the unused A key is not a constraint");
  }
  {
    Chord diminished = chord(Chord::typeDim7, 2);
    const auto* result = FindClosestChordConfig(diminished, mapping, mapping.getConfig(-3), &cMajor);
    check(result && diminished.root_ == 11 && ChordRootValue(diminished, *result) == 5,
      "Previous C triad selects leading-tone B dim7, even with D in the bass");
    for (int root : {2, 5, 8, 11})
    {
      Chord inversion = chord(Chord::typeDim7, root);
      const auto* same = FindClosestChordConfig(inversion, mapping, *result);
      check(same && same->valueForKey == result->valueForKey && inversion.root_ == 11,
        "Diminished inversion preserves B rather than respelling it Cb");
    }
    for (int root : {0, 4, 8})
    {
      Chord augmented = chord(Chord::typeAug, root);
      const auto* same = FindClosestChordConfig(augmented, mapping, mapping.getConfig(3));
      check(same && augmented.root_ == 0 && same->valueForKey[0] == 0
        && same->valueForKey[4] == 4 && same->valueForKey[8] == 8,
        "Augmented inversion preserves C E G# independently of bass");
    }
  }

  // Independent spelling/distance oracle over transpositions and EDO ranges.
  struct Reading { int root; Chord::EChordType type; bool omitted; std::vector<int> intervals; };
  int cases = 0;
  for (const auto& edo : kNtetMappings)
    for (int center : {int(edo.minValue), 0, int(edo.maxValue)})
      for (int root = 0; root < 12; ++root)
        for (auto type : {Chord::typeDom7, Chord::typeDom7f5, Chord::typeF5, Chord::typeDim7, Chord::typeAug})
        {
          const auto& reference = edo.getConfig(static_cast<int8_t>(center));
          std::vector<Reading> readings;
          if (type == Chord::typeDom7)
            readings = {{root, type, false, {4, 1, -2}}, {root, Chord::typeAug6th, false, {4, 1, 10}}};
          if (type == Chord::typeDom7f5)
            readings = {{root, type, false, {4, -6, -2}}, {root, Chord::typeFrench6th, false, {4, 6, 10}}};
          if (type == Chord::typeF5)
            readings = {{root, type, false, {4, -6}}, {(root + 9) % 12, Chord::typeMin6, true, {-3, 1, 3}}};
          if (type == Chord::typeDim7 || type == Chord::typeAug)
            for (int shift = 0; shift < 12; shift += type == Chord::typeDim7 ? 3 : 4)
              readings.push_back({(root + shift) % 12, type, false,
                type == Chord::typeDim7 ? std::vector<int>{-3, -6, -9} : std::vector<int>{4, 8}});
          using OracleScore = std::tuple<int, int, std::array<int8_t, 12>, int, int>;
          OracleScore bestScore;
          const Config* expected = nullptr;
          Reading bestReading{};
          for (const auto& reading : readings)
          {
            const auto consider = [&](const Config& candidate) {
              const int anchor = reading.omitted ? candidate.valueForKey[(reading.root + 3) % 12] + 3
                : candidate.valueForKey[reading.root];
              uint16_t keys = uint16_t{1} << root; // Actual bass remains sounding.
              if (!reading.omitted) keys |= uint16_t{1} << reading.root;
              for (int interval : reading.intervals)
              {
                const int key = mod(reading.root + interval * 7, 12);
                if (candidate.valueForKey[key] != anchor + interval) return;
                keys |= uint16_t{1} << key;
              }
              int local = 0, global = 0;
              for (int key = 0; key < 12; ++key)
              {
                const int difference = std::abs(candidate.valueForKey[key] - reference.valueForKey[key]);
                global += difference;
                if (keys & (1 << key)) local += difference;
              }
              const OracleScore score{local, global, candidate.valueForKey, anchor, reading.type};
              if (!expected || score < bestScore)
              { expected = &candidate; bestScore = score; bestReading = reading; }
            };
            consider(reference);
            for (int next = edo.minValue; next <= edo.maxValue; ++next)
              consider(edo.getConfig(static_cast<int8_t>(next)));
          }
          Chord actualChord = chord(type, root);
          const auto* actual = FindClosestChordConfig(actualChord, edo, reference);
          check(bool(actual) == bool(expected), "Complete ambiguous chord candidate enumeration");
          if (actual)
            check(actual->valueForKey == expected->valueForKey && actualChord.root_ == bestReading.root
              && actualChord.type_ == bestReading.type && actualChord.omitRoot_ == bestReading.omitted,
              "Independent minimum-distance oracle for all five ambiguous families");
          ++cases;
        }

  int melodicCases = 0;
  for (const auto& edo : kNtetMappings)
    for (int center = edo.minValue; center <= edo.maxValue; ++center)
    {
      const auto& reference = edo.getConfig(static_cast<int8_t>(center));
      Config relabelled = reference;
      relabelled.tuningCenter = Config::invalid;
      int8_t tonicA = Config::invalid, tonicB = Config::invalid;
      bool minorA = false, minorB = false;
      const auto* a = FindClosestScaleConfig(edo, reference, (1 << 4) | (1 << 6) | (1 << 8), 1 << 8, 0, tonicA, minorA);
      const auto* b = FindClosestScaleConfig(edo, relabelled, (1 << 4) | (1 << 6) | (1 << 8), 1 << 8, 0, tonicB, minorB);
      check(bool(a) == bool(b) && tonicA == tonicB && minorA == minorB
        && (!a || a->valueForKey == b->valueForKey), "Melodic interpretation ignores center metadata in every EDO");
      ++melodicCases;
    }
  {
    Session melody;
    for (int note : {64, 66, 68})
    { melody.on(note); melody.finish(); melody.off(note); }
    const auto keys = melody.controller.keyValues();
    check(keys[4].toInt() == 4 && keys[6].toInt() == 6 && keys[8].toInt() == 8,
      "E F# Ab from C mapping resolves E F# G# without rewriting preceding notes Fb Gb");
  }
  for (bool eContext : {false, true})
    for (int family = 0; family < 3; ++family)
    {
      Session session;
      const std::vector<int> preceding = eContext ? std::vector<int>{64,68,71} : std::vector<int>{65,69,72};
      for (int note : preceding) session.on(note);
      session.finish();
      for (int note : preceding) session.off(note);
      const std::vector<int> next = family == 0 ? std::vector<int>{60,64,67,70}
        : family == 1 ? std::vector<int>{60,64,66,70} : std::vector<int>{60,64,66};
      for (int note : next) session.on(note);
      session.finish();
      const auto keys = session.controller.keyValues();
      if (family != 0) check(keys[6].toInt() == (eContext ? 6 : -6), "Validated previous chord selects F# or Gb");
      if (family != 2) check(keys[10].toInt() == (eContext ? 10 : -2), "Validated previous chord selects A# or Bb");
      if (family == 2 && eContext)
        check(session.controller.chordDescription().contains("Am6(no root)"), "Rootless sixth displays its virtual root");
    }
  {
    Session a, b;
    QSettings presets(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
    presets.setValue("tuningPresetsV2/31/count", 2);
    for (int i = 0; i < 2; ++i)
    {
      const QString group = QString("tuningPresetsV2/31/preset%1/").arg(i);
      presets.setValue(group + "values", a.controller.keyValues());
      presets.setValue(group + "tuningCenter", i == 0 ? 0 : 6);
      presets.setValue(group + "globalOffsetCents", 0.0);
    }
    presets.sync();
    a.controller.applyPreset(0); b.controller.applyPreset(1);
    check(a.controller.tuningCenter() != b.controller.tuningCenter()
      && a.controller.keyValues() == b.controller.keyValues(), "Same twelve notes with different preset centers");
    for (int note : {64, 66, 68, 69, 71, 73})
    {
      a.on(note); a.finish(); a.off(note);
      b.on(note); b.finish(); b.off(note);
      check(a.controller.keyValues() == b.controller.keyValues()
        && a.controller.keyDescription() == b.controller.keyDescription(),
        "Full melodic MIDI sequence and inferred key ignore differing center metadata");
    }
    presets.remove("tuningPresetsV2/31"); presets.sync();
  }
  {
    Session session;
    for (int note : {60,64,67}) session.on(note);
    session.finish();
    for (int note : {67,60,64}) session.off(note);
    // This rejected chord must never replace the preceding C major context.
    for (int note : {62,66,69}) session.on(note);
    for (int note : {69,66,62}) session.off(note, true);
    session.finish();
    for (int note : {62,65,68,71}) session.on(note);
    session.finish();
    check(session.controller.chordDescription().contains("Bdim7/D"),
      "Diminished root follows previous clean C chord, ignoring a dirty D chord");
    const auto spelling = session.controller.keyValues();
    for (int note : {71,68,65,62}) session.off(note);
    for (int note : {65,68,71,74}) session.on(note);
    session.finish();
    check(session.controller.keyValues() == spelling
      && session.controller.chordDescription().contains("Bdim7/F"),
      "Revoicing a diminished chord preserves its root and spelling without pivots");
  }
  std::cout << "PASS: " << cases << " ambiguous chord oracles, " << melodicCases
    << " center-independent melodic cases, previous-chord context and real MIDI sequences.\n";
}

void runAdaptiveWindowTests(const QString& temporarySettingsFile)
{
  QSettings settings(temporarySettingsFile, QSettings::IniFormat);
  QSettings controllerSettings(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  check(QSettings::defaultFormat() == QSettings::IniFormat
    && settings.fileName() == controllerSettings.fileName(), "Settings must be isolated");
  settings.clear();
  settings.sync();
  {
    MidiController midi(std::make_unique<RecordingOutput>(), 1);
    TuningController controller(&midi);
    check(controller.dirtyNoteThresholdMs() == 100, "Default threshold is 100 ms");
    controller.setDirtyNoteThresholdMs(150);
    controller.setDirtyNoteThresholdMs(-1);
    controller.setDirtyNoteThresholdMs(1001);
    TuningController restored(&midi);
    check(restored.dirtyNoteThresholdMs() == 150 && restored.uiSnapshot().dirtyNoteThresholdMs == 150,
      "Threshold persistence, snapshot and bounds");
  }

    int scored = 0;
  for (const auto& mapping : kNtetMappings)
    for (int center = mapping.minValue; center <= mapping.maxValue; ++center)
      for (bool minor : {false, true})
      {
        const auto& reference = mapping.getConfig(static_cast<int8_t>(center));
        const int root = minor ? 9 : 4;
        Chord chord;
        chord.root_ = chord.bass_ = root;
        chord.type_ = minor ? Chord::typeMinor : Chord::typeMajor;
        const Config* expected = nullptr;
        const auto consider = [&](const Config& candidate) {
          if (matches(candidate, root, minor)
            && (!expected || score(candidate, reference, root, minor) < score(*expected, reference, root, minor)))
            expected = &candidate;
        };
        consider(reference);
        for (int next = mapping.minValue; next <= mapping.maxValue; ++next)
          consider(mapping.getConfig(static_cast<int8_t>(next)));
        const auto* actual = FindClosestChordConfig(chord, mapping, reference);
        check(bool(actual) == bool(expected), "Harmonic search candidate completeness");
        if (actual) check(actual->valueForKey == expected->valueForKey, "Independent minimum harmonic distance oracle");
        ++scored;
      }


  int permutations = 0;
  for (int edo : {19, 31, 43, 53})
  {
    std::array<int, 3> notes{64, 68, 71};
    do {
      std::array<int, 3> releases{64, 68, 71};
      do {
        Session session(edo);
        for (int note : notes) session.on(note);
        session.finish();
        const auto keys = session.controller.keyValues();
        check(keys[4].toInt() == 4 && keys[8].toInt() == 8 && keys[11].toInt() == 5,
          "One synchronous batch always resolves E G# B, regardless of press order");
        for (int note : releases) session.off(note);
        const auto final = state(session.controller);
        session.finish();
        check(state(session.controller) == final, "Releases never schedule extra evaluations");
        ++permutations;
      } while (std::next_permutation(releases.begin(), releases.end()));
    } while (std::next_permutation(notes.begin(), notes.end()));
  }

  std::array<int, 3> notes{64, 68, 71};
  do {
    Session session;
    session.on(notes[0]);
    session.finish();
    const int pivot = session.controller.keyValues()[notes[0] % 12].toInt();
    session.on(notes[1]);
    session.on(notes[2]);
    session.finish();
    const auto keys = session.controller.keyValues();
    check(keys[notes[0] % 12].toInt() == pivot, "Only the previously validated note acts as a pivot");
    check(keys[4].toInt() == (notes[0] == 68 ? -8 : 4), "An early Ab intentionally selects Fb major");
  } while (std::next_permutation(notes.begin(), notes.end()));

  {
    Session session(31, 60);
    const auto original = state(session.controller);
    session.on(68);
    waitMs(35);
    check(state(session.controller) == original, "No early evaluation");
    session.on(64);
    waitMs(35);
    check(session.timer()->isActive() && state(session.controller) == original,
      "New note restarts the complete interval, without treating earlier arrivals as pivots");
    session.on(71);
    session.finish();
    check(session.controller.keyValues()[4].toInt() == 4, "Restarted batch remains E major");
  }
  {
    Session session;
    session.on(68); session.finish(); // Established Ab pivot.
    session.on(64);
    session.off(68);
    session.on(71);
    session.finish();
    check(session.controller.keyValues()[4].toInt() == 4, "Released pivot ceases to constrain the group");
  }
  {
    Session session(31, 40);
    const auto original = state(session.controller);
    session.on(68);
    session.off(68, true);
    session.finish();
    waitMs(45);
    check(state(session.controller) == original, "A dirty-only burst never changes harmonic state");
  }
  {
    Session clean, noisy;
    clean.on(64); clean.on(71); clean.finish();
    noisy.on(64); noisy.on(68); noisy.on(71); noisy.off(68, true); noisy.finish();
    check(state(clean.controller) == state(noisy.controller), "Dirty voice cannot contaminate chord recognition");
  }
  {
    Session clean, noisy;
    for (int note : {64, 66})
    {
      clean.on(note); clean.finish(); clean.off(note);
      noisy.on(note); noisy.finish(); noisy.off(note);
    }
    noisy.on(61); noisy.off(61, true); noisy.finish();
    for (int note : {68, 69})
    {
      clean.on(note); clean.finish(); clean.off(note);
      noisy.on(note); noisy.finish(); noisy.off(note);
    }
    check(state(clean.controller) == state(noisy.controller),
      "Dirty noise neither adds to nor erases valid melodic history");
  }
  {
    // A sufficiently long note can end during a restarted window. It is only
    // melodic history, never a sounding chord voice or a retrigger target.
    Session session(31, 40);
    session.on(64); session.on(68); session.off(64); session.on(71);
    session.finish();
    check(!session.controller.pressedKeys()[4].toBool(), "Released clean note is not held");
  }
  {
    Session session(31, 40);
    session.on(68);
    const auto original = state(session.controller);
    session.controller.setDirtyNoteThresholdMs(80);
    waitMs(50);
    check(session.timer()->isActive() && state(session.controller) == original,
      "Editing the threshold restarts a pending interval safely");
    session.finish();
  }
  {
    Session session(31, 40);
    session.on(68);
    session.controller.selectTuningCenter(1);
    const auto manual = state(session.controller);
    waitMs(50);
    check(state(session.controller) == manual, "Explicit center selection cancels stale timeout");
    session.controller.setEdoIndex(edoIndex(53));
    const auto changedEdo = state(session.controller);
    waitMs(45);
    check(state(session.controller) == changedEdo, "EDO change cannot receive stale decisions");
  }
  {
    Session session;
    session.controller.setAdaptingEnabled(false);
    const auto original = session.controller.keyValues();
    for (int note : {68, 64, 71}) session.on(note);
    session.finish();
    check(session.controller.keyValues() == original, "RT off still forwards notes but prevents tuning changes");
  }
  {
    Session session(31, 0);
    session.on(68); session.on(64); session.on(71); session.finish();
    check(session.controller.keyValues()[4].toInt() == 4, "Zero threshold still evaluates through the queued timer");
  }
  {
    Session session(31, 15);
    QThread worker;
    QThread* mainThread = QThread::currentThread();
    session.midi.moveToThread(&worker);
    session.controller.moveToThread(&worker);
    const bool timerMoved = session.timer()->thread() == &worker;
    worker.start();
    QMetaObject::invokeMethod(&session.controller, [&session]() {
      session.midi.midiNoteOnReceived(68, 88, 1000);
      session.midi.midiNoteOnReceived(64, 84, 1001);
      session.midi.midiNoteOnReceived(71, 91, 1002);
    }, Qt::BlockingQueuedConnection);
    bool finished = false;
    QElapsedTimer deadline;
    deadline.start();
    while (!finished && deadline.elapsed() < 1500)
    {
      QMetaObject::invokeMethod(&session.controller, [&]() {
        finished = !session.timer()->isActive() && session.controller.keyValues()[8].toInt() == 8;
      }, Qt::BlockingQueuedConnection);
      waitMs(1);
    }
    QMetaObject::invokeMethod(&session.controller, [&]() {
      session.midi.moveToThread(mainThread);
      session.controller.moveToThread(mainThread);
    }, Qt::BlockingQueuedConnection);
    worker.quit(); worker.wait();
    check(timerMoved && finished, "Timer must follow the actual tuning worker thread");
  }
  {
    int late = 0;
    {
      Session session(31, 20);
      session.on(68);
      QObject::connect(&session.controller, &TuningController::tuningStateChanged, [&late]() { ++late; });
    }
    waitMs(30);
    check(late == 0, "Destroying the controller cancels pending callbacks");
  }
  runHarmonyInterpretationTests();
  settings.clear(); settings.sync();
  std::cout << "PASS: " << scored << " harmonic choices, " << permutations
    << " grouped press/release orders, dirty-note isolation, automatic pivots, timer restart, MIDI ordering and worker lifecycle.\n";
}
