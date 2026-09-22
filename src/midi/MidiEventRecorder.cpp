#include "MidiEventRecorder.h"
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

MidiEventRecorder::MidiEventRecorder(QObject* parent, const QString& directory)
  : QObject(parent), directory_(directory.isEmpty()
      ? QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
          .filePath("Intona/Recordings") : directory)
{
  flushTimer_.setInterval(1000);
  connect(&flushTimer_, &QTimer::timeout, this, [this]() {
    if (recording() && !file_.flush()) fail(file_.errorString());
  });
}

MidiEventRecorder::~MidiEventRecorder() { stop(); }

bool MidiEventRecorder::start(const QString& label)
{
  if (recording()) return false;
  error_.clear();
  if (!QDir().mkpath(directory_))
  { fail(tr("Cannot create the recording folder: %1").arg(directory_)); return false; }
  QString safeLabel = label.trimmed().left(60);
  safeLabel.replace(QRegularExpression("[^\\p{L}\\p{N}_-]+"), "-");
  if (safeLabel.isEmpty()) safeLabel = "performance";
  const auto now = QDateTime::currentDateTimeUtc();
  const QString name = now.toString("yyyyMMdd-HHmmss-zzz") + "-" + safeLabel
    + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8) + ".jsonl";
  file_.setFileName(QDir(directory_).filePath(name));
  if (!file_.open(QIODevice::WriteOnly | QIODevice::NewOnly))
  { fail(file_.errorString()); return false; }
  sequence_ = 0;
  inputCount_ = 0;
  clock_.start();
  if (!append({{"type", "session_start"}, {"schema_version", 1},
      {"started_utc", now.toString(Qt::ISODateWithMs)}, {"label", label.trimmed()},
      {"context", QJsonObject::fromVariantMap(context_)},
      {"timing", "midi_time_ms: original uint32 backend clock; capture_ms: observer delivery time"},
      {"scope", "Selected input channel, before dirty-note filtering and tuning; input configuration may suppress other channel messages"}})) return false;
  if (!file_.flush()) { fail(file_.errorString()); return false; }
  flushTimer_.start();
  emit changed();
  return true;
}

void MidiEventRecorder::stop()
{
  if (!recording()) return;
  flushTimer_.stop();
  if (!append({{"type", "session_end"}, {"input_events", inputCount_}})) return;
  if (!file_.flush()) { fail(file_.errorString()); return; }
  file_.close();
  emit changed();
}

void MidiEventRecorder::setContext(const QVariantMap& context)
{
  if (context == context_) return;
  context_ = context;
  if (recording()) append({{"type", "context_snapshot"},
    {"context", QJsonObject::fromVariantMap(context_)}});
}

void MidiEventRecorder::recordInput(int status, int data1, int data2,
  quint32 timeMs, int channel)
{
  if (!recording()) return;
  const bool on = status == 0x90 && data2 > 0;
  const bool off = status == 0x80 || (status == 0x90 && data2 == 0);
  QJsonObject record{{"type", on ? "note_on" : off ? "note_off" : "channel_message"},
    {"status", status}, {"data1", data1}, {"data2", data2},
    {"channel", channel}, {"midi_time_ms", qint64(timeMs)}};
  if (on || off) { record["note"] = data1; record["velocity"] = data2; }
  ++inputCount_;
  append(record);
}

bool MidiEventRecorder::append(QJsonObject record)
{
  record["sequence"] = sequence_++;
  record["capture_ms"] = clock_.nsecsElapsed() / 1000000.0;
  const QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
  if (file_.write(line) != line.size()) { fail(file_.errorString()); return false; }
  return true;
}

void MidiEventRecorder::fail(const QString& message)
{
  error_ = message;
  flushTimer_.stop();
  file_.close();
  emit changed();
}
