import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

import Intona
import NaadaLab.Ui as SharedUi

ApplicationWindow {
    id: root

    visible: true
    width: 1200
    height: 700
    minimumWidth: 640
    minimumHeight: 360

    title: "Intona"
    color: SharedUi.Theme.background
    font.family: "Arial"

    Material.theme: Material.Dark
    Material.accent: Material.Amber

    NavigationShell {
        anchors.fill: parent
    }
}