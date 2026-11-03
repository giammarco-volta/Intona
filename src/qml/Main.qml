import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

import Intona
import NaadaLab.Ui as SharedUi

ApplicationWindow {
    id: root
    Rectangle { anchors.fill: parent; z: -1000; color: SharedUi.Theme.background }
    SharedUi.LicensePanel {
        id: licensePanel
        showBadge: false
        anchors.fill: parent
        z: 10000
        license: root.licensePreviewState ? previewLicense : LicenseAccess
        appName: "Intona"
    }

    // Desktop previews never call or modify the real licensing backend.
    property string licensePreviewState: ""
    function previewLicenseState(state) {
        if (!viewportSimulationEnabled)
            return
        licensePreviewState = state
        licensePanel.expanded = false
    }
    QtObject {
        id: previewLicense
        readonly property bool enabled: true
        readonly property bool allowed: root.licensePreviewState !== "expired"
        readonly property var status: ({
            state: root.licensePreviewState,
            configured: false, canBuy: false, busy: false,
            price: "€9.99",
            message: "Windows preview — purchase and restore actions are disabled.",
            trialEndsAt: root.licensePreviewState === "trial" ? Date.now() / 1000 + 3 * 86400 : 0
        })
        signal accessGranted()
        function activate() {}
        function restore() {}
        function buy() {}
        function unlockReview(code) {}
        function copySupportReference() { return false }
    }
    Shortcut {
        sequence: "Ctrl+Alt+1"
        enabled: root.viewportSimulationEnabled
        onActivated: root.viewportProfile = Main.MobileLandscape
    }
    Shortcut {
        sequence: "Ctrl+Alt+2"
        enabled: root.viewportSimulationEnabled
        onActivated: root.viewportProfile = Main.TabletLandscape
    }
    Shortcut {
        sequence: "Ctrl+Alt+4"
        enabled: root.viewportSimulationEnabled
        onActivated: root.previewLicenseState("purchased")
    }
    Shortcut {
        sequence: "Ctrl+Alt+5"
        enabled: root.viewportSimulationEnabled
        onActivated: root.previewLicenseState("trial")
    }
    Shortcut {
        sequence: "Ctrl+Alt+6"
        enabled: root.viewportSimulationEnabled
        onActivated: root.previewLicenseState("expired")
    }
    Shortcut {
        sequence: "Ctrl+Alt+0"
        enabled: root.viewportSimulationEnabled
        onActivated: root.previewLicenseState("")
    }

    Shortcut {
        sequence: "F8"
        enabled: root.viewportSimulationEnabled && !captureNotice.opened
        onActivated: StoreScreenshot.capture(root.contentItem,
            root.viewportProfile === Main.MobileLandscape ? "phone"
            : root.viewportProfile === Main.TabletLandscape ? "tablet" : "desktop")
    }
    Connections {
        target: StoreScreenshot
        function onFinished(success, message) {
            captureNotice.title = success ? "Screenshot saved" : "Screenshot not saved"
            captureMessage.text = message
            captureNotice.open()
        }
    }
    Dialog {
        id: captureNotice
        anchors.centerIn: parent
        width: Math.min(560, root.width - 32)
        modal: true
        standardButtons: Dialog.Ok
        Label { id: captureMessage; width: parent.width; wrapMode: Text.WrapAnywhere }
    }

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
        Qt.platform.os === "windows"

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
            width = 640
            height = 360

            minimumWidth = 640
            maximumWidth = 640
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
        licenseVisible: licensePanel.visible
        licenseState: licensePanel.info.state || ""
        licensePending: licensePanel.pending
        onLicenseRequested: licensePanel.expanded = true
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
