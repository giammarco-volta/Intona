import QtQuick
import QtQuick.Controls

import NaadaLab.Ui as SharedUi

Item {
    id: root

    property var keyNames: []
    property var canRaiseKeys: []
    property var canLowerKeys: []
    property var pressedKeys: []

    signal stepRequested(int keyIndex, int direction)

    readonly property real keyboardHeight:
        width * 217 / 416
    readonly property real buttonBand:
        Math.max(34, keyboardHeight * 0.24)

    readonly property var stepX: [
        0.075, 0.145, 0.215, 0.285,
        0.355, 0.500, 0.575, 0.645,
        0.715, 0.785, 0.855, 0.925
    ]

    readonly property var whiteKeys: [
        { "key": 0,  "x": 0.075 },
        { "key": 2,  "x": 0.215 },
        { "key": 4,  "x": 0.355 },
        { "key": 5,  "x": 0.500 },
        { "key": 7,  "x": 0.645 },
        { "key": 9,  "x": 0.785 },
        { "key": 11, "x": 0.925 }
    ]

    readonly property var blackKeys: [
        { "key": 1,  "x": 0.150 },
        { "key": 3,  "x": 0.290 },
        { "key": 6,  "x": 0.555 },
        { "key": 8,  "x": 0.725 },
        { "key": 10, "x": 0.875 }
    ]

    height: keyboardHeight + buttonBand * 2

    Image {
        id: keyboardImage

        x: 0
        y: root.buttonBand
        width: root.width
        height: root.keyboardHeight

        source: "qrc:/images/keyboard.png"
        fillMode: Image.Stretch
        smooth: true
    }

    Repeater {
        model: root.whiteKeys

        delegate: Label {
            required property var modelData

            x: root.width * modelData.x - width / 2
            y: root.buttonBand
               + root.keyboardHeight * 0.78
            width: root.width * 0.11
            height: root.keyboardHeight * 0.14

            text: root.keyNames.length > modelData.key
                  ? root.keyNames[modelData.key] : ""
            color: root.pressedKeys.length > modelData.key
                   && root.pressedKeys[modelData.key]
                   ? SharedUi.Theme.success
                   : SharedUi.Theme.background
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.bold: true
            font.pixelSize: Math.max(8,
                root.keyboardHeight * 0.085)
        }
    }

    Repeater {
        model: root.blackKeys

        delegate: Label {
            required property var modelData

            x: root.width * modelData.x - width / 2
            y: root.buttonBand
               + root.keyboardHeight * 0.33
            width: root.width * 0.09
            height: root.keyboardHeight * 0.12

            text: root.keyNames.length > modelData.key
                  ? root.keyNames[modelData.key] : ""
            color: root.pressedKeys.length > modelData.key
                   && root.pressedKeys[modelData.key]
                   ? SharedUi.Theme.success
                   : SharedUi.Theme.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.bold: true
            font.pixelSize: Math.max(7,
                root.keyboardHeight * 0.065)
        }
    }

    Repeater {
        model: 12

        delegate: Item {
            required property int index

            readonly property bool raiseEnabled:
                root.canRaiseKeys.length > index
                && root.canRaiseKeys[index]
            readonly property bool lowerEnabled:
                root.canLowerKeys.length > index
                && root.canLowerKeys[index]

            x: root.width * root.stepX[index] - width / 2
            width: Math.max(24, root.width * 0.065)
            height: root.height

            Label {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: root.buttonBand

                text: "\u25B2"
                color: parent.raiseEnabled
                       ? SharedUi.Theme.text
                       : Qt.darker(
                             SharedUi.Theme.disabledText, 1.55)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.bold: true
                font.pixelSize: Math.max(18,
                    root.keyboardHeight * 0.14)

                MouseArea {
                    anchors.fill: parent
                    enabled: parent.parent.raiseEnabled
                    cursorShape: enabled
                                 ? Qt.PointingHandCursor
                                 : Qt.ArrowCursor
                    onClicked:
                        root.stepRequested(index, 1)
                }
            }

            Label {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: root.buttonBand

                text: "\u25BC"
                color: parent.lowerEnabled
                       ? SharedUi.Theme.text
                       : Qt.darker(
                             SharedUi.Theme.disabledText, 1.55)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.bold: true
                font.pixelSize: Math.max(18,
                    root.keyboardHeight * 0.14)

                MouseArea {
                    anchors.fill: parent
                    enabled: parent.parent.lowerEnabled
                    cursorShape: enabled
                                 ? Qt.PointingHandCursor
                                 : Qt.ArrowCursor
                    onClicked:
                        root.stepRequested(index, -1)
                }
            }
        }
    }
}
