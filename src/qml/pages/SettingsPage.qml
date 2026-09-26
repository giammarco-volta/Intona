import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import NaadaLab.Ui as SharedUi

Rectangle {
    id: root

    readonly property bool mobileLayout:
        ApplicationWindow.window
            ? ApplicationWindow.window.layoutClass === SharedUi.UiMetrics.Phone
            : false

    color: SharedUi.Theme.background

    ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: root.mobileLayout ? 10 : SharedUi.Theme.pageMargin
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: SharedUi.Theme.pageSpacing

            Label {
                text: qsTr("Settings")
                color: SharedUi.Theme.text
                font.pixelSize: root.mobileLayout ? 18 : 24
                font.bold: true
            }

            SharedUi.FormSection {
                title: qsTr("Held notes")
                Layout.fillWidth: true

                CheckBox {
                    objectName: "retriggerHeldNotesSelector"
                    text: qsTr("Retrigger held notes")
                    Layout.fillWidth: true
                    checked: TuningController.retriggerHeldNotes
                    onToggled: TuningController.retriggerHeldNotes = checked
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                    text: qsTr("When enabled, adaptive tuning stops and restarts affected notes around the tuning change. When disabled, Intona sends only the tuning: the instrument may update held notes itself, or apply the new pitch only at the next attack. This preference is saved automatically.")
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                    text: qsTr("Test your instrument with a sustained sound, such as an organ. Release all keys and the sustain pedal. A two-second note will play on the selected MIDI output channels; its tuning changes halfway through. The previous tuning is restored afterwards.")
                }
                Button {
                    objectName: "retuningTestButton"
                    text: TuningController.retuningTestRunning ? qsTr("Stop test") : qsTr("Test held-note tuning")
                    onClicked: {
                        testAnswer.text = ""
                        if (TuningController.retuningTestRunning) TuningController.cancelRetuningTest()
                        else TuningController.startRetuningTest()
                    }
                }
                Label {
                    objectName: "retuningTestStatus"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.text
                    text: TuningController.retuningTestMessage
                    visible: text.length > 0
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: TuningController.retuningTestAwaitingAnswer
                    Button {
                        text: qsTr("Yes, the pitch changed")
                        onClicked: testAnswer.text = qsTr("The instrument updates held notes itself. You can disable retriggering to avoid another attack.")
                    }
                    Button {
                        text: qsTr("No, it stayed the same")
                        onClicked: testAnswer.text = qsTr("Keep retriggering enabled if held notes should follow tuning changes. Disable it if you prefer the original pitch to continue until release.")
                    }
                    Button {
                        text: qsTr("I could not tell")
                        onClicked: testAnswer.text = qsTr("Try again with a sustained sound. If you heard no note, check the MIDI output and channels. The setting has not been changed.")
                    }
                }
                Label {
                    id: testAnswer
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                    visible: text.length > 0 && TuningController.retuningTestAwaitingAnswer
                }
            }

            SharedUi.FormSection {
                title: qsTr("Record a performance")
                Layout.fillWidth: true

                Label {
                    text: qsTr("Record MIDI notes and pedal events for timing analysis. Start before playing and stop after releasing all keys. No audio is recorded.")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                }
                TextField {
                    id: recordingLabel
                    objectName: "recordingLabel"
                    placeholderText: qsTr("Take name, e.g. Bach slow or Bach fast")
                    enabled: !PerformanceRecorder.recording
                    maximumLength: 60
                    Layout.fillWidth: true
                    Layout.maximumWidth: 560
                    Accessible.name: qsTr("Take name")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 560
                    Button {
                        objectName: "recordingToggle"
                        text: PerformanceRecorder.recording ? qsTr("Stop recording") : qsTr("Start recording")
                        Layout.fillWidth: true
                        onClicked: {
                            if (PerformanceRecorder.recording) PerformanceRecorder.stop()
                            else PerformanceRecorder.start(recordingLabel.text)
                        }
                    }
                    Button {
                        text: qsTr("Open folder")
                        Layout.fillWidth: true
                        enabled: PerformanceRecorder.filePath.length > 0
                        onClicked: Qt.openUrlExternally(PerformanceRecorder.directoryUrl)
                    }
                }
                Label {
                    objectName: "recordingStatus"
                    text: PerformanceRecorder.recording ? qsTr("Recording MIDI events…")
                        : PerformanceRecorder.filePath.length > 0 ? qsTr("Recording stopped") : ""
                    visible: text.length > 0
                    color: SharedUi.Theme.text
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                TextArea {
                    text: PerformanceRecorder.filePath
                    visible: text.length > 0
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    Layout.fillWidth: true
                    Accessible.name: qsTr("Recording file")
                }
                Label {
                    text: PerformanceRecorder.error
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    color: SharedUi.Theme.text
                }
            }

            SharedUi.FormSection {
                title: qsTr("Note names")
                Layout.fillWidth: true

                ComboBox {
                    id: namingMode
                    objectName: "noteNamingModeSelector"
                    Layout.fillWidth: true
                    model: [
                        qsTr("Cycle of fifths"),
                        qsTr("Simplified names and +/-")
                    ]
                    currentIndex: TuningController.noteNamingMode
                    onActivated: function(index) {
                        TuningController.noteNamingMode = index
                    }
                    Accessible.name: qsTr("Note naming convention")
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                    text: namingMode.currentIndex === 0
                        ? qsTr("Names follow the cycle of fifths, with as many sharps or flats as needed.")
                        : qsTr("Simplify the tuning centre to at most two accidentals, using + or - for each EDO step when needed. Derive the other eleven selected names from its exact intervals, allowing additional accidentals and sharing its step modifier. Unselected notes use the nearest name with at most two accidentals.")
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                    text: qsTr("This preference is saved automatically and applies to all displayed note names.")
                }
            }
        }
    }
}
