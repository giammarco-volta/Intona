#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QQuickItem>
#include <cmath>
#include <QQuickStyle>
#include <QElapsedTimer>
#include <QThread>
#include <QImage>
#include <QDir>
#include <QFontDatabase>
#include <iostream>
#include <memory>
#include <QTemporaryDir>
#include "midi/MidiEventRecorder.h"

static void settle()
{
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 200)
  {
    QCoreApplication::processEvents();
    QThread::msleep(2);
  }
}

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
#ifdef Q_OS_WIN
  // The offscreen platform does not discover Windows system fonts itself.
  const QString fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
  QFontDatabase::addApplicationFont(fonts + "arial.ttf");
  QFontDatabase::addApplicationFont(fonts + "arialbd.ttf");
#endif
  QQuickStyle::setStyle("Material");
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  QQmlEngine engine;
  QTemporaryDir recordingDirectory;
  MidiEventRecorder recorder(nullptr, recordingDirectory.path());
  engine.rootContext()->setContextProperty("PerformanceRecorder", &recorder);
  QQmlPropertyMap settings;
  settings.insert("noteNamingMode", 0);
  settings.insert("useScaleTriadAdapting", false);
  engine.rootContext()->setContextProperty("TuningController", &settings);
  QQmlComponent component(&engine);
  const auto pages = QUrl::fromLocalFile(QStringLiteral(INTONA_SOURCE_DIR "/src/qml/pages")).toString();
  const QString qml = QStringLiteral(R"(
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import NaadaLab.Ui as SharedUi
import "%1" as Pages
ApplicationWindow {
    width: 1200; height: 700; visible: true
    font.family: "Arial"
    Material.theme: Material.Dark
    Material.accent: Material.Amber
    readonly property int layoutClass: SharedUi.UiMetrics.layoutClassForHeight(height)
    Pages.SettingsPage { anchors.fill: parent }
})").arg(pages);
  component.setData(qml.toUtf8(), QUrl());
  if (component.isError())
  {
    std::cerr << component.errorString().toStdString();
    return 1;
  }
  std::unique_ptr<QObject> root(component.create());
  auto* window = qobject_cast<QQuickWindow*>(root.get());
  auto* selector = root ? root->findChild<QObject*>("noteNamingModeSelector") : nullptr;
  if (!window || !selector)
  {
    std::cerr << component.errorString().toStdString() << "Missing window or selector\n";
    return 1;
  }
  auto* threshold = root->findChild<QObject*>("dirtyNoteThresholdSelector");
  auto* recordButton = root->findChild<QObject*>("recordingToggle");
  auto* recordLabel = root->findChild<QObject*>("recordingLabel");
  if (!recordButton || !recordLabel) return 25;
  recordLabel->setProperty("text", "Bach slow");
  QMetaObject::invokeMethod(recordButton, "clicked");
  if (!recorder.recording()) return 26;
  recorder.recordInput(0x90,60,90,1000,1);
  recorder.recordInput(0x80,60,0,1125,1);
  QMetaObject::invokeMethod(recordButton, "clicked");
  if (recorder.recording() || !QFile::exists(recorder.filePath())) return 27;
  auto* adapting = root->findChild<QObject*>("scaleTriadAdaptingSelector");
  if (!adapting || adapting->property("checked").toBool()) return 11;
  adapting->setProperty("checked", true);
  QMetaObject::invokeMethod(adapting, "toggled");
  if (!settings.value("useScaleTriadAdapting").toBool()) return 12;
  if (threshold || root->findChild<QObject*>("historyDecaySlopeSelector")
    || root->findChild<QObject*>("historyWindowIntervalsSelector")) return 16;
  settings.insert("useScaleTriadAdapting", false);
  settle();
  if (adapting->property("checked").toBool()) return 13;
  if (selector->property("currentIndex").toInt() != 0)
    return 2;
  QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1));
  if (settings.value("noteNamingMode").toInt() != 1)
    return 3;
  settings.insert("noteNamingMode", 0);
  settle();
  if (selector->property("currentIndex").toInt() != 0)
    return 4;
  settings.insert("noteNamingMode", 1);
  settle();
  if (selector->property("currentIndex").toInt() != 1)
    return 5;

  const QString screenshotDir = qEnvironmentVariable("INTONA_TEST_SCREENSHOT_DIR");
  if (!screenshotDir.isEmpty())
  {
    QDir().mkpath(screenshotDir);
    if (!window->grabWindow().save(screenshotDir + "/settings-desktop.png"))
      return 6;
  }
  window->resize(800, 360);
  settle();
  if (!screenshotDir.isEmpty()
    && !window->grabWindow().save(screenshotDir + "/settings-mobile.png"))
    return 7;
  window->resize(360,700);
  settle();
  if(!screenshotDir.isEmpty() && !window->grabWindow().save(screenshotDir+"/settings-portrait.png"))return 24;
  std::cout << "PASS: Settings page loads, obsolete controls are absent, recording is available, naming and algorithm controls write preferences, external updates preserve bindings, desktop and mobile render.\n";
  return 0;
}
