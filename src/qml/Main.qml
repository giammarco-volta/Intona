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

    readonly property int layoutClass:
        SharedUi.UiMetrics.layoutClassForHeight(contentItem.height)

    readonly property string layoutProfileName:
        layoutClass === SharedUi.UiMetrics.Phone
            ? "Mobile"
            : layoutClass === SharedUi.UiMetrics.Tablet
                ? "Tablet"
                : "Desktop"

    readonly property real viewportWidth: contentItem.width
    readonly property real viewportHeight: contentItem.height

    enum ViewportProfile {
        Desktop,
        MobileLandscape,
        TabletLandscape
    }

    property int viewportProfile: Main.TabletLandscape

    readonly property bool viewportSimulationEnabled:
        Qt.platform.os === "windows" && DebugBuild

    function cycleViewportProfile() {
        if (!viewportSimulationEnabled)
            return

        viewportProfile = (viewportProfile + 1) % 3
    }

    function applyViewportProfile() {
        minimumWidth = 0
        minimumHeight = 0
        maximumWidth = 16777215
        maximumHeight = 16777215

        switch (viewportProfile) {
        case Main.MobileLandscape:
            width = 800
            height = 360

            minimumWidth = 800
            maximumWidth = 800
            minimumHeight = 360
            maximumHeight = 360
            break

        case Main.TabletLandscape:
            width = 1280
            height = 720

            minimumWidth = 1280
            maximumWidth = 1280
            minimumHeight = 720
            maximumHeight = 720
            break

        case Main.Desktop:
        default:
            width = 1400
            height = 900

            minimumWidth = 640
            minimumHeight = 360
            break
        }
    }

    Component.onCompleted: {
        if (viewportSimulationEnabled)
            applyViewportProfile()
    }

    onViewportProfileChanged: {
        if (viewportSimulationEnabled)
            applyViewportProfile()
    }

    NavigationShell {
        anchors.fill: parent
    }

    onLayoutClassChanged: {
        console.log(
            "Layout profile:",
            layoutProfileName,
            "viewport:",
            viewportWidth,
            "x",
            viewportHeight
        )
    }
}
