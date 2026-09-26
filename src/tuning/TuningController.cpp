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
#include "IMidiOut.h"

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

  retriggerHeldNotes_ = settings.value("retriggerHeldNotes", true).toBool();
  retuningTestTimer_ = new QTimer(this);
  retuningTestTimer_->setSingleShot(true);
  retuningTestTimer_->setTimerType(Qt::PreciseTimer);
  connect(retuningTestTimer_, &QTimer::timeout, this, &TuningController::advanceRetuningTest);
  connect(midiController_, &MidiController::midiOutputAboutToChange,
    this, &TuningController::cancelRetuningTest);

  const int savedNamingMode = settings.value("noteNamingMode", 0).toInt();
  if (savedNamingMode == static_cast<int>(NoteNamingMode::LimitedAccidentals))
    noteNamingMode_ = NoteNamingMode::LimitedAccidentals;

  int savedEdoIndex =
    settings.value("edoIndex", 10).toInt();

  adaptingEnabled_ =
    settings.value("adaptingEnabled", true).toBool();

  const int legacyAction = settings.value("aftertouchBehaviour", 0).toInt();
  control_.source = std::clamp(settings.value("controlSource", 0).toInt(), 0, 123);
  control_.action = std::clamp(settings.value("controlAction", legacyAction == 1 ? 1 : 0).toInt(), 0, 3);
  control_.enabled = settings.value("controlEnabled", legacyAction != 2).toBool();
  control_.threshold = std::clamp(settings.value("controlThreshold",
    settings.value("aftertouchThreshol", 10)).toInt(), 1, 127);
  settings.endGroup();
  saveControlBinding(); // One-time migration, including the old misspelled threshold key.
  connect(midiController_, &MidiController::midiInPortChanged, this, [this]() { control_.clearInput(); });
  connect(midiController_, &MidiController::midiInChannelChanged, this, [this]() { control_.clearInput(); });

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

  applySteppedConfig(candidate, false);
}

void TuningController::applySteppedConfig(const Config& candidate, bool retrigger)
{
  if (candidate.valueForKey == currentConfig_.valueForKey) return;
  const auto& mapping = kNtetMappings[edoIndex_];
  auto* out = midiController_->midiOut();
  const auto channels = midiController_->midiOutChannelMask();
  std::vector<ActiveNote> restart;
  if (out && retrigger && retriggerHeldNotes_)
    for (const auto& note : activeNotes_)
      if (mod((candidate.valueForKey[note.key] - currentConfig_.valueForKey[note.key])
              * mapping.fifthStep, mapping.N) != 0)
        restart.push_back(note);
  // A gesture changes all affected classes together: stop them all before the
  // single tuning message, then restart each held octave at its own velocity.
  for (const auto& note : restart)
    for (int channel = 0; channel < 16; ++channel)
      if (channels & (quint32{1} << channel))
        sendNoteOff(*out, uint8_t(channel), note.midiNote, 0);

  resetAdaptiveState();
  currentConfig_ = candidate;
  currentPresetIndex_ = -1;
  rebuildConfigMask(currentConfig_);
  const auto* matchingConfig = findConfigByValues(mapping, currentConfig_.valueForKey);
  currentConfig_.tuningCenter = matchingConfig ? matchingConfig->tuningCenter : Config::invalid;
  sendCurrentTuning(false);
  for (const auto& note : restart)
    for (int channel = 0; channel < 16; ++channel)
      if (channels & (quint32{1} << channel))
        sendNoteOn(*out, uint8_t(channel), note.midiNote, note.velocity);
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

QStringList TuningController::controlSources()
{
  QStringList result{tr("Channel aftertouch"), tr("Polyphonic aftertouch"),
    tr("Pitch bend up"), tr("Pitch bend down")};
  const std::map<int, QString> names{
    {0, tr("Bank select")}, {1, tr("Modulation wheel")}, {2, tr("Breath controller")},
    {4, tr("Foot controller")}, {5, tr("Portamento time")}, {7, tr("Volume")},
    {10, tr("Pan")}, {11, tr("Expression / expression pedal")},
    {12, tr("Effect control 1")}, {13, tr("Effect control 2")},
    {16, tr("General purpose 1")}, {17, tr("General purpose 2")},
    {18, tr("General purpose 3")}, {19, tr("General purpose 4")},
    {64, tr("Sustain pedal")}, {65, tr("Portamento switch")},
    {66, tr("Sostenuto pedal")}, {67, tr("Soft pedal")}, {68, tr("Legato switch")},
    {69, tr("Hold 2")}, {70, tr("Sound variation")}, {71, tr("Resonance")},
    {72, tr("Release time")}, {73, tr("Attack time")}, {74, tr("Brightness")},
    {80, tr("General purpose 5")}, {81, tr("General purpose 6")},
    {82, tr("General purpose 7")}, {83, tr("General purpose 8")},
    {84, tr("Portamento control")}, {91, tr("Reverb depth")}, {93, tr("Chorus depth")}};
  for (int cc = 0; cc < 120; ++cc) {
    const auto it = names.find(cc);
    result.append(it == names.end() ? tr("CC %1").arg(cc)
      : tr("%1 (CC %2)").arg(it->second).arg(cc));
  }
  return result;
}

QString TuningController::controlText() const
{
  QString source;
  if (control_.source == 0) source = tr("Aftertouch");
  else if (control_.source == 1) source = tr("Poly AT");
  else if (control_.source == 2) source = tr("Bend up");
  else if (control_.source == 3) source = tr("Bend down");
  else source = tr("CC %1").arg(control_.source - 4);
  const QStringList actions{tr("step +"), tr("step −"), tr("next preset"), tr("previous preset")};
  return source + " = " + (control_.enabled ? actions[control_.action] : tr("off"));
}

bool TuningController::controlEnabled() const { return control_.enabled; }

void TuningController::saveControlBinding()
{
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  settings.beginGroup("status");
  settings.setValue("controlSource", control_.source);
  settings.setValue("controlAction", control_.action);
  settings.setValue("controlEnabled", control_.enabled);
  settings.setValue("controlThreshold", control_.threshold);
  settings.remove("aftertouchBehaviour");
  settings.remove("aftertouchThreshol");
}

void TuningController::setControlSource(int source)
{
  if (source < 0 || source >= 124 || source == control_.source) return;
  control_.source = source;
  control_.rearm();
  releaseReservedControl();
  saveControlBinding();
  emit tuningStateChanged();
}
void TuningController::setControlAction(int action)
{
  if (action < 0 || action > 3 || action == control_.action) return;
  control_.action = action;
  control_.rearm();
  saveControlBinding();
  emit tuningStateChanged();
}
void TuningController::setControlThreshold(int threshold)
{
  if (threshold < 1 || threshold > 127 || threshold == control_.threshold) return;
  control_.threshold = threshold;
  control_.rearm();
  saveControlBinding();
  emit tuningStateChanged();
}
void TuningController::setControlEnabled(bool enabled)
{
  if (enabled == control_.enabled) return;
  control_.enabled = enabled;
  control_.rearm();
  releaseReservedControl();
  saveControlBinding();
  emit tuningStateChanged();
}
void TuningController::toggleControlDirection() { setControlAction(control_.action ^ 1); }

void TuningController::releaseReservedControl()
{
  if (!control_.enabled) return;
  const int cc = control_.source - 4;
  // Avoid leaving a native sustain/sostenuto active when its release is captured.
  if ((cc == 64 || cc == 66 || cc == 69) && forwardedControls_[cc] >= 64)
    forwardControlMessage(0xb0, cc, 0);
  if ((control_.source == 2 || control_.source == 3) && forwardedPitchBend_)
    forwardControlMessage(0xe0, 0, 64);
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
  snapshot.retriggerHeldNotes = retriggerHeldNotes_;
  snapshot.retuningTestRunning = retuningTestStage_ != 0;
  snapshot.retuningTestAwaitingAnswer = retuningTestAwaitingAnswer_;
  snapshot.retuningTestMessage = retuningTestMessage_;
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
  snapshot.controlSource = controlSource();
  snapshot.controlAction = controlAction();
  snapshot.controlThreshold = controlThreshold();
  snapshot.controlText = controlText();
  snapshot.controlEnabled = controlEnabled();
  snapshot.pressedKeys = pressedKeys();
  return snapshot;
}

void TuningController::sendCurrentTuning(
  bool sendGlobalOffset)
{
  cancelRetuningTest();
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

void TuningController::setRetriggerHeldNotes(bool enabled)
{
  if (retriggerHeldNotes_ == enabled) return;
  retriggerHeldNotes_ = enabled;
  QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "NaadaLab", "Intona");
  settings.setValue("status/retriggerHeldNotes", enabled);
  emit tuningStateChanged();
}

void TuningController::startRetuningTest()
{
  if (retuningTestStage_) return;
  retuningTestAwaitingAnswer_ = false;
  auto* out = midiController_->midiOut();
  const auto channels = uint16_t(midiController_->midiOutChannelMask());
  if (!out || !channels || !activeNotes_.empty())
  {
    retuningTestMessage_ = tr("Release all keys and select a MIDI output channel before starting the test.");
    emit tuningStateChanged();
    return;
  }
  // Resolve pending rollback before snapshotting the tuning being tested.
  if (adaptingEnabled_) { scaleTriads_.advance(eventNow()); applyScaleConfig(); }
  scaleVerificationTimer_->stop();
  const auto& mapping = kNtetMappings[edoIndex_];
  retuningTestTable_ = computeMtsTable(mapping.N, mapping.fifthStep,
    currentConfig_, currentGlobalOffsetCents_);
  retuningTestChannels_ = channels;
  if (!sendTuningTable(*out, channels, retuningTestTable_))
  {
    retuningTestMessage_ = tr("Unable to send the test. Check the MIDI output connection.");
    emit tuningStateChanged();
    scheduleScaleVerification();
    return;
  }
  retuningTestStage_ = 1;
  bool sent = true;
  for (int channel = 0; channel < 16; ++channel)
    if (channels & (uint16_t{1} << channel))
      sent = out->sendShort(0x90 | channel, 60, 80) && sent;
  if (!sent)
  {
    finishRetuningTest(false);
    retuningTestMessage_ = tr("Unable to send the test. Check the MIDI output connection.");
  }
  else
  {
    retuningTestMessage_ = tr("Listen: the pitch should change halfway through the two-second note.");
    retuningTestTimer_->start(1000);
  }
  emit tuningStateChanged();
}

void TuningController::advanceRetuningTest()
{
  if (retuningTestStage_ == 1)
  {
    auto changed = retuningTestTable_;
    // An 80-cent change within MTS's +/-100-cent range, on C only.
    changed[0] = uint16_t(int(changed[0]) + (changed[0] <= 8192 ? 6554 : -6554));
    if (!sendTuningTable(*midiController_->midiOut(), retuningTestChannels_, changed))
    {
      finishRetuningTest(false);
      retuningTestMessage_ = tr("Unable to send the tuning change. Check the MIDI output connection.");
      emit tuningStateChanged();
      return;
    }
    retuningTestStage_ = 2;
    retuningTestTimer_->start(1000);
  }
  else if (retuningTestStage_ == 2) finishRetuningTest(true);
}

void TuningController::cancelRetuningTest()
{
  if (retuningTestStage_) finishRetuningTest(false);
}

void TuningController::finishRetuningTest(bool completed)
{
  retuningTestTimer_->stop();
  retuningTestStage_ = 0;
  if (auto* out = midiController_->midiOut())
  {
    for (int channel = 0; channel < 16; ++channel)
      if (retuningTestChannels_ & (uint16_t{1} << channel))
        sendNoteOff(*out, uint8_t(channel), 60, 0);
    completed = sendTuningTable(*out, retuningTestChannels_, retuningTestTable_) && completed;
  }
  retuningTestAwaitingAnswer_ = completed;
  retuningTestMessage_ = completed ? tr("Did you hear the pitch change while the note was sounding?")
    : tr("Test interrupted. The previous tuning has been restored where the MIDI connection is available.");
  scheduleScaleVerification();
  emit tuningStateChanged();
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
  cancelRetuningTest();
  observeEventClock(timeMs);

  // A repeated Note On is a fresh articulation, never the old note's pivot.
  if (std::any_of(activeNotes_.begin(), activeNotes_.end(),
      [note](const ActiveNote& active) { return active.midiNote == note; }))
    handleMidiNoteOff(note, 0, timeMs);

  if (adaptingEnabled_) { scaleTriads_.advance(eventNow()); applyScaleConfig(); }
  control_.releaseNote(note);
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
  cancelRetuningTest();
  observeEventClock(timeMs);

  control_.releaseNote(note);
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

void TuningController::executeControlAction(int polyNote)
{
  if (control_.action >= 2) {
    const int count = int(loadPresets().size());
    if (!count) return;
    const bool next = control_.action == 2;
    const int index = currentPresetIndex_ < 0 ? (next ? 0 : count - 1)
      : (currentPresetIndex_ + (next ? 1 : count - 1)) % count;
    applyPreset(index);
    return;
  }
  // MTS octave tuning affects pitch classes, not individual octaves. Deduplicate
  // octave doublings so a global gesture moves each key by exactly one step.
  std::array<bool, 12> keys{};
  for (const auto& note : activeNotes_)
    if (polyNote < 0 || note.midiNote == polyNote) keys[note.key] = true;
  Config candidate = currentConfig_;
  const auto& mapping = kNtetMappings[edoIndex_];
  for (int key = 0; key < 12; ++key)
    if (keys[key])
      if (const auto value = steppedValueForKey(key, control_.action == 0 ? 1 : -1,
            candidate, mapping, currentGlobalOffsetCents_))
        candidate.valueForKey[key] = *value;
  applySteppedConfig(candidate, true);
}

void TuningController::handleMidiPressure(int pressure)
{
  handleMidiChannelMessage(0xd0, pressure, 0);
}

void TuningController::handleMidiChannelMessage(int code, int data1, int data2)
{
  if (data1 < 0 || data1 > 127 || data2 < 0 || data2 > 127) return;
  cancelRetuningTest();
  const bool selected = control_.matches(code, data1);
  const bool companion = control_.companion(code, data1);
  const bool triggered = control_.update(code, data1, data2);
  if (control_.enabled && (selected || companion)) {
    if (triggered) executeControlAction(code == 0xa0 ? data1 : -1);
    return;
  }
  // Preserve the previous routing exclusions for unassigned controls only.
  if (code == 0xb0 && !selected && !companion &&
    (data1 == 0 || data1 == 7 || data1 == 10 || data1 == 11 || data1 == 32 || data1 == 71 || data1 == 74)) return;
  forwardControlMessage(code, data1, data2);
}

void TuningController::forwardControlMessage(int code, int data1, int data2)
{
  auto* out = midiController_->midiOut();
  if (!out) return;
  if (code == 0xb0) forwardedControls_[data1] = data2;
  if (code == 0xe0) forwardedPitchBend_ = data1 != 0 || data2 != 64;
  const auto channels = midiController_->midiOutChannelMask();
  for (int channel = 0; channel < 16; ++channel)
    if (channels & (quint32{1} << channel))
      sendChannelMessage(*out, uint8_t(channel), uint8_t(code), uint8_t(data1), uint8_t(data2));
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
    if (retriggerHeldNotes_ && mod((candidate.valueForKey[note.key] - note.interpretedValue) * mapping.fifthStep, mapping.N) != 0)
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

  cancelRetuningTest();
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
