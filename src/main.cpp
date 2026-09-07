#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "MainWindow.h"
#include "StyleUtils.h"
#include "ManualDocumentParser.h"
#include "About.h"
#include "midi/MidiController.h"

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
  QApplication app(argc, argv);
  QQuickStyle::setStyle("Material");

  if (QCoreApplication::arguments().contains("--legacy-widgets"))
  {
    app.setStyle("Fusion");
    app.setStyleSheet(loadStyleSheet(":/qdarkstyle/darkstyle.qss"));

    MainWindow window;
    window.show();

    return app.exec();
  }

  MidiController midiController;

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

  engine.rootContext()->setContextProperty("MidiController", &midiController);

  engine.rootContext()->setContextProperty("DebugBuild", debugBuild);

  engine.loadFromModule("Intona", "Main");

#ifdef Q_OS_ANDROID
  QTimer::singleShot(500, &app, []()
    {
      enableKeepScreenOn();
    });
#endif

  return app.exec();
}
