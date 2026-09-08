#include "MainWindow.h"

#include <QTabWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QString>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDesktopServices>

#include <array>

#include "ui/tabs/MidiSettingsTab.h"
#include "ui/tabs/SurfaceTab.h"

#include "ui/widgets/NtetCircleWidget.h"
#include "ui/widgets/ConfigPresetListWidget.h"

#include "ManualWidget.h"

#include "CentsUtilities.hpp"
#include "StringUtilities.hpp"
#include "Chords.h"
#include "TuningCenterFinder.h"
#include "tuning/TuningAlgorithms.h"
#include "tuning/TuningMidiOutput.h"
#include "About.h"

//-----------------------------------------------------------
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
//-----------------------------------------------------------
{
  setWindowTitle("Intona");

  auto* tabs = new QTabWidget(this);

  midiSettingTab_ = new MidiSettingsTab(this);

  tabs->addTab(midiSettingTab_, tr("Midi Settings"));

  connect(midiSettingTab_, &MidiSettingsTab::midiNoteOnReceived, this, &MainWindow::onMidiNoteOnReceived);
  connect(midiSettingTab_, &MidiSettingsTab::midiNoteOffReceived, this, &MainWindow::onMidiNoteOffReceived);
  connect(midiSettingTab_, &MidiSettingsTab::midiPressureReceived, this, &MainWindow::onMidiPressureReceived);
  connect(midiSettingTab_, &MidiSettingsTab::midiChannelMsgReceived, this, &MainWindow::onMidiChannelMsgReceived);

  setCentralWidget(tabs);
  resize(1200, 700);

  surfaceTab_ = new SurfaceTab(this);
  tabs->addTab(surfaceTab_, tr("Tuning Surface"));

  connect(surfaceTab_->getNtetCircleWidget(), &NtetCircleWidget::tuningCenterSelected, this, &MainWindow::onTuningCenterSelected);
  connect(surfaceTab_->getNtetCircleWidget(), &NtetCircleWidget::adaptingEnabledToggled,
    this, [this]()
    {
      enableRTAdapting(!adaptingEnabled_);
    });

  connect(surfaceTab_->getNtetCircleWidget(), &NtetCircleWidget::keyPitchRaiseRequested, this, &MainWindow::onKeyPitchRaiseRequested);
  connect(surfaceTab_->getNtetCircleWidget(), &NtetCircleWidget::keyPitchLowerRequested, this, &MainWindow::onKeyPitchLowerRequested);

  connect(surfaceTab_->getNtetCircleWidget(), &NtetCircleWidget::edoLabelClicked, this, &MainWindow::showEdoMenu);
  connect(surfaceTab_->getNtetCircleWidget(), &NtetCircleWidget::capturePresetRequested, this, &MainWindow::captureCurrentConfigPreset);

  connect(surfaceTab_->getNtetCircleWidget(), &NtetCircleWidget::afterTouchBehaviourClicked, this, &MainWindow::onAfterTouchBehaviourChanged);

  connect(
    surfaceTab_->getConfigPresetListWidget(),
    &ConfigPresetListWidget::presetSelected,
    this, [this](int index)
    {
      const TuningPreset& preset = configPresets_[index];
      const NtetMapping& mapping = kNtetMappings[edoIdx_];

      Config candidate = currentConfig_;
      candidate.valueForKey = preset.values;
      candidate.tuningCenter = preset.tuningCenter;

      const auto offset = Intona::Tuning::findGlobalOffsetCents(
        candidate,
        mapping,
        preset.globalOffsetCents);

      if (!offset)
      {
        qWarning()
          << "Cannot apply tuning preset"
          << index
          << "for EDO"
          << int(mapping.N)
          << ": no compatible global offset.";

        return;
      }

      currentConfig_ = candidate;
      currentGlobalOffsetCents_ = *offset;

      if (currentConfig_.tuningCenter == Config::invalid
        || !Intona::Tuning::isKeyCompatibleWithTuningCenter(
          currentConfig_.tuningCenter,
          currentKeyTonic_,
          currentKeyIsMinor_))
      {
        currentKeyTonic_ = Config::invalid;
        currentKeyIsMinor_ = false;

        surfaceTab_->getNtetCircleWidget()->setKey(
          currentKeyTonic_,
          currentKeyIsMinor_);
      }

      sendRpnCoarseFineTuning(currentGlobalOffsetCents_);
      applyCurrentConfig(midiSettingTab_->MidiOut(), true, index);
    });

  connect(surfaceTab_->getConfigPresetListWidget(), &ConfigPresetListWidget::presetDeleteRequested,
    this, [this](int index, int current)
    {
      configPresets_.erase(configPresets_.begin() + index);
      surfaceTab_->getConfigPresetListWidget()->setPresets(configPresets_);
      if (index == current)
        surfaceTab_->getConfigPresetListWidget()->setCurrentPresetIndex(-1);
      else if (current > index)
        surfaceTab_->getConfigPresetListWidget()->setCurrentPresetIndex(current - 1);
      savePresetsForCurrentEDO();
    });

  surfaceTab_->getNtetCircleWidget()->setConfig(currentConfig_);

  auto* helpTabs = new QTabWidget(tabs);
  auto* userManualWidget = new UserManualWidget(":/manual/IntonaUserManual.pdf", helpTabs);
  helpTabs->addTab(userManualWidget, tr("Manual"));

  auto* aboutTab = new QWidget(helpTabs);
  auto* aboutLayout = new QVBoxLayout(aboutTab);

  auto* aboutText = new QTextBrowser(aboutTab);
  aboutText->setFrameShape(QFrame::StyledPanel);
  aboutText->setHtml(tr(about));
   
  // Importante: disabilita l'apertura automatica dei link.
  // Così possiamo intercettarli noi.
  aboutText->setOpenLinks(false);
  aboutText->setOpenExternalLinks(false);

  connect(aboutText, &QTextBrowser::anchorClicked,
    this,
    [helpTabs, userManualWidget](const QUrl& url)
    {
      if (url.toString() == "intona:user-manual")
      {
        helpTabs->setCurrentWidget(userManualWidget);
      }
      else
      {
        QDesktopServices::openUrl(url);
      }
    });

  aboutLayout->addWidget(aboutText);

  helpTabs->addTab(aboutTab, tr("About"));
  tabs->addTab(helpTabs, tr("Help"));

  loadInitSettings();
  onAfterTouchBehaviourChanged();
  updateStepButtonEnablement();

  Chord::InitChordNames();
}

//--------------------------------------------
void MainWindow::enableRTAdapting(bool enable)
//--------------------------------------------
{
  if (enable != adaptingEnabled_)
  {
    adaptingEnabled_ = enable;
    surfaceTab_->getNtetCircleWidget()->setAdaptingEnabled(adaptingEnabled_);
    saveAdaptingEnabled();
  }
}

//---------------------------------------------------------------------------------------------------
void MainWindow::onMidiNoteOnReceived(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out)
//---------------------------------------------------------------------------------------------------
{
  handleIncomingNoteOn(note, velocity, timeMs, out);
}

//----------------------------------------------------------------------------------------------------
void MainWindow::onMidiNoteOffReceived(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out)
//----------------------------------------------------------------------------------------------------
{
  handleIncomingNoteOff(note, velocity, timeMs, out);
}

//---------------------------------------------------------------------------------------
void MainWindow::onMidiPressureReceived(uint8_t pressure, uint32_t timeMs, IMidiOut* out)
//---------------------------------------------------------------------------------------
{
  if (readyToBehaveAftertouch && pressure >= afterTouchThreshold_)
  {
    if (afterTouch_ == AfterTouch::stepUp)
    {
      for (const auto& n : activeNotes_)
        onKeyPitchRaiseRequested(n.key);
    }
    else if (afterTouch_ == AfterTouch::stepDown)
    {
      for (const auto& n : activeNotes_)
        onKeyPitchLowerRequested(n.key);

    }
    readyToBehaveAftertouch = false;
  }
  else if (pressure == 0)
    readyToBehaveAftertouch = true;
}

//-------------------------------------------------------------------------------------------------------------------
void MainWindow::onMidiChannelMsgReceived(uint8_t code, uint8_t data1, uint8_t data2, uint32_t timeMs, IMidiOut* out)
//-------------------------------------------------------------------------------------------------------------------
{
  handleIncomingChannelMsg(code, data1, data2, timeMs, out);
}

//----------------------------------------------------------
void MainWindow::onTuningCenterSelected(int8_t tuningCenter)
//----------------------------------------------------------
{
  const auto& mapping = kNtetMappings[edoIdx_];

  setConfig(
    &Intona::Tuning::configForTuningCenter(
      mapping,
      tuningCenter),
    mapping);

  applyCurrentConfig(midiSettingTab_->MidiOut(), false, -1);

  if (!Intona::Tuning::isKeyCompatibleWithTuningCenter(currentConfig_.tuningCenter, currentKeyTonic_, currentKeyIsMinor_))
  {
    currentKeyTonic_ = Config::invalid;
    surfaceTab_->getNtetCircleWidget()->setKey(currentKeyTonic_, currentKeyIsMinor_);
    currentKeyIsMinor_ = false;
  }
}

//-----------------------------------------------------------------
void MainWindow::setConfig(const Config* cfg, const NtetMapping& m)
//-----------------------------------------------------------------
{
  currentConfig_ = *cfg;
  auto offset = Intona::Tuning::findGlobalOffsetCents(currentConfig_, m, currentGlobalOffsetCents_);

  if (!offset)
  {
    // config non applicabile via Standard Tuning + RPN
    qDebug() << "Warning: config with tuning center" << currentConfig_.tuningCenter
      << "is not compatible with any global offset within +/-99 cents. No global offset will be applied.";
    return;
  }
  else
    qDebug() << "currentGlobalOffsetCents_" << *offset;

  currentGlobalOffsetCents_ = *offset;

  sendRpnCoarseFineTuning(currentGlobalOffsetCents_);
}

//----------------------------------------------------
void MainWindow::sendRpnCoarseFineTuning(double cents)
//----------------------------------------------------
{
  uint16_t channelMask = 0;
  for (int channel = 0; channel < 16; ++channel)
    if (midiSettingTab_->isOutChnEnabled(channel))
      channelMask |= uint16_t(1) << channel;

  Intona::Tuning::sendRpnCoarseFineTuning(
    midiSettingTab_->MidiOut(), channelMask, cents);
}
//---------------------------------------------------------------------------
void MainWindow::SendTuningSysex(uint8_t N, uint8_t fifthStep, IMidiOut* out)
//---------------------------------------------------------------------------
{
  uint16_t channelMask = 0;
  for (int channel = 0; channel < 16; ++channel)
    if (midiSettingTab_->isOutChnEnabled(channel))
      channelMask |= uint16_t(1) << channel;

  Intona::Tuning::sendTuningSysEx(
    *out,
    channelMask,
    N,
    fifthStep,
    currentConfig_,
    currentGlobalOffsetCents_);
}


/*
POSSIBILE BRANO DEL MANUALE D'USO


Intona continuously adapts the spelling and tuning of notes according to the harmonic context.
When no notes are held, the system chooses the interpretation that is harmonically closest to the current region of the EDO.

When one or more notes are held (legato playing), Intona preserves the interpretation of the held notes whenever possible.
This constrains the possible reinterpretations of the newly played notes and allows the performer to intentionally navigate different
harmonic regions of the EDO while maintaining continuity.

For example, the keyboard pattern:

4 8 11

is interpreted as:

E G# B

If the performer then moves to:

5 8 0

the result depends on whether the middle note is released or held:

without legato, the system may reinterpret the structure as:
F Ab C
if the G#/Ab key remains held, the system preserves its previous interpretation and therefore prefers:
E# G# B#

This behaviour gives the performer partial control over enharmonic interpretation through voice leading and fingering continuity.
With practice, it becomes possible to intentionally move across different regions of the EDO without unexpected respellings.
*/

//------------------------------------------------------------
QString MainWindow::getAftertouchString(AfterTouch aftertouch)
//------------------------------------------------------------
{
  switch (aftertouch)
  {
  case AfterTouch::stepUp:    return QString::fromUtf8("✓ Aftertouch = stepUp");
  case AfterTouch::stepDown:  return QString::fromUtf8("✓ Aftertouch = stepDown");
  case AfterTouch::off:       return QString::fromUtf8("✕ Aftertouch = off");
  }

  return QString();
}

//------------------------------------------------------------------------
static bool keyContainsMask(uint16_t keyMask12, uint16_t keyPressedMask12)
//------------------------------------------------------------------------
{
  return (keyPressedMask12 & ~keyMask12) == 0;
}

//------------------------------------------------------
static uint16_t keyMaskFor(int16_t tonic5, bool isMinor)
//------------------------------------------------------
{
  const uint8_t tonic12 = uint8_t(fifthToSemitone(tonic5));
  return isMinor ? minorKeyMasks[tonic12] : majorKeyMasks[tonic12];
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
std::optional<MainWindow::KeyChoice> MainWindow::chooseBestLocalKey(uint16_t pressedKeyMask12, const Config& config, const NtetMapping& mapping) const
//----------------------------------------------------------------------------------------------------------------------------------------------------
{
  if (auto dominantKey = Intona::Tuning::inferKeyFromDominantSignature(pressedKeyMask12, config))
  {
    return dominantKey;
  }

  const KeyChoice candidates[9] =
  {
    { config.tuningCenter, false },                  // C major
    { int8_t(config.tuningCenter - 3), false },      // Eb major
    { config.tuningCenter, true  },                  // C minor

    { int8_t(config.tuningCenter + 1), false },     // G major
    { int8_t(config.tuningCenter - 2), false },     // Bb major
    { int8_t(config.tuningCenter + 1), true  },     // G minor

    { int8_t(config.tuningCenter - 1), false },     // F major
    { int8_t(config.tuningCenter - 4), false },     // Ab major
    { int8_t(config.tuningCenter - 1), true  }      // F minor
  };

  // Prima prova a mantenere la tonalità corrente.
  for (const auto& k : candidates)
  {
    if (k.tonic != currentKeyTonic_)
      continue;

    if (k.isMinor != currentKeyIsMinor_)
      continue;

    if (k.tonic < mapping.minValue || k.tonic > mapping.maxValue)
      continue;

    if (keyContainsMask(keyMaskFor(k.tonic, k.isMinor), pressedKeyMask12))
      return k;
  }

  // Altrimenti cerca la tonalità locale compatibile più vicina.
  std::optional<KeyChoice> best;
  int bestDistance = INT_MAX;

  for (const auto& k : candidates)
  {
    if (k.tonic < mapping.minValue || k.tonic > mapping.maxValue)
      continue;

    if (!keyContainsMask(keyMaskFor(k.tonic, k.isMinor), pressedKeyMask12))
      continue;

    const int distance = std::abs(int(k.tonic) - int(currentKeyTonic_));

    if (!best
      || distance < bestDistance
      || (distance == bestDistance && k.isMinor == currentKeyIsMinor_))
    {
      best = k;
      bestDistance = distance;
    }
  }

  return best;
}

//----------------------------------------------------------------------------------------------------------------------------------------------
MainWindow::AdaptiveChoice MainWindow::chooseBestInterpretationAndConfigByChords(uint8_t midiNote, uint8_t velocity, uint32_t timeMs, bool isOn)
//----------------------------------------------------------------------------------------------------------------------------------------------
{
  AdaptiveChoice choice;

  const Chord chord = chordRecognizer_.Recognize();

  const auto& m = kNtetMappings[edoIdx_];

  const Config* selectedConfig = &currentConfig_;
  bool inferChordFromCurrent = false;

  const uint8_t numOfNotes = isOn ? (activeNotes_.size() + 1) : (activeNotes_.size() - 1);
  const bool canAdapt = adaptingEnabled_ && numOfNotes >= minNoteNumberForAdapting_ && !areTwoAdjacentOrDistance2(keyPressedMask12_);

  if (canAdapt && isOn)
  {
    std::vector<int8_t> oldNotes;
    for (const auto& n : activeNotes_)//activeNotes_ doesn't contain yet the new note "note".
      oldNotes.push_back(n.interpretedValue);

    selectedConfig = FindConfig(chord, oldNotes, m, currentConfig_, KeepOldNotes::Yes);

    if (!selectedConfig)
      selectedConfig = FindConfig(chord, oldNotes, m, currentConfig_, KeepOldNotes::No);

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
    if (adaptingEnabled_ && !isOn)
      selectedConfig = findConfigByScale(midiNote, timeMs, choice.keyTonic, choice.keyIsMinor);

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
      timeMs
      });
  }
  else
  {
    choice.resolvedNotes.erase(
      std::remove_if(
        choice.resolvedNotes.begin(),
        choice.resolvedNotes.end(),
        [midiNote](const auto& p)
        {
          return p.midiNote == midiNote;
        }),
      choice.resolvedNotes.end());
  }

  choice.config = selectedConfig;
  choice.pressedMask5 = 0;

  for (auto& n : choice.resolvedNotes)
  {
    n.interpretedValue = choice.config->valueForKey[n.key];
    choice.pressedMask5 |= valueToPoolBit(n.interpretedValue);
  }

  std::vector<ActiveNote>* oldNotes = isOn ? &activeNotes_ : &choice.resolvedNotes;

  for (const auto& oldNote : *oldNotes)
  {
    auto it = std::find_if(
      choice.resolvedNotes.begin(),
      choice.resolvedNotes.end(),
      [&](const ActiveNote& n)
      {
        return n.midiNote == oldNote.midiNote;
      });

    if (it != choice.resolvedNotes.end() && it->interpretedValue != oldNote.interpretedValue)
    {
      choice.notesToRetrigger.push_back({
        oldNote.midiNote,
        oldNote.velocity,
        oldNote.interpretedValue,
        it->interpretedValue
        });
    }
  }

  if (choice.keyTonic == Config::invalid)
  {
    auto keyChoice = chooseBestLocalKey(keyPressedMask12_, *choice.config, m);

    if (keyChoice && chord.type_ != Chord::typeAug6th)//typeAug6th is possible only in the current key, furthermore it has a dominant signature in map12 and that is misleading
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
    auto rootAnalysis = Intona::Tuning::inferChordRootByStack(choice.resolvedNotes);

    choice.chordRootValid = rootAnalysis.valid;
    choice.chordRoot = rootAnalysis.root;
    choice.chordStructure = rootAnalysis.structure;
  }
  else
  {
    choice.chordRootValid = true;
    choice.chordRoot = selectedConfig->valueForKey[chord.root_];
    choice.chordStructure = ChordStructure::Tertian;
    choice.chordName = noteNameFromFifths(choice.chordRoot) + chord.GetChordString();
    if (chord.bass_ != chord.root_)
    {
      int8_t bass = selectedConfig->valueForKey[chord.bass_];
      choice.chordName += "/" + noteNameFromFifths(bass);
    }
  }

  return choice;
}

//------------------------------------------------------------------------------------------------------------------
const Config* MainWindow::findConfigByScale(uint8_t midiNoteOff, uint32_t timeMs, int8_t& rKeyTonic, bool& rIsMinor)
//------------------------------------------------------------------------------------------------------------------
{
  auto it = std::find_if(
    activeNotes_.begin(),
    activeNotes_.end(),
    [&](const ActiveNote& n)
    {
      return n.midiNote == midiNoteOff;
    });

  uint32_t duration = (it != activeNotes_.end()) ? timeMs - it->startMs : 0;
  if (duration < 100)
    return nullptr;

  const auto& m = kNtetMappings[edoIdx_];
  int8_t bestKeyTonic = Config::invalid;
  uint32_t bestScore = 0;//The score in this case is the number of notes belonging to a key.
  bool bestIsMinor = false;

  for (uint8_t t = 0; t < 12; t++)
  {
    uint8_t k = (midiNoteOff - t) % 12;//relative to the tonic t

    if (hasKey12(majorKeyMasks[t], midiNoteOff))
    {
      majorScaleMask_[t] |= uint16_t{ 1 } << k;
      uint8_t s = popcount(majorScaleMask_[t]);
      if (s > bestScore || (s == bestScore && std::abs(currentConfig_.valueForKey[t] - currentKeyTonic_) < std::abs(bestKeyTonic - currentKeyTonic_)))
      {
        bestKeyTonic = currentConfig_.valueForKey[t];
        bestScore = s;
        bestIsMinor = false;
      }
    }

    if (hasKey12(minorKeyMasks[t], midiNoteOff))
    {
      static constexpr uint8_t minorSeventh = 10;
      static constexpr uint8_t majorSixth = 9;

      if (k == minorSeventh && hasKey12(minorScaleMask_[t], majorSixth))
        continue;

      if (k == majorSixth && hasKey12(minorScaleMask_[t], minorSeventh))
        continue;

      minorScaleMask_[t] |= uint16_t{ 1 } << k;
      uint8_t s = popcount(minorScaleMask_[t]);
      if (s > bestScore || (s == bestScore && std::abs(currentConfig_.valueForKey[t] - currentKeyTonic_) < std::abs(bestKeyTonic - currentKeyTonic_)))
      {
        bestKeyTonic = currentConfig_.valueForKey[t];
        bestScore = s;
        bestIsMinor = true;
      }
    }
  }

  if ((bestScore < 3) || (bestKeyTonic == currentKeyTonic_ && bestIsMinor == currentKeyIsMinor_))
    return nullptr;

  bestKeyTonic = wrapFifthsToMappingRange(bestKeyTonic, m);

  rKeyTonic = bestKeyTonic;
  rIsMinor = bestIsMinor;

  resetScaleData();

  return Intona::Tuning::isKeyCompatibleWithTuningCenter(currentConfig_.tuningCenter, bestKeyTonic, bestIsMinor)
          ? &currentConfig_
          : &m.getConfig(bestKeyTonic);
}

//-------------------------------
void MainWindow::resetScaleData()
//-------------------------------
{
  for (auto& s : majorScaleMask_)
    s = 0;

  for (auto& s : minorScaleMask_)
    s = 0;
}

//-------------------------------------------------------------------------------------
void MainWindow::applyCurrentConfig(IMidiOut& out, bool rebuildMask, uint8_t presetIdx)
//-------------------------------------------------------------------------------------
{
  if (rebuildMask)
    Intona::Tuning::rebuildConfigMask(currentConfig_);

  updateStepButtonEnablement();
  SendTuningSysex(kNtetMappings[edoIdx_].N, kNtetMappings[edoIdx_].fifthStep, &out);
  surfaceTab_->getNtetCircleWidget()->setConfig(currentConfig_);
  surfaceTab_->getConfigPresetListWidget()->setCurrentPresetIndex(presetIdx);
}

//-------------------------------------------
void MainWindow::updateStepButtonEnablement()
//-------------------------------------------
{
  const auto& mapping = kNtetMappings[edoIdx_];
  auto* circle = surfaceTab_->getNtetCircleWidget();

  std::array<bool, 12> canRaise{};
  std::array<bool, 12> canLower{};

  for (int key = 0; key < 12; ++key)
  {
    canRaise[key] =
      Intona::Tuning::steppedValueForKey(
        key,
        +1,
        currentConfig_,
        mapping,
        currentGlobalOffsetCents_).has_value();

    canLower[key] =
      Intona::Tuning::steppedValueForKey(
        key,
        -1,
        currentConfig_,
        mapping,
        currentGlobalOffsetCents_).has_value();
  }

  circle->setKeyStepButtonEnabled(canRaise, canLower);
}

//---------------------------------------------------------------------------------------------------
void MainWindow::handleIncomingNoteOn(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out)
//---------------------------------------------------------------------------------------------------
{
  //qDebug() << "NoteOn" << note << timeMs;

  keyPressedMask12_ |= uint16_t{ 1 } << (note % 12);

  chordRecognizer_.onNoteOn(note, velocity, timeMs);

  process(note, velocity, timeMs, out, true);

  for (uint8_t trackIndex = 0; trackIndex < 16; ++trackIndex)
    if (midiSettingTab_->isOutChnEnabled(trackIndex))
      Intona::Tuning::sendNoteOn(
        *out, trackIndex, note, velocity);
}

//----------------------------------------------------------------------------------------------------
void MainWindow::handleIncomingNoteOff(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out)
//----------------------------------------------------------------------------------------------------
{
  //qDebug() << "NoteOff" << note << timeMs;
  
  keyPressedMask12_ &= ~(uint16_t{ 1 } << (note % 12));

  chordRecognizer_.onNoteOff(note, velocity, timeMs);

  process(note, velocity, timeMs, out, false);

  for (uint8_t trackIndex = 0; trackIndex < 16; ++trackIndex)
    if (midiSettingTab_->isOutChnEnabled(trackIndex))
      Intona::Tuning::sendNoteOff(
        *out, trackIndex, note, velocity);
}

//-------------------------------------------------------------------------------------------------
void MainWindow::process(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out, bool isOn)
//-------------------------------------------------------------------------------------------------
{
  AdaptiveChoice choice = chooseBestInterpretationAndConfigByChords(note, velocity, timeMs, isOn);

  for (const auto& n : choice.notesToRetrigger)
    for (uint8_t trackIndex = 0; trackIndex < 16; ++trackIndex)
      if (midiSettingTab_->isOutChnEnabled(trackIndex))
      Intona::Tuning::sendNoteOff(
        *out, trackIndex, n.midiNote, 0);

  activeNotes_ = choice.resolvedNotes;
  pressedMask5_ = choice.pressedMask5;
  currentKeyTonic_ = choice.keyTonic;
  currentKeyIsMinor_ = choice.keyIsMinor;
  currentChordRoot_ = choice.chordRootValid && activeNotes_.size() >= 3 ? choice.chordRoot : Config::invalid;
  currentChordName_ = choice.chordName;

  surfaceTab_->getNtetCircleWidget()->setPressedMask(pressedMask5_);
  surfaceTab_->getNtetCircleWidget()->setKey(currentKeyTonic_, currentKeyIsMinor_);
  surfaceTab_->getNtetCircleWidget()->setChord(currentChordRoot_, currentChordName_);

  if (choice.config->valueForKey != currentConfig_.valueForKey)
  {
    setConfig(choice.config, kNtetMappings[edoIdx_]);
    applyCurrentConfig(*out, false, -1);//A standard tuning sysex will be sent here.
  }

  for (const auto& n : choice.notesToRetrigger)
    for (uint8_t trackIndex = 0; trackIndex < 16; ++trackIndex)
      if (midiSettingTab_->isOutChnEnabled(trackIndex))
        Intona::Tuning::sendNoteOn(
          *out, trackIndex, n.midiNote, n.velocity);
}

//-------------------------------------------------------------------------------------------------------------------
void MainWindow::handleIncomingChannelMsg(uint8_t code, uint8_t data1, uint8_t data2, uint32_t timeMs, IMidiOut* out)
//-------------------------------------------------------------------------------------------------------------------
{
  for (uint8_t trackIndex = 0; trackIndex < 16; ++trackIndex)
    if (midiSettingTab_->isOutChnEnabled(trackIndex))
      Intona::Tuning::sendChannelMessage(
        *out, trackIndex, code, data1, data2);
}

//--------------------------------------
void MainWindow::saveMidiOutPort() const
//--------------------------------------
{
  if (midiSettingTab_ == nullptr)
    return;

  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("midi");
  settings.setValue("outPortName", midiSettingTab_->getSelectedOutPort());
  settings.endGroup();
}

//--------------------------------------
void MainWindow::saveMidiInPort() const
//--------------------------------------
{
  if (midiSettingTab_ == nullptr)
    return;

  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("midi");
  settings.setValue("inPortName", midiSettingTab_->getSelectedInPort());
  settings.endGroup();
}

//----------------------------------------
void MainWindow::saveMidiInChannel() const
//----------------------------------------
{
  if (midiSettingTab_ == nullptr)
    return;

  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("midi");
  settings.setValue("midiInChn", midiSettingTab_->getInChannel());
  settings.endGroup();
}

//------------------------------------------
void MainWindow::saveMidiOutChannels() const
//------------------------------------------
{
  if (midiSettingTab_ == nullptr)
    return;

  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("midi");
  for (uint8_t ch = 0; ch < 16; ++ch)
    settings.setValue(QString("outChn%1").arg(ch), midiSettingTab_->getOutChannelEnabled(ch));
  settings.endGroup();
}

//-----------------------------------
void MainWindow::saveEdoIndex() const
//-----------------------------------
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("status");
  settings.setValue("edoIndex", edoIdx_);
  settings.endGroup();
}

//------------------------------------------
void MainWindow::saveAdaptingEnabled() const
//------------------------------------------
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("status");
  settings.setValue("adaptingEnabled", adaptingEnabled_);
  settings.endGroup();
}

//----------------------------------------------
void MainWindow::saveAftertouchBehaviour() const
//----------------------------------------------
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("status");
  settings.setValue("aftertouchBehaviour", static_cast<int>(afterTouch_));
  settings.setValue("aftertouchThreshol", afterTouchThreshold_);
  settings.endGroup();
}

//-----------------------------------------------
void MainWindow::savePresetsForCurrentEDO() const
//-----------------------------------------------
{
  const int edo = kNtetMappings[edoIdx_].N;

  QSettings settings("NaadaLab", "Intona");

  settings.beginGroup(
    QString("tuningPresetsV2/%1").arg(edo));

  settings.remove("");
  settings.setValue("count", int(configPresets_.size()));

  for (int i = 0; i < int(configPresets_.size()); ++i)
  {
    const TuningPreset& preset = configPresets_[i];

    settings.beginGroup(QString("preset%1").arg(i));

    QVariantList values;

    for (const int8_t value : preset.values)
      values << int(value);

    settings.setValue("values", values);
    settings.setValue(
      "tuningCenter",
      int(preset.tuningCenter));
    settings.setValue(
      "globalOffsetCents",
      preset.globalOffsetCents);

    settings.endGroup();
  }

  settings.endGroup();
}
//---------------------------------
void MainWindow::loadInitSettings()
//---------------------------------
{
  loadMidiSettings();
  loadEdoIndex();//it will call loadPresetsForCurrentEDO();
  loadAdaptingEnabled();
  loadAftertouchBehaviour();
}

//---------------------------------
void MainWindow::loadMidiSettings()
//---------------------------------
{
  QSettings settings("NaadaLab", "Intona");

  MidiSettings init;
  settings.beginGroup("midi");
  init.midiOutPort = settings.value("outPortName").toString();
  init.midiInPort = settings.value("inPortName").toString();
  init.midiInChannel = settings.value("midiInChn", 0).toUInt();
  for (uint8_t ch = 0; ch < 16; ++ch)
    init.outChnEnabled[ch] = settings.value(QString("outChn%1").arg(ch), false).toBool();
  settings.endGroup();

  midiSettingTab_->set(init);
}

//-----------------------------
void MainWindow::loadEdoIndex()
//-----------------------------
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("status");
  int edoIdx = settings.value("edoIndex", 10).toInt();
  settings.endGroup();

  if (edoIdx < 0 || edoIdx >= int(kNtetMappings.size()))
    edoIdx = 10;

  setEDO(static_cast<uint8_t>(edoIdx));
}

//------------------------------------
void MainWindow::loadAdaptingEnabled()
//------------------------------------
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("status");
  adaptingEnabled_ = settings.value("adaptingEnabled", true).toBool();
  settings.endGroup();

  surfaceTab_->getNtetCircleWidget()->setAdaptingEnabled(adaptingEnabled_);
}

//----------------------------------------
void MainWindow::loadAftertouchBehaviour()
//----------------------------------------
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("status");
  int aftertouch = settings.value("aftertouchBehaviour", 0).toInt();
  int aftertouchThreshold = settings.value("aftertouchThreshol", 10).toInt();
  settings.endGroup();

  if (aftertouch < 0 || aftertouch > 2)
    aftertouch = 0;

  if (afterTouchThreshold_ < 10 || afterTouchThreshold_ > 127)
    aftertouch = 10;

  afterTouch_ = static_cast<AfterTouch>(aftertouch);
  afterTouchThreshold_ = static_cast<uint8_t>(aftertouchThreshold);

  surfaceTab_->getNtetCircleWidget()->setAfterTouchBehaviourText(getAftertouchString(afterTouch_), afterTouch_ != AfterTouch::off);
}

//-----------------------------------------
void MainWindow::loadPresetsForCurrentEDO()
//-----------------------------------------
{
  configPresets_.clear();

  const NtetMapping& mapping = kNtetMappings[edoIdx_];

  QSettings settings("NaadaLab", "Intona");

  settings.beginGroup(
    QString("tuningPresetsV2/%1").arg(mapping.N));

  const int count = settings.value("count", 0).toInt();

  for (int i = 0; i < count; ++i)
  {
    settings.beginGroup(QString("preset%1").arg(i));

    const bool complete =
      settings.contains("values")
      && settings.contains("tuningCenter")
      && settings.contains("globalOffsetCents");

    const QVariantList list =
      settings.value("values").toList();

    const int tuningCenter =
      settings.value(
        "tuningCenter",
        Config::invalid).toInt();

    const double globalOffset =
      settings.value(
        "globalOffsetCents",
        0.0).toDouble();

    settings.endGroup();

    if (!complete || list.size() != 12)
      continue;

    TuningPreset preset;
    bool valid = std::isfinite(globalOffset);

    for (int key = 0; key < 12 && valid; ++key)
    {
      const int value = list[key].toInt();

      if (value < kConfigMaskMin
        || value > kConfigMaskMax)
      {
        valid = false;
        break;
      }

      preset.values[key] =
        static_cast<int8_t>(value);
    }

    if (tuningCenter != Config::invalid
      && (tuningCenter < mapping.minValue
        || tuningCenter > mapping.maxValue))
    {
      valid = false;
    }

    if (!valid)
      continue;

    preset.tuningCenter =
      static_cast<int8_t>(tuningCenter);

    preset.globalOffsetCents = globalOffset;

    Config candidate{
      preset.tuningCenter,
      preset.values,
      ConfigMask{}
    };

    const auto validatedOffset =
      Intona::Tuning::findGlobalOffsetCents(
        candidate,
        mapping,
        preset.globalOffsetCents);

    if (!validatedOffset
      || std::abs(
        *validatedOffset
        - preset.globalOffsetCents) >= 0.0001)
    {
      continue;
    }

    configPresets_.push_back(preset);
  }

  settings.endGroup();

  surfaceTab_->getConfigPresetListWidget()->setPresets(
    configPresets_);
}

//--------------------------------------------
void MainWindow::onOutPortChanged(uint8_t idx)
//--------------------------------------------
{
  saveMidiOutPort();
}

//-------------------------------------------
void MainWindow::onInPortChanged(uint8_t idx)
//-------------------------------------------
{
  saveMidiInPort();
}

//---------------------------------------------
void MainWindow::onInChannelChanged(uint8_t id)
//---------------------------------------------
{
  saveMidiInChannel();
}

//------------------------------------------------
void MainWindow::onOutChannelChanged(bool checked)
//------------------------------------------------
{
  saveMidiOutChannels();
}

//----------------------------------
void MainWindow::setEDO(uint8_t idx)
//----------------------------------
{
  edoIdx_ = idx;
  surfaceTab_->getNtetCircleWidget()->setCurrentMapping(&kNtetMappings[edoIdx_]);
  surfaceTab_->getConfigPresetListWidget()->setMapping(&kNtetMappings[edoIdx_]);

  const auto& m = kNtetMappings[edoIdx_];
  invFifthStep_ = modInverse(m.fifthStep, m.N);

  if (!activeNotes_.empty())
  {
    for (int ch = 0; ch < 16; ++ch)
      if (midiSettingTab_->isOutChnEnabled(ch))
        Intona::Tuning::sendAllNotesOff(
          midiSettingTab_->MidiOut(), ch);

    activeNotes_.clear();
    pressedMask5_ = 0;
  }

  setConfig(&m.getConfig(0), m);
  applyCurrentConfig(midiSettingTab_->MidiOut(), false, -1);

  loadPresetsForCurrentEDO();
  saveEdoIndex();
}

//-----------------------------------------------------------------
void MainWindow::FindBetterTuningCenter(const NtetMapping& mapping)
//-----------------------------------------------------------------
{
  const Config* matchingConfig =
    Intona::Tuning::findConfigByValues(
      mapping,
      currentConfig_.valueForKey);

  if (matchingConfig)
  {
    currentConfig_.tuningCenter =
      matchingConfig->tuningCenter;

    if (!Intona::Tuning::isKeyCompatibleWithTuningCenter(
          currentConfig_.tuningCenter,
          currentKeyTonic_,
          currentKeyIsMinor_))
    {
      currentKeyTonic_ = Config::invalid;

      surfaceTab_->getNtetCircleWidget()->setKey(
        currentKeyTonic_,
        currentKeyIsMinor_);

      currentKeyIsMinor_ = false;
    }

    return;
  }

  currentKeyTonic_ = Config::invalid;

  surfaceTab_->getNtetCircleWidget()->setKey(
    currentKeyTonic_,
    currentKeyIsMinor_);

  currentKeyIsMinor_ = false;
  currentConfig_.tuningCenter = Config::invalid;
}

//--------------------------------------------------------
void MainWindow::stepKeyPitch(int keyIndex, int direction)
//--------------------------------------------------------
{
  const auto& mapping = kNtetMappings[edoIdx_];

  const auto targetValue =
    Intona::Tuning::steppedValueForKey(
      keyIndex,
      direction,
      currentConfig_,
      mapping,
      currentGlobalOffsetCents_);

  if (!targetValue)
    return;

  currentConfig_.valueForKey[keyIndex] = *targetValue;

  FindBetterTuningCenter(mapping);
  applyCurrentConfig(
    midiSettingTab_->MidiOut(),
    true,
    -1);

  enableRTAdapting(false);
}

//-----------------------------------------------------
void MainWindow::onKeyPitchRaiseRequested(int keyIndex)
//-----------------------------------------------------
{
  stepKeyPitch(keyIndex, +1);
}

//-----------------------------------------------------
void MainWindow::onKeyPitchLowerRequested(int keyIndex)
//-----------------------------------------------------
{
  stepKeyPitch(keyIndex, -1);
}

//----------------------------
void MainWindow::showEdoMenu()
//----------------------------
{
  QMenu menu(this);

  for (int i = 0; i < std::size(kNtetMappings); ++i)
  {
    const int N = kNtetMappings[i].N;

    QAction* action = menu.addAction(QString("%1 EDO").arg(N));
    action->setCheckable(true);
    action->setChecked(i == edoIdx_);

    connect(action, &QAction::triggered, this, [this, i]()
      {
        setEDO(static_cast<uint8_t>(i));
      });
  }

  menu.exec(QCursor::pos());
}

//-------------------------------------------
void MainWindow::captureCurrentConfigPreset()
//-------------------------------------------
{
  const NtetMapping& mapping = kNtetMappings[edoIdx_];

  const auto offset = Intona::Tuning::findGlobalOffsetCents(
    currentConfig_,
    mapping,
    currentGlobalOffsetCents_);

  if (!offset)
  {
    qWarning()
      << "Cannot capture tuning preset for EDO"
      << int(mapping.N)
      << ": no compatible global offset.";

    return;
  }

  const TuningPreset preset{
    currentConfig_.valueForKey,
    currentConfig_.tuningCenter,
    *offset
  };

  const auto existing = std::find_if(
    configPresets_.begin(),
    configPresets_.end(),
    [&](const TuningPreset& saved)
    {
      return saved.values == preset.values
        && saved.tuningCenter == preset.tuningCenter
        && std::abs(
          saved.globalOffsetCents
          - preset.globalOffsetCents) < 0.0001;
    });

  if (existing != configPresets_.end())
    return;

  configPresets_.push_back(preset);

  surfaceTab_->getConfigPresetListWidget()->setPresets(
    configPresets_);

  surfaceTab_->getConfigPresetListWidget()->setCurrentPresetIndex(
      static_cast<int>(configPresets_.size()) - 1);

  savePresetsForCurrentEDO();
}
//---------------------------------------------
void MainWindow::onAfterTouchBehaviourChanged()
//---------------------------------------------
{
  switch (afterTouch_)
  {
  case AfterTouch::stepUp:    afterTouch_ = AfterTouch::stepDown; break;
  case AfterTouch::stepDown:  afterTouch_ = AfterTouch::off;      break;
  case AfterTouch::off:       afterTouch_ = AfterTouch::stepUp;   break;
  }

  saveAftertouchBehaviour();

  surfaceTab_->getNtetCircleWidget()->setAfterTouchBehaviourText(getAftertouchString(afterTouch_), afterTouch_ != AfterTouch::off);
}
