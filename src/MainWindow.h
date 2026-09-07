#pragma once
#include <QMainWindow>
#include <memory>
#include <array>
#include <vector>

#include "IMidiOut.h"
#include "MidiMonoIn.h"

#include "Config.hpp"
#include "ChordRecognizer.h"
#include "tuning/TuningTypes.h"

class MidiSettingsTab;
class SurfaceTab;


//-----------------------------------
class MainWindow : public QMainWindow
//-----------------------------------
{
  Q_OBJECT

  using RetunedNote = Intona::Tuning::RetunedNote;
  using ActiveNote = Intona::Tuning::ActiveNote;
  using KeyChoice = Intona::Tuning::KeyChoice;
  using ChordStructure = Intona::Tuning::ChordStructure;
  using AdaptiveChoice = Intona::Tuning::AdaptiveChoice;
  using ChordRootAnalysis = Intona::Tuning::ChordRootAnalysis;
  using AfterTouch = Intona::Tuning::AfterTouch;
  using ScaleKeyScore = Intona::Tuning::ScaleKeyScore;

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override {}

private:
  void setConfig(const Config* cfg, const NtetMapping& m);
  void sendRpnCoarseFineTuning(double cents);
  void SendTuningSysex(uint8_t N, uint8_t fifthStep, IMidiOut* out);

  std::optional<KeyChoice> chooseBestLocalKey(uint16_t pressedKeyMask12, const Config& config, const NtetMapping& mapping) const;

  const Config* chooseConfigThroughTuningCenter(int8_t tuningCenter);
  AdaptiveChoice chooseBestInterpretationAndConfigByChords(uint8_t midiNote, uint8_t velocity, uint32_t timeMs, bool isOn);

  const Config* findConfigByScale(uint8_t midiNoteOff, uint32_t timeMs, int8_t& rKeyTonic, bool& rIsMinor);
  void resetScaleData();

  void FindBetterTuningCenter(const NtetMapping& m);

  void applyCurrentConfig(IMidiOut& out, bool rebuildMask, uint8_t presetIdx);

  void rebuildAllowedValuesPerKey();
  void updateStepButtonEnablement();
  void stepKeyPitch(int keyIndex, int direction);
  bool isValueAllowedForKey(int keyIndex, int value) const;
  static bool isKeyCompatibleWithTuningCenter(int8_t tuningCenter, int8_t keyTonic, bool isMinor);

  void setEDO(uint8_t idx);
  void enableRTAdapting(bool enable);

  static void rebuildConfigMask(Config& config, const NtetMapping& mapping);
  static std::optional<KeyChoice> inferKeyFromDominantSignature(uint16_t keyPressedMask12, const Config& config);
  static ChordRootAnalysis inferChordRootByStack(const std::vector<ActiveNote>& notes);

  static QString getAftertouchString(AfterTouch aftertouch);

  void handleIncomingNoteOn(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out);
  void handleIncomingNoteOff(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out);
  void handleIncomingChannelMsg(uint8_t code, uint8_t data1, uint8_t data2, uint32_t timeMs, IMidiOut* out);

  void process(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out, bool isOn);

  void loadInitSettings();// tutto all’avvio

  void saveMidiOutPort() const;           // only midi port when it changes
  void saveMidiInPort() const;            // only midi port when it changes
  void saveMidiInChannel() const;         // only midi channel when it changes
  void saveMidiOutChannels() const;       // only midi channels when they change
  void saveEdoIndex() const;              // only EDO index when it changes
  void saveAdaptingEnabled() const;       // only adapting flag when it changes
  void saveAftertouchBehaviour() const;   // only after touch behaviour changes
  void savePresetsForCurrentEDO() const;  // only preset list when it changes

  void loadMidiSettings();
  void loadEdoIndex();
  void loadAdaptingEnabled();
  void loadAftertouchBehaviour();
  void loadPresetsForCurrentEDO();

public slots:
  void onInPortChanged(uint8_t idx);
  void onOutPortChanged(uint8_t idx);
  void onInChannelChanged(uint8_t id);
  void onOutChannelChanged(bool checked);

private slots:
  void onKeyPitchRaiseRequested(int keyIndex);
  void onKeyPitchLowerRequested(int keyIndex);

  void showEdoMenu();
  void captureCurrentConfigPreset();

  void onAfterTouchBehaviourChanged();

  void onMidiNoteOnReceived(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out);
  void onMidiNoteOffReceived(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out);
  void onMidiPressureReceived(uint8_t pressure, uint32_t timeMs, IMidiOut* out);
  void onMidiChannelMsgReceived(uint8_t code, uint8_t data1, uint8_t data2, uint32_t timeMs, IMidiOut* out);
  void onTuningCenterSelected(int8_t tuningCenter);

private:
  MidiSettingsTab* midiSettingTab_ = nullptr;
  SurfaceTab* surfaceTab_ = nullptr;

  std::vector<ActiveNote> activeNotes_;
  int8_t currentKeyTonic_ = Config::invalid;
  bool currentKeyIsMinor_ = false;
  int8_t currentChordRoot_ = Config::invalid;
  QString currentChordName_;

  uint8_t minNoteNumberForAdapting_ = 2;

  ConfigMask pressedMask5_ = 0;
  uint16_t keyPressedMask12_ = 0;
  uint8_t edoIdx_ = 10;
  Config currentConfig_;
  uint8_t invFifthStep_ = 0;

  bool adaptingEnabled_ = true;

  double currentGlobalOffsetCents_ = 0.0;

  AfterTouch afterTouch_ = AfterTouch::stepUp;
  uint8_t afterTouchThreshold_ = 64;
  bool readyToBehaveAftertouch = true;

  std::array<std::vector<int>, 12> allowedValuesPerKey_;

  std::vector<std::array<int8_t, 12>> configPresets_;

  ChordRecognizer chordRecognizer_;

  std::array<uint16_t, 12> majorScaleMask_;
  std::array<uint16_t, 12> minorScaleMask_;
};
