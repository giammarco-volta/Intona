#include "IMidiOut.h"
#include "midi/MidiController.h"
#include "tuning/ScaleTriadAdapting.h"
#include "tuning/TuningAlgorithms.h"
#include "tuning/TuningController.h"
#include "tuning/TuningViewModel.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Intona::Tuning;
namespace
{
void require(bool ok, const char *what)
{
  if (!ok)
    throw std::runtime_error(what);
}
void wait(int ms)
{
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < ms)
  {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  QCoreApplication::processEvents();
}
struct Message
{
  int status, note, velocity;
};
class Output : public IMidiOut
{
public:
  std::vector<Message> messages;
  std::vector<std::vector<uint8_t>> sysexes;
  bool acceptMessages = true;
  QStringList listOutputs() const override
  {
    throw std::runtime_error("No MIDI hardware access");
  }
  bool open(int) override
  {
    throw std::runtime_error("No MIDI hardware access");
  }
  void close() override
  {
  }
  bool sendShort(uint8_t s, uint8_t n, uint8_t v) override
  {
    messages.push_back({s, n, v});
    return acceptMessages;
  }
  bool sendSysEx(const std::vector<uint8_t>& bytes) override
  {
    messages.push_back({0xf0, 0, 0});
    sysexes.push_back(bytes);
    return acceptMessages;
  }
};
void replay(const QString &path)
{
  QFile f(path);
  require(f.open(QIODevice::ReadOnly), "Missing replay fixtures");
  const auto cases = QJsonDocument::fromJson(f.readAll()).array();
  require(!cases.empty(), "Empty replay fixtures");
  size_t attacks = 0;
  for (const auto &test : cases)
    for (int anchor : {0, 3, 7, 11})
    {
      const auto obj = test.toObject();
      ScaleTriadAdapting engine;
      engine.reset(relativeKeyboardConfig(obj["initial"].toInt(), anchor));
      double time = 0;
      uint64_t id = 0;
      for (const auto &v : obj["events"].toArray())
      {
        const auto e = v.toArray();
        time = e[0].toDouble();
        id = uint64_t(e[3].toInteger()) + 1;
        // At equal timestamps the original MIDI event order precedes timeout.
        engine.advance(time - .0001);
        if (e[2].toInt())
        {
          engine.noteOn(id, (e[4].toInt() + anchor) % 12, time);
          ++attacks;
          if (engine.config().tuningCenter != e[5].toInt())
          {
            std::cerr << obj["name"].toString().toStdString() << " event " << id << " time " << time
                      << " anchor " << anchor << " expected " << e[5].toInt() << " got "
                      << engine.config().tuningCenter << '\n';
            throw std::runtime_error("C++ / independent Python decision mismatch");
          }
        }
        else
          engine.noteOff(id, time);
      }
      engine.advance(time + 1000);
      require(engine.config().tuningCenter == obj["final"].toInt(),
              "Final center / rollback parity");
      require(engine.notes().size() < 150, "Old released history must remain bounded");
    }
  std::cout << "PASS: " << cases.size() << " causal replays, four keyboard anchors, " << attacks
            << " matching attack decisions.\n";
}
struct Performance
{
  ScaleTriadAdapting engine;
  uint64_t next = 1;
  int anchor;
  explicit Performance(int a = 0) : anchor(a)
  {
    engine.reset(relativeKeyboardConfig(0, anchor));
  }
  uint64_t on(int key, double time)
  {
    engine.advance(time - .0001);
    const auto id = next++;
    engine.noteOn(id, (key + anchor) % 12, time);
    return id;
  }
  void off(uint64_t id, double time)
  {
    engine.advance(time - .0001);
    engine.noteOff(id, time);
  }
  int value(int key) const { return engine.config().valueForKey[(key + anchor) % 12]; }
  bool confirmed(uint64_t id) const
  {
    return engine.notes().at(id).reading.status == ScaleTriadAdapting::Status::Confirmed;
  }
  std::array<uint64_t, 3> modulate()
  {
    const std::vector<std::vector<int>> groups = {
        {6, 9, 1}, {9, 1, 4}, {3, 6, 10}, {6, 1}, {5, 8, 0}};
    double time = 400;
    for (const auto &group : groups)
    {
      std::vector<uint64_t> ids;
      for (int key : group) ids.push_back(on(key, time));
      for (auto id : ids) off(id, time + 200);
      time += 400;
    }
    std::array<uint64_t, 3> chord{on(2, 2400), on(6, 2400), on(9, 2400)};
    require(engine.config().tuningCenter == 6 && engine.stableCenter() == 9,
            "Reproduce F# current / D# stable after modulation");
    require(value(2) == 2 && value(6) == 6 && value(9) == 3,
            "Recognized D major before the partial release");
    require(!confirmed(chord[0]) && !confirmed(chord[2]),
            "Regression requires the previously unprotected pending D and A");
    return chord;
  }
  std::array<uint64_t, 3> provisionalE()
  {
    auto c = on(0, 0); off(c, 180);
    auto d = on(2, 250); off(d, 430);
    auto e = on(4, 500);
    auto gs = on(8, 1000);
    auto b = on(11, 1010);
    engine.advance(1100);
    require(value(8) == 8 && !confirmed(gs), "E major has a provisional G#");
    return {e, gs, b};
  }
};
void triadPivotTests()
{
  for (int anchor : {0, 3, 7, 11})
  {
    Performance p(anchor);
    auto chord = p.modulate();
    p.off(chord[1], 2900);
    require(p.engine.config().tuningCenter == 6 && !p.confirmed(chord[0]) &&
                !p.confirmed(chord[2]), "Release alone neither retunes nor acquires pivots");
    p.on(5, 2910);
    require(p.value(2) == 2 && p.value(5) == -1 && p.value(9) == 3,
            "Partial D major release must yield D F A, never C## E# G##");
    require(p.confirmed(chord[0]) && p.confirmed(chord[2]),
            "The next gesture acquires both retained triad members");
    p.engine.advance(3100);
    require(p.value(2) == 2 && p.value(5) == -1 && p.value(9) == 3,
            "Triad pivots survive proof verification");

    Performance single(anchor);
    chord = single.modulate();
    single.off(chord[1], 2900); single.off(chord[2], 2900);
    single.on(5, 2910); single.on(9, 2920);
    require(single.confirmed(chord[0]) && single.value(2) == 2 && single.value(5) == -1 &&
                single.value(9) == 3, "One retained triad member is also a pivot");

    Performance full(anchor);
    auto e = full.provisionalE();
    full.on(7, 1250);
    require(full.value(8) == -4 && full.value(7) == 1,
            "Fully held E G# B remains revisable to E Ab B with incoming G");

    Performance partial(anchor);
    e = partial.provisionalE();
    partial.off(e[2], 1200);
    partial.on(0, 1250);
    require(partial.confirmed(e[1]) && partial.value(8) == 8,
            "A retained provisional member of a recognized major triad becomes a pivot");

    Performance doubled(anchor);
    e = doubled.provisionalE();
    auto upperB = doubled.on(11, 1150);
    doubled.off(e[2], 1200);
    doubled.on(7, 1250);
    require(doubled.value(8) == -4 && doubled.value(7) == 1,
            "Releasing an octave doubling does not count as a partial triad release");
    (void)upperB;

    Performance shortOverlap(anchor);
    shortOverlap.engine.reset(relativeKeyboardConfig(2, anchor));
    auto f = shortOverlap.on(5, 0);
    auto ab = shortOverlap.on(8, 500);
    shortOverlap.on(0, 510);
    shortOverlap.off(f, 540); // Old F, but only 30ms of simultaneous triad.
    shortOverlap.engine.advance(590);
    shortOverlap.on(8, 600); // Same class adds no new distinct evidence.
    require(!shortOverlap.confirmed(ab), "Brief legato triad must not manufacture a pivot");
  }
  std::cout << "PASS: triad-derived pivots, one/two retained members, full-triad revision, "
               "octave doublings and brief legato, at four keyboard anchors.\n";
}
void midiTests()
{
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  settings.setValue("status/useScaleTriadAdapting", false);
  settings.setValue("status/useHarmonicCostAdapting", false);
  settings.setValue("status/dirtyNoteThresholdMs", 1000);
  settings.setValue("status/historyDecaySlope", 7);
  settings.setValue("status/historyWindowIntervals", 1);
  auto *output = new Output;
  MidiController midi{std::unique_ptr<IMidiOut>(output), 5};
  TuningController controller(&midi);
  require(!settings.contains("status/useScaleTriadAdapting") &&
              !settings.contains("status/useHarmonicCostAdapting") &&
              !settings.contains("status/dirtyNoteThresholdMs"),
          "Remove obsolete mode and timing preferences even for legacy users");
  require(!settings.contains("status/historyDecaySlope") &&
              !settings.contains("status/historyWindowIntervals"),
          "Retire unused history preferences");
  for (int i = 0; i < int(kNtetMappings.size()); ++i)
    if (kNtetMappings[i].N == 31)
      controller.setEdoIndex(i);
  controller.setAdaptingEnabled(true);
  controller.selectTuningCenter(2);
  output->messages.clear();
  midi.midiNoteOnReceived(68, 91, 1000); // Starts as G# in center D.
  midi.midiNoteOnReceived(65, 83, 1010);
  output->messages.clear();
  midi.midiNoteOnReceived(60, 79, 1020); // Completes F minor.
  require(controller.keyValues()[8].toInt() == -4,
          "Triad corrects G# to Ab before forwarding the final note");
  for (int channel : {0, 2})
  {
    auto off = std::find_if(output->messages.begin(), output->messages.end(),
                            [&](auto m) { return m.status == (0x80 | channel) && m.note == 68; });
    auto tuning = std::find_if(output->messages.begin(), output->messages.end(),
                               [](auto m) { return m.status == 0xf0; });
    auto on = std::find_if(output->messages.begin(), output->messages.end(), [&](auto m) {
      return m.status == (0x90 | channel) && m.note == 68 && m.velocity == 91;
    });
    auto arriving = std::find_if(output->messages.begin(), output->messages.end(), [&](auto m) {
      return m.status == (0x90 | channel) && m.note == 60 && m.velocity == 79;
    });
    require(off < tuning && tuning < on && on < arriving && arriving != output->messages.end(),
            "NoteOff -> tuning -> original velocity NoteOn -> arriving NoteOn");
  }
  wait(90);
  require(controller.keyValues()[8].toInt() == -4, "Sustained evidence survives verification");
  for (int note : {68, 65, 60})
    midi.midiNoteOffReceived(note, 0, 1200);
  controller.selectTuningCenter(2);
  midi.midiNoteOnReceived(65, 88, 2000);
  midi.midiNoteOnReceived(68, 89, 2010);
  midi.midiNoteOnReceived(60, 90, 2020);
  midi.midiNoteOffReceived(68, 0, 2062); // 52ms supporting note, final trigger survives.
  output->messages.clear();
  wait(90);
  require(controller.tuningCenter() == 2,
          "Real proof timer restores previous center when support was dirty");
  for (int note : {65, 60})
    midi.midiNoteOffReceived(note, 0, 2300);
  controller.selectTuningCenter(0);
  quint32 stamp = 4000;
  for (const auto &group : std::vector<std::vector<int>>{
           {66, 69, 61}, {69, 61, 64}, {63, 66, 70}, {66, 61}, {65, 68, 60}})
  {
    for (int note : group) midi.midiNoteOnReceived(note, 90, stamp);
    for (int note : group) midi.midiNoteOffReceived(note, 0, stamp + 200);
    stamp += 400;
  }
  for (int note : {62, 66, 69}) midi.midiNoteOnReceived(note, 90, stamp);
  require(controller.tuningCenter() == 6, "MIDI reproduction reaches F# by modulation");
  midi.midiNoteOffReceived(66, 0, stamp + 500);
  output->messages.clear();
  midi.midiNoteOnReceived(65, 87, stamp + 510);
  require(controller.keyValues()[2].toInt() == 2 && controller.keyValues()[5].toInt() == -1 &&
              controller.keyValues()[9].toInt() == 3, "MIDI output uses D F A after partial release");
  require(std::none_of(output->messages.begin(), output->messages.end(), [](auto m) {
            return ((m.status & 0xf0) == 0x80 || (m.status & 0xf0) == 0x90) &&
                   (m.note == 62 || m.note == 69);
          }), "Retained D and A receive neither NoteOff nor retrigger");
  auto tuning = std::find_if(output->messages.begin(), output->messages.end(),
                             [](auto m) { return m.status == 0xf0; });
  for (int channel : {0, 2})
  {
    auto arriving = std::find_if(output->messages.begin(), output->messages.end(), [&](auto m) {
      return m.status == (0x90 | channel) && m.note == 65 && m.velocity == 87;
    });
    require(tuning < arriving && arriving != output->messages.end(),
            "F tuning is sent before its immediate NoteOn on both output channels");
  }
  wait(90);
  require(controller.keyValues()[2].toInt() == 2 && controller.keyValues()[5].toInt() == -1 &&
              controller.keyValues()[9].toInt() == 3, "MIDI proof timer preserves the new pivots");
  for (int note : {62, 65, 69}) midi.midiNoteOffReceived(note, 0, stamp + 800);
  controller.selectTuningCenter(0);
  const auto unchanged = controller.keyValues();
  controller.setAdaptingEnabled(false);
  for (int note : {64, 66, 68})
    midi.midiNoteOnReceived(note, 90, 8000 + note);
  wait(90);
  require(controller.keyValues() == unchanged, "RT off bypasses adaptive decisions");
  for (int note : {64, 66, 68})
    midi.midiNoteOffReceived(note, 0, 8300);
  controller.setAdaptingEnabled(true);
  TuningController restored(&midi);
  restored.selectTuningCenter(2);
  for (int note : {65, 68, 60}) midi.midiNoteOnReceived(note, 90, 9000);
  require(restored.keyValues()[8].toInt() == -4,
          "A fresh controller always runs the scale engine without a selector");
  for (int note : {65, 68, 60}) midi.midiNoteOffReceived(note, 0, 9200);
  std::cout << "PASS: MIDI ordering, immediate output, 70ms rollback, held triad pivots, RT off and migration.\n";
}
void retriggerPreferenceAndProbeTests()
{
  auto* output = new Output;
  MidiController midi{std::unique_ptr<IMidiOut>(output), 0x8101};
  TuningController controller(&midi);
  require(controller.retriggerHeldNotes(), "Retrigger stays enabled by default");
  controller.setRetriggerHeldNotes(false);
  {
    auto* second = new Output;
    MidiController other{std::unique_ptr<IMidiOut>(second), 1};
    TuningController restored(&other);
    require(!restored.retriggerHeldNotes(), "Retrigger preference persists");
  }
  for (int i = 0; i < int(kNtetMappings.size()); ++i)
    if (kNtetMappings[i].N == 31) controller.setEdoIndex(i);
  controller.setAdaptingEnabled(true);
  controller.selectTuningCenter(2);
  midi.midiNoteOnReceived(68, 91, 1000);
  midi.midiNoteOnReceived(65, 83, 1010);
  output->messages.clear();
  midi.midiNoteOnReceived(60, 79, 1020);
  require(controller.keyValues()[8].toInt() == -4, "Disabling retrigger leaves adaptive choice unchanged");
  require(std::any_of(output->messages.begin(), output->messages.end(), [](auto m) { return m.status == 0xf0; }),
    "Disabled retrigger still sends tuning");
  require(std::none_of(output->messages.begin(), output->messages.end(), [](auto m) {
    return (m.status & 0xf0) == 0x80 || ((m.status & 0xf0) == 0x90 && m.note != 60);
  }), "Held notes receive no off/on when retrigger is disabled");
  wait(90);
  output->messages.clear();
  controller.startRetuningTest();
  require(!controller.uiSnapshot().retuningTestRunning && output->messages.empty(),
    "Test cannot interfere with held performance notes");
  for (int n : {68, 65, 60}) midi.midiNoteOffReceived(n, 0, 1200);
  controller.selectTuningCenter(0);
  const auto values = controller.keyValues();
  const int center = controller.tuningCenter();
  output->messages.clear(); output->sysexes.clear();
  controller.startRetuningTest();
  require(controller.uiSnapshot().retuningTestRunning, "Probe starts with output");
  const auto initialCount = output->messages.size();
  controller.startRetuningTest();
  require(output->messages.size() == initialCount, "Repeated start cannot overlap probes");
  wait(850);
  require(output->sysexes.size() == 1, "No tuning change before first second");
  wait(300);
  require(output->sysexes.size() == 2 && controller.uiSnapshot().retuningTestRunning,
    "Probe sends tuning while note still sounds");
  wait(1000);
  require(!controller.uiSnapshot().retuningTestRunning && controller.uiSnapshot().retuningTestAwaitingAnswer,
    "Two-second probe completes and asks for human observation");
  require(output->sysexes.size() == 3 && output->sysexes.front() == output->sysexes.back(),
    "Probe restores exact prior tuning table");
  const auto& before = output->sysexes[0];
  const auto& changed = output->sysexes[1];
  require(before.size() == 33 && before[1] == 0x7f && before[3] == 8 && before[4] == 9 &&
    before[5] == 2 && before[6] == 2 && before[7] == 1 && before.back() == 0xf7,
    "Probe uses production real-time MTS scale/octave format and selected channels");
  require(std::equal(before.begin() + 10, before.end(), changed.begin() + 10),
    "Only C tuning changes in probe");
  const int a = before[8] * 128 + before[9], b = changed[8] * 128 + changed[9];
  require(std::abs(a - b) == 6554, "Audible 80-cent change");
  for (int channel : {0, 8, 15})
  {
    require(std::count_if(output->messages.begin(), output->messages.end(), [channel](auto m) {
      return m.status == (0x90 | channel) && m.note == 60 && m.velocity == 80;
    }) == 1, "Probe never retriggers the held note");
    require(std::count_if(output->messages.begin(), output->messages.end(), [channel](auto m) {
      return m.status == (0x80 | channel) && m.note == 60;
    }) == 1, "Probe ends its note on each selected channel");
  }
  require(controller.keyValues() == values && controller.tuningCenter() == center &&
    !controller.pressedKeys().contains(true), "Probe does not enter musical or held-note state");
  require(!controller.retriggerHeldNotes(), "Test never changes the user's retrigger choice");

  output->messages.clear(); output->sysexes.clear();
  controller.startRetuningTest();
  wait(1100);
  controller.cancelRetuningTest();
  require(!controller.uiSnapshot().retuningTestRunning && !controller.uiSnapshot().retuningTestAwaitingAnswer &&
    output->sysexes.front() == output->sysexes.back(), "Cancellation after pitch change restores tuning");
  const auto stoppedCount = output->messages.size();
  wait(1100);
  require(output->messages.size() == stoppedCount, "Cancelled timer sends nothing later");
  controller.startRetuningTest();
  output->messages.clear();
  midi.midiNoteOnReceived(60, 99, 5000);
  require(!controller.uiSnapshot().retuningTestRunning && !output->messages.empty() &&
    output->messages.front().status == 0x80 && output->messages.back().velocity == 99,
    "Incoming playing stops probe before forwarding player's note");
  midi.midiNoteOffReceived(60, 0, 5200);
  controller.startRetuningTest();
  midi.setMidiOutChannelEnabled(9, false);
  require(!controller.uiSnapshot().retuningTestRunning, "Changing output channels cancels probe");
  controller.startRetuningTest();
  midi.stop();
  require(!controller.uiSnapshot().retuningTestRunning, "Shutdown cancels probe before closing output");
  output->acceptMessages = false;
  controller.startRetuningTest();
  require(!controller.uiSnapshot().retuningTestRunning && !controller.uiSnapshot().retuningTestAwaitingAnswer,
    "Disconnected output does not produce a misleading test result");
  output->acceptMessages = true;
  controller.setRetriggerHeldNotes(true);
  std::cout << "PASS: saved retrigger preference, tuning-only mode and two-second MIDI probe lifecycle.\n";
}

void assignableControlTests()
{
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  for (const auto* key : {"controlSource", "controlAction", "controlEnabled", "controlThreshold"})
    settings.remove(QString("status/") + key);
  settings.setValue("status/aftertouchBehaviour", 1);
  settings.setValue("status/aftertouchThreshol", 42);
  auto* output = new Output;
  MidiController midi{std::unique_ptr<IMidiOut>(output), 5};
  TuningController controller(&midi);
  require(controller.controlSource() == 0 && controller.controlAction() == 1 &&
    controller.controlThreshold() == 42 && controller.controlEnabled(), "Migrate old aftertouch direction and threshold");
  require(!settings.contains("status/aftertouchBehaviour"), "Legacy aftertouch preferences retired");
  require(TuningController::controlSources().size() == 124, "All assignable CCs and pressure/bend sources listed");
  for (int i = 0; i < int(kNtetMappings.size()); ++i)
    if (kNtetMappings[i].N == 31) controller.setEdoIndex(i);
  controller.selectTuningCenter(0);
  controller.setAdaptingEnabled(false);
  controller.setControlAction(0);
  controller.setControlThreshold(64);
  midi.midiNoteOnReceived(64, 90, 1000);
  midi.midiNoteOnReceived(76, 90, 1010);
  midi.midiNoteOnReceived(67, 90, 1020);
  controller.setControlSource(1);
  midi.midiChannelMessageReceived(0xa0, 64, 64, 1030);
  require(controller.keyNames()[4] == "Fb" && controller.keyNames()[7] == "G",
    "Poly pressure changes addressed class only, including octave doubles");
  const auto once = controller.keyValues();
  midi.midiChannelMessageReceived(0xa0, 64, 127, 1040);
  midi.midiChannelMessageReceived(0xa0, 64, 50, 1050);
  midi.midiChannelMessageReceived(0xa0, 64, 80, 1060);
  require(controller.keyValues() == once, "Pressure jitter and sustained pressure cannot repeat an action");
  midi.midiChannelMessageReceived(0xa0, 79, 100, 1070);
  require(controller.keyValues() == once, "Poly step ignores notes not physically held");
  midi.midiChannelMessageReceived(0xa0, 64, 30, 1080);
  controller.toggleControlDirection();
  require(controller.controlEnabled() && controller.controlAction() == 1, "Direction toggle leaves activation untouched");
  midi.midiChannelMessageReceived(0xa0, 64, 64, 1090);
  require(controller.keyNames()[4] == "E", "Returning below half threshold rearms the gesture");
  controller.setControlSource(0);
  controller.setControlAction(0);
  midi.midiPressureReceived(127, 1100);
  require(controller.keyNames()[4] == "Fb", "Channel pressure steps an octave-doubled class only once");
  for (int n : {64, 76, 67}) midi.midiNoteOffReceived(n, 0, 1200);
  controller.selectTuningCenter(0);
  midi.midiNoteOnReceived(64, 90, 1300);
  controller.setControlSource(15); // CC11 expression, formerly discarded upstream.
  output->messages.clear();
  midi.midiChannelMessageReceived(0xb0, 11, 127, 1310);
  midi.midiChannelMessageReceived(0xb0, 43, 127, 1320);
  require(controller.keyNames()[4] == "Fb", "Expression CC can trigger tuning");
  require(std::none_of(output->messages.begin(), output->messages.end(), [](auto m) {
    return (m.status & 0xf0) == 0xb0 && (m.note == 11 || m.note == 43);
  }), "Reserved controller and fine-resolution companion do not affect instrument expression");
  controller.setControlEnabled(false);
  output->messages.clear();
  midi.midiChannelMessageReceived(0xb0, 11, 0, 1330);
  require(output->messages.size() == 2 && output->messages.front().note == 11,
    "Disabled assignment passes selected control on all output channels");
  controller.setControlSource(68); // sustain
  midi.midiChannelMessageReceived(0xb0, 64, 127, 1340);
  output->messages.clear();
  controller.setControlEnabled(true);
  require(output->messages.size() == 2 && output->messages.front().note == 64 &&
    output->messages.front().velocity == 0, "Reserving an already-held sustain releases native sustain first");
  controller.setControlSource(2);
  controller.selectTuningCenter(0);
  midi.midiChannelMessageReceived(0xe0, 0, 0, 1400);
  require(controller.keyNames()[4] == "E", "Bend opposite to selected direction does not trigger");
  midi.midiChannelMessageReceived(0xe0, 127, 127, 1410);
  require(controller.keyNames()[4] == "Fb", "Positive pitch bend triggers once");
  midi.midiNoteOffReceived(64, 0, 1500);

  settings.remove("tuningPresetsV2/31");
  controller.selectTuningCenter(0); controller.captureCurrentPreset();
  controller.selectTuningCenter(1); controller.captureCurrentPreset();
  controller.setControlSource(68); controller.setControlAction(2);
  const auto pedal = [&](int value) { midi.midiChannelMessageReceived(0xb0, 64, value, 1600); };
  pedal(0); pedal(127);
  require(controller.currentPresetIndex() == 0, "Next preset wraps from last to first");
  pedal(127);
  require(controller.currentPresetIndex() == 0, "Held pedal does not race through presets");
  pedal(0); pedal(127);
  require(controller.currentPresetIndex() == 1, "Next pedal gesture advances one preset");
  controller.toggleControlDirection();
  require(controller.controlAction() == 3, "Preset direction toggles independently of step actions");
  pedal(0); pedal(127);
  require(controller.currentPresetIndex() == 0, "Previous preset uses saved order");
  pedal(0); pedal(127);
  require(controller.currentPresetIndex() == 1, "Previous preset wraps from first to last");
  controller.selectTuningCenter(2); pedal(0); pedal(127);
  require(controller.currentPresetIndex() == 1, "Previous starts at last when no preset selected");
  settings.remove("tuningPresetsV2/31");
  const auto unchanged = controller.keyValues();
  pedal(0); pedal(127);
  require(controller.keyValues() == unchanged, "Empty preset list is harmless");
  controller.setControlEnabled(false);
  {
    auto* second = new Output;
    MidiController other{std::unique_ptr<IMidiOut>(second), 1};
    TuningController restored(&other);
    require(restored.controlSource() == 68 && restored.controlAction() == 3 &&
      restored.controlThreshold() == 64 && !restored.controlEnabled(), "Persist the complete control assignment");
  }
  std::cout << "PASS: assignable MIDI controls, poly pressure, octave deduplication, gesture latch and preset navigation.\n";
}

void controlRetriggerTests()
{
  for (int source : {0, 15, 1})
    for (int action : {0, 1})
      for (bool enabled : {false, true})
      {
        auto* output = new Output;
        MidiController midi{std::unique_ptr<IMidiOut>(output), 5};
        TuningController controller(&midi);
        for (int i = 0; i < int(kNtetMappings.size()); ++i)
          if (kNtetMappings[i].N == 31) controller.setEdoIndex(i);
        controller.selectTuningCenter(0);
        controller.setAdaptingEnabled(false);
        controller.setControlSource(source);
        controller.setControlAction(action);
        controller.setControlThreshold(64);
        controller.setControlEnabled(true);
        controller.setRetriggerHeldNotes(enabled);
        midi.midiNoteOnReceived(64, 91, 1000);
        midi.midiNoteOnReceived(76, 83, 1010);
        midi.midiNoteOnReceived(67, 72, 1020);
        const auto previous = controller.keyValues();
        output->messages.clear();
        const auto gesture = [&]() {
          if (source == 0) midi.midiPressureReceived(127, 1030);
          else midi.midiChannelMessageReceived(source == 1 ? 0xa0 : 0xb0,
            source == 1 ? 64 : 11, 127, 1030);
        };
        gesture();
        const auto& messages = output->messages;
        const int affected = source == 1 ? 2 : 3;
        const size_t tuningIndex = enabled ? affected * 2 : 0;
        require(messages.size() == size_t(enabled ? affected * 4 + 1 : 1),
          "Control gesture uses a single tuning message and optional off/on pairs only");
        require(messages[tuningIndex].status == 0xf0, "All control NoteOffs precede tuning; all NoteOns follow it");
        for (size_t i = 0; i < tuningIndex; ++i)
          require((messages[i].status & 0xf0) == 0x80, "Stop all affected notes before changing tuning");
        for (size_t i = tuningIndex + 1; i < messages.size(); ++i)
          require((messages[i].status & 0xf0) == 0x90, "Restart only after tuning");
        if (enabled)
          for (int channel : {0, 2})
            for (auto [note, velocity] : std::vector<std::pair<int, int>>{{64, 91}, {76, 83}, {67, 72}})
            {
              const int count = source == 1 && note == 67 ? 0 : 1;
              require(std::count_if(messages.begin(), messages.end(), [&](auto m) {
                return m.status == (0x90 | channel) && m.note == note && m.velocity == velocity;
              }) == count, "Preserve velocities, channels and octave copies; leave unaffected notes alone");
            }
        require(controller.keyValues()[4] != previous[4] && controller.pressedKeys()[4].toBool(),
          "Control tuning applies with either preference and preserves held-key state");
        if (source == 1) require(controller.keyValues()[7] == previous[7], "Poly control leaves other classes unchanged");
        output->messages.clear();
        gesture();
        require(output->messages.empty(), "A held control does not repeatedly retrigger notes");
        for (int note : {64, 76, 67}) midi.midiNoteOffReceived(note, 0, 1200);
      }
  std::cout << "PASS: aftertouch, CC and poly steps obey retrigger preference with one atomic MIDI tuning update.\n";
}

void pressedKeyDisplayTests()
{
  {
    auto *output = new Output;
    MidiController midi{std::unique_ptr<IMidiOut>(output), 1};
    TuningController controller(&midi);
    for (int i = 0; i < int(kNtetMappings.size()); ++i)
      if (kNtetMappings[i].N == 31) controller.setEdoIndex(i);
    controller.selectTuningCenter(0);
    controller.setAdaptingEnabled(false);
    controller.setNoteNamingMode(0);
    midi.midiNoteOnReceived(64, 90, 1000);
    midi.midiNoteOnReceived(76, 90, 1010);
    wait(10);
    require(controller.pressedKeys()[4].toBool(), "E octaves are physically held");
    controller.stepKeyPitch(4, 1);
    require(controller.keyNames()[4] == "Fb" && controller.pressedKeys()[4].toBool(),
            "E -> Fb must update the name without losing the held-key highlight");
    controller.stepKeyPitch(4, -1);
    require(controller.keyNames()[4] == "E" && controller.pressedKeys()[4].toBool(),
            "Lowering a held key also preserves its highlight");
    controller.moveKeyPitchBySteps(4, 1);
    require(controller.keyNames()[4] == "Fb" && controller.pressedKeys()[4].toBool(),
            "Manual drag/step path preserves the physical highlight");
    midi.midiNoteOffReceived(64, 0, 1200);
    require(controller.pressedKeys()[4].toBool(), "One remaining octave keeps the key active");
    midi.midiNoteOffReceived(76, 0, 1210);
    require(!controller.pressedKeys()[4].toBool(), "Last octave release clears the highlight");
    controller.selectTuningCenter(0);
    controller.setControlSource(0);
    controller.setControlAction(0);
    controller.setControlEnabled(true);
    midi.midiNoteOnReceived(64, 90, 2000);
    wait(10);
    midi.midiPressureReceived(0, 2020);
    midi.midiPressureReceived(127, 2030);
    require(controller.keyNames()[4] == "Fb" && controller.pressedKeys()[4].toBool(),
            "Aftertouch E -> Fb retains the held highlight");
    midi.midiNoteOnReceived(64, 0, 2100);
    require(!controller.pressedKeys()[4].toBool(), "Velocity-zero release clears the highlight");
  }
  std::cout << "PASS: pressed-key display survives manual and aftertouch retuning with scale adaptation.\n";
}
void viewModelNotificationTests()
{
  auto *output = new Output;
  MidiController midi{std::unique_ptr<IMidiOut>(output), 1};
  auto *worker = new TuningController(&midi);
  for (int i = 0; i < int(kNtetMappings.size()); ++i)
    if (kNtetMappings[i].N == 31) worker->setEdoIndex(i);
  worker->setAdaptingEnabled(false);
  worker->selectTuningCenter(0);
  worker->setNoteNamingMode(0);
  TuningViewModel model(worker);
  int circleChanges = 0, pressedChanges = 0;
  QObject::connect(&model, &TuningViewModel::circleEntriesChanged, [&]() { ++circleChanges; });
  QObject::connect(&model, &TuningViewModel::pressedKeysChanged, [&]() { ++pressedChanges; });
  QThread thread;
  worker->moveToThread(&thread);
  QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
  thread.start();
  const auto until = [&](auto predicate) {
    QElapsedTimer deadline;
    deadline.start();
    while (!predicate() && deadline.elapsed() < 500) wait(1);
    return predicate();
  };
  midi.midiNoteOnReceived(64, 90, 1000);
  const bool held = until([&]() { return model.pressedKeys()[4].toBool(); });
  const bool onsetSignals = circleChanges == 0 && pressedChanges == 1;
  model.stepKeyPitch(4, 1);
  const bool renamed = until([&]() { return model.keyNames()[4] == "Fb"; });
  const bool tuningSignals = circleChanges == 1 && pressedChanges == 1 &&
                             model.pressedKeys()[4].toBool();
  if (!renamed || !tuningSignals)
    std::cerr << "UI retune: name=" << model.keyNames()[4].toStdString()
              << " circle=" << circleChanges << " pressed=" << pressedChanges
              << " held=" << model.pressedKeys()[4].toBool() << '\n';
  midi.midiNoteOffReceived(64, 0, 1200);
  const bool released = until([&]() { return !model.pressedKeys()[4].toBool(); });
  const bool releaseSignals = circleChanges == 1 && pressedChanges == 2;
  model.requestInitialRefresh();
  wait(20);
  const bool unchangedSignals = circleChanges == 1 && pressedChanges == 2;
  thread.quit();
  thread.wait();
  require(held && onsetSignals, "A key press updates the keyboard without refreshing circle entries");
  require(renamed && tuningSignals, "Retuning refreshes circle names without clearing the physical key");
  require(released && releaseSignals && unchangedSignals,
          "Releases and unchanged snapshots do not refresh circle entries");
  std::cout << "PASS: queued UI notifications update only changed circle/key states.\n";
}
} // namespace
int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir temporary;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
  try
  {
    if (argc > 1)
      replay(QString::fromLocal8Bit(argv[1]));
    else
    {
      triadPivotTests();
      replay(QStringLiteral(INTONA_SOURCE_DIR "/tests/data/ScaleTriadCases.json"));
      midiTests();
      retriggerPreferenceAndProbeTests();
      pressedKeyDisplayTests();
      assignableControlTests();
      controlRetriggerTests();
      viewModelNotificationTests();
    }
  }
  catch (const std::exception &ex)
  {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
