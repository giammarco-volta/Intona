#include "TuningController.h"
#include "TuningAlgorithms.h"
#include "TuningMidiOutput.h"
#include "RelativeKeyboard.h"

#include "../midi/MidiController.h"

#include <QVariantMap>
#include <algorithm>
#include <climits>
#include <cmath>
#include <QSettings>
#include <QTimer>

#include "../StringUtilities.hpp"

namespace Intona::Tuning
{

TuningController::TuningController(
  MidiController* midiController,
  QObject* parent)
  : QObject(parent),
    midiController_(midiController)
{
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.beginGroup("status");

  for (const auto* obsolete : {"useScaleTriadAdapting", "useHarmonicCostAdapting",
       "useScaleMapAdapting", "dirtyNoteThresholdMs", "historyWindowIntervals",
       "historyDecaySlope", "chromaticCost"}) settings.remove(obsolete);

  scaleVerificationTimer_ = new QTimer(this);
  scaleVerificationTimer_->setSingleShot(true);
  scaleVerificationTimer_->setTimerType(Qt::PreciseTimer);
  connect(scaleVerificationTimer_, &QTimer::timeout, this, &TuningController::verifyScaleEvidence);
  scaleTriads_.admissible = [this](const Config& candidate) {
    return findGlobalOffsetCents(candidate, kNtetMappings[edoIndex_], currentGlobalOffsetCents_).has_value();
  };

  const int savedNamingMode = settings.value("noteNamingMode", 0).toInt();
  if (savedNamingMode == static_cast<int>(NoteNamingMode::LimitedAccidentals))
    noteNamingMode_ = NoteNamingMode::LimitedAccidentals;

  int savedEdoIndex =
    settings.value("edoIndex", 10).toInt();

  adaptingEnabled_ =
    settings.value("adaptingEnabled", true).toBool();

  int aftertouch =
    settings.value("aftertouchBehaviour", 0).toInt();

  int aftertouchThreshold =
    settings.value("aftertouchThreshol", 10).toInt();

  settings.endGroup();

  if (aftertouch < 0 || aftertouch > 2)
    aftertouch = 0;

  if (afterTouchThreshold_ < 10
    || afterTouchThreshold_ > 127)
    aftertouch = 10;

  afterTouch_ = static_cast<AfterTouch>(aftertouch);
  afterTouchThreshold_ =
    static_cast<uint8_t>(aftertouchThreshold);

  if (savedEdoIndex < 0
    || savedEdoIndex >= int(kNtetMappings.size()))
  {
    savedEdoIndex = 10;
  }

  edoIndex_ = savedEdoIndex;

  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  currentConfig_ = mapping.getConfig(0);
  currentPresetIndex_ = -1;

  if (const auto offset = findGlobalOffsetCents(
        currentConfig_, mapping, 0.0))
  {
    currentGlobalOffsetCents_ = *offset;
  }

  connect(
    midiController_,
    &MidiController::midiNoteOnReceived,
    this,
    [this](int note, int velocity, quint32 timeMs)
    {
      handleMidiNoteOn(note, velocity, timeMs);
    });

  connect(
    midiController_,
    &MidiController::midiNoteOffReceived,
    this,
    [this](int note, int velocity, quint32 timeMs)
    {
      handleMidiNoteOff(note, velocity, timeMs);
    });

  connect(
    midiController_,
    &MidiController::midiPressureReceived,
    this,
    [this](int pressure, quint32)
    {
      handleMidiPressure(pressure);
    });

  connect(
    midiController_,
    &MidiController::midiChannelMessageReceived,
    this,
    [this](int code, int data1, int data2, quint32)
    {
      handleMidiChannelMessage(code, data1, data2);
    });

  // La GUI legacy richiama questa stessa transizione dopo
  // il caricamento delle impostazioni.
  cycleAftertouchMode();
  resetAdaptiveState();
}

void TuningController::observeEventClock(quint32 stamp)
{
  const double elapsed = eventNow();
  if (eventClockStarted_)
    eventClockValue_ = std::max(elapsed, eventClockValue_ + std::max(0, int32_t(stamp-eventClockStamp_)));
  else eventClockStarted_ = true;
  eventClockStamp_ = stamp;
  eventClockElapsed_.restart();
}

double TuningController::eventNow() const
{
  return eventClockValue_ + (eventClockElapsed_.isValid() ? eventClockElapsed_.elapsed() : 0);
}

void TuningController::setNoteNamingMode(int mode)
{
  if (mode < static_cast<int>(NoteNamingMode::Fifths)
    || mode > static_cast<int>(NoteNamingMode::LimitedAccidentals)
    || mode == noteNamingMode())
  {
    return;
  }

  noteNamingMode_ = static_cast<NoteNamingMode>(mode);
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.setValue("status/noteNamingMode", mode);
  emit tuningStateChanged();
}

QString TuningController::noteName(int fifths) const
{
  return displayNoteName(
    fifths, kNtetMappings[edoIndex_], noteNamingMode_, currentConfig_.tuningCenter);
}

int TuningController::edoIndex() const
{
  return edoIndex_;
}

void TuningController::setEdoIndex(int index)
{
  if (index < 0
    || index >= int(kNtetMappings.size()))
  {
    return;
  }

  if (edoIndex_ == index)
    return;

  if (!activeNotes_.empty())
  {
    sendAllNotesOff();
    activeNotes_.clear();
  }

  edoIndex_ = index;
  keyPressedMask12_ = 0;

  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  currentConfig_ = mapping.getConfig(0);

  if (const auto offset = findGlobalOffsetCents(
        currentConfig_,
        mapping,
        currentGlobalOffsetCents_))
  {
    currentGlobalOffsetCents_ = *offset;
  }

  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.beginGroup("status");
  settings.setValue("edoIndex", edoIndex_);
  settings.endGroup();

  sendCurrentTuning(true);
  resetAdaptiveState();
  emit tuningStateChanged();
}

int TuningController::edo() const
{
  return kNtetMappings[edoIndex_].N;
}

QVariantList TuningController::availableEdos() const
{
  QVariantList values;
  values.reserve(int(kNtetMappings.size()));

  for (const NtetMapping& mapping : kNtetMappings)
    values.append(int(mapping.N));

  return values;
}

int TuningController::tuningCenter() const
{
  return currentConfig_.tuningCenter;
}

void TuningController::selectTuningCenter(int value)
{
  resetAdaptiveState();
  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  currentConfig_ =
    configForTuningCenter(
      mapping,
      static_cast<int>(value));
  currentPresetIndex_ = -1;

  if (const auto offset = findGlobalOffsetCents(
        currentConfig_,
        mapping,
        currentGlobalOffsetCents_))
  {
    currentGlobalOffsetCents_ = *offset;
  }

  // Held MIDI keys now play the newly assigned notes as well.
  for (auto& note : activeNotes_)
  {
    note.interpretedValue = currentConfig_.valueForKey[note.key];
  }
  sendCurrentTuning(true);
  resetAdaptiveState();
  emit tuningStateChanged();
}

QVariantList TuningController::keyValues() const
{
  QVariantList values;
  values.reserve(12);

  for (const int value : currentConfig_.valueForKey)
    values.append(int(value));

  return values;
}

QStringList TuningController::keyNames() const
{
  QStringList names;
  names.reserve(12);

  for (const int value : currentConfig_.valueForKey)
    names.append(noteName(value));

  return names;
}

QVariantList TuningController::canRaiseKeys() const
{
  QVariantList result;
  result.reserve(12);

  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  for (int key = 0; key < 12; ++key)
  {
    result.append(
      steppedValueForKey(
        key, 1, currentConfig_, mapping,
        currentGlobalOffsetCents_).has_value());
  }

  return result;
}

QVariantList TuningController::canLowerKeys() const
{
  QVariantList result;
  result.reserve(12);

  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  for (int key = 0; key < 12; ++key)
  {
    result.append(
      steppedValueForKey(
        key, -1, currentConfig_, mapping,
        currentGlobalOffsetCents_).has_value());
  }

  return result;
}

void TuningController::stepKeyPitch(
  int keyIndex,
  int direction)
{
  moveKeyPitchBySteps(keyIndex, direction);
}

void TuningController::moveKeyPitchBySteps(
  int keyIndex,
  int stepCount)
{
  if (stepCount == 0)
    return;

  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  Config candidate = currentConfig_;
  const int direction = stepCount > 0 ? 1 : -1;

  for (int step = 0; step < std::abs(stepCount); ++step)
  {
    const auto targetValue = steppedValueForKey(
      keyIndex,
      direction,
      candidate,
      mapping,
      currentGlobalOffsetCents_);

    if (!targetValue)
      return;

    candidate.valueForKey[keyIndex] = *targetValue;
  }

  resetAdaptiveState();
  currentConfig_.valueForKey[keyIndex] =
    candidate.valueForKey[keyIndex];
  currentPresetIndex_ = -1;
  rebuildConfigMask(currentConfig_);

  const Config* matchingConfig =
    findConfigByValues(
      mapping,
      currentConfig_.valueForKey);

  currentConfig_.tuningCenter = matchingConfig
    ? matchingConfig->tuningCenter
    : Config::invalid;

  sendCurrentTuning(false);
  setAdaptingEnabled(false);
  resetAdaptiveState();
  emit tuningStateChanged();
}

QVariantList TuningController::circleEntries() const
{
  QVariantList entries;

  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  entries.reserve(mapping.N);

  for (int pitchStep = 0;
       pitchStep < mapping.N;
       ++pitchStep)
  {
    const auto value =
      spellingForPitchStep(
        pitchStep,
        currentConfig_,
        mapping);

    if (!value)
      continue;

    const auto selectedKey = std::find(
        currentConfig_.valueForKey.begin(),
        currentConfig_.valueForKey.end(),
        *value);

    const int keyIndex = selectedKey
      != currentConfig_.valueForKey.end()
        ? int(std::distance(
            currentConfig_.valueForKey.begin(),
            selectedKey))
        : -1;

    const bool selected = keyIndex >= 0;

    QVariantMap entry;
    entry.insert("pitchStep", pitchStep);
    entry.insert("value", int(*value));
    entry.insert(
      "name",
      noteName(*value));
    entry.insert(
      "cents",
      1200.0 * double(pitchStep)
        / double(mapping.N));
    entry.insert("selected", selected);
    entry.insert("keyIndex", keyIndex);

    entries.append(entry);
  }

  return entries;
}

QVariantList TuningController::presetEntries() const
{
  QVariantList entries;
  const auto presets = loadPresets();

  for (int index = 0;
       index < int(presets.size());
       ++index)
  {
    const TuningPreset& preset = presets[index];

    QStringList names;
    names.reserve(12);

    for (const int value : preset.values)
      names.append(displayNoteName(
        value, kNtetMappings[edoIndex_], noteNamingMode_, preset.tuningCenter));

    QVariantMap entry;
    entry.insert("index", index);
    entry.insert("text", names.join("  "));

    entries.append(entry);
  }

  return entries;
}

int TuningController::currentPresetIndex() const
{
  return currentPresetIndex_;
}

std::vector<TuningPreset>
TuningController::loadPresets() const
{
  std::vector<TuningPreset> presets;
  const NtetMapping& mapping = kNtetMappings[edoIndex_];

  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.beginGroup(
    QString("tuningPresetsV2/%1").arg(mapping.N));

  const int count = settings.value("count", 0).toInt();

  for (int i = 0; i < count; ++i)
  {
    settings.beginGroup(QString("preset%1").arg(i));
    const bool complete = settings.contains("values")
      && settings.contains("tuningCenter")
      && settings.contains("globalOffsetCents");
    const QVariantList list =
      settings.value("values").toList();
    int center = settings.value(
      "tuningCenter", Config::invalid).toInt();
    const double offset = settings.value(
      "globalOffsetCents", 0.0).toDouble();
    const bool relative = settings.value("relativeKeyboard", false).toBool();
    settings.endGroup();
    if (!relative && center == -128) center = Config::invalid; // V2 legacy custom preset.

    if (!complete || list.size() != 12
      || !std::isfinite(offset))
      continue;

    TuningPreset preset;
    bool valid = true;

    for (int key = 0; key < 12; ++key)
    {
      const int value = list[key].toInt();
      if ((!relative && (value < kConfigMaskMin || value > kConfigMaskMax))
        || value < -1000000 || value > 1000000)
      {
        valid = false;
        break;
      }
      preset.values[key] = static_cast<int>(value);
    }

    if (!relative && center != Config::invalid
      && (center < mapping.minValue
        || center > mapping.maxValue))
      valid = false;

    if (!valid)
      continue;

    preset.tuningCenter = static_cast<int>(center);
    preset.globalOffsetCents = offset;
    preset.relativeKeyboard = relative;

    Config candidate{
      preset.tuningCenter, preset.values, ConfigMask{}, preset.relativeKeyboard};
    if (relative && center != Config::invalid
      && (center < -1000000 || center > 1000000 || !relativeKeyboardAnchor(candidate))) continue;
    const auto validated = findGlobalOffsetCents(
      candidate, mapping, offset);

    if (!validated
      || std::abs(*validated - offset) >= 0.0001)
      continue;

    presets.push_back(preset);
  }

  settings.endGroup();
  return presets;
}

void TuningController::savePresets(
  const std::vector<TuningPreset>& presets) const
{
  const int currentEdo = kNtetMappings[edoIndex_].N;
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.beginGroup(
    QString("tuningPresetsV2/%1").arg(currentEdo));
  settings.remove("");
  settings.setValue("count", int(presets.size()));

  for (int i = 0; i < int(presets.size()); ++i)
  {
    settings.beginGroup(QString("preset%1").arg(i));
    QVariantList values;
    for (const int value : presets[i].values)
      values.append(int(value));
    settings.setValue("values", values);
    settings.setValue("tuningCenter",
      int(presets[i].tuningCenter));
    settings.setValue("relativeKeyboard", presets[i].relativeKeyboard);
    settings.setValue("globalOffsetCents",
      presets[i].globalOffsetCents);
    settings.endGroup();
  }
  settings.endGroup();
}

void TuningController::captureCurrentPreset()
{
  const NtetMapping& mapping = kNtetMappings[edoIndex_];
  const auto offset = findGlobalOffsetCents(
    currentConfig_, mapping, currentGlobalOffsetCents_);
  if (!offset)
    return;

  TuningPreset preset{
    currentConfig_.valueForKey,
    currentConfig_.tuningCenter,
    *offset, currentConfig_.relativeKeyboard};
  auto presets = loadPresets();
  const auto existing = std::find_if(
    presets.begin(), presets.end(),
    [&](const TuningPreset& saved)
    {
      return saved.values == preset.values
        && saved.tuningCenter == preset.tuningCenter
        && std::abs(saved.globalOffsetCents
          - preset.globalOffsetCents) < 0.0001;
    });
  if (existing != presets.end())
    return;

  presets.push_back(preset);
  savePresets(presets);
  currentPresetIndex_ = int(presets.size()) - 1;
  emit tuningStateChanged();
}

void TuningController::applyPreset(int index)
{
  const auto presets = loadPresets();
  if (index < 0 || index >= int(presets.size()))
    return;

  const TuningPreset& preset = presets[index];
  const NtetMapping& mapping = kNtetMappings[edoIndex_];
  Config candidate{
    preset.tuningCenter, preset.values, ConfigMask{}, preset.relativeKeyboard};
  const auto offset = findGlobalOffsetCents(
    candidate, mapping, preset.globalOffsetCents);
  if (!offset)
    return;

  rebuildConfigMask(candidate);
  resetAdaptiveState();
  currentConfig_ = candidate;
  currentGlobalOffsetCents_ = *offset;
  currentPresetIndex_ = index;
  sendCurrentTuning(true);
  resetAdaptiveState();
  emit tuningStateChanged();
}

void TuningController::deletePreset(int index)
{
  auto presets = loadPresets();
  if (index < 0 || index >= int(presets.size()))
    return;

  presets.erase(presets.begin() + index);
  if (index == currentPresetIndex_)
    currentPresetIndex_ = -1;
  else if (currentPresetIndex_ > index)
    --currentPresetIndex_;

  savePresets(presets);
  emit tuningStateChanged();
}

bool TuningController::adaptingEnabled() const
{
  return adaptingEnabled_;
}

void TuningController::setAdaptingEnabled(bool enabled)
{
  if (adaptingEnabled_ == enabled)
    return;

  adaptingEnabled_ = enabled;
  resetAdaptiveState();

  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.beginGroup("status");
  settings.setValue("adaptingEnabled", adaptingEnabled_);
  settings.endGroup();

  emit tuningStateChanged();
}

QString TuningController::aftertouchText() const
{
  switch (afterTouch_)
  {
  case AfterTouch::stepUp:
    return QString::fromUtf8("✓ Aftertouch = stepUp");
  case AfterTouch::stepDown:
    return QString::fromUtf8("✓ Aftertouch = stepDown");
  case AfterTouch::off:
    return QString::fromUtf8("✕ Aftertouch = off");
  }

  return {};
}

bool TuningController::aftertouchEnabled() const
{
  return afterTouch_ != AfterTouch::off;
}

void TuningController::cycleAftertouchMode()
{
  switch (afterTouch_)
  {
  case AfterTouch::stepUp:
    afterTouch_ = AfterTouch::stepDown;
    break;
  case AfterTouch::stepDown:
    afterTouch_ = AfterTouch::off;
    break;
  case AfterTouch::off:
    afterTouch_ = AfterTouch::stepUp;
    break;
  }

  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.beginGroup("status");
  settings.setValue(
    "aftertouchBehaviour",
    static_cast<int>(afterTouch_));
  settings.setValue(
    "aftertouchThreshol",
    afterTouchThreshold_);
  settings.endGroup();

  emit tuningStateChanged();
}

QVariantList TuningController::pressedKeys() const
{
  QVariantList result;
  result.reserve(12);

  // Physical key state is independent of its current spelling/intonation.
  for (int key = 0; key < 12; ++key)
    result.append(hasKey12(keyPressedMask12_, key));

  return result;
}

TuningUiSnapshot TuningController::uiSnapshot() const
{
  TuningUiSnapshot snapshot;
  snapshot.noteNamingMode = noteNamingMode();
  snapshot.edoIndex = edoIndex();
  snapshot.edo = edo();
  snapshot.availableEdos = availableEdos();
  snapshot.tuningCenter = tuningCenter();
  snapshot.keyValues = keyValues();
  snapshot.keyNames = keyNames();
  snapshot.canRaiseKeys = canRaiseKeys();
  snapshot.canLowerKeys = canLowerKeys();
  snapshot.circleEntries = circleEntries();
  snapshot.presetEntries = presetEntries();
  snapshot.currentPresetIndex = currentPresetIndex();
  snapshot.adaptingEnabled = adaptingEnabled();
  snapshot.aftertouchText = aftertouchText();
  snapshot.aftertouchEnabled = aftertouchEnabled();
  snapshot.pressedKeys = pressedKeys();
  return snapshot;
}

void TuningController::sendCurrentTuning(
  bool sendGlobalOffset)
{
  if (!midiController_)
    return;

  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  const uint16_t channelMask = static_cast<uint16_t>(
    midiController_->midiOutChannelMask() & 0xffffu);

  if (sendGlobalOffset)
  {
    sendRpnCoarseFineTuning(
      *out,
      channelMask,
      currentGlobalOffsetCents_);
  }

  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  sendTuningSysEx(
    *out,
    channelMask,
    mapping.N,
    mapping.fifthStep,
    currentConfig_,
    currentGlobalOffsetCents_);
}

void TuningController::rebuildPressedKeys()
{
  keyPressedMask12_ = 0;
  for (const auto& note : activeNotes_)
  {
    keyPressedMask12_ |= uint16_t{1} << note.key;
  }
}

void TuningController::handleMidiNoteOn(int note, int velocity, quint32 timeMs)
{
  if (note < 0 || note > 127 || velocity < 0 || velocity > 127)
    return;
  if (velocity == 0)
  {
    handleMidiNoteOff(note, 0, timeMs);
    return;
  }
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;
  observeEventClock(timeMs);

  // A repeated Note On is a fresh articulation, never the old note's pivot.
  if (std::any_of(activeNotes_.begin(), activeNotes_.end(),
      [note](const ActiveNote& active) { return active.midiNote == note; }))
    handleMidiNoteOff(note, 0, timeMs);

  if (adaptingEnabled_) { scaleTriads_.advance(eventNow()); applyScaleConfig(); }
  ActiveNote arriving{static_cast<uint8_t>(note), static_cast<uint8_t>(note % 12),
    static_cast<uint8_t>(velocity), currentConfig_.valueForKey[note % 12],
    ++nextNoteGeneration_};
  const double attackTime = eventNow();
  if (adaptingEnabled_)
  {
    scaleTriads_.noteOn(arriving.generation, arriving.key, attackTime);
    applyScaleConfig(); // Only previously sounding notes can be retriggered.
  }
  arriving.interpretedValue = currentConfig_.valueForKey[arriving.key];
  activeNotes_.push_back(arriving);
  rebuildPressedKeys();
  const auto channels = midiController_->midiOutChannelMask();
  for (int channel = 0; channel < 16; ++channel)
    if (channels & (quint32{1} << channel))
      sendNoteOn(*out, static_cast<uint8_t>(channel), arriving.midiNote, arriving.velocity);
  scheduleScaleVerification();
  emit tuningStateChanged();
}

void TuningController::handleMidiNoteOff(int note, int velocity, quint32 timeMs)
{
  if (note < 0 || note > 127 || velocity < 0 || velocity > 127)
    return;
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;
  observeEventClock(timeMs);

  const auto active = std::find_if(activeNotes_.begin(), activeNotes_.end(),
    [note](const ActiveNote& held) { return held.midiNote == note; });
  if (active != activeNotes_.end())
  {
    if (adaptingEnabled_) scaleTriads_.noteOff(active->generation, eventNow());
    activeNotes_.erase(active);
  }
  const auto channels = midiController_->midiOutChannelMask();
  for (int channel = 0; channel < 16; ++channel)
    if (channels & (quint32{1} << channel))
      sendNoteOff(*out, static_cast<uint8_t>(channel), static_cast<uint8_t>(note), static_cast<uint8_t>(velocity));
  rebuildPressedKeys(); // Release is never a new harmonic decision.
  scheduleScaleVerification();
  emit tuningStateChanged();
}

void TuningController::handleMidiPressure(int pressure)
{
  if (readyToBehaveAftertouch_
    && pressure >= afterTouchThreshold_)
  {
    if (afterTouch_ == AfterTouch::stepUp)
    {
      for (const auto& note : activeNotes_)
        stepKeyPitch(note.key, 1);
    }
    else if (afterTouch_ == AfterTouch::stepDown)
    {
      for (const auto& note : activeNotes_)
        stepKeyPitch(note.key, -1);
    }

    readyToBehaveAftertouch_ = false;
  }
  else if (pressure == 0)
  {
    readyToBehaveAftertouch_ = true;
  }
}

void TuningController::handleMidiChannelMessage(
  int code,
  int data1,
  int data2)
{
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  const quint32 channelMask =
    midiController_->midiOutChannelMask();

  for (int channel = 0; channel < 16; ++channel)
  {
    if ((channelMask & (quint32(1) << channel)) == 0)
      continue;

    sendChannelMessage(
      *out,
      static_cast<uint8_t>(channel),
      static_cast<uint8_t>(code),
      static_cast<uint8_t>(data1),
      static_cast<uint8_t>(data2));
  }
}

void TuningController::resetAdaptiveState()
{
  scaleVerificationTimer_->stop();
  scaleTriads_.reset(currentConfig_);
  for (auto& note : activeNotes_)
  {
    note.interpretedValue = currentConfig_.valueForKey[note.key];
    scaleTriads_.seed(note.generation, note.key, eventNow() - ScaleTriadAdapting::verificationMs);
  }
}

void TuningController::scheduleScaleVerification()
{
  scaleVerificationTimer_->stop();
  if (!adaptingEnabled_) return;
  if (const auto deadline = scaleTriads_.nextDeadline())
    scaleVerificationTimer_->start(std::max(1, int(std::ceil(*deadline - eventNow()))));
}

void TuningController::verifyScaleEvidence()
{
  if (!adaptingEnabled_) return;
  scaleTriads_.advance(eventNow());
  applyScaleConfig();
  rebuildPressedKeys();
  scheduleScaleVerification();
  emit tuningStateChanged();
}

void TuningController::applyScaleConfig()
{
  const auto candidate = scaleTriads_.config();
  if (candidate.valueForKey == currentConfig_.valueForKey
    && candidate.tuningCenter == currentConfig_.tuningCenter) return;
  auto* out = midiController_->midiOut();
  if (!out) return;
  const auto& mapping = kNtetMappings[edoIndex_];
  const auto channels = midiController_->midiOutChannelMask();
  std::vector<ActiveNote> retrigger;
  for (const auto& note : activeNotes_)
    if (mod((candidate.valueForKey[note.key] - note.interpretedValue) * mapping.fifthStep, mapping.N) != 0)
      retrigger.push_back(note);
  for (const auto& note : retrigger)
    for (int channel=0; channel<16; ++channel) if (channels & (quint32{1} << channel))
      sendNoteOff(*out, static_cast<uint8_t>(channel), note.midiNote, 0);
  adoptConfig(candidate, mapping);
  currentPresetIndex_ = -1;
  sendCurrentTuning(false);
  for (auto& note : activeNotes_) note.interpretedValue = candidate.valueForKey[note.key];
  for (const auto& note : retrigger)
    for (int channel=0; channel<16; ++channel) if (channels & (quint32{1} << channel))
      sendNoteOn(*out, static_cast<uint8_t>(channel), note.midiNote, note.velocity);
}

bool TuningController::adoptConfig(
  const Config& config,
  const NtetMapping& mapping)
{
  currentConfig_ = config;

  const auto offset = findGlobalOffsetCents(
    currentConfig_,
    mapping,
    currentGlobalOffsetCents_);

  if (!offset)
    return false;

  currentGlobalOffsetCents_ = *offset;

  if (IMidiOut* out = midiController_->midiOut())
  {
    sendRpnCoarseFineTuning(
      *out,
      static_cast<uint16_t>(
        midiController_->midiOutChannelMask() & 0xffffu),
      currentGlobalOffsetCents_);
  }

  return true;
}

void TuningController::sendAllNotesOff()
{
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  const quint32 channelMask =
    midiController_->midiOutChannelMask();

  for (int channel = 0; channel < 16; ++channel)
  {
    if ((channelMask & (quint32(1) << channel)) != 0)
    {
      Intona::Tuning::sendAllNotesOff(
        *out,
        static_cast<uint8_t>(channel));
    }
  }
}

} // namespace Intona::Tuning
