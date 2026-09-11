#pragma once

#include <QObject>
#include <QStringList>

#include <cstdint>
#include <memory>

class IMidiOut;
class MidiIn_MonoInterpreter;

struct MidiUiSnapshot
{
  QStringList midiInPorts;
  QStringList midiOutPorts;
  QString midiInPort;
  QString midiOutPort;
  int midiInChannel = 1;
  quint32 midiOutChannelMask = 0;
  QString midiInStatus;
  QString midiOutStatus;
};

class MidiController final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QStringList midiInPorts
             READ midiInPorts
             NOTIFY midiInPortsChanged)

  Q_PROPERTY(QStringList midiOutPorts
             READ midiOutPorts
             NOTIFY midiOutPortsChanged)

  Q_PROPERTY(QString midiInPort
             READ midiInPort
             WRITE setMidiInPort
             NOTIFY midiInPortChanged)

  Q_PROPERTY(QString midiOutPort
             READ midiOutPort
             WRITE setMidiOutPort
             NOTIFY midiOutPortChanged)

  Q_PROPERTY(int midiInChannel
             READ midiInChannel
             WRITE setMidiInChannel
             NOTIFY midiInChannelChanged)

  Q_PROPERTY(quint32 midiOutChannelMask
             READ midiOutChannelMask
             NOTIFY midiOutChannelMaskChanged)

  Q_PROPERTY(QString midiInStatus
             READ midiInStatus
             NOTIFY midiInStatusChanged)

  Q_PROPERTY(QString midiOutStatus
             READ midiOutStatus
             NOTIFY midiOutStatusChanged)

public:
  explicit MidiController(QObject* parent = nullptr);
  MidiController(std::unique_ptr<IMidiOut> output, quint32 channelMask,
    QObject* parent = nullptr);
  ~MidiController() override;

  Q_INVOKABLE void start();
  Q_INVOKABLE void stop();

  QStringList midiInPorts() const;
  QStringList midiOutPorts() const;

  QString midiInPort() const;
  QString midiOutPort() const;

  int midiInChannel() const;
  quint32 midiOutChannelMask() const;

  QString midiInStatus() const;
  QString midiOutStatus() const;

  void setMidiInPort(const QString& portName);
  void setMidiOutPort(const QString& portName);
  void setMidiInChannel(int channel);

  Q_INVOKABLE void refreshMidiInPorts();
  Q_INVOKABLE void refreshMidiOutPorts();

  Q_INVOKABLE bool midiOutChannelEnabled(int channel) const;
  Q_INVOKABLE void setMidiOutChannelEnabled(int channel, bool enabled);

  IMidiOut* midiOut() const noexcept;
  MidiUiSnapshot uiSnapshot() const;

signals:
  void midiInPortsChanged();
  void midiOutPortsChanged();

  void midiInPortChanged();
  void midiOutPortChanged();
  void midiInChannelChanged();
  void midiOutChannelMaskChanged();

  void midiInStatusChanged();
  void midiOutStatusChanged();

  void midiNoteOnReceived(int note, int velocity, quint32 timeMs);
  void midiNoteOffReceived(int note, int velocity, quint32 timeMs);
  void midiPressureReceived(int pressure, quint32 timeMs);
  void midiChannelMessageReceived(
    int code,
    int data1,
    int data2,
    quint32 timeMs);

private:
  void loadSettings();
  void installMidiInCallback();

  void setMidiInStatus(const QString& status);
  void setMidiOutStatus(const QString& status);

  std::unique_ptr<MidiIn_MonoInterpreter> midiIn_;
  std::unique_ptr<IMidiOut> midiOut_;

  QStringList midiInPorts_;
  QStringList midiOutPorts_;

  QString midiInPort_;
  QString midiOutPort_;

  int midiInChannel_ = 1;
  quint32 midiOutChannelMask_ = 0;

  QString midiInStatus_;
  QString midiOutStatus_;
};
