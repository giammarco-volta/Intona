#include "MidiController.h"

#include "IntonaMidiConfiguration.h"

#include "IMidiOut.h"
#include "MidiInFactory.h"
#include "MidiOutFactory.h"

#include <QMetaObject>
#include <QSettings>

#include <algorithm>

MidiController::MidiController(QObject* parent)
  : QObject(parent)
{
  loadSettings();
}

void MidiController::start()
{
  if (!midiIn_)
    midiIn_ = createMidiIn(makeIntonaMidiInConfiguration());

  if (!midiOut_)
    midiOut_ = createMidiOut();

  if (midiIn_)
  {
    midiIn_->setSourceChannel(
      static_cast<uint8_t>(midiInChannel_ - 1));

    installMidiInCallback();
  }

  refreshMidiInPorts();
  refreshMidiOutPorts();
}

MidiController::~MidiController()
{
  stop();
}

void MidiController::stop()
{
  if (midiIn_)
  {
    midiIn_->setCallback({});
    midiIn_->close();
  }

  if (midiOut_)
    midiOut_->close();
}

QStringList MidiController::midiInPorts() const
{
  return midiInPorts_;
}

QStringList MidiController::midiOutPorts() const
{
  return midiOutPorts_;
}

QString MidiController::midiInPort() const
{
  return midiInPort_;
}

QString MidiController::midiOutPort() const
{
  return midiOutPort_;
}

int MidiController::midiInChannel() const
{
  return midiInChannel_;
}

quint32 MidiController::midiOutChannelMask() const
{
  return midiOutChannelMask_;
}

QString MidiController::midiInStatus() const
{
  return midiInStatus_;
}

QString MidiController::midiOutStatus() const
{
  return midiOutStatus_;
}

void MidiController::setMidiInPort(const QString& portName)
{
  const bool changed = midiInPort_ != portName;

  if (midiIn_)
    midiIn_->close();

  midiInPort_ = portName;

  if (changed)
  {
    QSettings settings("NaadaLab", "Intona");
    settings.beginGroup("midi");
    settings.setValue("inPortName", midiInPort_);
    settings.endGroup();

    emit midiInPortChanged();
  }

  if (!midiIn_)
  {
    setMidiInStatus(tr("MIDI IN backend not available"));
    return;
  }

  const int index = midiInPorts_.indexOf(midiInPort_);

  if (index < 0)
  {
    setMidiInStatus(
      midiInPort_.isEmpty()
        ? tr("No MIDI IN device selected")
        : tr("MIDI IN device not available"));
    return;
  }

  const bool opened = midiIn_->open(index);

  setMidiInStatus(
    opened
      ? tr("Connected: %1").arg(midiInPort_)
      : tr("Unable to open: %1").arg(midiInPort_));
}

void MidiController::setMidiOutPort(const QString& portName)
{
  const bool changed = midiOutPort_ != portName;

  if (midiOut_)
    midiOut_->close();

  midiOutPort_ = portName;

  if (changed)
  {
    QSettings settings("NaadaLab", "Intona");
    settings.beginGroup("midi");
    settings.setValue("outPortName", midiOutPort_);
    settings.endGroup();

    emit midiOutPortChanged();
  }

  if (!midiOut_)
  {
    setMidiOutStatus(tr("MIDI OUT backend not available"));
    return;
  }

  const int index = midiOutPorts_.indexOf(midiOutPort_);

  if (index < 0)
  {
    setMidiOutStatus(
      midiOutPort_.isEmpty()
        ? tr("No MIDI OUT device selected")
        : tr("MIDI OUT device not available"));
    return;
  }

  const bool opened = midiOut_->open(index);

  setMidiOutStatus(
    opened
      ? tr("Connected: %1").arg(midiOutPort_)
      : tr("Unable to open: %1").arg(midiOutPort_));
}

void MidiController::setMidiInChannel(int channel)
{
  channel = std::clamp(channel, 1, 16);

  if (midiInChannel_ == channel)
    return;

  midiInChannel_ = channel;

  if (midiIn_)
  {
    midiIn_->setSourceChannel(
      static_cast<uint8_t>(midiInChannel_ - 1));
  }

  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("midi");
  settings.setValue("midiInChn", midiInChannel_);
  settings.endGroup();

  emit midiInChannelChanged();
}

void MidiController::refreshMidiInPorts()
{
  if (!midiIn_)
  {
    midiInPorts_.clear();
    emit midiInPortsChanged();
    setMidiInStatus(tr("MIDI IN backend not available"));
    return;
  }

  const QStringList ports = midiIn_->listInputs();

  if (midiInPorts_ != ports)
  {
    midiInPorts_ = ports;
    emit midiInPortsChanged();
  }

  if (midiInPorts_.isEmpty())
  {
    midiIn_->close();
    setMidiInStatus(tr("No MIDI IN devices found"));
    return;
  }

  if (midiInPort_.isEmpty())
  {
    setMidiInPort(midiInPorts_.first());
    return;
  }

  if (!midiInPorts_.contains(midiInPort_))
  {
    midiIn_->close();
    setMidiInStatus(
      tr("MIDI IN device not available: %1").arg(midiInPort_));
    return;
  }

  setMidiInPort(midiInPort_);
}

void MidiController::refreshMidiOutPorts()
{
  if (!midiOut_)
  {
    midiOutPorts_.clear();
    emit midiOutPortsChanged();
    setMidiOutStatus(tr("MIDI OUT backend not available"));
    return;
  }

  const QStringList ports = midiOut_->listOutputs();

  if (midiOutPorts_ != ports)
  {
    midiOutPorts_ = ports;
    emit midiOutPortsChanged();
  }

  if (midiOutPorts_.isEmpty())
  {
    midiOut_->close();
    setMidiOutStatus(tr("No MIDI OUT devices found"));
    return;
  }

  if (midiOutPort_.isEmpty())
  {
    setMidiOutPort(midiOutPorts_.first());
    return;
  }

  if (!midiOutPorts_.contains(midiOutPort_))
  {
    midiOut_->close();
    setMidiOutStatus(
      tr("MIDI OUT device not available: %1").arg(midiOutPort_));
    return;
  }

  setMidiOutPort(midiOutPort_);
}

bool MidiController::midiOutChannelEnabled(int channel) const
{
  if (channel < 1 || channel > 16)
    return false;

  const quint32 bit = quint32(1) << (channel - 1);
  return (midiOutChannelMask_ & bit) != 0;
}

void MidiController::setMidiOutChannelEnabled(
  int channel,
  bool enabled)
{
  if (channel < 1 || channel > 16)
    return;

  const quint32 bit = quint32(1) << (channel - 1);
  const quint32 newMask = enabled
    ? midiOutChannelMask_ | bit
    : midiOutChannelMask_ & ~bit;

  if (newMask == midiOutChannelMask_)
    return;

  midiOutChannelMask_ = newMask;

  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("midi");
  settings.setValue(
    QString("outChn%1").arg(channel - 1),
    enabled);
  settings.endGroup();

  emit midiOutChannelMaskChanged();
}

IMidiOut* MidiController::midiOut() const noexcept
{
  return midiOut_.get();
}

MidiUiSnapshot MidiController::uiSnapshot() const
{
  return {
    midiInPorts_,
    midiOutPorts_,
    midiInPort_,
    midiOutPort_,
    midiInChannel_,
    midiOutChannelMask_,
    midiInStatus_,
    midiOutStatus_};
}

void MidiController::loadSettings()
{
  QSettings settings("NaadaLab", "Intona");
  settings.beginGroup("midi");

  midiInPort_ = settings.value("inPortName").toString();
  midiOutPort_ = settings.value("outPortName").toString();

  midiInChannel_ = std::clamp(
    settings.value("midiInChn", 1).toInt(),
    1,
    16);

  midiOutChannelMask_ = 0;

  for (int channel = 0; channel < 16; ++channel)
  {
    if (settings.value(
          QString("outChn%1").arg(channel),
          false).toBool())
    {
      midiOutChannelMask_ |= quint32(1) << channel;
    }
  }

  settings.endGroup();
}

void MidiController::installMidiInCallback()
{
  midiIn_->setCallback(
    [this](const MidiInEvent& event)
    {
      const auto state = midiIn_->getState();

      if (state.type == EventType::noteOn)
      {
        const int note = state.currentNote;
        const int velocity = state.currentVel;
        const quint32 timeMs = state.lastTimeMs;

        QMetaObject::invokeMethod(
          this,
          [this, note, velocity, timeMs]()
          {
            emit midiNoteOnReceived(note, velocity, timeMs);
          },
          Qt::QueuedConnection);

        return;
      }

      if (state.type == EventType::noteOff)
      {
        const int note = event.data1;
        const int velocity = event.data2;
        const quint32 timeMs = state.lastTimeMs;

        QMetaObject::invokeMethod(
          this,
          [this, note, velocity, timeMs]()
          {
            emit midiNoteOffReceived(note, velocity, timeMs);
          },
          Qt::QueuedConnection);

        return;
      }

      const int code = event.message();
      const int data1 = event.data1;
      const int data2 = event.data2;
      const quint32 timeMs = state.lastTimeMs;

      if (code == 0xD0)
      {
        QMetaObject::invokeMethod(
          this,
          [this, data1, timeMs]()
          {
            emit midiPressureReceived(data1, timeMs);
          },
          Qt::QueuedConnection);

        return;
      }

      QMetaObject::invokeMethod(
        this,
        [this, code, data1, data2, timeMs]()
        {
          emit midiChannelMessageReceived(
            code,
            data1,
            data2,
            timeMs);
        },
        Qt::QueuedConnection);
    });
}

void MidiController::setMidiInStatus(const QString& status)
{
  if (midiInStatus_ == status)
    return;

  midiInStatus_ = status;
  emit midiInStatusChanged();
}

void MidiController::setMidiOutStatus(const QString& status)
{
  if (midiOutStatus_ == status)
    return;

  midiOutStatus_ = status;
  emit midiOutStatusChanged();
}
