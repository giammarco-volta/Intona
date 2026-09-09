#pragma once

#include "MidiController.h"

#include <QObject>

class MidiViewModel final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QStringList midiInPorts READ midiInPorts NOTIFY midiInPortsChanged)
  Q_PROPERTY(QStringList midiOutPorts READ midiOutPorts NOTIFY midiOutPortsChanged)
  Q_PROPERTY(QString midiInPort READ midiInPort WRITE setMidiInPort NOTIFY midiInPortChanged)
  Q_PROPERTY(QString midiOutPort READ midiOutPort WRITE setMidiOutPort NOTIFY midiOutPortChanged)
  Q_PROPERTY(int midiInChannel READ midiInChannel WRITE setMidiInChannel NOTIFY midiInChannelChanged)
  Q_PROPERTY(quint32 midiOutChannelMask READ midiOutChannelMask NOTIFY midiOutChannelMaskChanged)
  Q_PROPERTY(QString midiInStatus READ midiInStatus NOTIFY midiInStatusChanged)
  Q_PROPERTY(QString midiOutStatus READ midiOutStatus NOTIFY midiOutStatusChanged)

public:
  explicit MidiViewModel(
    MidiController* worker,
    QObject* parent = nullptr);

  QStringList midiInPorts() const { return state_.midiInPorts; }
  QStringList midiOutPorts() const { return state_.midiOutPorts; }
  QString midiInPort() const { return state_.midiInPort; }
  QString midiOutPort() const { return state_.midiOutPort; }
  int midiInChannel() const { return state_.midiInChannel; }
  quint32 midiOutChannelMask() const { return state_.midiOutChannelMask; }
  QString midiInStatus() const { return state_.midiInStatus; }
  QString midiOutStatus() const { return state_.midiOutStatus; }

  void setMidiInPort(const QString& portName);
  void setMidiOutPort(const QString& portName);
  void setMidiInChannel(int channel);
  Q_INVOKABLE void refreshMidiInPorts();
  Q_INVOKABLE void refreshMidiOutPorts();
  Q_INVOKABLE bool midiOutChannelEnabled(int channel) const;
  Q_INVOKABLE void setMidiOutChannelEnabled(int channel, bool enabled);

  void requestInitialRefresh();

signals:
  void midiInPortsChanged();
  void midiOutPortsChanged();
  void midiInPortChanged();
  void midiOutPortChanged();
  void midiInChannelChanged();
  void midiOutChannelMaskChanged();
  void midiInStatusChanged();
  void midiOutStatusChanged();

private:
  void scheduleRefresh();
  void refreshFromWorker();

  MidiController* worker_ = nullptr;
  MidiUiSnapshot state_;
  bool refreshPending_ = false;
};
