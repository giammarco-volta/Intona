import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Intona
import NaadaLab.Ui as SharedUi

Item {
    id: root

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
                 height / Math.max(1, sections.length))

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
        case "manual":
            return 2
        case "about":
            return 3
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

            UserManualPage {
            }

            AboutPage {
                onOpenManualRequested:
                    root.currentSection = "manual"
            }
        }
    }
}
