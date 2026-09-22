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
                title: qsTr("RT Adapting")
                Layout.fillWidth: true

                CheckBox {
                    objectName: "scaleTriadAdaptingSelector"
                    text: qsTr("Alternative: scales and triads")
                    Layout.fillWidth: true
                    checked: TuningController.useScaleTriadAdapting
                    onToggled: TuningController.useScaleTriadAdapting = checked
                    Accessible.name: text
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                    text: qsTr("Use scales and major/minor triads to interpret incoming notes, with progressive confirmation. Disable this option to compare with the original chord and melody logic.")
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: SharedUi.Theme.secondaryText
                    text: qsTr("Notes sound immediately. Changes are checked after 70 ms to reject short notes and brief overlaps. Provisional readings can be revised as more notes arrive; established held notes act as pivots.")
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
