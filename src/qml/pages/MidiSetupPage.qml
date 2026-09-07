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
        mobileLayout ? 10 : SharedUi.Theme.pageMargin

    readonly property real channelButtonHeight:
        SharedUi.UiMetrics.controlHeight(layoutClass)

    color: SharedUi.Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.pageMargin
        spacing: mobileLayout
                 ? SharedUi.Theme.spacing
                 : SharedUi.Theme.pageSpacing

        Label {
            text: qsTr("MIDI Setup")
            color: SharedUi.Theme.text
            font.pixelSize: mobileLayout ? 18 : 24
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: mobileLayout
                     ? SharedUi.Theme.spacing
                     : SharedUi.Theme.pageSpacing

            SharedUi.FormSection {
                title: qsTr("MIDI IN")

                Layout.fillWidth: true
                Layout.fillHeight: true

                SharedUi.MidiPortSelector {
                    title: qsTr("Device")
                    ports: MidiController.midiInPorts
                    currentPort: MidiController.midiInPort
                    refreshText: qsTr("Refresh")

                    Layout.fillWidth: true

                    onPortSelected: function(portName) {
                        MidiController.midiInPort = portName
                    }

                    onRefreshRequested:
                        MidiController.refreshMidiInPorts()
                }

                Label {
                    text: qsTr("Input channel")
                    color: SharedUi.Theme.secondaryText
                    font.pixelSize: SharedUi.Theme.labelFontSize
                }

                MidiChannelGrid {
                    multipleSelection: false
                    selectedChannel: MidiController.midiInChannel
                    selectedChannelMask: 0
                    buttonHeight: root.channelButtonHeight

                    Layout.fillWidth: true

                    onSelectionRequested: function(channel, selected) {
                        if (selected)
                            MidiController.midiInChannel = channel
                    }
                }
            }

            SharedUi.FormSection {
                title: qsTr("MIDI OUT")

                Layout.fillWidth: true
                Layout.fillHeight: true

                SharedUi.MidiPortSelector {
                    title: qsTr("Device")
                    ports: MidiController.midiOutPorts
                    currentPort: MidiController.midiOutPort
                    refreshText: qsTr("Refresh")

                    Layout.fillWidth: true

                    onPortSelected: function(portName) {
                        MidiController.midiOutPort = portName
                    }

                    onRefreshRequested:
                        MidiController.refreshMidiOutPorts()
                }

                Label {
                    text: qsTr("Output channels")
                    color: SharedUi.Theme.secondaryText
                    font.pixelSize: SharedUi.Theme.labelFontSize
                }

                MidiChannelGrid {
                    multipleSelection: true
                    selectedChannel: 0
                    selectedChannelMask:
                        MidiController.midiOutChannelMask
                    buttonHeight: root.channelButtonHeight

                    Layout.fillWidth: true

                    onSelectionRequested: function(channel, selected) {
                        MidiController.setMidiOutChannelEnabled(
                            channel,
                            selected)
                    }
                }
            }
        }
    }
}
