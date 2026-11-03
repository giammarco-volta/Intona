import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Intona
import NaadaLab.Ui as SharedUi

Item {
    id: root
    property bool licenseVisible: false
    property string licenseState: ""
    property bool licensePending: false
    signal licenseRequested()
    readonly property real licenseButtonHeight: licenseVisible ? (mobileLayout ? 44 : 56) : 0
    readonly property int licenseIconState: !licensePending && (licenseState === "purchased" || licenseState === "review") ? 0
        : !licensePending && licenseState === "trial" ? 1 : 2
    readonly property string licenseLabel: licensePending ? qsTr("Payment pending")
        : licenseState === "review" ? qsTr("Review access")
        : licenseIconState === 0 ? qsTr("Full version")
        : licenseIconState === 1 ? qsTr("Trial active") : qsTr("Review license access")


    readonly property int layoutClass:
        ApplicationWindow.window
            ? ApplicationWindow.window.layoutClass
            : SharedUi.UiMetrics.Desktop

    readonly property bool mobileLayout:
        layoutClass === SharedUi.UiMetrics.Phone

    readonly property bool tabletLayout:
        layoutClass === SharedUi.UiMetrics.Tablet

    readonly property real railWidth:
        mobileLayout ? 52 : tabletLayout ? 60 : 64

    readonly property real sectionButtonHeight:
        Math.min(mobileLayout ? 52 : 64,
                 (height - licenseButtonHeight) / Math.max(1, sections.length))

    readonly property real sectionIconScale:
        Math.min(1, sectionButtonHeight / 56)

    readonly property real pageTitleFontSize:
        mobileLayout ? 18 : tabletLayout ? 22 : 24

    property string currentSection: "surface"

    readonly property var sections: [
        {
            section: "surface",
            name: qsTr("Tuning Surface"),
            icon: "tuning"
        },
        {
            section: "midi",
            name: qsTr("MIDI Setup"),
            icon: "midi"
        },
        {
            section: "settings",
            name: qsTr("Settings"),
            icon: "settings"
        },
        {
            section: "manual",
            name: qsTr("Manual"),
            icon: "manual"
        },
        {
            section: "about",
            name: qsTr("About"),
            icon: "about"
        }
    ]

    function sectionIndex(section) {
        switch (section) {
        case "surface":
            return 0
        case "midi":
            return 1
        case "settings":
            return 2
        case "manual":
            return 3
        case "about":
            return 4
        default:
            return 0
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SharedUi.NavigationRail {
            railWidth: root.railWidth

            Layout.preferredWidth: root.railWidth
            Layout.minimumWidth: root.railWidth
            Layout.maximumWidth: root.railWidth
            Layout.fillHeight: true

            sections: root.sections
            currentSection: root.currentSection
            buttonHeight: root.sectionButtonHeight
            iconScale: root.sectionIconScale

            ToolButton {
                id: licenseButton
                anchors.bottom: parent.bottom
                width: parent.width
                height: root.licenseButtonHeight
                visible: root.licenseVisible
                hoverEnabled: true
                Accessible.name: root.licenseLabel
                ToolTip.visible: hovered || pressed
                ToolTip.text: root.licenseLabel
                ToolTip.delay: 400
                onClicked: root.licenseRequested()
                background: Rectangle {
                    color: licenseButton.down ? SharedUi.Theme.selection : "transparent"
                    Rectangle { width: parent.width; height: 1; color: SharedUi.Theme.border }
                }
                contentItem: Item {
                    Canvas {
                        id: licenseIcon
                        anchors.centerIn: parent
                        width: 28; height: 28
                        property int status: root.licenseIconState
                        onStatusChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = status === 0 ? "#91d7ae" : status === 1 ? "#f1ce6d" : "#ffb574"
                            ctx.lineWidth = 2
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.arc(14, 14, 11, 0, 2 * Math.PI)
                            ctx.stroke()
                            ctx.beginPath()
                            if (status === 0) {
                                ctx.moveTo(8, 14); ctx.lineTo(12, 18); ctx.lineTo(20, 10)
                            } else if (status === 1) {
                                ctx.moveTo(14, 7); ctx.lineTo(14, 14); ctx.lineTo(19, 17)
                            } else {
                                ctx.moveTo(14, 7); ctx.lineTo(14, 15)
                                ctx.moveTo(14, 20); ctx.lineTo(14, 20.5)
                            }
                            ctx.stroke()
                        }
                    }
                }
            }


            onSectionActivated: function(section) {
                root.currentSection = section
            }

            onSectionPressAndHold: function(section) {
                if (!DebugBuild || section !== "about")
                    return

                const appWindow = ApplicationWindow.window

                if (appWindow
                        && appWindow.viewportSimulationEnabled
                        && appWindow.cycleViewportProfile) {
                    appWindow.cycleViewportProfile()
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: root.sectionIndex(root.currentSection)

            TuningSurfacePage {
            }

            MidiSetupPage {
            }

            SettingsPage {
            }

            UserManualPage {
            }

            AboutPage {
                onOpenManualRequested:
                    root.currentSection = "manual"
            }
        }
    }
}
