import QtQuick
import QtQuick.Layouts

import Intona
import NaadaLab.Ui as SharedUi

Item {
    id: root

    property string currentSection: "surface"

    readonly property var sections: [
        {
            section: "surface",
            name: qsTr("Tuning Surface"),
            icon: "curves"
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
            Layout.preferredWidth: 64
            Layout.minimumWidth: 64
            Layout.maximumWidth: 64
            Layout.fillHeight: true

            sections: root.sections
            currentSection: root.currentSection
            buttonHeight: 64

            onSectionActivated: function(section) {
                root.currentSection = section
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: root.sectionIndex(root.currentSection)

            PlaceholderPage {
                pageTitle: qsTr("Tuning Surface")
            }

            PlaceholderPage {
                pageTitle: qsTr("MIDI Setup")
            }

            PlaceholderPage {
                pageTitle: qsTr("Manual")
            }

            PlaceholderPage {
                pageTitle: qsTr("About")
            }
        }
    }
}