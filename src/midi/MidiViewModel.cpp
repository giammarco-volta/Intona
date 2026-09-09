#include "MidiViewModel.h"

#include <QMetaObject>

MidiViewModel::MidiViewModel(
  MidiController* worker,
  QObject* parent)
  : QObject(parent),
    worker_(worker),
    state_(worker->uiSnapshot())
{
  const auto changed = [this]() { scheduleRefresh(); };
  connect(worker_, &MidiController::midiInPortsChanged, this, changed);
  connect(worker_, &MidiController::midiOutPortsChanged, this, changed);
  connect(worker_, &MidiController::midiInPortChanged, this, changed);
  connect(worker_, &MidiController::midiOutPortChanged, this, changed);
  connect(worker_, &MidiController::midiInChannelChanged, this, changed);
  connect(worker_, &MidiController::midiOutChannelMaskChanged, this, changed);
  connect(worker_, &MidiController::midiInStatusChanged, this, changed);
  connect(worker_, &MidiController::midiOutStatusChanged, this, changed);
}

void MidiViewModel::setMidiInPort(const QString& portName)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, portName]() { worker->setMidiInPort(portName); },
    Qt::QueuedConnection);
}

void MidiViewModel::setMidiOutPort(const QString& portName)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, portName]() { worker->setMidiOutPort(portName); },
    Qt::QueuedConnection);
}

void MidiViewModel::setMidiInChannel(int channel)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, channel]() { worker->setMidiInChannel(channel); },
    Qt::QueuedConnection);
}

void MidiViewModel::refreshMidiInPorts()
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_]() { worker->refreshMidiInPorts(); },
    Qt::QueuedConnection);
}

void MidiViewModel::refreshMidiOutPorts()
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_]() { worker->refreshMidiOutPorts(); },
    Qt::QueuedConnection);
}

bool MidiViewModel::midiOutChannelEnabled(int channel) const
{
  if (channel < 1 || channel > 16)
    return false;

  return (state_.midiOutChannelMask
    & (quint32(1) << (channel - 1))) != 0;
}

void MidiViewModel::setMidiOutChannelEnabled(
  int channel,
  bool enabled)
{
  QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, channel, enabled]()
    {
      worker->setMidiOutChannelEnabled(channel, enabled);
    },
    Qt::QueuedConnection);
}

void MidiViewModel::requestInitialRefresh()
{
  scheduleRefresh();
}

void MidiViewModel::scheduleRefresh()
{
  if (refreshPending_)
    return;

  refreshPending_ = true;
  QMetaObject::invokeMethod(
    this,
    [this]() { refreshFromWorker(); },
    Qt::QueuedConnection);
}

void MidiViewModel::refreshFromWorker()
{
  MidiUiSnapshot snapshot;
  const bool read = QMetaObject::invokeMethod(
    worker_,
    [worker = worker_, &snapshot]()
    {
      snapshot = worker->uiSnapshot();
    },
    Qt::BlockingQueuedConnection);

  refreshPending_ = false;
  if (!read)
    return;

  const MidiUiSnapshot old = state_;
  state_ = std::move(snapshot);

  if (old.midiInPorts != state_.midiInPorts)
    emit midiInPortsChanged();
  if (old.midiOutPorts != state_.midiOutPorts)
    emit midiOutPortsChanged();
  if (old.midiInPort != state_.midiInPort)
    emit midiInPortChanged();
  if (old.midiOutPort != state_.midiOutPort)
    emit midiOutPortChanged();
  if (old.midiInChannel != state_.midiInChannel)
    emit midiInChannelChanged();
  if (old.midiOutChannelMask != state_.midiOutChannelMask)
    emit midiOutChannelMaskChanged();
  if (old.midiInStatus != state_.midiInStatus)
    emit midiInStatusChanged();
  if (old.midiOutStatus != state_.midiOutStatus)
    emit midiOutStatusChanged();
}
