import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import NaadaLab.Ui as SharedUi
import "../controls"

Rectangle {
    id: root

    readonly property int layoutClass:
        ApplicationWindow.window
            ? ApplicationWindow.window.layoutClass
            : SharedUi.UiMetrics.Desktop

    readonly property bool mobileLayout:
        layoutClass === SharedUi.UiMetrics.Phone

    readonly property real pageMargin:
        mobileLayout ? 6 : 10

    readonly property var presets:
        TuningController.presetEntries

    color: SharedUi.Theme.background

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.pageMargin
        spacing: mobileLayout ? 6 : 10

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: root.width * 0.68

            color: Qt.darker(
                       SharedUi.Theme.background, 1.8)
            border.color: SharedUi.Theme.border
            border.width: 1

            TuningCircle {
                anchors.fill: parent
                anchors.margins: 4

                compactLayout: root.mobileLayout

                entries: TuningController.circleEntries
                edo: TuningController.edo
                edoIndex: TuningController.edoIndex
                availableEdos:
                    TuningController.availableEdos
                tuningCenter:
                    TuningController.tuningCenter
                tuningCenterName:
                    TuningController.tuningCenterName
                keyNames: TuningController.keyNames
                canRaiseKeys: TuningController.canRaiseKeys
                canLowerKeys: TuningController.canLowerKeys
                pressedKeys: TuningController.pressedKeys
                adaptingEnabled:
                    TuningController.adaptingEnabled
                aftertouchText:
                    TuningController.aftertouchText
                aftertouchEnabled:
                    TuningController.aftertouchEnabled
                keyDescription:
                    TuningController.keyDescription
                chordDescription:
                    TuningController.chordDescription

                onEdoSelected: function(index) {
                    TuningController.edoIndex = index
                }

                onTuningCenterSelected: function(value) {
                    TuningController.selectTuningCenter(value)
                }

                onKeyStepRequested: function(keyIndex, direction) {
                    TuningController.stepKeyPitch(
                        keyIndex, direction)
                }

                onCapturePresetRequested:
                    TuningController.captureCurrentPreset()

                onAdaptingToggled:
                    TuningController.adaptingEnabled =
                        !TuningController.adaptingEnabled

                onAftertouchModeRequested:
                    TuningController.cycleAftertouchMode()
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth:
                root.width * (mobileLayout ? 0.34 : 0.32)

            Layout.minimumWidth:
                mobileLayout ? 190 : 260

            color: Qt.darker(
                       SharedUi.Theme.background, 1.8)
            border.color: SharedUi.Theme.border
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: mobileLayout ? 6 : 10
                spacing: 8

                Label {
                    text: qsTr("Presets")
                    color: SharedUi.Theme.text
                    font.pixelSize: mobileLayout ? 15 : 18
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: presetList

                        anchors.fill: parent
                        spacing: 6
                        clip: true

                        model: root.presets

                        delegate: Rectangle {
                            id: presetDelegate

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: mobileLayout ? 38 : 46

                            radius: 5
                            color: SharedUi.Theme.panel
                            border.color:
                                index === TuningController.currentPresetIndex
                                ? SharedUi.Theme.accent
                                : SharedUi.Theme.border
                            border.width: 1

                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 40

                                text: modelData.text
                                color: SharedUi.Theme.text
                                font.pixelSize:
                                    mobileLayout ? 11 : 13

                                verticalAlignment:
                                    Text.AlignVCenter

                                elide: Text.ElideRight
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked:
                                    TuningController.applyPreset(
                                        presetDelegate.index)
                            }

                            Label {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 36

                                text: "×"
                                color: "#AA5A5A"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: mobileLayout ? 17 : 20

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked:
                                        TuningController.deletePreset(
                                            presetDelegate.index)
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent

                        visible: root.presets.length === 0

                        text: qsTr("No presets for this EDO")
                        color: SharedUi.Theme.secondaryText
                        font.pixelSize:
                            SharedUi.Theme.labelFontSize
                    }
                }
            }
        }
    }
}
