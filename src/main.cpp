#include <QGuiApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QThread>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QDebug>

#include "ManualDocumentParser.h"
#include "About.h"
#include "midi/MidiController.h"
#include "midi/MidiEventRecorder.h"
#include "midi/MidiViewModel.h"
#include "tuning/TuningController.h"
#include "tuning/TuningViewModel.h"

#ifdef Q_OS_ANDROID
#include <QDebug>
#include <QTimer>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>

static void enableKeepScreenOn()
{
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]()
    {
      QJniObject activity = QNativeInterface::QAndroidApplication::context();

      if (!activity.isValid())
      {
        qWarning() << "Android activity/context not valid";
        return;
      }

      QJniObject window =
        activity.callObjectMethod(
          "getWindow",
          "()Landroid/view/Window;");

      if (!window.isValid())
      {
        qWarning() << "Android window not valid";
        return;
      }

      constexpr int FLAG_KEEP_SCREEN_ON = 128;

      window.callMethod<void>(
        "addFlags",
        "(I)V",
        FLAG_KEEP_SCREEN_ON);

      qDebug() << "FLAG_KEEP_SCREEN_ON set";
    });
}
#endif

int main(int argc, char** argv)
{
  QGuiApplication app(argc, argv);
  const bool startupCheck = app.arguments().contains(QStringLiteral("--startup-check"));
  std::unique_ptr<QTemporaryDir> startupSettings;
  if (startupCheck)
  {
    startupSettings = std::make_unique<QTemporaryDir>();
    if (!startupSettings->isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, startupSettings->path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, startupSettings->path());
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
      "NaadaLab", "Intona");
    // The organization/application-only constructor always uses NativeFormat.
    // Fail closed if this diagnostic run is not using the temporary INI store.
    if (settings.format() != QSettings::IniFormat
        || QFileInfo(settings.fileName()).absolutePath()
             != QDir(startupSettings->path()).filePath("NaadaLab")) return 2;
    // Enumerate the real backend, but never auto-select/open a hardware port.
    const QString unavailablePort = QStringLiteral("Intona startup check ")
      + QUuid::createUuid().toString();
    settings.setValue("midi/inPortName", unavailablePort);
    settings.setValue("midi/outPortName", unavailablePort);
    settings.sync();
    if (settings.status() != QSettings::NoError) return 2;
  }
  app.setWindowIcon(QIcon(QStringLiteral(":/icons/Intona.svg")));
  QQuickStyle::setStyle("Material");

  QThread midiThread;
  midiThread.setObjectName(QStringLiteral("IntonaMidiThread"));

  auto* midiWorker = new MidiController;
  auto* tuningWorker =
    new Intona::Tuning::TuningController(midiWorker);

  MidiViewModel midiViewModel(midiWorker);
  Intona::Tuning::TuningViewModel tuningViewModel(
    tuningWorker);

  MidiEventRecorder performanceRecorder;
  QObject::connect(midiWorker, &MidiController::midiInputObserved,
    &performanceRecorder, &MidiEventRecorder::recordInput, Qt::QueuedConnection);
  // UI snapshots are contextual annotations, not timing measurements. They
  // may coalesce worker updates; original input events are recorded separately.
  const auto updateRecordingContext = [&]() {
    performanceRecorder.setContext({
      {"midi_input", midiViewModel.midiInPort()},
      {"midi_channel", midiViewModel.midiInChannel()},
      {"edo", tuningViewModel.edo()},
      {"tuning_center", tuningViewModel.tuningCenter()},
      {"key_values", tuningViewModel.keyValues()},
      {"rt_adapting", tuningViewModel.adaptingEnabled()},
      {"retrigger_held_notes", tuningViewModel.retriggerHeldNotes()},
      {"scale_verification_ms", 70},
      {"adaptive_algorithm", "scales_triads_v1"}});
  };
  QObject::connect(&tuningViewModel, &Intona::Tuning::TuningViewModel::tuningStateChanged,
    &performanceRecorder, updateRecordingContext);
  QObject::connect(&midiViewModel, &MidiViewModel::midiInPortChanged,
    &performanceRecorder, updateRecordingContext);
  QObject::connect(&midiViewModel, &MidiViewModel::midiInChannelChanged,
    &performanceRecorder, updateRecordingContext);
  updateRecordingContext();

  midiWorker->moveToThread(&midiThread);
  tuningWorker->moveToThread(&midiThread);

  QObject::connect(
    &midiThread,
    &QThread::started,
    midiWorker,
    &MidiController::start);

  QObject::connect(
    &midiThread,
    &QThread::started,
    &midiViewModel,
    &MidiViewModel::requestInitialRefresh);

  QObject::connect(
    &midiThread,
    &QThread::started,
    &tuningViewModel,
    &Intona::Tuning::TuningViewModel::requestInitialRefresh);

  QObject::connect(
    &midiThread,
    &QThread::finished,
    midiWorker,
    &QObject::deleteLater);

  QObject::connect(
    &midiThread,
    &QThread::finished,
    tuningWorker,
    &QObject::deleteLater);

  bool startupMidiReady = false;
  QObject::connect(&midiThread, &QThread::started, &app,
    [&startupMidiReady]() { startupMidiReady = true; });
  midiThread.start(QThread::TimeCriticalPriority);

  const QVariantList userManualBlocks =
    NaadaLab::ManualDocumentParser::loadFromResource(
      QStringLiteral(":/manual/IntonaUserManual.html"),
      QStringLiteral("qrc:/manual/"));

  QQmlApplicationEngine engine;

  engine.rootContext()->setContextProperty(
    "AboutHtml",
    QString::fromUtf8(about));

  QObject::connect(
    &engine,
    &QQmlApplicationEngine::objectCreationFailed,
    &app,
    []()
    {
      QCoreApplication::exit(-1);
    },
    Qt::QueuedConnection);

  engine.rootContext()->setContextProperty("UserManualBlocks", userManualBlocks);

#ifdef NDEBUG
  constexpr bool debugBuild = false;
#else
  constexpr bool debugBuild = true;
#endif

  engine.rootContext()->setContextProperty("MidiController", &midiViewModel);

  engine.rootContext()->setContextProperty("TuningController", &tuningViewModel);
  engine.rootContext()->setContextProperty("PerformanceRecorder", &performanceRecorder);

  engine.rootContext()->setContextProperty("DebugBuild", debugBuild);

  engine.loadFromModule("Intona", "Main");

#ifdef Q_OS_ANDROID
  QTimer::singleShot(500, &app, []()
    {
      enableKeepScreenOn();
    });
#endif

  if (startupCheck)
  {
    QTimer::singleShot(1000, &app, [&]()
      {
        const bool ready = startupMidiReady && !engine.rootObjects().isEmpty();
        if (ready) qInfo("PASS: application startup, MIDI enumeration and QML loading.");
        else qCritical("FAIL: application startup did not complete.");
        app.exit(ready ? 0 : 2);
      });
  }
  const int result = app.exec();

  QMetaObject::invokeMethod(
    midiWorker,
    &MidiController::stop,
    Qt::BlockingQueuedConnection);

  midiThread.quit();
  midiThread.wait();

  return result;
}
