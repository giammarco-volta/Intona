#pragma once

#include "../Config.hpp"
#include "../ChordRecognizer.h"
#include "TuningTypes.h"

#include <QObject>
#include <optional>
#include <QString>
#include <QStringList>
#include <QVariantList>

class MidiController;

namespace Intona::Tuning
{

struct TuningUiSnapshot
{
  int edoIndex = 0;
  int edo = 0;
  QVariantList availableEdos;
  int tuningCenter = Config::invalid;
  QString tuningCenterName;
  QVariantList keyValues;
  QStringList keyNames;
  QVariantList canRaiseKeys;
  QVariantList canLowerKeys;
  QVariantList circleEntries;
  QVariantList presetEntries;
  int currentPresetIndex = -1;
  bool adaptingEnabled = false;
  QString aftertouchText;
  bool aftertouchEnabled = false;
  QString keyDescription;
  QString chordDescription;
  QVariantList pressedKeys;
};

class TuningController final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int edoIndex
             READ edoIndex
             WRITE setEdoIndex
             NOTIFY tuningStateChanged)

  Q_PROPERTY(int edo
             READ edo
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList availableEdos
             READ availableEdos
             CONSTANT)

  Q_PROPERTY(int tuningCenter
             READ tuningCenter
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QString tuningCenterName
             READ tuningCenterName
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList keyValues
             READ keyValues
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QStringList keyNames
             READ keyNames
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList canRaiseKeys
             READ canRaiseKeys
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList canLowerKeys
             READ canLowerKeys
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList circleEntries
             READ circleEntries
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList presetEntries
             READ presetEntries
             NOTIFY tuningStateChanged)

  Q_PROPERTY(int currentPresetIndex
             READ currentPresetIndex
             NOTIFY tuningStateChanged)

  Q_PROPERTY(bool adaptingEnabled
             READ adaptingEnabled
             WRITE setAdaptingEnabled
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QString aftertouchText
             READ aftertouchText
             NOTIFY tuningStateChanged)

  Q_PROPERTY(bool aftertouchEnabled
             READ aftertouchEnabled
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QString keyDescription
             READ keyDescription
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QString chordDescription
             READ chordDescription
             NOTIFY tuningStateChanged)

  Q_PROPERTY(QVariantList pressedKeys
             READ pressedKeys
             NOTIFY tuningStateChanged)

public:
  explicit TuningController(
    MidiController* midiController,
    QObject* parent = nullptr);

  int edoIndex() const;
  void setEdoIndex(int index);

  int edo() const;

  QVariantList availableEdos() const;

  int tuningCenter() const;
  QString tuningCenterName() const;

  Q_INVOKABLE void selectTuningCenter(int value);

  QVariantList keyValues() const;
  QStringList keyNames() const;
  QVariantList canRaiseKeys() const;
  QVariantList canLowerKeys() const;

  Q_INVOKABLE void stepKeyPitch(
    int keyIndex,
    int direction);
  
  QVariantList circleEntries() const;
  QVariantList presetEntries() const;
  int currentPresetIndex() const;

  Q_INVOKABLE void captureCurrentPreset();
  Q_INVOKABLE void applyPreset(int index);
  Q_INVOKABLE void deletePreset(int index);

  bool adaptingEnabled() const;
  void setAdaptingEnabled(bool enabled);

  QString aftertouchText() const;
  bool aftertouchEnabled() const;
  Q_INVOKABLE void cycleAftertouchMode();

  QString keyDescription() const;
  QString chordDescription() const;
  QVariantList pressedKeys() const;
  TuningUiSnapshot uiSnapshot() const;

signals:
  void tuningStateChanged();

private:
  MidiController* midiController_ = nullptr;
  int edoIndex_ = 10;
  Config currentConfig_;
  double currentGlobalOffsetCents_ = 0.0;
  int currentPresetIndex_ = -1;

  std::vector<ActiveNote> activeNotes_;
  int8_t currentKeyTonic_ = Config::invalid;
  bool currentKeyIsMinor_ = false;
  int8_t currentChordRoot_ = Config::invalid;
  QString currentChordName_;

  uint8_t minNoteNumberForAdapting_ = 2;
  ConfigMask pressedMask5_ = 0;
  uint16_t keyPressedMask12_ = 0;
  bool adaptingEnabled_ = true;

  AfterTouch afterTouch_ = AfterTouch::stepUp;
  uint8_t afterTouchThreshold_ = 64;
  bool readyToBehaveAftertouch_ = true;

  ChordRecognizer chordRecognizer_;
  std::array<uint16_t, 12> majorScaleMask_{};
  std::array<uint16_t, 12> minorScaleMask_{};

  std::vector<TuningPreset> loadPresets() const;
  void savePresets(
    const std::vector<TuningPreset>& presets) const;
  void sendCurrentTuning(bool sendGlobalOffset);
  void handleMidiNoteOn(
    int note, int velocity, quint32 timeMs);
  void handleMidiNoteOff(
    int note, int velocity, quint32 timeMs);
  void handleMidiPressure(int pressure);
  void handleMidiChannelMessage(
    int code,
    int data1,
    int data2);

  void processMidiNote(
    uint8_t note,
    uint8_t velocity,
    uint32_t timeMs,
    bool isOn);

  AdaptiveChoice chooseBestInterpretationAndConfigByChords(
    uint8_t midiNote,
    uint8_t velocity,
    uint32_t timeMs,
    bool isOn);

  std::optional<KeyChoice> chooseBestLocalKey(
    uint16_t pressedKeyMask12,
    const Config& config,
    const NtetMapping& mapping) const;

  const Config* findConfigByScale(
    uint8_t midiNoteOff,
    uint32_t timeMs,
    int8_t& keyTonic,
    bool& isMinor);

  void resetScaleData();
  bool adoptConfig(
    const Config& config,
    const NtetMapping& mapping);
  void invalidateIncompatibleKey();
  void sendAllNotesOff();
};

} // namespace Intona::Tuning
