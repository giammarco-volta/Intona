#include "TuningController.h"
#include "TuningAlgorithms.h"
#include "TuningMidiOutput.h"

#include "../midi/MidiController.h"

#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <QSettings>

#include "../StringUtilities.hpp"

namespace Intona::Tuning
{

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

  settings.endGroup();

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
    [this](int note, int velocity, quint32)
    {
      handleMidiNoteOn(note, velocity);
    });

  connect(
    midiController_,
    &MidiController::midiNoteOffReceived,
    this,
    [this](int note, int velocity, quint32)
    {
      handleMidiNoteOff(note, velocity);
    });

  connect(
    midiController_,
    &MidiController::midiChannelMessageReceived,
    this,
    [this](int code, int data1, int data2, quint32)
    {
      handleMidiChannelMessage(code, data1, data2);
    });
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

  sendCurrentTuning(false);
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
  int velocity)
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

    sendNoteOn(
      *out,
      static_cast<uint8_t>(channel),
      static_cast<uint8_t>(note),
      static_cast<uint8_t>(velocity));
  }
}

void TuningController::handleMidiNoteOff(
  int note,
  int velocity)
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

    sendNoteOff(
      *out,
      static_cast<uint8_t>(channel),
      static_cast<uint8_t>(note),
      static_cast<uint8_t>(velocity));
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

} // namespace Intona::Tuning
