#include "midi/MidiController.h"
#include "midi/IntonaMidiConfiguration.h"
#include "MidiMonoIn.h"
#include "IMidiOut.h"
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char *message)
{
  if (!ok) throw std::runtime_error(message);
}
struct Input : IMidiIn
{
  QStringList ports{"Input A", "Input B"};
  int opened = -1, opens = 0;
  Callback callback;
  QStringList listInputs() const override { return ports; }
  bool open(int index) override { opened = index; ++opens; return true; }
  void close() override { opened = -1; }
  void setCallback(Callback cb) override { callback = std::move(cb); }
};
struct Output : IMidiOut
{
  QStringList ports{"Output A", "Output B"};
  int opened = -1, opens = 0;
  QStringList listOutputs() const override { return ports; }
  bool open(int index) override { opened = index; ++opens; return true; }
  void close() override { opened = -1; }
  bool sendShort(uint8_t, uint8_t, uint8_t) override { return opened >= 0; }
  bool sendSysEx(const std::vector<uint8_t> &) override { return opened >= 0; }
};
int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir temporary;
  if (!temporary.isValid()) return 2;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
  QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
  QSettings settings(QSettings::IniFormat, QSettings::UserScope, "NaadaLab", "Intona");
  settings.setValue("midi/inPortName", "Input B");
  settings.setValue("midi/outPortName", "Output B");
  settings.setValue("midi/midiInChn", 3);
  settings.setValue("midi/outChn1", true);
  settings.sync();
  try
  {
    for (int run = 0; run < 2; ++run)
    {
      auto *input = new Input;
      auto *output = new Output;
      if (run)
      {
        input->ports = {"Input B", "Input A"};
        output->ports = {"Output B", "Output A"};
      }
      auto interpreter = std::make_unique<MidiIn_MonoInterpreter>(
          std::unique_ptr<IMidiIn>(input), makeIntonaMidiInConfiguration());
      MidiController controller(std::move(interpreter), std::unique_ptr<IMidiOut>(output));
      const QString in = run ? "Input A" : "Input B", out = run ? "Output A" : "Output B";
      require(controller.midiInPort() == in && controller.midiOutPort() == out,
              "Restore saved port names from the isolated settings store");
      require(controller.midiInChannel() == 3 && controller.midiOutChannelMask() == 2,
              "Restore channel selection");
      int noteOns = 0;
      QObject::connect(&controller, &MidiController::midiNoteOnReceived,
                       [&](int note, int velocity, quint32) {
                         if (note == 60 && velocity == 90) ++noteOns;
                       });
      controller.start();
      require(input->opened == 1 && output->opened == 1 && input->opens == 1 && output->opens == 1,
              "Startup opens both saved ports even though their names have not changed");
      MidiInEvent event;
      event.status = 0x92; event.data1 = 60; event.data2 = 90; event.timeMs = 1000;
      input->callback(event);
      QCoreApplication::processEvents();
      require(noteOns == 1 && controller.midiOut()->sendShort(0x91, 60, 90),
              "Input callback and output work immediately, without reselecting either port");
      if (!run)
      {
        controller.setMidiInPort("Input A");
        controller.setMidiOutPort("Output A");
        require(input->opened == 0 && output->opened == 0, "User selections open the chosen ports");
        settings.sync();
        require(settings.value("midi/inPortName").toString() == "Input A" &&
                    settings.value("midi/outPortName").toString() == "Output A",
                "User selections persist in the same isolated store");
      }
      else
      {
        controller.stop();
        require(input->opened == -1 && output->opened == -1, "Stop closes the ports");
        controller.start();
        require(input->opened == 1 && output->opened == 1, "Restart reopens unchanged selections");
        input->ports.clear(); output->ports.clear();
        controller.refreshMidiInPorts(); controller.refreshMidiOutPorts();
        require(controller.midiInPort() == in && controller.midiOutPort() == out,
                "A temporarily missing device does not erase the saved selection");
        input->ports = {in}; output->ports = {out};
        controller.refreshMidiInPorts(); controller.refreshMidiOutPorts();
        require(input->opened == 0 && output->opened == 0, "Refresh reconnects returning devices by name");
      }
    }
    std::cout << "PASS: isolated MIDI preferences, persistence across restart/reordering, "
                 "immediate input/output and reconnect without reselection.\n";
  }
  catch (const std::exception &error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
