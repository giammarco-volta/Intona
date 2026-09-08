import QtQuick
import QtQuick.Controls

import NaadaLab.Ui as SharedUi

Item {
    id: root

    property var entries: []
    property int edo: 12
    property int edoIndex: -1
    property var availableEdos: []
    property int tuningCenter: -128
    property string tuningCenterName: ""
    property var keyNames: []
    property var canRaiseKeys: []
    property var canLowerKeys: []

    signal edoSelected(int index)
    signal tuningCenterSelected(int value)
    signal keyStepRequested(int keyIndex, int direction)
    signal capturePresetRequested()

    readonly property real diameter:
        Math.max(0, Math.min(width, height) - 20)

    readonly property real outerRadius:
        diameter * 0.48

    readonly property real innerRadius:
        diameter * 0.405

    readonly property real nameRadius:
        diameter * 0.4475

    readonly property real centsRadius:
        diameter * 0.375

    readonly property real noteTouchTarget:
        Math.max(
            28,
            Math.min(
                46,
                2 * Math.PI * nameRadius
                    / Math.max(1, edo) * 0.82))

    readonly property real centerX: width / 2
    readonly property real centerY: height / 2

    Rectangle {
        x: root.centerX - root.outerRadius
        y: root.centerY - root.outerRadius
        width: root.outerRadius * 2
        height: width
        radius: width / 2

        color: "transparent"
        border.color: Qt.lighter(
                          SharedUi.Theme.border, 1.35)
        border.width: 2
    }

    Rectangle {
        x: root.centerX - root.innerRadius
        y: root.centerY - root.innerRadius
        width: root.innerRadius * 2
        height: width
        radius: width / 2

        color: "transparent"
        border.color: Qt.lighter(
                          SharedUi.Theme.border, 1.35)
        border.width: 2
    }

    Repeater {
        model: root.entries

        delegate: Item {
            id: entryDelegate

            required property var modelData

            anchors.fill: parent

            readonly property real angle:
                -Math.PI / 2
                + 2 * Math.PI
                    * modelData.pitchStep
                    / Math.max(1, root.edo)

            Label {
                x: root.centerX
                   + root.nameRadius * Math.cos(parent.angle)
                   - width / 2

                y: root.centerY
                   + root.nameRadius * Math.sin(parent.angle)
                   - height / 2

                text: parent.modelData.name

                color: parent.modelData.selected
                       ? SharedUi.Theme.accent
                       : SharedUi.Theme.disabledText

                font.pixelSize:
                    Math.max(
                        9,
                        Math.min(15, root.diameter / 34))

                font.bold: parent.modelData.selected

                MouseArea {
                    anchors.centerIn: parent
                    width: Math.max(
                               parent.width,
                               root.noteTouchTarget)
                    height: Math.max(
                                parent.height,
                                root.noteTouchTarget)

                    cursorShape: Qt.PointingHandCursor
                    preventStealing: true

                    onClicked: {
                        root.tuningCenterSelected(
                            entryDelegate.modelData.value)
                    }
                }
            }

            Rectangle {
                readonly property real dotRadius:
                    root.outerRadius + 7

                x: root.centerX
                   + dotRadius * Math.cos(parent.angle)
                   - width / 2

                y: root.centerY
                   + dotRadius * Math.sin(parent.angle)
                   - height / 2

                width: Math.max(6, root.diameter / 90)
                height: width
                radius: width / 2

                visible:
                    parent.modelData.value
                    === root.tuningCenter

                color: SharedUi.Theme.accent
            }

            Label {
                x: root.centerX
                   + root.centsRadius * Math.cos(parent.angle)
                   - width / 2

                y: root.centerY
                   + root.centsRadius * Math.sin(parent.angle)
                   - height / 2

                text: Number(parent.modelData.cents).toFixed(1)

                color: parent.modelData.selected
                       ? SharedUi.Theme.accent
                       : SharedUi.Theme.disabledText

                opacity: parent.modelData.selected ? 0.9 : 0.65

                font.pixelSize:
                    Math.max(
                        7,
                        Math.min(11, root.diameter / 46))
            }
        }
    }

    Item {
        id: edoSelector

        x: root.centerX - width / 2
        y: root.centerY - root.centsRadius
           + Math.max(36, root.diameter / 14)

        width: edoRow.implicitWidth
        height: edoRow.implicitHeight

        Row {
            id: edoRow

            spacing: 5

            Label {
                text: qsTr("%1 EDO").arg(root.edo)
                color: SharedUi.Theme.accent
                font.pixelSize:
                    Math.max(16, root.diameter / 22)
                font.bold: true
            }

            Label {
                y: (parent.height - height) / 2

                text: "\u25BC"
                color: SharedUi.Theme.accent
                font.pixelSize:
                    Math.max(10, root.diameter / 32)
            }

        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: edoPopup.open()
        }
    }

    Label {
        x: root.centerX - width / 2
        y: root.centerY - height / 2

        text: root.tuningCenterName.length > 0
              ? qsTr("Tuning center: %1")
                    .arg(root.tuningCenterName)
              : qsTr("Custom tuning")

        color: SharedUi.Theme.secondaryText
        font.pixelSize:
            Math.max(11, root.diameter / 35)
    }

    TuningKeyboard {
        id: tuningKeyboard

        x: root.centerX - width / 2
        y: root.centerY - height / 2
        width: Math.min(root.width * 0.68,
                        root.innerRadius * 1.50)

        keyNames: root.keyNames
        canRaiseKeys: root.canRaiseKeys
        canLowerKeys: root.canLowerKeys

        onStepRequested: function(keyIndex, direction) {
            root.keyStepRequested(keyIndex, direction)
        }
    }

    Label {
        x: root.centerX + root.outerRadius - width - 14
        y: root.centerY - root.nameRadius - height / 2

        text: qsTr("Keep it →")
        color: SharedUi.Theme.accent
        font.bold: true
        font.pixelSize: Math.max(11, root.diameter / 38)

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.capturePresetRequested()
        }
    }

    Popup {
        id: edoPopup

        anchors.centerIn: parent
        width: Math.min(220, root.width - 24)
        height: Math.min(
                    contentItem.implicitHeight + 20,
                    root.height - 24)

        padding: 10
        modal: true
        focus: true
        closePolicy:
            Popup.CloseOnEscape
            | Popup.CloseOnPressOutside

        background: Rectangle {
            color: SharedUi.Theme.panel
            border.color: SharedUi.Theme.border
            border.width: 1
            radius: 6
        }

        contentItem: ListView {
            implicitHeight:
                Math.min(contentHeight, root.height * 0.7)

            spacing: 4
            clip: true
            model: root.availableEdos

            delegate: Rectangle {
                required property int index
                required property var modelData

                width: ListView.view.width
                height: 42
                radius: 4

                color: index === root.edoIndex
                       ? SharedUi.Theme.accent
                       : SharedUi.Theme.panelDark

                border.color: SharedUi.Theme.border
                border.width: 1

                Label {
                    anchors.centerIn: parent

                    text: qsTr("%1 EDO")
                              .arg(parent.modelData)

                    color: parent.index === root.edoIndex
                           ? SharedUi.Theme.background
                           : SharedUi.Theme.text

                    font.bold:
                        parent.index === root.edoIndex
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        root.edoSelected(parent.index)
                        edoPopup.close()
                    }
                }
            }
        }
    }
}
