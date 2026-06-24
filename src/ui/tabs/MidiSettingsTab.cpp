#include "MidiSettingsTab.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QDir>
#include <QFileInfoList>
#include <QDebug>

#include <fstream>
#include <filesystem>  // Richiede C++17


#include "../../MainWindow.h"

#include "../../../../Common/src/midi/MidiOutFactory.h"
#include "../../../../Common/src/midi/MidiInFactory.h"
#include "../../../../Common/src/midi/MidiMessage.h"
#include "../../../../Common/src/ui/widgets/MidiChannelSelector.h"


//--------------------------------------------------
static void updateLabel(QLabel* l, const QString& s)
//--------------------------------------------------
{
  if (l)
    l->setText(s);
}

//------------------------------------
void setupTrackCheckBox(QCheckBox* cb)
//------------------------------------
{
  auto update = [cb]()
    {
      QFont f = cb->font();
      f.setBold(cb->isChecked());
      cb->setFont(f);
    };

  QObject::connect(cb, &QCheckBox::toggled, cb, update);
  update();
}

//------------------------------------------------------------------------------------------------------------------------
MidiSettingsTab::MidiSettingsTab(MainWindow* parent) : QWidget(parent), midiOut_(createMidiOut()), midiIn_(createMidiIn())
//------------------------------------------------------------------------------------------------------------------------
{
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(10);

  // =========================================================
  // MIDI Settings group
  // =========================================================
  auto* midiSettingsGroup = new QGroupBox(tr("MIDI Setup"), this);
  auto* midiSettingsLayout = new QVBoxLayout(midiSettingsGroup);
  midiSettingsLayout->setSpacing(10);

  // Only for debug -----------------------
  inStatus_ = new QLabel(tr("Ready"), midiSettingsGroup);
  midiSettingsLayout->addWidget(inStatus_);
  // --------------------------------------

  {
    auto* box = new QGroupBox(tr("MIDI IN"), midiSettingsGroup);
    auto* layout = new QVBoxLayout(box);

    // Top row: device + refresh
    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Device:"), box));

    inPorts_ = new QComboBox(box);
    top->addWidget(inPorts_, 1);

    inRefresh_ = new QPushButton(tr("Refresh"), box);
    top->addWidget(inRefresh_);

    layout->addLayout(top);

    // Channel row: single spinbox 0..15
    auto* channelRow = new QHBoxLayout();
    channelRow->addWidget(new QLabel(tr("Input channel:"), box));

    inChannelSpin_ = new MidiChannelSelector(box);
    inChannelSpin_->setValue(1);

    channelRow->addWidget(inChannelSpin_);
    channelRow->addStretch();

    layout->addLayout(channelRow);

    midiSettingsLayout->addWidget(box);

    connect(inRefresh_, &QPushButton::clicked, this, &MidiSettingsTab::onRefreshInPorts);

    connect(inPorts_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MidiSettingsTab::onInPortChanged);
    connect(inPorts_, qOverload<int>(&QComboBox::currentIndexChanged), parent, &MainWindow::onInPortChanged);

    connect(inChannelSpin_, &MidiChannelSelector::valueChanged, this, &MidiSettingsTab::onInChannelChanged);
    connect(inChannelSpin_, &MidiChannelSelector ::valueChanged, parent, &MainWindow::onInChannelChanged);
  }

  {
    auto* box = new QGroupBox(tr("MIDI OUT"), midiSettingsGroup);
    auto* layout = new QVBoxLayout(box);

    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Device:"), box));

    outPorts_ = new QComboBox(box);
    top->addWidget(outPorts_, 1);

    outRefresh_ = new QPushButton(tr("Refresh"), box);
    top->addWidget(outRefresh_);

    layout->addLayout(top);

    // channels grid
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);

    for (int ch = 0; ch < 16; ++ch)
    {
      chEnable_[ch] = new QCheckBox(QString::number(ch + 1), box);
      chEnable_[ch]->setChecked(true); // default: all on
      int r = ch / 8;
      int c = ch % 8;
      grid->addWidget(chEnable_[ch], r, c);
      connect(chEnable_[ch], &QCheckBox::toggled, parent, &MainWindow::onOutChannelChanged);
      setupTrackCheckBox(chEnable_[ch]);
    }
    layout->addLayout(grid);

    midiSettingsLayout->addWidget(box);

    connect(outRefresh_, &QPushButton::clicked, this, &MidiSettingsTab::onRefreshOutPorts);

    connect(outPorts_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MidiSettingsTab::onOutPortChanged);
    connect(outPorts_, qOverload<int>(&QComboBox::currentIndexChanged), parent, &MainWindow::onOutPortChanged);
  }

  mainLayout->addWidget(midiSettingsGroup);

  // =========================================================

  mainLayout->addStretch();

  onRefreshOutPorts();
  onRefreshInPorts();
}

//-----------------------------------------------------
bool MidiSettingsTab::isOutChnEnabled(uint8_t ch) const
//-----------------------------------------------------
{
  return ch < 16 && chEnable_[ch] && chEnable_[ch]->isChecked();
}

//-----------------------------------------------------
void MidiSettingsTab::set(const MidiSettings& settings)
//-----------------------------------------------------
{
  int index = inPorts_->findText(settings.midiInPort);
  if (index >= 0)
    inPorts_->setCurrentIndex(index);

  index = outPorts_->findText(settings.midiOutPort);
  if (index >= 0)
    outPorts_->setCurrentIndex(index);

  inChannelSpin_->setValue(settings.midiInChannel);

  for (int ch = 0; ch < 16; ++ch)
    if (chEnable_[ch])
      chEnable_[ch]->setChecked(settings.outChnEnabled[ch]);
}

//------------------------------------------------
QString MidiSettingsTab::getSelectedInPort() const
//------------------------------------------------
{
  return inPorts_ ? inPorts_->currentText() : QString();
}

//-------------------------------------------------
QString MidiSettingsTab::getSelectedOutPort() const
//-------------------------------------------------
{
  return outPorts_ ? outPorts_->currentText() : QString();
}

//-------------------------------------------
uint8_t MidiSettingsTab::getInChannel() const
//-------------------------------------------
{
  return inChannelSpin_ ? static_cast<uint8_t>(inChannelSpin_->value()) : 0;
}

//----------------------------------------------------------
bool MidiSettingsTab::getOutChannelEnabled(uint8_t ch) const
//----------------------------------------------------------
{
  return ch < 16 && chEnable_[ch] && chEnable_[ch]->isChecked();
}

//------------------------------------------------------
void MidiSettingsTab::connectMidiIn(uint8_t deviceIndex)
//------------------------------------------------------
{
  if (!midiIn_)
    midiIn_ = createMidiIn();

  if (!midiIn_)
  {
    updateLabel(inStatus_, tr("MIDI IN: backend not available"));
    return;
  }

  midiIn_->close();

  // IMPORTANT: WinMM callback may come from a non-Qt thread.
  // Keep this callback light; if you need to touch UI, forward via invokeMethod queued.
  midiIn_->setCallback([this](const MidiInEvent& ev) {
      auto st = midiIn_->getState();
      if (st.type == EventType::noteOn)
      {
        const uint8_t ch = inChannelSpin_->value() - 1;
        const uint8_t note = st.currentNote;
        const uint8_t vel = st.currentVel;
        const uint32_t time = st.lastTimeMs;

        if (ch != ev.channel())
          qDebug() << "Warning: MIDI IN channel mismatch: expected" << ch << "got" << ev.channel();

        QMetaObject::invokeMethod(this, [this, ch, note, vel, time]() {
          updateLabel(inStatus_, tr("IN CH%1 NoteOn %2 vel %3").arg(ch + 1).arg(note).arg(vel));
          emit midiNoteOnReceived(note, vel, time, midiOut_.get());
          }, Qt::QueuedConnection);
      }
      else if (st.type == EventType::noteOff)
      {
        const uint8_t ch = inChannelSpin_->value() - 1;
        const uint8_t note = ev.data1;
        const uint8_t vel  = ev.data2;
        const uint32_t time = st.lastTimeMs;

        if (ch != ev.channel())
          qDebug() << "Warning: MIDI IN channel mismatch: expected" << ch << "got" << ev.channel();

        QMetaObject::invokeMethod(this, [this, ch, note, vel, time]() {
          updateLabel(inStatus_, tr("IN CH%1 NoteOff").arg(ch + 1));
          emit midiNoteOffReceived(note, vel, time, midiOut_.get());
          }, Qt::QueuedConnection);
      }
      else
      {
        const uint8_t ch = inChannelSpin_->value();
        const uint8_t code = ev.message();
        const uint8_t data1 = ev.data1;
        const uint8_t data2 = ev.data2;
        const uint32_t time = st.lastTimeMs;

        if (ch != ev.channel())
          qDebug() << "Warning: MIDI IN channel mismatch: expected" << ch << "got" << ev.channel();

        if (code == 0xD0)
        {
          QMetaObject::invokeMethod(this, [this, ch, data1, time]() {
            updateLabel(inStatus_, tr("IN CH%1 Pressure %2").arg(ch + 1).arg(data1));
            emit midiPressureReceived(data1, time, midiOut_.get());
            }, Qt::QueuedConnection);
        }
        else
        {
          QMetaObject::invokeMethod(this, [this, ch, code, data1, data2, time]() {
            updateLabel(inStatus_, tr("IN CH%1 ChnMsg code %2 data1 %3 data2 %4").arg(ch + 1).arg(code).arg(data1).arg(data2));
            emit midiChannelMsgReceived(code, data1, data2, time, midiOut_.get());
            }, Qt::QueuedConnection);
        }
      }
    });

  if (deviceIndex < 0)
  {
    updateLabel(inStatus_, tr("MIDI IN: no device selected"));
    return;
  }

  const bool ok = midiIn_->open(deviceIndex);
  updateLabel(inStatus_, ok ? tr("MIDI IN: connected") : tr("MIDI IN: open failed"));
}

//--------------------------------------------------
void MidiSettingsTab::updateStatus(const QString& s)
//--------------------------------------------------
{
  if (status_)
    status_->setText(s);
}

//--------------------------------------
void MidiSettingsTab::onRefreshInPorts()
//--------------------------------------
{
  if (!inPorts_) return;

  if (!midiIn_)
    midiIn_ = createMidiIn();

  inPorts_->clear();

  if (!midiIn_)
  {
    inPorts_->addItem(tr("<MIDI IN backend not available>"));
    updateLabel(inStatus_, tr("MIDI IN: backend not available"));
    return;
  }

  const auto ins = midiIn_->listInputs();
  inPorts_->addItems(ins);

  updateLabel(inStatus_, tr("Found %1 MIDI IN devices").arg(ins.size()));

  if (ins.size() > 0)
    onInPortChanged(0);
}

//------------------------------------------------
void MidiSettingsTab::onInPortChanged(uint8_t idx)
//-----------------------------------------------
{
  if (!inPorts_)
    return;

  // idx corresponds to midiIn_->listInputs() order
  const QString name = inPorts_->currentText();
  updateLabel(inStatus_, tr("Selected IN: %1").arg(name));

  connectMidiIn(idx);
}

//--------------------------------------------------
void MidiSettingsTab::onInChannelChanged(uint8_t id)
//--------------------------------------------------
{
  // id = 1..16
  if (id < 1 || id > 16)
    return;

  updateLabel(inStatus_, tr("Input channel: %1").arg(id));
  midiIn_->setSourceChannel(id - 1);
}

//------------------------------------
void MidiSettingsTab::onRefreshOutPorts()
//------------------------------------
{
  outPorts_->clear();
  auto outs = midiOut_->listOutputs();
  outPorts_->addItems(outs);
  updateStatus(QString("Found %1 outputs").arg(outs.size()));
}

//----------------------------------------------
void MidiSettingsTab::onOutPortChanged(uint8_t idx)
//----------------------------------------------
{
  Q_UNUSED(idx);
  if (!midiOut_->open(idx))
    updateStatus("Open failed (or not implemented on this platform)");
  else
  {
    updateStatus(QString("Opened: %1").arg(outPorts_->currentText()));
  }
}
