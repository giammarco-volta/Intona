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
    property var pressedKeys: []
    property bool adaptingEnabled: true
    property string aftertouchText: ""
    property bool aftertouchEnabled: true
    property string keyDescription: ""
    property string chordDescription: ""
    property bool compactLayout: false
    property bool noteDragActive: false
    property int noteDragTargetStep: -1

    signal edoSelected(int index)
    signal tuningCenterSelected(int value)
    signal keyStepRequested(int keyIndex, int direction)
    signal keyMoveRequested(int keyIndex, int stepCount)
    signal capturePresetRequested()
    signal adaptingToggled()
    signal aftertouchModeRequested()

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

    readonly property real sideLabelsX:
        compactLayout
        ? 6
        : Math.max(
              12,
              centerX - outerRadius - diameter * 0.10)

    readonly property real edoVerticalInset:
        compactLayout
        ? Math.max(24, diameter / 18)
        : Math.max(36, diameter / 14)

    readonly property real optionTouchHeight:
        Math.max(40, diameter / 15)

    readonly property real statusFontSize:
        Math.max(10, diameter / 40)

    readonly property real statusLineHeight:
        statusFontSize + 7

    readonly property real centerX: width / 2
    readonly property real centerY: height / 2

    function entryAtPitchStep(pitchStep) {
        for (let index = 0; index < entries.length; ++index) {
            if (entries[index].pitchStep === pitchStep)
                return entries[index]
        }

        return null
    }

    function pitchStepAt(point) {
        let angle = Math.atan2(
                        point.y - centerY,
                        point.x - centerX) + Math.PI / 2

        if (angle < 0)
            angle += 2 * Math.PI

        return Math.round(
                    angle * Math.max(1, edo)
                    / (2 * Math.PI)) % Math.max(1, edo)
    }

    function resetNoteDrag() {
        noteDragActive = false
        noteDragTargetStep = -1
    }

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
                id: noteLabel

                x: root.centerX
                   + root.nameRadius * Math.cos(parent.angle)
                   - width / 2

                y: root.centerY
                   + root.nameRadius * Math.sin(parent.angle)
                   - height / 2

                text: parent.modelData.name

                color: parent.modelData.pressed
                       ? SharedUi.Theme.success
                       : root.noteDragActive
                         && root.noteDragTargetStep
                            === parent.modelData.pitchStep
                         ? SharedUi.Theme.link
                       : parent.modelData.selected
                         ? SharedUi.Theme.accent
                         : SharedUi.Theme.disabledText

                font.pixelSize:
                    Math.max(
                        9,
                        Math.min(15, root.diameter / 34))

                font.bold:
                    parent.modelData.selected
                    || parent.modelData.pressed

                scale: root.noteDragActive
                       && root.noteDragTargetStep
                          === parent.modelData.pitchStep
                       ? 1.2
                       : 1.0

                Behavior on scale {
                    NumberAnimation { duration: 80 }
                }

                MouseArea {
                    id: noteArea

                    anchors.centerIn: parent
                    width: Math.max(
                               parent.width,
                               root.noteTouchTarget)
                    height: Math.max(
                                parent.height,
                                root.noteTouchTarget)

                    cursorShape: Qt.PointingHandCursor
                    preventStealing: true

                    property point pressPoint: Qt.point(0, 0)
                    property int previousPitchStep: -1
                    property int accumulatedSteps: 0

                    onPressed: function(mouse) {
                        pressPoint = mapToItem(
                            root, mouse.x, mouse.y)
                        previousPitchStep =
                            entryDelegate.modelData.pitchStep
                        accumulatedSteps = 0
                        root.resetNoteDrag()
                    }

                    onPositionChanged: function(mouse) {
                        if (!pressed
                            || !entryDelegate.modelData.selected)
                            return

                        const point = mapToItem(
                            root, mouse.x, mouse.y)

                        if (!root.noteDragActive) {
                            const dx = point.x - pressPoint.x
                            const dy = point.y - pressPoint.y
                            const threshold = Math.max(
                                8, Qt.styleHints.startDragDistance)

                            if (Math.sqrt(dx * dx + dy * dy)
                                < threshold)
                                return

                            root.noteDragActive = true
                        }

                        const pitchStep = root.pitchStepAt(point)
                        let delta = pitchStep - previousPitchStep
                        const halfEdo = root.edo / 2

                        if (delta > halfEdo)
                            delta -= root.edo
                        else if (delta < -halfEdo)
                            delta += root.edo

                        accumulatedSteps += delta
                        previousPitchStep = pitchStep
                        root.noteDragTargetStep = pitchStep
                    }

                    onReleased: function(mouse) {
                        if (!root.noteDragActive) {
                            root.tuningCenterSelected(
                                entryDelegate.modelData.value)
                            return
                        }

                        const target = root.entryAtPitchStep(
                            root.noteDragTargetStep)

                        if (target
                            && !target.selected
                            && accumulatedSteps !== 0) {
                            root.keyMoveRequested(
                                entryDelegate.modelData.keyIndex,
                                accumulatedSteps)
                        }

                        root.resetNoteDrag()
                    }

                    onCanceled: root.resetNoteDrag()
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

            Rectangle {
                readonly property real dotRadius:
                    root.outerRadius + 14

                x: root.centerX
                   + dotRadius * Math.cos(parent.angle)
                   - width / 2

                y: root.centerY
                   + dotRadius * Math.sin(parent.angle)
                   - height / 2

                width: Math.max(5, root.diameter / 105)
                height: width
                radius: width / 2
                visible: parent.modelData.keyTonic
                color: SharedUi.Theme.link
            }

            Rectangle {
                readonly property real dotRadius:
                    root.outerRadius + 21

                x: root.centerX
                   + dotRadius * Math.cos(parent.angle)
                   - width / 2

                y: root.centerY
                   + dotRadius * Math.sin(parent.angle)
                   - height / 2

                width: Math.max(4, root.diameter / 125)
                height: width
                radius: width / 2
                visible: parent.modelData.chordRoot
                color: SharedUi.Theme.error
            }

            Label {
                x: root.centerX
                   + root.centsRadius * Math.cos(parent.angle)
                   - width / 2

                y: root.centerY
                   + root.centsRadius * Math.sin(parent.angle)
                   - height / 2

                text: Number(parent.modelData.cents).toFixed(1)

                color: parent.modelData.pressed
                       ? SharedUi.Theme.success
                       : parent.modelData.selected
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
           + root.edoVerticalInset

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

    Column {
        x: root.sideLabelsX
        y: root.centerY - root.nameRadius
        spacing: 0

        Item {
            width: Math.max(
                       aftertouchLabel.implicitWidth + 20,
                       root.diameter * 0.32)
            height: root.optionTouchHeight

            Label {
                id: aftertouchLabel

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -parent.height / 2

                text: root.aftertouchText
                color: root.aftertouchEnabled
                       ? SharedUi.Theme.success
                       : SharedUi.Theme.disabledText
                font.bold: true
                font.pixelSize: Math.max(11, root.diameter / 38)
            }

            MouseArea {
                anchors.fill: parent
                anchors.topMargin: -parent.height / 2
                anchors.bottomMargin: parent.height / 2
                cursorShape: Qt.PointingHandCursor
                preventStealing: true
                onClicked: root.aftertouchModeRequested()
            }
        }

        Item {
            width: Math.max(
                       adaptingLabel.implicitWidth + 20,
                       root.diameter * 0.32)
            height: root.optionTouchHeight

            Label {
                id: adaptingLabel

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                text: root.adaptingEnabled
                      ? qsTr("✓ RT Adapting")
                      : qsTr("✕ RT Adapting")
                color: root.adaptingEnabled
                       ? SharedUi.Theme.success
                       : SharedUi.Theme.disabledText
                font.bold: true
                font.pixelSize: Math.max(11, root.diameter / 38)
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                preventStealing: true
                onClicked: root.adaptingToggled()
            }
        }
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
        pressedKeys: root.pressedKeys

        onStepRequested: function(keyIndex, direction) {
            root.keyStepRequested(keyIndex, direction)
        }
    }

    Column {
        x: root.sideLabelsX
        y: root.centerY + root.nameRadius
           - height
        width: Math.max(180, root.diameter * 0.36)
        height: root.statusLineHeight * 3 + spacing * 2
        spacing: 3

        Label {
            width: parent.width
            height: root.statusLineHeight
            text: root.tuningCenterName.length > 0
                  ? qsTr("Tuning Center = %1")
                        .arg(root.tuningCenterName)
                  : ""
            color: SharedUi.Theme.accent
            font.pixelSize: root.statusFontSize
        }

        Label {
            width: parent.width
            height: root.statusLineHeight
            text: root.keyDescription
            color: SharedUi.Theme.link
            font.pixelSize: root.statusFontSize
        }

        Label {
            width: parent.width
            height: root.statusLineHeight
            text: root.chordDescription
            color: SharedUi.Theme.error
            font.pixelSize: root.statusFontSize
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
