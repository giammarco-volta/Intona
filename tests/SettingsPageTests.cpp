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
#include "synth/PatchData.h"
static bool qmlRuntimeError = false;

class PageSettings : public QQmlPropertyMap {
  Q_OBJECT
  Q_PROPERTY(QVariantList spectrumPreview READ spectrumPreview NOTIFY spectrumPreviewChanged)
  Q_PROPERTY(int spectrumNote READ spectrumNote WRITE setSpectrumNote NOTIFY spectrumPreviewChanged)
public:
  PageSettings() : QQmlPropertyMap(this, nullptr) {}
  Q_INVOKABLE QVariantList synthShapeAmplitudes(int shape) const { return Intona::Synth::shapeAmplitudes(shape); }
  Q_INVOKABLE QVariantMap synthShapeData(int shape, double width) const { return Intona::Synth::shapeData(shape, float(width)); }
  int spectrumNote() const { return note_; }
  void setSpectrumNote(int note) { note_ = note; emit spectrumPreviewChanged(); }
  QVariantList spectrumPreview() const {
    const auto patch = Intona::Synth::readPatch(value("synthPatch").toMap());
    return patch ? Intona::Synth::spectrumData(*patch, value("edo").toInt(), 440 * std::exp2((note_ - 69) / 12.)) : QVariantList{};
  }
  Q_INVOKABLE bool setSynthPatch(const QVariantMap& value) {
    const auto parsed = Intona::Synth::readPatch(value);
    if (!parsed) return false;
    insert("synthPatch", Intona::Synth::patchData(*parsed));
    emit spectrumPreviewChanged();
    return true;
  }
signals:
  void spectrumPreviewChanged();
private:
  int note_ = 69;
};

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
  qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) {
    if (message.contains("ReferenceError") || message.contains("TypeError") || message.contains("Binding loop") ||
        message.contains("Unable to assign")) qmlRuntimeError = true;
    std::cerr << message.toStdString() << '\n';
  });
#ifdef Q_OS_WIN
  // The offscreen platform does not discover Windows system fonts itself.
  const QString fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
  QFontDatabase::addApplicationFont(fonts + "arial.ttf");
  QFontDatabase::addApplicationFont(fonts + "arialbd.ttf");
#endif
  QQuickStyle::setStyle("Material");
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  QQmlEngine engine;
  engine.rootContext()->setContextProperty("RecordingUiEnabled", false);
  QTemporaryDir recordingDirectory;
  MidiEventRecorder recorder(nullptr, recordingDirectory.path());
  engine.rootContext()->setContextProperty("PerformanceRecorder", &recorder);
  PageSettings settings;
  settings.insert("internalAudio", false);
  settings.insert("audioError", "");
  settings.insert("synthPatch", Intona::Synth::patchData({}));
  settings.insert("edo", 19);
  settings.insert("keyValues", QVariantList{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
  settings.insert("keyNames", QStringList{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
  settings.insert("tuningCenter", 0);
  settings.insert("adaptingEnabled", true);
  settings.insert("currentPresetIndex", -1);
  settings.insert("controlSources", QStringList{"Channel aftertouch", "Polyphonic aftertouch", "Pitch bend up"});
  settings.insert("controlSource", 0);
  settings.insert("controlAction", 0);
  settings.insert("controlThreshold", 10);
  settings.insert("controlEnabled", true);
  settings.insert("noteNamingMode", 0);
  settings.insert("retriggerHeldNotes", true);
  settings.insert("retuningTestRunning", false);
  settings.insert("retuningTestAwaitingAnswer", false);
  settings.insert("retuningTestMessage", "");
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
  if (root->findChild<QObject*>("internalAudioSelector")) return 70;
  QQmlPropertyMap midi;
  midi.insert("midiInPorts", QStringList{"Input"});
  midi.insert("midiOutPorts", QStringList{"Output"});
  midi.insert("midiInPort", "Input");
  midi.insert("midiOutPort", "Output");
  midi.insert("midiInChannel", 1);
  midi.insert("midiOutChannelMask", 2);
  engine.rootContext()->setContextProperty("MidiController", &midi);
  QQmlComponent editor(&engine);
  editor.setData(QStringLiteral(R"(
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import "%1" as Pages
ApplicationWindow {
    Material.theme: Material.Dark
    Material.accent: Material.Amber
    width: 1000; height: 680; visible: true
    property int layoutClass: 0
    Pages.SynthPage { anchors.fill: parent }
    Pages.MidiSetupPage { visible: false }
})").arg(pages).toUtf8(), QUrl());
  std::unique_ptr<QObject> synthWindow(editor.create());
  if (!synthWindow) { std::cerr << editor.errorString().toStdString(); return 71; }
  auto* toggle = synthWindow->findChild<QObject*>("internalAudioSelector");
  auto* spectrum = synthWindow->findChild<QObject*>("synthSpectrumSelector");
  auto* midiIn = synthWindow->findChild<QObject*>("midiInputPanel");
  auto* midiOut = synthWindow->findChild<QObject*>("midiOutputPanel");
  if (!toggle || !spectrum || !midiIn || !midiOut) return 72;
  for (bool internal : {true, false}) {
    toggle->setProperty("checked", internal);
    QMetaObject::invokeMethod(toggle, "toggled");
    settle();
    if (!midiIn->property("enabled").toBool() || midiOut->property("enabled").toBool() == internal ||
        settings.value("internalAudio").toBool() != internal) return 73;
  }
  QMetaObject::invokeMethod(spectrum, "activated", Q_ARG(int, 2));
  if (settings.value("synthPatch").toMap().value("spectrum").toInt() != 3) return 74;
  const auto before = settings.value("synthPatch").toMap().value("temporal").toList();
  auto* env = synthWindow->findChild<QObject*>("timeEnvelopeEditor");
  if (!env || !QMetaObject::invokeMethod(env, "updatePoint", Q_ARG(QVariant, 0.), Q_ARG(QVariant, .25))) return 75;
  const auto after = settings.value("synthPatch").toMap().value("temporal").toList();
  if (before == after || after.first().toMap().value("y").toFloat() != .25f) return 76;
  auto* shape = synthWindow->findChild<QObject*>("synthShapeSelector");
  auto* partial = synthWindow->findChild<QObject*>("synthPartialSelector");
  auto* view = synthWindow->findChild<QObject*>("synthSpectrumView");
  auto* details = synthWindow->findChild<QObject*>("synthPartialDetails");
  auto* status = synthWindow->findChild<QObject*>("synthTemperamentStatus");
  if (!shape || !partial || !view || !details || !status) return 77;
  QMetaObject::invokeMethod(shape, "activated", Q_ARG(int, 2));
  partial->setProperty("value", 3);
  QMetaObject::invokeMethod(partial, "valueModified");
  settle();
  if (settings.value("synthPatch").toMap().value("shape").toInt() != 2 ||
      view->property("selected").toInt() != 2 || !details->property("text").toString().contains("Inverted") ||
      !status->property("text").toString().contains("19 EDO")) return 78;
  auto* amplitude = synthWindow->findChild<QObject*>("synthPartialAmplitudeValue");
  auto* modified = synthWindow->findChild<QObject*>("synthSpectrumModified");
  if (!amplitude || !modified || spectrum->property("count").toInt() != 3 ||
      synthWindow->findChild<QObject*>("countSlider")) return 79;
  amplitude->setProperty("value", 1234); QMetaObject::invokeMethod(amplitude, "valueModified");
  auto limited = settings.value("synthPatch").toMap();
  if (std::abs(limited["amplitudes"].toList()[2].toFloat() + .1234f) > 1e-6 ||
      limited["amplitudesB"].toList()[2].toFloat() != 1 ||
      !modified->property("text").toString().contains("Custom")) return 87;
  limited["shape"] = 4; limited["amplitudes"] = settings.synthShapeAmplitudes(4);
  limited["phaseOffsets"] = settings.synthShapeData(4, .5)["phaseOffsets"]; limited["spectrum"] = 1;
  settings.setSynthPatch(limited); settle();
  auto* endpoint = synthWindow->findChild<QObject*>("synthEndpointSelector");
  auto* copy = synthWindow->findChild<QObject*>("synthCopySpectrum");
  auto* spectralEnvelope = synthWindow->findChild<QObject*>("synthSpectralEnvelopeEditor");
  auto* axis = synthWindow->findChild<QObject*>("synthSpectralAxisSelector");
  if (!endpoint || !copy || !spectralEnvelope || !axis) return 80;
  QMetaObject::invokeMethod(endpoint, "activated", Q_ARG(int, 1));
  QMetaObject::invokeMethod(shape, "activated", Q_ARG(int, 3));
  QMetaObject::invokeMethod(spectralEnvelope, "updatePoint", Q_ARG(QVariant, 1.), Q_ARG(QVariant, .4));
  auto ab = settings.value("synthPatch").toMap();
  if (ab["shape"].toInt() != 4 || ab["shapeB"].toInt() != 3 || ab["spectral"] == ab["spectralB"]) return 81;
  QMetaObject::invokeMethod(axis, "activated", Q_ARG(int, 2));
  ab = settings.value("synthPatch").toMap();
  if (ab["spectralAxis"].toInt() != 0 || ab["spectralAxisB"].toInt() != 2 ||
      ab["spectralB"].toList().first().toMap()["x"].toFloat() != 440) return 82;
  QMetaObject::invokeMethod(copy, "clicked");
  ab = settings.value("synthPatch").toMap();
  for (const auto field : {"shape", "amplitudes", "phaseOffsets", "pulseWidth", "spectralAxis", "spectral"})
    if (ab[field] != ab[QString(field) + "B"]) return 83;
  QMetaObject::invokeMethod(endpoint, "activated", Q_ARG(int, 0));
  QMetaObject::invokeMethod(shape, "activated", Q_ARG(int, 2));
  QMetaObject::invokeMethod(copy, "clicked");
  if (settings.value("synthPatch").toMap()["shapeB"].toInt() != 2) return 84;
  for (const auto field : {std::pair{"morph", .5}, {"morphLfoHz", .25}, {"morphSeconds", .8}}) {
    auto* slider = synthWindow->findChild<QObject*>(QString(field.first) + "Slider");
    if (!slider) return 85;
    slider->setProperty("value", field.second);
    QMetaObject::invokeMethod(slider, "moved");
    if (std::abs(settings.value("synthPatch").toMap()[field.first].toDouble() - field.second) > 1e-6) return 86;
  }
  auto* mode = synthWindow->findChild<QObject*>("synthMorphModeSelector");
  auto* stretch = synthWindow->findChild<QObject*>("stretchValue");
  auto* secondStep = synthWindow->findChild<QObject*>("synthEdoSecondStep");
  if (!mode || !stretch || !secondStep) return 88;
  for (int choice : {1, 2, 0}) {
    QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, choice));
    if (settings.value("synthPatch").toMap()["morphMode"].toInt() != choice) return 89;
  }
  stretch->setProperty("value", 103); QMetaObject::invokeMethod(stretch, "valueModified");
  secondStep->setProperty("value", 19); QMetaObject::invokeMethod(secondStep, "valueModified");
  if (std::abs(settings.value("synthPatch").toMap()["stretch"].toDouble() - 1.03) > 1e-6 ||
      settings.value("synthPatch").toMap()["edoSecondStep"].toInt() != 19) return 90;
  if (spectralEnvelope->property("axisKind").toInt() != 2 || env->property("axisKind").toInt() != 3 ||
      !spectralEnvelope->findChild<QObject*>("envelopeGrid") || !env->findChild<QObject*>("envelopeGrid")) return 91;
  if (shape->property("count").toInt() != 10) return 94;
  QMetaObject::invokeMethod(shape, "activated", Q_ARG(int, 0));
  ab = settings.value("synthPatch").toMap();
  if (ab["phaseOffsets"].toList()[0].toFloat() != .25f || shape->property("currentText").toString() != "Impulse") return 95;
  QMetaObject::invokeMethod(shape, "activated", Q_ARG(int, 9));
  auto* pulseWidth = synthWindow->findChild<QObject*>("synthPulseWidthValue");
  if (!pulseWidth || !pulseWidth->property("visible").toBool()) return 96;
  pulseWidth->setProperty("value", 250); QMetaObject::invokeMethod(pulseWidth, "valueModified");
  ab = settings.value("synthPatch").toMap();
  if (ab["pulseWidth"].toFloat() != .25f || ab["pulseWidthB"].toFloat() != .5f ||
      std::abs(ab["amplitudes"].toList()[1].toFloat() - .5f) > 1e-6 ||
      ab["phaseOffsets"].toList()[0].toFloat() != .125f) return 97;
  QMetaObject::invokeMethod(copy, "clicked");
  ab = settings.value("synthPatch").toMap();
  for (const auto field : {"shape", "amplitudes", "phaseOffsets", "pulseWidth"})
    if (ab[field] != ab[QString(field) + "B"]) return 98;
  ab = settings.value("synthPatch").toMap();
  ab["shapeB"] = 4;
  ab["amplitudesB"] = settings.synthShapeAmplitudes(4);
  ab["phaseOffsetsB"] = settings.synthShapeData(4, .5)["phaseOffsets"];
  ab["morphMode"] = 1;
  ab["spectralAxis"] = ab["spectralAxisB"] = 0;
  ab["spectral"] = ab["spectralB"] = QVariantList{QVariantMap{{"x", 0}, {"y", 1}}};
  settings.setSynthPatch(ab); settle();
  const auto renderDirectory = qEnvironmentVariable("INTONA_TEST_SCREENSHOT_DIR");
  if (!renderDirectory.isEmpty()) {
    QDir().mkpath(renderDirectory);
    auto* synthView = qobject_cast<QQuickWindow*>(synthWindow.get());
    synthView->grabWindow().save(renderDirectory + "/synth-desktop.png");
    synthView->resize(640, 360); settle();
    synthView->grabWindow().save(renderDirectory + "/synth-mobile.png");
    synthView->resize(400, 780); settle();
    synthView->grabWindow().save(renderDirectory + "/synth-portrait.png");
    synthView->resize(1100, 850); settle();
    auto* scroll = synthWindow->findChild<QObject*>("synthEditorScroll");
    auto* flick = scroll ? scroll->property("contentItem").value<QObject*>() : nullptr;
    if (!flick) return 93;
    for (int unit : {0, 1, 2}) {
      QMetaObject::invokeMethod(axis, "activated", Q_ARG(int, unit)); settle();
      flick->setProperty("contentY", flick->property("contentHeight").toDouble() - flick->property("height").toDouble()); settle();
      synthView->grabWindow().save(renderDirectory + QString("/envelopes-%1.png").arg(unit));
    }
  }
  synthWindow.reset();

  auto* source = root->findChild<QObject*>("midiControlSource");
  auto* action = root->findChild<QObject*>("midiControlAction");
  auto* enabled = root->findChild<QObject*>("midiControlEnabled");
  if (!source || !action || !enabled || !root->findChild<QObject*>("midiControlThreshold")) return 34;
  QMetaObject::invokeMethod(source, "activated", Q_ARG(int, 1));
  QMetaObject::invokeMethod(action, "activated", Q_ARG(int, 2));
  if (settings.value("controlSource").toInt() != 1 || settings.value("controlAction").toInt() != 2) return 35;
  auto* retrigger = root->findChild<QObject*>("retriggerHeldNotesSelector");
  if (!retrigger || !retrigger->property("checked").toBool()
      || !root->findChild<QObject*>("retuningTestButton")) return 30;
  retrigger->setProperty("checked", false);
  QMetaObject::invokeMethod(retrigger, "toggled");
  if (settings.value("retriggerHeldNotes").toBool()) return 31;
  auto* threshold = root->findChild<QObject*>("dirtyNoteThresholdSelector");
  auto* recordingPanel = root->findChild<QObject*>("recordingPanel");
  if (!recordingPanel || recordingPanel->property("visible").toBool()) return 50;
  engine.rootContext()->setContextProperty("RecordingUiEnabled", true);
  settle();
  if (!recordingPanel->property("visible").toBool()) return 51;
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
  engine.rootContext()->setContextProperty("RecordingUiEnabled", false);
  settle();
  if (recordingPanel->property("visible").toBool()) return 52;
  auto* adapting = root->findChild<QObject*>("scaleTriadAdaptingSelector");
  if (adapting || threshold || root->findChild<QObject*>("historyDecaySlopeSelector")
    || root->findChild<QObject*>("historyWindowIntervalsSelector")) return 16;
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
  window->resize(640, 360);
  settle();
  if (!screenshotDir.isEmpty()
    && !window->grabWindow().save(screenshotDir + "/settings-mobile.png"))
    return 7;
  window->resize(360,700);
  settle();
  if(!screenshotDir.isEmpty() && !window->grabWindow().save(screenshotDir+"/settings-portrait.png"))return 24;
  settings.insert("noteNamingMode", 2);
  if (selector->property("count").toInt() != 3) return 46;
  settings.insert("retuningTestAwaitingAnswer", true);
  settings.insert("retuningTestMessage", "Did you hear the note change by almost a semitone (80 cents) halfway through?");
  settle();
  if (!root->findChild<QObject*>("retuningTestStatus")->property("visible").toBool()) return 32;
  if (!screenshotDir.isEmpty()
      && !window->grabWindow().save(screenshotDir + "/settings-test-result.png")) return 33;
  QQmlComponent surface(&engine);
  const auto controls = QUrl::fromLocalFile(QStringLiteral(INTONA_SOURCE_DIR "/src/qml/controls")).toString();
  // The two side-by-side MIDI panels leave about 253 logical pixels per grid
  // on a 640px phone. Check double-digit labels, including the bold selection.
  QQmlComponent channelComponent(&engine);
  channelComponent.setData(QStringLiteral(R"(
import QtQuick
import QtQuick.Controls
import "%1" as Controls
ApplicationWindow {
    width: 253; height: 90; visible: true
    font.family: "Arial"
    Controls.MidiChannelGrid {
        objectName: "channelGrid"
        width: 253
        selectedChannel: 12
        buttonHeight: 40
    }
})").arg(controls).toUtf8(), QUrl());
  std::unique_ptr<QObject> channelWindow(channelComponent.create());
  if (!channelWindow) { std::cerr << channelComponent.errorString().toStdString(); return 60; }
  settle();
  auto* channelGrid = channelWindow->findChild<QQuickItem*>("channelGrid");
  int checkedChannels = 0;
  for (auto* button : channelGrid->childItems()) {
    if (!button->property("channel").isValid()) continue;
    auto* content = qvariant_cast<QQuickItem*>(button->property("contentItem"));
    if (!content) return 61;
    bool foundLabel = false;
    for (auto* label : content->childItems()) {
      if (label->property("text").toString() != button->property("text").toString()) continue;
      const QPointF origin = label->mapToItem(button, QPointF(0, 0));
      if (origin.x() < -0.5 || origin.x() + label->width() > button->width() + 0.5
          || std::abs(origin.x() + label->width()/2 - button->width()/2) > 0.5)
        return 62;
      foundLabel = true;
    }
    if (!foundLabel) return 63;
    ++checkedChannels;
  }
  if (checkedChannels != 16) return 64;
  if (!screenshotDir.isEmpty())
    qobject_cast<QQuickWindow*>(channelWindow.get())->grabWindow().save(screenshotDir + "/midi-channels-phone.png");
  channelWindow.reset();
  const QString surfaceQml = QStringLiteral(R"(
import QtQuick
import QtQuick.Controls
import "%1" as Controls
ApplicationWindow {
    id: testWindow
    width:1000; height:720; visible:true
    property int toggleCount:0
    property int directionCount:0
    Controls.TuningCircle {
        anchors.fill:parent
        keyNames: ["C\u0307", "D\u0323b", "D\u0307", "E\u0323b", "E\u0307", "F\u0323", "F\u0307#", "G\u0323", "A\u0307b", "A\u0323", "B\u0307b", "B\u0323"]
        pressedKeys: [true,true,false,false,true,false,false,true,false,false,false,false]
        edo: 12; tuningCenter: 0
        entries: keyNames.map(function(name, i) { return {name:name, value:i, pitchStep:i, cents:i*100, selected:true} })
        controlText:"CC 11 = preset next"; controlEnabled:true
        onControlToggled: { testWindow.toggleCount++; controlEnabled = !controlEnabled }
        onControlDirectionRequested: testWindow.directionCount++
    }
})").arg(controls);
  surface.setData(surfaceQml.toUtf8(), QUrl());
  std::unique_ptr<QObject> surfaceRoot(surface.create());
  if (!surfaceRoot) { std::cerr << surface.errorString().toStdString(); return 36; }
  auto* enableArea = surfaceRoot->findChild<QQuickItem*>("controlEnabledHitArea");
  auto* directionArea = surfaceRoot->findChild<QQuickItem*>("controlDirectionHitArea");
  if (!enableArea || !directionArea) return 37;
  settle();
  if (enableArea->mapToScene(QPointF(enableArea->width(), 0)).x()
      > directionArea->mapToScene(QPointF(0, 0)).x() + 0.1) return 38;
  void* clickEvent = nullptr;
  QMetaObject::invokeMethod(enableArea, "clicked", Qt::DirectConnection, QGenericArgument("QQuickMouseEvent*", &clickEvent));
  if (surfaceRoot->property("toggleCount").toInt() != 1 || surfaceRoot->property("directionCount").toInt()) return 39;
  QMetaObject::invokeMethod(directionArea, "clicked", Qt::DirectConnection, QGenericArgument("QQuickMouseEvent*", &clickEvent));
  if (surfaceRoot->property("toggleCount").toInt() != 1 || surfaceRoot->property("directionCount").toInt() != 1) return 40;
  if (!screenshotDir.isEmpty()) {
    auto* surfaceWindow = qobject_cast<QQuickWindow*>(surfaceRoot.get());
    if (!surfaceWindow || !surfaceWindow->grabWindow().save(screenshotDir + "/dotted-surface.png")) return 45;
  }
  surfaceRoot.reset();
  settle();
  // The saved name arrives before asynchronous MIDI enumeration at startup.
  QQmlComponent midiSelector(&engine);
  midiSelector.setData(R"(
import QtQuick
import NaadaLab.Ui as SharedUi
SharedUi.MidiPortSelector {
    width: 500
    currentPort: "Pa5X"
    property int selections: 0
    onPortSelected: function(name) { selections++; currentPort = name }
})", QUrl());
  std::unique_ptr<QObject> portRoot(midiSelector.create());
  if (!portRoot) { std::cerr << midiSelector.errorString().toStdString(); return 41; }
  QObject* portCombo = nullptr;
  for (auto* child : portRoot->findChildren<QObject*>())
    if (child->metaObject()->indexOfProperty("currentText") >= 0) { portCombo = child; break; }
  if (!portCombo) return 42;
  portRoot->setProperty("ports", QStringList{"Unconnected output", "Pa5X", "Other output"});
  settle();
  if (portCombo->property("currentText").toString() != "Pa5X") {
    std::cerr << "Saved output Pa5X is displayed as " << portCombo->property("currentText").toString().toStdString() << "\n";
    return 43;
  }
  portRoot->setProperty("ports", QStringList{"Pa5X", "Other output", "Unconnected output"});
  settle();
  if (portCombo->property("currentText").toString() != "Pa5X") return 44;
  portRoot->setProperty("ports", QStringList{"Other output"});
  settle();
  if (portCombo->property("currentIndex").toInt() != -1) return 45;
  portRoot->setProperty("ports", QStringList{"Other output", "Pa5X"});
  settle();
  if (portCombo->property("currentText").toString() != "Pa5X" || portRoot->property("selections").toInt()) return 46;
  portRoot->setProperty("currentPort", "Other output");
  settle();
  if (portCombo->property("currentText").toString() != "Other output") return 47;
  portCombo->setProperty("currentIndex", 1);
  QMetaObject::invokeMethod(portCombo, "activated", Q_ARG(int, 1));
  settle();
  if (portRoot->property("currentPort").toString() != "Pa5X" || portRoot->property("selections").toInt() != 1) return 48;
  portRoot->setProperty("ports", QStringList{"Pa5X", "Other output"});
  settle();
  if (portCombo->property("currentText").toString() != "Pa5X") return 49;
  portRoot.reset();
  std::cout << "PASS: saved MIDI port remains visible through asynchronous enumeration, reorder and reconnect.\n";
  // Exercise the real shared rail and its packaged SVG with the software
  // renderer: a shader-only tint used to leave this icon completely blank.
  QQmlComponent navigation(&engine);
  navigation.setData(R"(
import QtQuick
import QtQuick.Controls
import NaadaLab.Ui as SharedUi
ApplicationWindow {
    width:64; height:64; visible:true; color:"#202020"
    SharedUi.NavigationRail {
        objectName:"iconRail"; anchors.fill:parent
        sections:[{section:"settings", name:"Settings", icon:"settings"}]
        currentSection:"settings"
    }
})", QUrl());
  std::unique_ptr<QObject> navigationRoot(navigation.create());
  auto *navigationWindow = qobject_cast<QQuickWindow *>(navigationRoot.get());
  auto *rail = navigationRoot ? navigationRoot->findChild<QObject *>("iconRail") : nullptr;
  if (!navigationWindow || !rail)
  {
    std::cerr << navigation.errorString().toStdString();
    return 28;
  }
  const auto iconPixels = [](const QImage &image, QColor color) {
    int count = 0;
    const double scale = image.devicePixelRatio();
    for (int y = int(8 * scale); y < int(48 * scale); ++y)
      for (int x = int(8 * scale); x < int(56 * scale); ++x)
      {
        const auto pixel = image.pixelColor(x, y);
        if (std::abs(pixel.red() - color.red()) < 10 &&
            std::abs(pixel.green() - color.green()) < 10 &&
            std::abs(pixel.blue() - color.blue()) < 10) ++count;
      }
    return count;
  };
  settle();
  auto icon = navigationWindow->grabWindow();
  if (icon.isNull() || iconPixels(icon, QColor("#D8B85A")) < 50) return 29;
  if (!screenshotDir.isEmpty()) icon.save(screenshotDir + "/settings-icon-selected.png");
  rail->setProperty("currentSection", "");
  settle();
  icon = navigationWindow->grabWindow();
  if (icon.isNull() || iconPixels(icon, QColor("#E0E0E0")) < 50) return 30;
  std::cout << "PASS: Settings navigation icon is visible and changes tint with software rendering.\n";
  std::cout << "PASS: Settings page loads, obsolete controls are absent, recording is available, naming control writes preferences, external updates preserve bindings, desktop and mobile render.\n";
  return qmlRuntimeError ? 92 : 0;
}

#include "SettingsPageTests.moc"
