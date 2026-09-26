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

            GridLayout {
                Layout.fillWidth: true
                columns: scroll.availableWidth >= 740 ? 2 : 1
                columnSpacing: SharedUi.Theme.pageSpacing
                rowSpacing: SharedUi.Theme.pageSpacing
                SharedUi.FormSection {
                    id: controlSettingsPanel
                    objectName: "controlSettingsPanel"
                    title: qsTr("Keyboard and pedal control")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    CheckBox {
                        objectName: "midiControlEnabled"
                        text: qsTr("Enable assigned control")
                        checked: TuningController.controlEnabled
                        onToggled: TuningController.controlEnabled = checked
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 500
                        columns: controlSettingsPanel.width >= 430 ? 2 : 1
                        columnSpacing: 12
                        rowSpacing: 8
                        Label { text: qsTr("Control"); color: SharedUi.Theme.text }
                        ComboBox {
                            objectName: "midiControlSource"
                            Layout.fillWidth: true
                            Layout.maximumWidth: 400
                            model: TuningController.controlSources
                            currentIndex: TuningController.controlSource
                            onActivated: function(index) { TuningController.controlSource = index }
                            Accessible.name: qsTr("MIDI control")
                        }
                        Label { text: qsTr("Action"); color: SharedUi.Theme.text }
                        ComboBox {
                            objectName: "midiControlAction"
                            Layout.fillWidth: true
                            Layout.maximumWidth: 400
                            model: [qsTr("Tuning step up"), qsTr("Tuning step down"),
                                    qsTr("Next preset"), qsTr("Previous preset")]
                            currentIndex: TuningController.controlAction
                            onActivated: function(index) { TuningController.controlAction = index }
                            Accessible.name: qsTr("Assigned action")
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 400
                        Label {
                            text: qsTr("Activation threshold")
                            color: SharedUi.Theme.text
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        SpinBox {
                            objectName: "midiControlThreshold"
                            from: 1; to: 127; editable: true
                            value: TuningController.controlThreshold
                            onValueModified: TuningController.controlThreshold = value
                            Accessible.name: qsTr("Activation threshold")
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: SharedUi.Theme.secondaryText
                        text: qsTr("One action per gesture. Enabled controls are reserved for Intona.")
                    }

                }

                SharedUi.FormSection {
                    objectName: "heldNotesSettingsPanel"
                    title: qsTr("Held notes")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop

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
                        text: qsTr("Restart held notes when their tuning changes.")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: SharedUi.Theme.secondaryText
                        text: qsTr("Test with a sustained sound and release keys and pedals.")
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
                            onClicked: testAnswer.text = qsTr("You can disable retriggering.")
                        }
                        Button {
                            text: qsTr("No, it stayed the same")
                            onClicked: testAnswer.text = qsTr("Enable retriggering to update held notes.")
                        }
                        Button {
                            text: qsTr("I could not tell")
                            onClicked: testAnswer.text = qsTr("Try a sustained sound and check the MIDI output.")
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
            }

            SharedUi.FormSection {
                title: qsTr("Record a performance")
                Layout.fillWidth: true

                Label {
                    text: qsTr("Record MIDI events for analysis. No audio.")
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
                    Layout.maximumWidth: 560
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
                    Layout.maximumWidth: 400
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
                        ? qsTr("Names follow the cycle of fifths.")
                        : qsTr("Simplified centre names with +/-; exact interval names for selected notes.")
                }


            }
        }
    }
}
