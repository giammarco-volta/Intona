#include "IMidiOut.h"
#include "midi/MidiController.h"
#include "tuning/ScaleTriadAdapting.h"
#include "tuning/TuningAlgorithms.h"
#include "tuning/TuningController.h"
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
  const auto unchanged = controller.keyValues();
  controller.setAdaptingEnabled(false);
  for (int note : {64, 66, 68})
    midi.midiNoteOnReceived(note, 90, 3000 + note);
  wait(90);
  require(controller.keyValues() == unchanged, "RT off bypasses alternative decisions");
  for (int note : {64, 66, 68})
    midi.midiNoteOffReceived(note, 0, 3300);
  controller.setAdaptingEnabled(true);
  controller.setUseScaleTriadAdapting(false);
  require(!controller.useScaleTriadAdapting(), "Original engine remains selectable");
  controller.setUseScaleTriadAdapting(true);
  TuningController restored(&midi);
  require(restored.useScaleTriadAdapting(), "New checkbox persists");
  std::cout << "PASS: MIDI ordering, immediate output, 70ms rollback, RT off, mode migration and "
               "persistence.\n";
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
      replay(QStringLiteral(INTONA_SOURCE_DIR "/tests/data/ScaleTriadCases.json"));
      midiTests();
    }
  }
  catch (const std::exception &ex)
  {
    std::cerr << ex.what() << '\n';
    return 1;
  }
  return 0;
}
