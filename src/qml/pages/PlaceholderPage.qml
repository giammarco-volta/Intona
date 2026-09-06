import QtQuick

import NaadaLab.Ui as SharedUi

Rectangle {
    id: root

    property string pageTitle: ""
    property real titleFontSize: 24

    color: SharedUi.Theme.background

    Text {
        anchors.centerIn: parent

        text: root.pageTitle
        color: SharedUi.Theme.text
        font.pixelSize: root.titleFontSize
        font.bold: true
    }
}
