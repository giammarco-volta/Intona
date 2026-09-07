import QtQuick

import NaadaLab.Ui as SharedUi

SharedUi.AboutView {
    id: root

    signal openManualRequested()

    html: AboutHtml

    onLinkActivated: function(link) {
        if (link === "intona:user-manual") {
            root.openManualRequested()
            return
        }

        Qt.openUrlExternally(link)
    }
}
