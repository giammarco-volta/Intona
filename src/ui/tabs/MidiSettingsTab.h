#pragma once

#include <QWidget>
#include "IMidiOut.h"
#include "MidiMonoIn.h"

#include "../../Config.hpp"

class MainWindow;
class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QRadioButton;
class QButtonGroup;
class QString;
class MidiChannelSelector;
struct MidiSetupData;
struct PresetData;

class MidiSettingsTab : public QWidget
{
  Q_OBJECT

public:
  explicit MidiSettingsTab(MainWindow* parent = nullptr);

  IMidiOut& MidiOut() const { return *midiOut_.get(); }

  bool isOutChnEnabled(uint8_t ch) const;

  void set(const MidiSettings& settings);

  QString getSelectedInPort() const;
  QString getSelectedOutPort() const;
  uint8_t getInChannel() const;
  bool getOutChannelEnabled(uint8_t ch) const;

private:
  void connectMidiIn(uint8_t deviceIndex);

  void updateStatus(const QString& s);

  void onRefreshOutPorts();
  void onOutPortChanged(uint8_t idx);

  void onRefreshInPorts();
  void onInPortChanged(uint8_t idx);
  void onInChannelChanged(uint8_t id);

signals:
  void midiNoteOnReceived(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out);
  void midiNoteOffReceived(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out);
  void midiPressureReceived(uint8_t pressure, uint32_t timeMs, IMidiOut* out);
  void midiChannelMsgReceived(uint8_t code, uint8_t data1, uint8_t data2, uint32_t timeMs, IMidiOut* out);

private:
  std::unique_ptr<IMidiOut>               midiOut_;
  std::unique_ptr<MidiIn_MonoInterpreter> midiIn_;

  QComboBox* outPorts_{};
  QPushButton* outRefresh_{};

  MidiChannelSelector* inChannelSpin_ = nullptr;
  QComboBox* inPorts_{};
  QPushButton* inRefresh_{};
  QLabel* inStatus_{};

  std::array<QCheckBox*, 16> chEnable_{};

  QLabel* status_{};
};