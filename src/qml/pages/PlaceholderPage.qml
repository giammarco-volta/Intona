import QtQuick

import NaadaLab.Ui as SharedUi

Rectangle {
    id: root

    property string pageTitle: ""

    color: SharedUi.Theme.background

    Text {
        anchors.centerIn: parent

        text: root.pageTitle
        color: SharedUi.Theme.text
        font.pixelSize: 24
        font.bold: true
    }
}