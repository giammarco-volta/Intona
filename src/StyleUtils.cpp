#include "StyleUtils.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

QString loadStyleSheet(const QString& resourcePath)
{
  QFile file(resourcePath);
  if (!file.open(QFile::ReadOnly | QFile::Text))
  {
    qDebug() << "Cannot open QSS:" << file.errorString();
    return QString();
  }

  QTextStream stream(&file);
  return stream.readAll();
}
