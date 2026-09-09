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

uint16_t keyMaskFor(int16_t tonic5, bool isMinor)
{
  const uint8_t tonic12 =
    uint8_t(fifthToSemitone(tonic5));

  return isMinor
    ? minorKeyMasks[tonic12]
    : majorKeyMasks[tonic12];
}

} // namespace

TuningController::TuningController(
  MidiController* midiController,
  QObject* parent)
  : QObject(parent),
    midiController_(midiController)
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("status");

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

  QSettings settings("NaadaLab", "Intona");
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

  return noteNameFromFifths(
    currentConfig_.tuningCenter);
}

void TuningController::selectTuningCenter(int value)
{
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
    names.append(noteNameFromFifths(value));

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
  const NtetMapping& mapping =
    kNtetMappings[edoIndex_];

  const auto targetValue =
    steppedValueForKey(
      keyIndex,
      direction,
      currentConfig_,
      mapping,
      currentGlobalOffsetCents_);

  if (!targetValue)
    return;

  currentConfig_.valueForKey[keyIndex] = *targetValue;
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

    const bool selected =
      std::find(
        currentConfig_.valueForKey.begin(),
        currentConfig_.valueForKey.end(),
        *value)
      != currentConfig_.valueForKey.end();

    QVariantMap entry;
    entry.insert("pitchStep", pitchStep);
    entry.insert("value", int(*value));
    entry.insert(
      "name",
      noteNameFromFifths(*value));
    entry.insert(
      "cents",
      1200.0 * double(pitchStep)
        / double(mapping.N));
    entry.insert("selected", selected);
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
      names.append(noteNameFromFifths(value));

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

  QSettings settings("NaadaLab", "Intona");
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
  QSettings settings("NaadaLab", "Intona");
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
  currentConfig_ = candidate;
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

  QSettings settings("NaadaLab", "Intona");
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

  QSettings settings("NaadaLab", "Intona");
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
    .arg(noteNameFromFifths(currentKeyTonic_))
    .arg(currentKeyIsMinor_ ? tr("minor") : tr("major"));
}

QString TuningController::chordDescription() const
{
  if (currentChordRoot_ == Config::invalid)
    return {};

  if (!currentChordName_.isEmpty())
    return tr("Chord = %1").arg(currentChordName_);

  return tr("Chord Root = %1")
    .arg(noteNameFromFifths(currentChordRoot_));
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

void TuningController::handleMidiNoteOn(
  int note,
  int velocity,
  quint32 timeMs)
{
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  const uint8_t midiNote = static_cast<uint8_t>(note);
  const uint8_t midiVelocity = static_cast<uint8_t>(velocity);

  keyPressedMask12_ |=
    uint16_t{1} << (midiNote % 12);

  chordRecognizer_.onNoteOn(
    midiNote, midiVelocity, timeMs);

  processMidiNote(
    midiNote, midiVelocity, timeMs, true);

  const quint32 channelMask =
    midiController_->midiOutChannelMask();

  for (int channel = 0; channel < 16; ++channel)
  {
    if ((channelMask & (quint32(1) << channel)) == 0)
      continue;

    sendNoteOn(
      *out,
      static_cast<uint8_t>(channel),
      midiNote,
      midiVelocity);
  }
}

void TuningController::handleMidiNoteOff(
  int note,
  int velocity,
  quint32 timeMs)
{
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  const uint8_t midiNote = static_cast<uint8_t>(note);
  const uint8_t midiVelocity = static_cast<uint8_t>(velocity);

  keyPressedMask12_ &=
    ~(uint16_t{1} << (midiNote % 12));

  chordRecognizer_.onNoteOff(
    midiNote, midiVelocity, timeMs);

  processMidiNote(
    midiNote, midiVelocity, timeMs, false);

  const quint32 channelMask =
    midiController_->midiOutChannelMask();

  for (int channel = 0; channel < 16; ++channel)
  {
    if ((channelMask & (quint32(1) << channel)) == 0)
      continue;

    sendNoteOff(
      *out,
      static_cast<uint8_t>(channel),
      midiNote,
      midiVelocity);
  }
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

void TuningController::processMidiNote(
  uint8_t note,
  uint8_t velocity,
  uint32_t timeMs,
  bool isOn)
{
  IMidiOut* out = midiController_->midiOut();
  if (!out)
    return;

  AdaptiveChoice choice =
    chooseBestInterpretationAndConfigByChords(
      note, velocity, timeMs, isOn);

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

  activeNotes_ = choice.resolvedNotes;
  pressedMask5_ = choice.pressedMask5;
  currentKeyTonic_ = choice.keyTonic;
  currentKeyIsMinor_ = choice.keyIsMinor;
  currentChordRoot_ =
    choice.chordRootValid && activeNotes_.size() >= 3
      ? choice.chordRoot
      : Config::invalid;
  currentChordName_ = choice.chordName;

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

  emit tuningStateChanged();
}

std::optional<KeyChoice>
TuningController::chooseBestLocalKey(
  uint16_t pressedKeyMask12,
  const Config& config,
  const NtetMapping& mapping) const
{
  if (auto dominantKey = inferKeyFromDominantSignature(
        pressedKeyMask12, config))
  {
    return dominantKey;
  }

  const KeyChoice candidates[9] =
  {
    {config.tuningCenter, false},
    {int8_t(config.tuningCenter - 3), false},
    {config.tuningCenter, true},

    {int8_t(config.tuningCenter + 1), false},
    {int8_t(config.tuningCenter - 2), false},
    {int8_t(config.tuningCenter + 1), true},

    {int8_t(config.tuningCenter - 1), false},
    {int8_t(config.tuningCenter - 4), false},
    {int8_t(config.tuningCenter - 1), true}
  };

  for (const auto& key : candidates)
  {
    if (key.tonic != currentKeyTonic_)
      continue;
    if (key.isMinor != currentKeyIsMinor_)
      continue;
    if (key.tonic < mapping.minValue
      || key.tonic > mapping.maxValue)
      continue;
    if (keyContainsMask(
          keyMaskFor(key.tonic, key.isMinor),
          pressedKeyMask12))
      return key;
  }

  std::optional<KeyChoice> best;
  int bestDistance = INT_MAX;

  for (const auto& key : candidates)
  {
    if (key.tonic < mapping.minValue
      || key.tonic > mapping.maxValue)
      continue;
    if (!keyContainsMask(
          keyMaskFor(key.tonic, key.isMinor),
          pressedKeyMask12))
      continue;

    const int distance =
      std::abs(int(key.tonic) - int(currentKeyTonic_));

    if (!best
      || distance < bestDistance
      || (distance == bestDistance
        && key.isMinor == currentKeyIsMinor_))
    {
      best = key;
      bestDistance = distance;
    }
  }

  return best;
}

AdaptiveChoice
TuningController::chooseBestInterpretationAndConfigByChords(
  uint8_t midiNote,
  uint8_t velocity,
  uint32_t timeMs,
  bool isOn)
{
  AdaptiveChoice choice;
  const Chord chord = chordRecognizer_.Recognize();
  const NtetMapping& mapping = kNtetMappings[edoIndex_];
  const Config* selectedConfig = &currentConfig_;
  bool inferChordFromCurrent = false;

  const uint8_t numberOfNotes = isOn
    ? uint8_t(activeNotes_.size() + 1)
    : uint8_t(activeNotes_.size() - 1);

  const bool canAdapt = adaptingEnabled_
    && numberOfNotes >= minNoteNumberForAdapting_
    && !areTwoAdjacentOrDistance2(keyPressedMask12_);

  if (canAdapt && isOn)
  {
    std::vector<int8_t> oldNotes;
    for (const auto& note : activeNotes_)
      oldNotes.push_back(note.interpretedValue);

    selectedConfig = FindConfig(
      chord,
      oldNotes,
      mapping,
      currentConfig_,
      KeepOldNotes::Yes);

    if (!selectedConfig)
    {
      selectedConfig = FindConfig(
        chord,
        oldNotes,
        mapping,
        currentConfig_,
        KeepOldNotes::No);
    }

    if (!selectedConfig)
    {
      selectedConfig = &currentConfig_;
      inferChordFromCurrent = true;
    }
    else
    {
      resetScaleData();
    }
  }
  else
  {
    if (adaptingEnabled_ && !isOn)
    {
      selectedConfig = findConfigByScale(
        midiNote,
        timeMs,
        choice.keyTonic,
        choice.keyIsMinor);
    }

    if (!selectedConfig)
      selectedConfig = &currentConfig_;

    if (!TestChordConfig(chord, *selectedConfig))
      inferChordFromCurrent = true;
  }

  choice.resolvedNotes = activeNotes_;

  if (isOn)
  {
    const uint8_t newKey = uint8_t(midiNote % 12);
    choice.resolvedNotes.push_back({
      midiNote,
      newKey,
      velocity,
      selectedConfig->valueForKey[newKey],
      timeMs});
  }
  else
  {
    choice.resolvedNotes.erase(
      std::remove_if(
        choice.resolvedNotes.begin(),
        choice.resolvedNotes.end(),
        [midiNote](const ActiveNote& note)
        {
          return note.midiNote == midiNote;
        }),
      choice.resolvedNotes.end());
  }

  choice.config = selectedConfig;
  choice.pressedMask5 = 0;

  for (auto& note : choice.resolvedNotes)
  {
    note.interpretedValue =
      choice.config->valueForKey[note.key];
    choice.pressedMask5 |=
      valueToPoolBit(note.interpretedValue);
  }

  const std::vector<ActiveNote>* oldNotes =
    isOn ? &activeNotes_ : &choice.resolvedNotes;

  for (const auto& oldNote : *oldNotes)
  {
    const auto it = std::find_if(
      choice.resolvedNotes.begin(),
      choice.resolvedNotes.end(),
      [&](const ActiveNote& note)
      {
        return note.midiNote == oldNote.midiNote;
      });

    if (it != choice.resolvedNotes.end()
      && it->interpretedValue
        != oldNote.interpretedValue)
    {
      choice.notesToRetrigger.push_back({
        oldNote.midiNote,
        oldNote.velocity,
        oldNote.interpretedValue,
        it->interpretedValue});
    }
  }

  if (choice.keyTonic == Config::invalid)
  {
    const auto keyChoice = chooseBestLocalKey(
      keyPressedMask12_, *choice.config, mapping);

    if (keyChoice && chord.type_ != Chord::typeAug6th)
    {
      choice.keyTonic = keyChoice->tonic;
      choice.keyIsMinor = keyChoice->isMinor;
    }
    else
    {
      choice.keyTonic = currentKeyTonic_;
      choice.keyIsMinor = currentKeyIsMinor_;
    }
  }

  if (inferChordFromCurrent)
  {
    const auto rootAnalysis =
      inferChordRootByStack(choice.resolvedNotes);

    choice.chordRootValid = rootAnalysis.valid;
    choice.chordRoot = rootAnalysis.root;
    choice.chordStructure = rootAnalysis.structure;
  }
  else
  {
    choice.chordRootValid = true;
    choice.chordRoot =
      selectedConfig->valueForKey[chord.root_];
    choice.chordStructure = ChordStructure::Tertian;
    choice.chordName =
      noteNameFromFifths(choice.chordRoot)
      + chord.GetChordString();

    if (chord.bass_ != chord.root_)
    {
      const int8_t bass =
        selectedConfig->valueForKey[chord.bass_];
      choice.chordName += "/" + noteNameFromFifths(bass);
    }
  }

  return choice;
}

const Config* TuningController::findConfigByScale(
  uint8_t midiNoteOff,
  uint32_t timeMs,
  int8_t& keyTonic,
  bool& isMinor)
{
  const auto active = std::find_if(
    activeNotes_.begin(),
    activeNotes_.end(),
    [midiNoteOff](const ActiveNote& note)
    {
      return note.midiNote == midiNoteOff;
    });

  const uint32_t duration = active != activeNotes_.end()
    ? timeMs - active->startMs
    : 0;

  if (duration < 100)
    return nullptr;

  const NtetMapping& mapping = kNtetMappings[edoIndex_];
  int8_t bestKeyTonic = Config::invalid;
  uint32_t bestScore = 0;
  bool bestIsMinor = false;

  for (uint8_t tonic = 0; tonic < 12; ++tonic)
  {
    const uint8_t key = (midiNoteOff - tonic) % 12;

    if (hasKey12(majorKeyMasks[tonic], midiNoteOff))
    {
      majorScaleMask_[tonic] |= uint16_t{1} << key;
      const uint8_t score = popcount(majorScaleMask_[tonic]);

      if (score > bestScore
        || (score == bestScore
          && std::abs(currentConfig_.valueForKey[tonic]
            - currentKeyTonic_)
          < std::abs(bestKeyTonic - currentKeyTonic_)))
      {
        bestKeyTonic = currentConfig_.valueForKey[tonic];
        bestScore = score;
        bestIsMinor = false;
      }
    }

    if (hasKey12(minorKeyMasks[tonic], midiNoteOff))
    {
      static constexpr uint8_t minorSeventh = 10;
      static constexpr uint8_t majorSixth = 9;

      if (key == minorSeventh
        && hasKey12(minorScaleMask_[tonic], majorSixth))
        continue;

      if (key == majorSixth
        && hasKey12(minorScaleMask_[tonic], minorSeventh))
        continue;

      minorScaleMask_[tonic] |= uint16_t{1} << key;
      const uint8_t score = popcount(minorScaleMask_[tonic]);

      if (score > bestScore
        || (score == bestScore
          && std::abs(currentConfig_.valueForKey[tonic]
            - currentKeyTonic_)
          < std::abs(bestKeyTonic - currentKeyTonic_)))
      {
        bestKeyTonic = currentConfig_.valueForKey[tonic];
        bestScore = score;
        bestIsMinor = true;
      }
    }
  }

  if (bestScore < 3
    || (bestKeyTonic == currentKeyTonic_
      && bestIsMinor == currentKeyIsMinor_))
  {
    return nullptr;
  }

  bestKeyTonic = wrapFifthsToMappingRange(
    bestKeyTonic,
    mapping);

  keyTonic = bestKeyTonic;
  isMinor = bestIsMinor;
  resetScaleData();

  return isKeyCompatibleWithTuningCenter(
      currentConfig_.tuningCenter,
      bestKeyTonic,
      bestIsMinor)
    ? &currentConfig_
    : &mapping.getConfig(bestKeyTonic);
}

void TuningController::resetScaleData()
{
  majorScaleMask_.fill(0);
  minorScaleMask_.fill(0);
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
