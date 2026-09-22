#include "midi/MidiEventRecorder.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
#include <cstdlib>

static void check(bool ok, const char* message)
{ if(!ok) { std::cerr << message << '\n'; std::exit(1); } }
static QList<QJsonObject> read(const QString& path)
{
  QFile file(path); check(file.open(QIODevice::ReadOnly), "Cannot read trace");
  QList<QJsonObject> rows;
  while (!file.atEnd()) {
    QJsonParseError error;
    auto doc=QJsonDocument::fromJson(file.readLine(), &error);
    check(error.error==QJsonParseError::NoError && doc.isObject(), "Invalid JSON line");
    rows.append(doc.object());
  }
  return rows;
}
int main(int argc,char**argv)
{
  QCoreApplication app(argc,argv);
  QTemporaryDir dir;check(dir.isValid(),"Temporary directory unavailable");
  MidiEventRecorder recorder(nullptr,dir.path());
  recorder.setContext({{"dirty_note_threshold_ms",100},{"edo",31}});
  recorder.recordInput(0x90,60,90,10,1); // Off by default.
  check(recorder.start("Bach / slow"),"Start failed");
  const QString first=recorder.filePath();
  check(!recorder.start("duplicate"),"Started twice");
  // Overlap, a 5 ms dirty note, velocity-zero off and uint32 wrap are preserved.
  recorder.recordInput(0x90,60,91,0xfffffff0u,1);
  recorder.recordInput(0x90,64,73,0xfffffff0u,1);
  recorder.recordInput(0x90,66,1,0xfffffff2u,1);
  recorder.recordInput(0x80,66,22,0xfffffff7u,1);
  recorder.recordInput(0xb0,64,127,0xfffffff8u,1);
  recorder.recordInput(0x90,60,0,20,1);
  recorder.recordInput(0x80,64,45,30,1);
  recorder.setContext({{"dirty_note_threshold_ms",150},{"edo",31}});
  recorder.stop();
  recorder.recordInput(0x90,70,90,50,1);
  auto rows=read(first);
  check(rows.size()==10,"Lost or duplicated events");
  check(rows[0]["context"].toObject()["edo"].toInt()==31,"Missing context");
  for(int i=0;i<rows.size();++i) check(rows[i]["sequence"].toInteger()==i,"Broken ordering");
  check(rows[1]["midi_time_ms"].toInteger()==4294967280LL,"Timestamp truncated");
  check(rows[2]["midi_time_ms"]==rows[1]["midi_time_ms"],"Lost simultaneous timestamp");
  check(rows[4]["midi_time_ms"].toInteger()-rows[3]["midi_time_ms"].toInteger()==5,"Dirty note filtered");
  check(rows[5]["data1"].toInt()==64 && rows[5]["midi_time_ms"].toInteger()==4294967288LL,"Lost pedal timestamp");
  check(rows[6]["type"].toString()=="note_off" && rows[6]["status"].toInt()==0x90,"Zero-velocity off lost");
  check(rows.back()["type"].toString()=="session_end" && rows.back()["input_events"].toInt()==7,"No complete footer");
  check(recorder.start("Bach / slow"),"Second take failed");
  check(recorder.filePath()!=first,"Overwrote previous take");recorder.stop();
  check(read(first).size()==10,"Changed previous take");
  QString autoClosed;
  { MidiEventRecorder r(nullptr,dir.path());check(r.start("close"),"Close take failed");autoClosed=r.filePath(); }
  check(read(autoClosed).back()["type"].toString()=="session_end","Shutdown did not finalize");
  MidiEventRecorder impossible(nullptr,first+"/folder");
  check(!impossible.start("fail") && !impossible.recording() && !impossible.error().isEmpty(),"No actionable file error");
  std::cout << "PASS: timestamps, overlaps, dirty notes, pedal, wrap, independent takes, shutdown and file errors.\n";
}
