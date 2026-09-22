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
    return true;
  }
  bool sendSysEx(const std::vector<uint8_t> &) override
  {
    messages.push_back({0xf0, 0, 0});
    return true;
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
  settings.setValue("status/useHarmonicCostAdapting", true);
  settings.setValue("status/historyDecaySlope", 7);
  settings.setValue("status/historyWindowIntervals", 1);
  auto *output = new Output;
  MidiController midi{std::unique_ptr<IMidiOut>(output), 5};
  TuningController controller(&midi);
  require(controller.useScaleTriadAdapting(), "Migrate previous alternative checkbox");
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
  require(controller.keyValues() == unchanged, "RT off bypasses alternative decisions");
  for (int note : {64, 66, 68})
    midi.midiNoteOffReceived(note, 0, 8300);
  controller.setAdaptingEnabled(true);
  controller.setUseScaleTriadAdapting(false);
  require(!controller.useScaleTriadAdapting(), "Original engine remains selectable");
  controller.setUseScaleTriadAdapting(true);
  TuningController restored(&midi);
  require(restored.useScaleTriadAdapting(), "New checkbox persists");
  std::cout << "PASS: MIDI ordering, immediate output, 70ms rollback, held triad pivots, RT off, mode migration and "
               "persistence.\n";
}
void pressedKeyDisplayTests()
{
  for (bool alternative : {false, true})
  {
    auto *output = new Output;
    MidiController midi{std::unique_ptr<IMidiOut>(output), 1};
    TuningController controller(&midi);
    controller.setUseScaleTriadAdapting(alternative);
    for (int i = 0; i < int(kNtetMappings.size()); ++i)
      if (kNtetMappings[i].N == 31) controller.setEdoIndex(i);
    controller.selectTuningCenter(0);
    controller.setAdaptingEnabled(false);
    controller.setDirtyNoteThresholdMs(0);
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
    for (int i = 0; i < 3 && !controller.aftertouchText().contains("stepUp"); ++i)
      controller.cycleAftertouchMode();
    midi.midiNoteOnReceived(64, 90, 2000);
    wait(10);
    midi.midiPressureReceived(0, 2020);
    midi.midiPressureReceived(127, 2030);
    require(controller.keyNames()[4] == "Fb" && controller.pressedKeys()[4].toBool(),
            "Aftertouch E -> Fb retains the held highlight");
    midi.midiNoteOnReceived(64, 0, 2100);
    require(!controller.pressedKeys()[4].toBool(), "Velocity-zero release clears the highlight");
  }
  std::cout << "PASS: pressed-key display survives manual and aftertouch retuning in both engines.\n";
}
void viewModelNotificationTests()
{
  auto *output = new Output;
  MidiController midi{std::unique_ptr<IMidiOut>(output), 1};
  auto *worker = new TuningController(&midi);
  for (int i = 0; i < int(kNtetMappings.size()); ++i)
    if (kNtetMappings[i].N == 31) worker->setEdoIndex(i);
  worker->setUseScaleTriadAdapting(false);
  worker->setAdaptingEnabled(false);
  worker->setDirtyNoteThresholdMs(1000);
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
          "Release/unchanged snapshots do not resend unchanged circle entries");
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
      pressedKeyDisplayTests();
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
