#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "MainWindow.h"
#include "StyleUtils.h"

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  if (QCoreApplication::arguments().contains("--legacy-widgets"))
  {
    app.setStyle("Fusion");
    app.setStyleSheet(loadStyleSheet(":/qdarkstyle/darkstyle.qss"));

    MainWindow window;
    window.show();

    return app.exec();
  }

  QQmlApplicationEngine engine;

  QObject::connect(
    &engine,
    &QQmlApplicationEngine::objectCreationFailed,
    &app,
    []()
    {
      QCoreApplication::exit(-1);
    },
    Qt::QueuedConnection);

#ifdef NDEBUG
  constexpr bool debugBuild = false;
#else
  constexpr bool debugBuild = true;
#endif

  engine.rootContext()->setContextProperty("DebugBuild", debugBuild);

  engine.loadFromModule("Intona", "Main");

  return app.exec();
}
