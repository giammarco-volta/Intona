#include <QApplication>
#include "MainWindow.h"
#include "StyleUtils.h"
#include <QFile>
#include <QDir>

// IMPORTANTISSIMO:
#include <QtCore/qresource.h>   // oppure #include <QResource>

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  //app.setStyle("Universal");   // IMPORTANTISSIMO
  app.setStyle("Fusion");   // IMPORTANTISSIMO
  app.setStyleSheet(loadStyleSheet(":/qdarkstyle/darkstyle.qss"));

  MainWindow w;
  w.show();
  return app.exec();
}