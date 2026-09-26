#pragma once

#include "../Config.hpp"
#include "TuningTypes.h"
#include "NoteNaming.h"
#include "ScaleTriadAdapting.h"
#include <QElapsedTimer>

#include <QObject>
#include <optional>
#include <QString>
#include <QStringList>
#include <QVariantList>

class MidiController;
class QTimer;

namespace Intona::Tuning
{

struct TuningUiSnapshot
{
  int noteNamingMode = 0;
  int edoIndex = 0;
  int edo = 0;
  QVariantList availableEdos;
  int tuningCenter = Config::invalid;
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

  Q_PROPERTY(QVariantList pressedKeys
             READ pressedKeys
             NOTIFY tuningStateChanged)

public:
  explicit TuningController(
    MidiController* midiController,
    QObject* parent = nullptr);

  int noteNamingMode() const { return static_cast<int>(noteNamingMode_); }
  void setNoteNamingMode(int mode);

  int edoIndex() const;
  void setEdoIndex(int index);

  int edo() const;

  QVariantList availableEdos() const;

  int tuningCenter() const;

  Q_INVOKABLE void selectTuningCenter(int value);

  QVariantList keyValues() const;
  QStringList keyNames() const;
  QVariantList canRaiseKeys() const;
  QVariantList canLowerKeys() const;

  Q_INVOKABLE void stepKeyPitch(
    int keyIndex,
    int direction);

  Q_INVOKABLE void moveKeyPitchBySteps(
    int keyIndex,
    int stepCount);
  
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

  QVariantList pressedKeys() const;
  TuningUiSnapshot uiSnapshot() const;

signals:
  void tuningStateChanged();

private:
  MidiController* midiController_ = nullptr;
  NoteNamingMode noteNamingMode_ = NoteNamingMode::Fifths;
  int edoIndex_ = 10;
  Config currentConfig_;
  double currentGlobalOffsetCents_ = 0.0;
  int currentPresetIndex_ = -1;

  std::vector<ActiveNote> activeNotes_;

  uint16_t keyPressedMask12_ = 0;
  bool adaptingEnabled_ = true;
  ScaleTriadAdapting scaleTriads_;
  QTimer* scaleVerificationTimer_ = nullptr;
  void scheduleScaleVerification();
  void verifyScaleEvidence();
  void applyScaleConfig();
  QElapsedTimer eventClockElapsed_;
  bool eventClockStarted_ = false;
  quint32 eventClockStamp_ = 0;
  double eventClockValue_ = 0;
  void observeEventClock(quint32 stamp);
  double eventNow() const;
  uint64_t nextNoteGeneration_ = 0;
  AfterTouch afterTouch_ = AfterTouch::stepUp;
  uint8_t afterTouchThreshold_ = 64;
  bool readyToBehaveAftertouch_ = true;

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

  void resetAdaptiveState();
  void rebuildPressedKeys();

  QString noteName(int fifths) const;
  bool adoptConfig(
    const Config& config,
    const NtetMapping& mapping);
  void sendAllNotesOff();
};

} // namespace Intona::Tuning
