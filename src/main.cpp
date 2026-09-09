#include <QGuiApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QThread>

#include "ManualDocumentParser.h"
#include "About.h"
#include "midi/MidiController.h"
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

  engine.rootContext()->setContextProperty("DebugBuild", debugBuild);

  engine.loadFromModule("Intona", "Main");

#ifdef Q_OS_ANDROID
  QTimer::singleShot(500, &app, []()
    {
      enableKeepScreenOn();
    });
#endif

  const int result = app.exec();

  QMetaObject::invokeMethod(
    midiWorker,
    &MidiController::stop,
    Qt::BlockingQueuedConnection);

  midiThread.quit();
  midiThread.wait();

  return result;
}
