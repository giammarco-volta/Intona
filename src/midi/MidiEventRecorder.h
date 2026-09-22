#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

// Lives on the GUI thread. Only original MIDI timestamps should be used to
// measure playing: delivery to this observer can lag behind the MIDI worker.
class MidiEventRecorder final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool recording READ recording NOTIFY changed)
  Q_PROPERTY(QString filePath READ filePath NOTIFY changed)
  Q_PROPERTY(QUrl directoryUrl READ directoryUrl CONSTANT)
  Q_PROPERTY(QString error READ error NOTIFY changed)
public:
  explicit MidiEventRecorder(QObject* parent = nullptr, const QString& directory = {});
  ~MidiEventRecorder() override;
  bool recording() const { return file_.isOpen(); }
  QString filePath() const { return file_.fileName(); }
  QUrl directoryUrl() const { return QUrl::fromLocalFile(directory_); }
  QString error() const { return error_; }
  Q_INVOKABLE bool start(const QString& label);
  Q_INVOKABLE void stop();
  void setContext(const QVariantMap& context);
  void recordInput(int status, int data1, int data2, quint32 timeMs, int channel);
signals:
  void changed();
private:
  bool append(QJsonObject record);
  void fail(const QString& message);
  QString directory_;
  QString error_;
  QFile file_;
  QTimer flushTimer_;
  QElapsedTimer clock_;
  QVariantMap context_;
  qint64 sequence_ = 0;
  qint64 inputCount_ = 0;
};
