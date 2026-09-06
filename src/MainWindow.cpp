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
#include "About.h"

//#define CONSIDER_QUARTAL_CHORDS


//-----------------------------------------------------------
double valueToCentsInOctave(int value5, const NtetMapping& m)
//-----------------------------------------------------------
{
  const int step = mod(value5 * m.fifthStep, m.N);
  return 1200.0 * double(step) / double(m.N);
}

//---------------------------------------------------------------
double rawOffsetForKey(int key, int value5, const NtetMapping& m)
//---------------------------------------------------------------
{
  double cents = valueToCentsInOctave(value5, m);
  double offset = cents - 100.0 * key;

  while (offset > 600.0)
    offset -= 1200.0;

  while (offset <= -600.0)
    offset += 1200.0;

  return offset;
}

//----------------------------------------------------------------------------------------------------------
std::optional<double> findGlobalOffsetCents(const Config& cfg, const NtetMapping& m, double preferredOffset)
//----------------------------------------------------------------------------------------------------------
{
  constexpr double kLimit = 99.0;

  double lo = -1e9;
  double hi = 1e9;

  for (int key = 0; key < 12; ++key)
  {
    const double raw = rawOffsetForKey(key, cfg.valueForKey[key], m);

    lo = std::max(lo, raw - kLimit);
    hi = std::min(hi, raw + kLimit);
  }

  if (lo > hi)
    return std::nullopt;

  return std::clamp(preferredOffset, lo, hi);
}

//----------------------------------------------------------------------------
void MainWindow::rebuildConfigMask(Config& config, const NtetMapping& mapping)
//----------------------------------------------------------------------------
{
  config.mask = 0;

  for (int key = 0; key < 12; ++key)
    config.mask |= valueToPoolBit(config.valueForKey[key]);
}

//---------------------------------------------------------------------------
static void sendRpnCoarseTuning(IMidiOut& out, uint8_t ch, uint8_t semitones)
//---------------------------------------------------------------------------
{
  assert(semitones < 128);

  // RPN Coarse Tuning: RPN 0,2 ;
  out.sendShort(0xB0 | (ch & 0x0F), 101, 0);        // CC101 RPN MSB
  out.sendShort(0xB0 | (ch & 0x0F), 100, 2);        // CC100 RPN LSB
  out.sendShort(0xB0 | (ch & 0x0F), 6, semitones);  // CC38 Data Entry MSB
  out.sendShort(0xB0 | (ch & 0x0F), 101, 127);      // deselect
  out.sendShort(0xB0 | (ch & 0x0F), 100, 127);
}

//------------------------------------------------------------------------------------------
static void sendRpnFineTuning(IMidiOut& out, uint8_t ch, uint8_t centsMSB, uint8_t centsLSB)
//------------------------------------------------------------------------------------------
{
  // RPN Coarse Tuning: RPN 0,1 ;
  out.sendShort(0xB0 | (ch & 0x0F), 101, 0);        // CC101 RPN MSB
  out.sendShort(0xB0 | (ch & 0x0F), 100, 1);        // CC100 RPN LSB
  out.sendShort(0xB0 | (ch & 0x0F),   6, centsMSB); // CC6 Data Entry MSB
  out.sendShort(0xB0 | (ch & 0x0F),  38, centsLSB); // CC38 Data Entry LSB
  out.sendShort(0xB0 | (ch & 0x0F), 101, 127);      // deselect
  out.sendShort(0xB0 | (ch & 0x0F), 100, 127);
}

//-----------------------------------------------------------------------
static void sendProgramChange(IMidiOut& out, uint8_t ch, uint8_t program)
//-----------------------------------------------------------------------
{
  static constexpr uint8_t dummyData2 = 0;
  out.sendShort(0xC0 | (ch & 0x0F), program, dummyData2);
}

//---------------------------------------------------------------------------------
static void sendControlChange(IMidiOut& out, uint8_t ch, uint8_t cc, uint8_t value)
//---------------------------------------------------------------------------------
{
  out.sendShort(0xB0 | (ch & 0x0F), cc, value);
}

//------------------------------------------------------------------------------
static void sendNoteOn(IMidiOut& out, uint8_t ch, uint8_t key, uint8_t velocity)
//------------------------------------------------------------------------------
{
  out.sendShort(0x90 | (ch & 0x0F), key, velocity);
}

//-------------------------------------------------------------------------------
static void sendNoteOff(IMidiOut& out, uint8_t ch, uint8_t key, uint8_t velocity)
//-------------------------------------------------------------------------------
{
  out.sendShort(0x80 | (ch & 0x0F), key, velocity);
}

//-----------------------------------------------------------------------------------------------
static void sendChannelMsg(IMidiOut& out, uint8_t ch, uint8_t code, uint8_t data1, uint8_t data2)
//-----------------------------------------------------------------------------------------------
{
  out.sendShort(code | (ch & 0x0F), data1, data2);
}

//----------------------------------------------------
static void sendAllNotesOff(IMidiOut& out, uint8_t ch)
//----------------------------------------------------
{
  out.sendShort(0xB0 | (ch & 0x0F), 123, 0);
}

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

  connect(surfaceTab_->getConfigPresetListWidget(), &ConfigPresetListWidget::presetSelected,
    this, [this](int index)
    {
      currentConfig_.valueForKey = configPresets_[index];
      FindBetterTuningCenter(kNtetMappings[edoIdx_]);
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
  setConfig(chooseConfigThroughTuningCenter(tuningCenter), kNtetMappings[edoIdx_]);
  applyCurrentConfig(midiSettingTab_->MidiOut(), false, -1);

  if (!isKeyCompatibleWithTuningCenter(currentConfig_.tuningCenter, currentKeyTonic_, currentKeyIsMinor_))
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
  auto offset = findGlobalOffsetCents(currentConfig_, m, currentGlobalOffsetCents_);

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
  int coarse = std::lround(cents / 100.0);
  double fine = cents - coarse * 100.0;
  assert(fine >= -50.0 && fine <= 50.0);

  int fine14 = std::lround((fine + 64.0) * 128.0);
  fine14 = std::clamp(fine14, 0, 16383);

  uint8_t fineMsb = fine14 / 128;
  uint8_t fineLsb = fine14 % 128;


  auto& out = midiSettingTab_->MidiOut();

  for (uint8_t ch = 0; ch < 16; ++ch)
  {
    if (midiSettingTab_->isOutChnEnabled(ch))
    {
      out.sendShort(0xB0 | (ch & 0x0F), 101, 0);          // CC101 RPN MSB

      //Coarse tuning: RPN 0,2
      out.sendShort(0xB0 | (ch & 0x0F), 100, 2);          // CC100 RPN LSB
      out.sendShort(0xB0 | (ch & 0x0F), 6, 64 + coarse);  // CC38 Data Entry MSB

      //Coarse tuning: RPN 0,2
      out.sendShort(0xB0 | (ch & 0x0F), 100, 1);          // CC100 RPN LSB
      out.sendShort(0xB0 | (ch & 0x0F), 6, fineMsb);      // CC6 Data Entry MSB
      out.sendShort(0xB0 | (ch & 0x0F), 38, fineLsb);     // CC38 Data Entry LSB

      out.sendShort(0xB0 | (ch & 0x0F), 101, 127);        // deselect
      out.sendShort(0xB0 | (ch & 0x0F), 100, 127);
    }
  }
}
//---------------------------------------------------------------------------
void MainWindow::SendTuningSysex(uint8_t N, uint8_t fifthStep, IMidiOut* out)
//---------------------------------------------------------------------------
{
  auto mts14BitToCents = [](uint16_t v)
    {
      return (double(v) - 8192.0) * 100.0 / 8192.0;
    };

  auto buildChannelMask = [this]() -> std::tuple<quint8, quint8, quint8>
    {
      quint8 ff = 0, gg = 0, hh = 0;

      // chEnable_[0] -> channel 1
      for (int ch = 0; ch < 16; ++ch)
      {
        if (midiSettingTab_->isOutChnEnabled(ch))
        {
          const int ch1 = ch + 1; // 1..16

          if (ch1 >= 1 && ch1 <= 7)
            hh |= quint8(1u << (ch1 - 1));           // bits 0..6
          else if (ch1 >= 8 && ch1 <= 14)
            gg |= quint8(1u << (ch1 - 8));           // bits 0..6
          else if (ch1 >= 15 && ch1 <= 16)
            ff |= quint8(1u << (ch1 - 15));          // bits 0..1
        }
      }

      // safety: keep only defined bits
      ff &= 0x03;
      gg &= 0x7F;
      hh &= 0x7F;
      return { ff, gg, hh };
    };

  const auto [ff, gg, hh] = buildChannelMask();

  const auto mtsTable = computeMtsTable(N, fifthStep, currentConfig_, currentGlobalOffsetCents_);


  // ------------------------------------------------------------
  // Build Real-Time Universal SysEx: Scale/Octave Tuning 2-byte
  // F0 7E <device> 08 09 ff gg hh [ss tt]... F7
  // where [ss tt] are 24 bytes for C..B.
  // ff/gg/hh: channel bitmask; easiest is "all channels".
  // ------------------------------------------------------------

  std::vector<uint8_t> syx;
  syx.reserve(1 + 1 + 1 + 1 + 1 + 3 + 24 + 1);

  syx.push_back(uint8_t(0xF0));
  syx.push_back(uint8_t(0x7F)); // real-time
  syx.push_back(uint8_t(0x7F)); // device id: all devices (broadcast)
  syx.push_back(uint8_t(0x08)); // subID1: MIDI Tuning Standard
  syx.push_back(uint8_t(0x09)); // subID2: Scale/Octave Tuning 2-byte form (Non-Real-Time) :contentReference[oaicite:3]{index=3}

  syx.push_back(uint8_t(ff));
  syx.push_back(uint8_t(gg));
  syx.push_back(uint8_t(hh));

  // 24 bytes for C..B in that exact order
  for (int s = 0; s < 12; ++s)
  {
    const uint16_t v = mtsTable[s]; // 0..16383, 8192 = 0 cents
    const uint8_t msb = uint8_t((v >> 7) & 0x7F);
    const uint8_t lsb = uint8_t(v & 0x7F);    syx.push_back(uint8_t(msb));
    syx.push_back(uint8_t(lsb));
  }

  syx.push_back(uint8_t(0xF7));

  qDebug()  << QString::number(      mts14BitToCents(mtsTable[0]) + currentGlobalOffsetCents_, 'f', 1) << QString::number( 100 + mts14BitToCents(mtsTable[ 1]) + currentGlobalOffsetCents_, 'f', 1) << QString::number( 200 + mts14BitToCents(mtsTable[ 2]) + currentGlobalOffsetCents_, 'f', 1)
            << QString::number(300 + mts14BitToCents(mtsTable[3]) + currentGlobalOffsetCents_, 'f', 1) << QString::number( 400 + mts14BitToCents(mtsTable[ 4]) + currentGlobalOffsetCents_, 'f', 1) << QString::number( 500 + mts14BitToCents(mtsTable[ 5]) + currentGlobalOffsetCents_, 'f', 1)
            << QString::number(600 + mts14BitToCents(mtsTable[6]) + currentGlobalOffsetCents_, 'f', 1) << QString::number( 700 + mts14BitToCents(mtsTable[ 7]) + currentGlobalOffsetCents_, 'f', 1) << QString::number( 800 + mts14BitToCents(mtsTable[ 8]) + currentGlobalOffsetCents_, 'f', 1)
            << QString::number(900 + mts14BitToCents(mtsTable[9]) + currentGlobalOffsetCents_, 'f', 1) << QString::number(1000 + mts14BitToCents(mtsTable[10]) + currentGlobalOffsetCents_, 'f', 1) << QString::number(1100 + mts14BitToCents(mtsTable[11]) + currentGlobalOffsetCents_, 'f', 1);

  out->sendSysEx(syx);
}

//----------------------------------------------------------------------------
const Config* MainWindow::chooseConfigThroughTuningCenter(int8_t tuningCenter)
//----------------------------------------------------------------------------
{
  while (tuningCenter > kNtetMappings[edoIdx_].maxValue)
    tuningCenter -= kNtetMappings[edoIdx_].N;

  while (tuningCenter < kNtetMappings[edoIdx_].minValue)
    tuningCenter += kNtetMappings[edoIdx_].N;

  for (int tc = kNtetMappings[edoIdx_].minValue; tc <= kNtetMappings[edoIdx_].maxValue; ++tc)
  {
    const Config& cfg = kNtetMappings[edoIdx_].getConfig(tc);
    if (tuningCenter == cfg.tuningCenter)
      return &cfg;
  }

  qDebug() << "Warning: tuning center" << tuningCenter << "not found in mapping for EDO" << int(kNtetMappings[edoIdx_].N);
  return nullptr;
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

//-------------------------------------
static int holeWeight(int holePosition)
//-------------------------------------
{
  // holePosition:
  //   0 = missing third / missing upper note of first fourth
  //   1 = missing fifth / second fourth level
  //   2 = missing seventh / third fourth level
  //   3 = missing ninth / higher level
  //
  // Lower holes are structurally much more important.

  static constexpr int weights[] =
  {
     10, // position 0
      6, // position 1
      8, // position 2
      5, // position 3
      4, // position 4
      3, // position 5
      2  // position 6
  };

  if (holePosition < 0)
    return 0;

  if (holePosition >= int(std::size(weights)))
    return 1;

  return weights[holePosition];
}

//---------------------------------------------------------------------------------------------------
MainWindow::ChordRootAnalysis MainWindow::inferChordRootByStack(const std::vector<ActiveNote>& notes)
//---------------------------------------------------------------------------------------------------
{
  struct Candidate
  {
    int16_t root = 0;
    ChordStructure structure = ChordStructure::None;
    uint8_t holes = 0;
    int holeCost = 0;
    uint8_t span = 0;
  };

  ChordRootAnalysis result;

  if (notes.size() < 3)
    return result;

  uint8_t bitmap7 = 0;

  struct RootMap
  {
    uint8_t degree7 = 0;
    int16_t value5 = 0;
  };

  std::vector<RootMap> roots;

  for (const auto& n : notes)
  {
    const uint8_t d7 = mod7(n.interpretedValue);

    bitmap7 |= uint8_t{ 1 } << d7;

    auto it = std::find_if(
      roots.begin(),
      roots.end(),
      [&](const RootMap& r)
      {
        return r.degree7 == d7;
      });

    if (it == roots.end())
      roots.push_back({ d7, n.interpretedValue });
  }

  const int noteCount = popcount(bitmap7);

  if (noteCount < 3)
    return result;

  auto structurePriority = [](ChordStructure s)
    {
      switch (s)
      {
      case ChordStructure::Tertian: return 0;
      case ChordStructure::Quartal: return 1;
      default: return 2;
      }
    };

  auto isBetter = [&](const Candidate& a, const Candidate& b)
    {
      if (a.holeCost != b.holeCost)
        return a.holeCost < b.holeCost;

      if (a.holes != b.holes)
        return a.holes < b.holes;

      if (a.span != b.span)
        return a.span < b.span;

      return structurePriority(a.structure) < structurePriority(b.structure);
    };

  auto bitAtRepeatedBitmap = [&](int pos)
    {
      return (bitmap7 & (uint8_t{ 1 } << (pos % 7))) != 0;
    };

  auto value5ForDegree7 = [&](uint8_t degree7)
    {
      for (const auto& r : roots)
      {
        if (r.degree7 == degree7)
          return r.value5;
      }

      return int16_t{ 0 };
    };

  Candidate bestCandidate;
  bool found = false;

  auto evaluateStructure = [&](ChordStructure structure, int step)
    {
      for (uint8_t x = 0; x < 7; ++x)
      {
        if (!bitAtRepeatedBitmap(x))
          continue;

        const int16_t startValue5 = value5ForDegree7(x);

        int ones = 0;
        int holes = 0;
        int holeCost = 0;
        int span = 0;
        bool valid = true;

        for (int k = 0; k < 7; ++k)
        {
          const int pos = int(x) + k * step;
          const uint8_t degree7 = uint8_t(pos % 7);

          if (bitAtRepeatedBitmap(pos))
          {
            if (structure == ChordStructure::Quartal)
            {
              const int16_t expectedValue5 = int16_t(startValue5 - k);
              const int16_t actualValue5 = value5ForDegree7(degree7);

              if (actualValue5 != expectedValue5)
              {
                valid = false;
                break;
              }
            }

            ++ones;

            if (ones == noteCount)
            {
              span = pos - int(x);
              break;
            }
          }
          else
          {
            ++holes;
            holeCost += holeWeight(k - 1);
          }
        }

        if (!valid || ones != noteCount)
          continue;

        Candidate c;

        if (structure == ChordStructure::Quartal)
        {
          // In a stack of perfect fourths, the perceived chord root is taken
          // as the upper note of the first fourth.
          //
          // Examples:
          //   G-C-F       -> root C
          //   B-E-A-D-G   -> root E
          c.root = int16_t(startValue5 - 1);
        }
        else
        {
          c.root = startValue5;
        }

        c.structure = structure;
        c.holes = uint8_t(holes);
        c.holeCost = holeCost;
        c.span = uint8_t(span);

        if (!found || isBetter(c, bestCandidate))
        {
          bestCandidate = c;
          found = true;
        }
      }
    };

  evaluateStructure(ChordStructure::Tertian, 2);
#ifdef CONSIDER_QUARTAL_CHORDS
  evaluateStructure(ChordStructure::Quartal, 3);
#endif

  if (!found)
    return result;

  result.valid = true;
  result.root = bestCandidate.root;
  result.structure = bestCandidate.structure;
  result.holes = bestCandidate.holes;

  return result;
}

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

//-----------------------------------------------------------------------------
static bool pressedMaskContainsFifth(uint16_t keyPressedMask12, int16_t value5)
//-----------------------------------------------------------------------------
{
  return (keyPressedMask12 & (uint16_t{ 1 } << fifthToSemitone(value5))) != 0;
}

//-----------------------------------------------------------------------------------------------------------------------------
std::optional<MainWindow::KeyChoice> MainWindow::inferKeyFromDominantSignature(uint16_t keyPressedMask12, const Config& config)
//-----------------------------------------------------------------------------------------------------------------------------
{
  for (int a = 0; a < 12; ++a)
  {
    if (!hasKey12(keyPressedMask12, a))
      continue;

    for (int b = a + 1; b < 12; ++b)
    {
      if (!hasKey12(keyPressedMask12, b))
        continue;

      if ((b - a) != 6)
        continue;

      const int8_t lower5 = config.valueForKey[a];
      const int8_t upper5 = config.valueForKey[b];
      const int8_t diff5 = upper5 - lower5;

      int8_t majorKeyTonic5 = 0;

      if (diff5 == -6)
      {
        // diminished fifth: lower note is the root of the diminished fifth.
        // Example: D-Ab -> key tonic Eb.
        majorKeyTonic5 = lower5 - 5;
      }
      else if (diff5 == 6)
      {
        // augmented fourth: upper note resolves upward by minor second.
        // Example: C-F# -> key tonic G.
        majorKeyTonic5 = upper5 - 5;
      }
      else
      {
        continue;
      }

      const int8_t relativeMinorTonic5 = majorKeyTonic5 + 3;
      const int8_t relativeMinorLeadingTone5 = relativeMinorTonic5 + 5;

      if (pressedMaskContainsFifth(keyPressedMask12, relativeMinorLeadingTone5))
      {
        return KeyChoice{ relativeMinorTonic5, true };
      }

      return KeyChoice{ majorKeyTonic5, false };
    }
  }

  return std::nullopt;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
std::optional<MainWindow::KeyChoice> MainWindow::chooseBestLocalKey(uint16_t pressedKeyMask12, const Config& config, const NtetMapping& mapping) const
//----------------------------------------------------------------------------------------------------------------------------------------------------
{
  if (auto dominantKey = inferKeyFromDominantSignature(pressedKeyMask12, config))
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
    auto rootAnalysis = inferChordRootByStack(choice.resolvedNotes);

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

  rKeyTonic = bestKeyTonic;
  rIsMinor = bestIsMinor;

  resetScaleData();

  return isKeyCompatibleWithTuningCenter(currentConfig_.tuningCenter, bestKeyTonic, bestIsMinor)
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
    rebuildConfigMask(currentConfig_, kNtetMappings[edoIdx_]);

  updateStepButtonEnablement();
  SendTuningSysex(kNtetMappings[edoIdx_].N, kNtetMappings[edoIdx_].fifthStep, &out);
  surfaceTab_->getNtetCircleWidget()->setConfig(currentConfig_);
  surfaceTab_->getConfigPresetListWidget()->setCurrentPresetIndex(presetIdx);
}

namespace
{
  constexpr double kMaxStdTuningCents = 99.0;

  double normalizeCentsDiff(double diff)
  {
    while (diff > 600.0)
      diff -= 1200.0;

    while (diff <= -600.0)
      diff += 1200.0;

    return diff;
  }

  double centsOffsetFromKey(int keyIndex, int value5, const NtetMapping& m)
  {
    const int pitchStep = mod(value5 * m.fifthStep, m.N);

    const double pitchCents =
      1200.0 * double(pitchStep) / double(m.N);

    const double keyCents =
      100.0 * double(keyIndex);

    return normalizeCentsDiff(pitchCents - keyCents);
  }
}

//-------------------------------------------
void MainWindow::rebuildAllowedValuesPerKey()
//-------------------------------------------
{
  const auto& m = kNtetMappings[edoIdx_];

  for (auto& v : allowedValuesPerKey_)
    v.clear();

  for (int value = kConfigMaskMin; value <= kConfigMaskMax; ++value)
  {
    for (int key = 0; key < 12; ++key)
    {
      const double offset = centsOffsetFromKey(key, value, m);

      if (offset >= -99.0 && offset <= 99.0)
        allowedValuesPerKey_[key].push_back(static_cast<int8_t>(value));
    }
  }
}

//-------------------------------------------
void MainWindow::updateStepButtonEnablement()
//-------------------------------------------
{
  const auto& m = kNtetMappings[edoIdx_];
  auto* circle = surfaceTab_->getNtetCircleWidget();

  std::array<bool, 12> canRaise{};
  std::array<bool, 12> canLower{};

  for (int key = 0; key < 12; ++key)
  {
    const int currentValue = currentConfig_.valueForKey[key];
    const int currentPitch = mod(currentValue * m.fifthStep, m.N);

    const int raisedPitch = mod(currentPitch + 1, m.N);
    const int loweredPitch = mod(currentPitch - 1, m.N);

    const auto raisedValue = circle->valueForPitchStep(raisedPitch);
    const auto loweredValue = circle->valueForPitchStep(loweredPitch);

    canRaise[key] = raisedValue.has_value() && isValueAllowedForKey(key, *raisedValue);
    canLower[key] = loweredValue.has_value() && isValueAllowedForKey(key, *loweredValue);
  }

  circle->setKeyStepButtonEnabled(canRaise, canLower);
}

//------------------------------------------------------------------
bool MainWindow::isValueAllowedForKey(int keyIndex, int value) const
//------------------------------------------------------------------
{
  const auto& values = allowedValuesPerKey_[keyIndex];

  if (std::find(values.begin(), values.end(), value) == values.end())
    return false;

  const auto& m = kNtetMappings[edoIdx_];

  const double candidateDetune = centsOffsetFromKey(keyIndex, value, m) - currentGlobalOffsetCents_;

  if (candidateDetune < -99.0 || candidateDetune > 99.0)
    return false;

  auto absoluteKeyboardCents = [&](int key, int value5) -> double
    {
      return 100.0 * double(key) + centsOffsetFromKey(key, value5, m);
    };

  const double candidateCents = absoluteKeyboardCents(keyIndex, value);

  const int prevKey = (keyIndex + 11) % 12;
  const int nextKey = (keyIndex +  1) % 12;

  const int prevValue = currentConfig_.valueForKey[prevKey];
  const int nextValue = currentConfig_.valueForKey[nextKey];

  if (prevValue != Config::invalid)
  {
    double prevCents = absoluteKeyboardCents(prevKey, prevValue);

    if (prevKey > keyIndex)
      prevCents -= 1200.0;

    if (candidateCents <= prevCents)
      return false;
  }

  if (nextValue != Config::invalid)
  {
    double nextCents = absoluteKeyboardCents(nextKey, nextValue);

    if (nextKey < keyIndex)
      nextCents += 1200.0;

    if (candidateCents >= nextCents)
      return false;
  }

  return true;
}

//---------------------------------------------------------------------------------------------------------
bool MainWindow::isKeyCompatibleWithTuningCenter(int8_t tuningCenter, int8_t keyTonic, bool isMinor)
//---------------------------------------------------------------------------------------------------------
{
  const KeyChoice candidates[9] =
  {
    {                     tuningCenter,      false },
    {                     tuningCenter,      true  },
    { static_cast<int8_t>(tuningCenter - 3), false },

    { static_cast<int8_t>(tuningCenter + 1), false },
    { static_cast<int8_t>(tuningCenter + 1), true  },
    { static_cast<int8_t>(tuningCenter - 2), false },

    { static_cast<int8_t>(tuningCenter - 1), false },
    { static_cast<int8_t>(tuningCenter - 1), true  },
    { static_cast<int8_t>(tuningCenter - 4), false }
  };

  for (const auto& k : candidates)
  {
    if (k.tonic == keyTonic && k.isMinor == isMinor)
      return true;
  }

  return false;
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
      sendNoteOn(*out, trackIndex, note, velocity);
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
      sendNoteOff(*out, trackIndex, note, velocity);
}

//-------------------------------------------------------------------------------------------------
void MainWindow::process(uint8_t note, uint8_t velocity, uint32_t timeMs, IMidiOut* out, bool isOn)
//-------------------------------------------------------------------------------------------------
{
  AdaptiveChoice choice = chooseBestInterpretationAndConfigByChords(note, velocity, timeMs, isOn);

  for (const auto& n : choice.notesToRetrigger)
    for (uint8_t trackIndex = 0; trackIndex < 16; ++trackIndex)
      if (midiSettingTab_->isOutChnEnabled(trackIndex))
        sendNoteOff(*out, trackIndex, n.midiNote, 0);

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
        sendNoteOn(*out, trackIndex, n.midiNote, n.velocity);
}

//-------------------------------------------------------------------------------------------------------------------
void MainWindow::handleIncomingChannelMsg(uint8_t code, uint8_t data1, uint8_t data2, uint32_t timeMs, IMidiOut* out)
//-------------------------------------------------------------------------------------------------------------------
{
  for (uint8_t trackIndex = 0; trackIndex < 16; ++trackIndex)
    if (midiSettingTab_->isOutChnEnabled(trackIndex))
      sendChannelMsg(*out, trackIndex, code, data1, data2);
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

  settings.beginGroup(QString("tuningPresets/%1").arg(edo));

  settings.remove(""); // remove all current EDO's presets

  settings.setValue("count", int(configPresets_.size()));

  for (int i = 0; i < int(configPresets_.size()); ++i)
  {
    QVariantList list;

    for (int k = 0; k < 12; ++k)
      list << int(configPresets_[i][k]);

    settings.setValue(QString("preset%1").arg(i), list);
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

  const int edo = kNtetMappings[edoIdx_].N;

  QSettings settings("NaadaLab", "Intona");

  settings.beginGroup(QString("tuningPresets/%1").arg(edo));

  const int count = settings.value("count", 0).toInt();

  for (int i = 0; i < count; ++i)
  {
    const QString key = QString("preset%1").arg(i);

    const QVariantList list = settings.value(key).toList();

    if (list.size() != 12)
      continue;

    std::array<int8_t, 12> cfg;

    bool valid = true;

    for (int k = 0; k < 12; ++k)
    {
      const int v = list[k].toInt();

      if (v < kNtetMappings[edoIdx_].minValue || v > kNtetMappings[edoIdx_].maxValue)
      {
        valid = false;
        break;
      }

      cfg[k] = static_cast<int8_t>(v);
    }

    if (!valid)
      continue;

    configPresets_.push_back(cfg);
  }

  settings.endGroup();

  surfaceTab_->getConfigPresetListWidget()->setPresets(configPresets_);
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

  rebuildAllowedValuesPerKey();

  if (!activeNotes_.empty())
  {
    for (int ch = 0; ch < 16; ++ch)
      if (midiSettingTab_->isOutChnEnabled(ch))
        sendAllNotesOff(midiSettingTab_->MidiOut(), ch);

    activeNotes_.clear();
    pressedMask5_ = 0;
  }

  setConfig(&m.getConfig(0), m);
  applyCurrentConfig(midiSettingTab_->MidiOut(), false, -1);

  loadPresetsForCurrentEDO();
  saveEdoIndex();
}

//-----------------------------------------------------------
void MainWindow::FindBetterTuningCenter(const NtetMapping& m)
//-----------------------------------------------------------
{
  for (int tc = kNtetMappings[edoIdx_].minValue; tc <= kNtetMappings[edoIdx_].maxValue; ++tc)
  {
    const Config& cfg = kNtetMappings[edoIdx_].getConfig(tc);
    if (currentConfig_.valueForKey == cfg.valueForKey)
    {
      currentConfig_.tuningCenter = cfg.tuningCenter;
      if (!isKeyCompatibleWithTuningCenter(currentConfig_.tuningCenter, currentKeyTonic_, currentKeyIsMinor_))
      {
        currentKeyTonic_ = Config::invalid;
        surfaceTab_->getNtetCircleWidget()->setKey(currentKeyTonic_, currentKeyIsMinor_);
        currentKeyIsMinor_ = false;
      }
      return;
    }
  }

  currentKeyTonic_ = Config::invalid;
  surfaceTab_->getNtetCircleWidget()->setKey(currentKeyTonic_, currentKeyIsMinor_);
  currentKeyIsMinor_ = false;
  currentConfig_.tuningCenter = Config::invalid;
}

//--------------------------------------------------------
void MainWindow::stepKeyPitch(int keyIndex, int direction)
//--------------------------------------------------------
{
  if (keyIndex < 0 || keyIndex >= 12)
    return;

  const auto& m = kNtetMappings[edoIdx_];

  const int currentValue = currentConfig_.valueForKey[keyIndex];
  const int currentPitch = mod(currentValue * m.fifthStep, m.N);
  const int targetPitch = mod(currentPitch + direction, m.N);

  // Nuovo punto chiave:
  // usa lo spelling già visualizzato sul circle per quello step EDO.
  const std::optional<int8_t> targetValue =
    surfaceTab_->getNtetCircleWidget()->valueForPitchStep(targetPitch);

  if (!targetValue)
    return;

  if (!isValueAllowedForKey(keyIndex, *targetValue))
    return;

  currentConfig_.valueForKey[keyIndex] = *targetValue;

  FindBetterTuningCenter(m);
  applyCurrentConfig(midiSettingTab_->MidiOut(), true, -1);

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
  if (std::find(configPresets_.begin(), configPresets_.end(), currentConfig_.valueForKey) != configPresets_.end())
    return;

  configPresets_.push_back(currentConfig_.valueForKey);
  surfaceTab_->getConfigPresetListWidget()->setPresets(configPresets_);
  surfaceTab_->getConfigPresetListWidget()->setCurrentPresetIndex(configPresets_.size() - 1);
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
