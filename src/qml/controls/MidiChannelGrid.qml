import QtQuick
import QtQuick.Layouts

import NaadaLab.Ui as SharedUi

GridLayout {
    id: root

    property bool multipleSelection: false

    // Usato quando multipleSelection è false.
    property int selectedChannel: 1

    // Bit 0 = canale 1, bit 15 = canale 16.
    property int selectedChannelMask: 0
    property real buttonHeight: SharedUi.Theme.controlHeight

    signal selectionRequested(int channel, bool selected)

    columns: 8
    columnSpacing: SharedUi.Theme.spacing
    rowSpacing: SharedUi.Theme.spacing

    function isChannelSelected(channel) {
        if (!multipleSelection)
            return selectedChannel === channel

        const bit = 1 << (channel - 1)
        return (selectedChannelMask & bit) !== 0
    }

    Repeater {
        model: 16

        SharedUi.ActionButton {
            required property int index

            readonly property int channel: index + 1
            readonly property bool channelSelected:
                root.isChannelSelected(channel)

            text: String(channel)
            selected: channelSelected

            Layout.fillWidth: true
            Layout.preferredHeight: root.buttonHeight

            onClicked: {
                if (root.multipleSelection) {
                    root.selectionRequested(
                        channel,
                        !channelSelected)
                } else if (!channelSelected) {
                    root.selectionRequested(channel, true)
                }
            }
        }
    }
}
