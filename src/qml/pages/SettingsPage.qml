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
