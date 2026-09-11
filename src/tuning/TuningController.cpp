#include "TuningController.h"
#include "TuningAlgorithms.h"
#include "TuningMidiOutput.h"

#include "../midi/MidiController.h"
#include "../Chords.h"
#include "../TuningCenterFinder.h"

#include <QVariantMap>
#include <algorithm>
#include <climits>
#include <cmath>
#include <QSettings>
#include <QTimer>
#include <tuple>

#include "../StringUtilities.hpp"

namespace Intona::Tuning
{

namespace
{

bool keyContainsMask(
  uint16_t keyMask12,
  uint16_t keyPressedMask12)
{
  return (keyPressedMask12 & ~keyMask12) == 0;
}

uint16_t keyMaskFor(int16_t tonic5, bool isMinor, const Config& config)
{
  const auto key = std::find(config.valueForKey.begin(), config.valueForKey.end(), tonic5);
  if (key == config.valueForKey.end())
    return 0;
  const int tonic12 = int(std::distance(config.valueForKey.begin(), key));
  return isMinor ? minorKeyMasks[tonic12] : majorKeyMasks[tonic12];
}

} // namespace

TuningController::TuningController(
  MidiController* midiController,
  QObject* parent)
  : QObject(parent),
    midiController_(midiController)
{
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
    "NaadaLab", "Intona");
  settings.beginGroup("status");

  bool thresholdValid = false;
  const int savedThreshold = settings.value("dirtyNoteThresholdMs", 100).toInt(&thresholdValid);
  if (thresholdValid && savedThreshold >= 0 && savedThreshold <= 1000)
    dirtyNoteThresholdMs_ = savedThreshold;
  noteWindowTimer_ = new QTimer(this);
  noteWindowTimer_->setSingleShot(true);
  noteWindowTimer_->setTimerType(Qt::PreciseTimer);
  connect(noteWindowTimer_, &QTimer::timeout, this, &TuningController::confirmNoteWindow);

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

  Chord::InitChordNames();

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
}

void TuningController::setDirtyNoteThresholdMs(int milliseconds)
{
  if (milliseconds < 0 || milliseconds > 1000 || milliseconds == dirtyNoteThresholdMs_)
    return;
  dirtyNoteThresholdMs_ = milliseconds;
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  settings.setValue("status/dirtyNoteThresholdMs", milliseconds);
  if (noteWindowTimer_->isActive())
  {
    for (auto& pending : pendingNotes_)
    {
      pending.note.minimumDurationMs = milliseconds;
      for (auto& active : activeNotes_)
        if (active.generation == pending.note.generation)
          active.minimumDurationMs = milliseconds;
    }
    pendingNotes_.erase(std::remove_if(pendingNotes_.begin(), pendingNotes_.end(),
      [milliseconds](const PendingNote& note) { return note.released && note.durationMs < uint32_t(milliseconds); }),
      pendingNotes_.end());
    if (pendingNotes_.empty())
      cancelNoteWindow();
    else
      noteWindowTimer_->start(milliseconds);
  }
  emit tuningStateChanged();
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
    pressedMask5_ = 0;
  }

  edoIndex_ = index;
  cancelNoteWindow();
  previousChord_.reset();
  confirmedNotes_.clear();
  chordRecognizer_ = ChordRecognizer{};
  keyPressedMask12_ = 0;
  currentChordRoot_ = Config::invalid;
  currentChordNameValid_ = false;
  currentKeyTonic_ = Config::invalid;
  resetScaleData();

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

QString TuningController::tuningCenterName() const
{
  if (currentConfig_.tuningCenter == Config::invalid)
    return {};

  return noteName(
    currentConfig_.tuningCenter);
}

void TuningController::selectTuningCenter(int value)
{
  cancelNoteWindow();
  previousChord_.reset();
  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  currentConfig_ =
    configForTuningCenter(
      mapping,
      static_cast<int8_t>(value));
  currentPresetIndex_ = -1;

  if (const auto offset = findGlobalOffsetCents(
        currentConfig_,
        mapping,
        currentGlobalOffsetCents_))
  {
    currentGlobalOffsetCents_ = *offset;
  }

  // Held MIDI keys now play the newly assigned notes as well.
  pressedMask5_.reset();
  for (auto& note : activeNotes_)
  {
    note.interpretedValue = currentConfig_.valueForKey[note.key];
    pressedMask5_ |= valueToPoolBit(note.interpretedValue);
  }
  currentChordRoot_ = Config::invalid;
  currentChordNameValid_ = false;
  resetScaleData();
  sendCurrentTuning(true);
  invalidateIncompatibleKey();
  emit tuningStateChanged();
}

QVariantList TuningController::keyValues() const
{
  QVariantList values;
  values.reserve(12);

  for (const int8_t value : currentConfig_.valueForKey)
    values.append(int(value));

  return values;
}

QStringList TuningController::keyNames() const
{
  QStringList names;
  names.reserve(12);

  for (const int8_t value : currentConfig_.valueForKey)
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

  cancelNoteWindow();
  previousChord_.reset();
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

  invalidateIncompatibleKey();
  sendCurrentTuning(false);
  setAdaptingEnabled(false);
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
    entry.insert(
      "pressed",
      (pressedMask5_ & valueToPoolBit(*value)) != 0);
    entry.insert("keyTonic", *value == currentKeyTonic_);
    entry.insert("chordRoot", *value == currentChordRoot_);

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

    for (const int8_t value : preset.values)
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
    const int center = settings.value(
      "tuningCenter", Config::invalid).toInt();
    const double offset = settings.value(
      "globalOffsetCents", 0.0).toDouble();
    settings.endGroup();

    if (!complete || list.size() != 12
      || !std::isfinite(offset))
      continue;

    TuningPreset preset;
    bool valid = true;

    for (int key = 0; key < 12; ++key)
    {
      const int value = list[key].toInt();
      if (value < kConfigMaskMin || value > kConfigMaskMax)
      {
        valid = false;
        break;
      }
      preset.values[key] = static_cast<int8_t>(value);
    }

    if (center != Config::invalid
      && (center < mapping.minValue
        || center > mapping.maxValue))
      valid = false;

    if (!valid)
      continue;

    preset.tuningCenter = static_cast<int8_t>(center);
    preset.globalOffsetCents = offset;

    Config candidate{
      preset.tuningCenter, preset.values, ConfigMask{}};
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
    for (const int8_t value : presets[i].values)
      values.append(int(value));
    settings.setValue("values", values);
    settings.setValue("tuningCenter",
      int(presets[i].tuningCenter));
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
    *offset};
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
    preset.tuningCenter, preset.values, ConfigMask{}};
  const auto offset = findGlobalOffsetCents(
    candidate, mapping, preset.globalOffsetCents);
  if (!offset)
    return;

  rebuildConfigMask(candidate);
  cancelNoteWindow();
  previousChord_.reset();
  currentConfig_ = candidate;
  resetScaleData();
  currentGlobalOffsetCents_ = *offset;
  currentPresetIndex_ = index;
  invalidateIncompatibleKey();
  sendCurrentTuning(true);
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
  cancelNoteWindow();
  previousChord_.reset();
  resetScaleData();

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

QString TuningController::keyDescription() const
{
  if (currentKeyTonic_ == Config::invalid)
    return {};

  return tr("Key = %1 %2")
    .arg(noteName(currentKeyTonic_))
    .arg(currentKeyIsMinor_ ? tr("minor") : tr("major"));
}

QString TuningController::chordDescription() const
{
  if (currentChordRoot_ == Config::invalid)
    return {};

  if (currentChordNameValid_)
  {
    QString name = noteName(currentChordRoot_);
    // Separate step modifiers from chord qualities such as augmented "+".
    if ((name.endsWith('+') || name.endsWith('-'))
      && !currentChordSuffix_.isEmpty())
    {
      name = "(" + name + ")";
    }
    name += currentChordSuffix_;
    if (currentChordBass_ != Config::invalid)
      name += "/" + noteName(currentChordBass_);
    return tr("Chord = %1").arg(name);
  }

  return tr("Chord Root = %1")
    .arg(noteName(currentChordRoot_));
}

QVariantList TuningController::pressedKeys() const
{
  QVariantList result;
  result.reserve(12);

  for (const int8_t value : currentConfig_.valueForKey)
  {
    result.append(
      (pressedMask5_ & valueToPoolBit(value)) != 0);
  }

  return result;
}

TuningUiSnapshot TuningController::uiSnapshot() const
{
  TuningUiSnapshot snapshot;
  snapshot.dirtyNoteThresholdMs = dirtyNoteThresholdMs_;
  snapshot.noteNamingMode = noteNamingMode();
  snapshot.edoIndex = edoIndex();
  snapshot.edo = edo();
  snapshot.availableEdos = availableEdos();
  snapshot.tuningCenter = tuningCenter();
  snapshot.tuningCenterName = tuningCenterName();
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
  snapshot.keyDescription = keyDescription();
  snapshot.chordDescription = chordDescription();
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

void TuningController::rebuildPressedMasks()
{
  keyPressedMask12_ = 0;
  pressedMask5_.reset();
  for (const auto& note : activeNotes_)
  {
    keyPressedMask12_ |= uint16_t{1} << note.key;
    pressedMask5_ |= valueToPoolBit(note.interpretedValue);
  }
}

void TuningController::cancelNoteWindow()
{
  noteWindowTimer_->stop();
  pendingNotes_.clear();
  pivotNotes_.clear();
  chordReference_.reset();
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

  // A repeated Note On is a fresh articulation, never the old note's pivot.
  if (std::any_of(activeNotes_.begin(), activeNotes_.end(),
      [note](const ActiveNote& active) { return active.midiNote == note; }))
    handleMidiNoteOff(note, 0, timeMs);

  if (!noteWindowTimer_->isActive())
  {
    chordReference_ = currentConfig_;
    pivotNotes_ = confirmedNotes_;
    for (auto& pivot : pivotNotes_)
      pivot.interpretedValue = currentConfig_.valueForKey[pivot.key];
    // Explicit UI actions may have cancelled validation of a still-held note.
    // It can rejoin this group, but cannot become a pivot before validation.
    for (auto& active : activeNotes_)
    {
      active.interpretedValue = currentConfig_.valueForKey[active.key];
      if (std::none_of(confirmedNotes_.begin(), confirmedNotes_.end(),
          [&active](const ActiveNote& accepted) { return accepted.generation == active.generation; }))
        pendingNotes_.push_back({active});
    }
  }

  ActiveNote arriving{static_cast<uint8_t>(note), static_cast<uint8_t>(note % 12),
    static_cast<uint8_t>(velocity), currentConfig_.valueForKey[note % 12], timeMs,
    ++nextNoteGeneration_, static_cast<uint32_t>(dirtyNoteThresholdMs_)};
  activeNotes_.push_back(arriving);
  pendingNotes_.push_back({arriving});
  // Only the deadline moves; pivots and the reference were frozen above.
  noteWindowTimer_->start(dirtyNoteThresholdMs_);
  rebuildPressedMasks();

  const auto channelMask = midiController_->midiOutChannelMask();
  for (int channel = 0; channel < 16; ++channel)
    if (channelMask & (quint32{1} << channel))
      sendNoteOn(*out, static_cast<uint8_t>(channel), arriving.midiNote, arriving.velocity);
  emit tuningStateChanged(); // Physical key highlighting, no harmonic decision.
}

void TuningController::handleMidiNoteOff(int note, int velocity, quint32 timeMs)
{
  if (note < 0 || note > 127 || velocity < 0 || velocity > 127)
    return;
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  const auto active = std::find_if(activeNotes_.begin(), activeNotes_.end(),
    [note](const ActiveNote& held) { return held.midiNote == note; });
  if (active != activeNotes_.end())
  {
    const ActiveNote released = *active;
    const uint32_t duration = timeMs - released.startMs;
    const auto pending = std::find_if(pendingNotes_.begin(), pendingNotes_.end(),
      [&released](const PendingNote& entry) { return entry.note.generation == released.generation; });
    if (pending != pendingNotes_.end())
    {
      if (duration < pending->note.minimumDurationMs)
        pendingNotes_.erase(pending); // Never reached recognizer or scale history.
      else
      {
        pending->released = true;
        pending->durationMs = duration;
      }
    }

    const auto confirmed = std::find_if(confirmedNotes_.begin(), confirmedNotes_.end(),
      [&released](const ActiveNote& entry) { return entry.generation == released.generation; });
    if (confirmed != confirmedNotes_.end())
    {
      chordRecognizer_.onNoteOff(released.midiNote, static_cast<uint8_t>(velocity), timeMs);
      if (duration >= released.minimumDurationMs && adaptingEnabled_)
        releasedNotes_.push_back(released); // Consumed only at a future timeout.
      confirmedNotes_.erase(confirmed);
    }
    pivotNotes_.erase(std::remove_if(pivotNotes_.begin(), pivotNotes_.end(),
      [&released](const ActiveNote& pivot) { return pivot.generation == released.generation; }), pivotNotes_.end());
    activeNotes_.erase(active);
    if (pendingNotes_.empty())
      cancelNoteWindow();
    rebuildPressedMasks();
  }

  const auto channelMask = midiController_->midiOutChannelMask();
  for (int channel = 0; channel < 16; ++channel)
    if (channelMask & (quint32{1} << channel))
      sendNoteOff(*out, static_cast<uint8_t>(channel), static_cast<uint8_t>(note), static_cast<uint8_t>(velocity));
  emit tuningStateChanged();
}

void TuningController::confirmNoteWindow()
{
  if (pendingNotes_.empty())
    return;

  if (adaptingEnabled_)
    for (const auto& released : releasedNotes_)
      recordScaleNote(released);
  releasedNotes_.clear();

  bool addedHeldNote = false;
  for (const auto& pending : pendingNotes_)
  {
    if (pending.released)
    {
      if (adaptingEnabled_)
        recordScaleNote(pending.note);
      continue;
    }
    confirmedNotes_.push_back(pending.note);
    chordRecognizer_.onNoteOn(pending.note.midiNote, pending.note.velocity, pending.note.startMs);
    addedHeldNote = true;
  }
  // Clean notes released while the timer was restarted contribute only history.
  // Released notes are not sounding chord voices and cannot be retriggered.
  if (addedHeldNote)
    evaluateNoteWindow();
  cancelNoteWindow();
  emit tuningStateChanged();
}

void TuningController::handleMidiPressure(int pressure)
{
  if (readyToBehaveAftertouch_
    && pressure >= afterTouchThreshold_)
  {
    if (afterTouch_ == AfterTouch::stepUp)
    {
      for (const auto& note : confirmedNotes_)
        stepKeyPitch(note.key, 1);
    }
    else if (afterTouch_ == AfterTouch::stepDown)
    {
      for (const auto& note : confirmedNotes_)
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

void TuningController::evaluateNoteWindow()
{
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  AdaptiveChoice choice =
    chooseBestInterpretationAndConfigByChords();

  const quint32 channelMask =
    midiController_->midiOutChannelMask();

  for (const auto& retriggered : choice.notesToRetrigger)
  {
    for (int channel = 0; channel < 16; ++channel)
    {
      if ((channelMask & (quint32(1) << channel)) == 0)
        continue;

      sendNoteOff(
        *out,
        static_cast<uint8_t>(channel),
        retriggered.midiNote,
        0);
    }
  }

  confirmedNotes_ = choice.resolvedNotes;
  for (auto& note : activeNotes_)
    note.interpretedValue = choice.config->valueForKey[note.key];
  rebuildPressedMasks();
  currentKeyTonic_ = choice.keyTonic;
  currentKeyIsMinor_ = choice.keyIsMinor;
  currentChordRoot_ =
    choice.chordRootValid && confirmedNotes_.size() >= 3
      ? choice.chordRoot
      : Config::invalid;
  currentChordNameValid_ = choice.chordNameValid;
  currentChordSuffix_ = choice.chordSuffix;
  currentChordBass_ = choice.chordBass;

  if (choice.config->valueForKey
    != currentConfig_.valueForKey)
  {
    adoptConfig(
      *choice.config,
      kNtetMappings[edoIndex_]);
    currentPresetIndex_ = -1;
    sendCurrentTuning(false);
  }

  for (const auto& retriggered : choice.notesToRetrigger)
  {
    for (int channel = 0; channel < 16; ++channel)
    {
      if ((channelMask & (quint32(1) << channel)) == 0)
        continue;

      sendNoteOn(
        *out,
        static_cast<uint8_t>(channel),
        retriggered.midiNote,
        retriggered.velocity);
    }
  }

}

std::optional<KeyChoice>
TuningController::chooseBestLocalKey(uint16_t keys, const Config& config,
  const NtetMapping& mapping) const
{
  if (auto dominant = inferKeyFromDominantSignature(keys, config))
    return dominant;
  std::optional<KeyChoice> best;
  std::tuple<int, int, int> bestScore;
  for (int key = 0; key < 12; ++key)
    for (int form = 0; form < 4; ++form)
      if (!(keys & ~ScaleKeys(key, form)) && FitsScale(config, key, form))
      {
        const int tonic = config.valueForKey[key];
        const auto score = std::make_tuple(std::abs(tonic), form, tonic);
        if (!best || score < bestScore)
        {
          best = KeyChoice{static_cast<int8_t>(tonic), form != 0};
          bestScore = score;
        }
      }
  return best;
}

AdaptiveChoice TuningController::chooseBestInterpretationAndConfigByChords()
{
  AdaptiveChoice choice;
  Chord chord = chordRecognizer_.Recognize();
  const NtetMapping& mapping = kNtetMappings[edoIndex_];
  const Config* selectedConfig = &currentConfig_;
  bool inferChordFromCurrent = false;
  uint16_t confirmedMask = 0;
  for (const auto& note : confirmedNotes_)
    confirmedMask |= uint16_t{1} << note.key;

  const bool canAdapt = adaptingEnabled_
    && confirmedNotes_.size() >= minNoteNumberForAdapting_
    && !areTwoAdjacentOrDistance2(confirmedMask);

  if (canAdapt)
  {
    uint16_t pivotKeys = 0;
    for (const auto& note : pivotNotes_)
      pivotKeys |= uint16_t{1} << note.key;
    selectedConfig = FindClosestChordConfig(chord, mapping, *chordReference_,
      previousChord_ ? &*previousChord_ : nullptr, pivotKeys);
    if (!selectedConfig)
    {
      selectedConfig = &currentConfig_;
      inferChordFromCurrent = true;
    }
    else
      resetScaleData();
  }
  else
  {
    if (adaptingEnabled_)
    {
      for (const auto& pending : pendingNotes_)
        if (!pending.released)
          recordScaleNote(pending.note);
      selectedConfig = findConfigByScale(choice.keyTonic, choice.keyIsMinor);
    }
    if (selectedConfig)
      for (const auto& pivot : pivotNotes_)
        if (selectedConfig->valueForKey[pivot.key] != pivot.interpretedValue)
        {
          selectedConfig = nullptr;
          choice.keyTonic = Config::invalid;
          break;
        }
    if (!selectedConfig)
      selectedConfig = &currentConfig_;
    if (!TestChordConfig(chord, *selectedConfig))
      inferChordFromCurrent = true;
  }

  choice.config = selectedConfig;
  choice.resolvedNotes = confirmedNotes_;
  for (auto& note : choice.resolvedNotes)
  {
    note.interpretedValue = selectedConfig->valueForKey[note.key];
    choice.pressedMask5 |= valueToPoolBit(note.interpretedValue);
  }
  // All notes already sound on the MIDI output, including the new group.
  for (const auto& oldNote : activeNotes_)
  {
    const int8_t next = selectedConfig->valueForKey[oldNote.key];
    if (mod((next - oldNote.interpretedValue) * mapping.fifthStep, mapping.N) != 0)
      choice.notesToRetrigger.push_back({oldNote.midiNote, oldNote.velocity, oldNote.interpretedValue, next});
  }

  if (choice.keyTonic == Config::invalid)
  {
    const auto keyChoice = chooseBestLocalKey(confirmedMask, *selectedConfig, mapping);
    if (keyChoice)
    {
      choice.keyTonic = keyChoice->tonic;
      choice.keyIsMinor = keyChoice->isMinor;
    }
    else
    {
      choice.keyTonic = Config::invalid;
      choice.keyIsMinor = false;
    }
  }
  if (inferChordFromCurrent)
  {
    const auto rootAnalysis = inferChordRootByStack(choice.resolvedNotes);
    choice.chordRootValid = rootAnalysis.valid;
    choice.chordRoot = rootAnalysis.root;
    choice.chordStructure = rootAnalysis.structure;
  }
  else if (!chord.IsNull())
  {
    choice.chordRootValid = true;
    choice.chordRoot = static_cast<int8_t>(ChordRootValue(chord, *selectedConfig));
    choice.chordStructure = ChordStructure::Tertian;
    choice.chordNameValid = true;
    choice.chordSuffix = chord.GetChordString();
    if (chord.omitRoot_)
      choice.chordSuffix += "(no root)";
    if (chord.bass_ != chord.root_)
      choice.chordBass = selectedConfig->valueForKey[chord.bass_];
  }
  // Commit context only for a validated, actually voiced chord. Releases,
  // dirty notes and intervening melody never replace the preceding chord.
  if (popcount(confirmedMask) >= 3)
  {
    HarmonicChordContext context;
    context.root = choice.chordRootValid ? choice.chordRoot : Config::invalid;
    context.plainTriad = !inferChordFromCurrent && popcount(confirmedMask) == 3
      && (chord.type_ == Chord::typeMajor || chord.type_ == Chord::typeMinor)
      && chord.t9_ == Chord::tensionVoid9 && chord.t11_ == Chord::tensionVoid11
      && chord.t13_ == Chord::tensionVoid13;
    for (int key = 0; key < 12; ++key)
      if (hasKey12(confirmedMask, key))
        context.notes.push_back(selectedConfig->valueForKey[key]);
    previousChord_ = std::move(context);
  }
  return choice;
}

void TuningController::recordScaleNote(const ActiveNote& note)
{
  melodicKeys_ |= uint16_t{1} << note.key;
}

const Config* TuningController::findConfigByScale(int8_t& tonic, bool& minor)
{
  uint16_t latest = 0, pivots = 0;
  for (const auto& note : confirmedNotes_)
    latest |= uint16_t{1} << note.key;
  for (const auto& note : pivotNotes_)
    pivots |= uint16_t{1} << note.key;
  const Config* selected = FindClosestScaleConfig(kNtetMappings[edoIndex_], currentConfig_,
    melodicKeys_, latest, pivots, tonic, minor);
  if (selected)
    resetScaleData();
  return selected;
}

void TuningController::resetScaleData()
{
  melodicKeys_ = 0;
  releasedNotes_.clear();
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

void TuningController::invalidateIncompatibleKey()
{
  if (currentConfig_.tuningCenter != Config::invalid
    && isKeyCompatibleWithTuningCenter(
      currentConfig_.tuningCenter,
      currentKeyTonic_,
      currentKeyIsMinor_))
  {
    return;
  }

  currentKeyTonic_ = Config::invalid;
  currentKeyIsMinor_ = false;
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
