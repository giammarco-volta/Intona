import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import NaadaLab.Ui as SharedUi

ColumnLayout {
    id: root
    required property var partials
    required property int edo
    property int selected: 0
    readonly property var current: partials.length ? partials[Math.min(selected, partials.length - 1)] : null
    readonly property real octaves: Math.max(1, Math.ceil(Math.log2(Math.max(partials.length,
        ...partials.map(p => p.ratio)))))
    Layout.fillWidth: true
    onPartialsChanged: { selected = Math.min(selected, Math.max(0, partials.length - 1)); graph.requestPaint() }
    onSelectedChanged: graph.requestPaint()
    onEdoChanged: graph.requestPaint()
    onOctavesChanged: graph.requestPaint()

    Label {
        Layout.fillWidth: true; wrapMode: Text.WordWrap
        text: qsTr("Morph preview • blue: A • violet: B • shaded: movement range • gray: natural harmonics • grid: EDO")
        color: SharedUi.Theme.secondaryText
    }
    Canvas {
        id: graph
        objectName: "synthSpectrumGraph"
        Layout.fillWidth: true; Layout.preferredHeight: width < 600 ? 210 : 240
        readonly property real plotLeft: 38
        readonly property real plotRight: width - 18
        readonly property real plotBottom: height - 28
        function px(ratio) { return plotLeft + Math.log2(ratio) / root.octaves * (plotRight - plotLeft) }
        function py(gain) { return 12 + (1 - Math.max(0, 1 + 20 * Math.log10(Math.max(0.001, gain)) / 60)) * (plotBottom - 12) }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = SharedUi.Theme.background
            ctx.fillRect(0, 0, width, height)
            ctx.font = "11px sans-serif"
            // Thin dense EDO grids to a readable spacing, keeping octave marks exact.
            const stride = Math.max(1, Math.ceil(root.edo * root.octaves * 5 / Math.max(1, plotRight - plotLeft)))
            ctx.strokeStyle = SharedUi.Theme.secondaryText
            ctx.globalAlpha = 0.12
            ctx.lineWidth = 1
            for (let d = 0; d <= root.edo * root.octaves; d += stride) {
                const x = px(Math.pow(2, d / root.edo))
                ctx.beginPath(); ctx.moveTo(x, 12); ctx.lineTo(x, plotBottom); ctx.stroke()
            }
            ctx.globalAlpha = 0.4
            for (let db = 0; db >= -60; db -= 20) {
                const y = py(Math.pow(10, db / 20))
                ctx.beginPath(); ctx.moveTo(plotLeft, y); ctx.lineTo(plotRight, y); ctx.stroke()
                ctx.fillStyle = SharedUi.Theme.secondaryText
                ctx.fillText(String(db), 2, y + 4)
            }
            const octaveStride = Math.max(1, Math.ceil(root.octaves * 50 / Math.max(1, plotRight - plotLeft)))
            for (let o = 0; o <= root.octaves; o += octaveStride) {
                const x = px(Math.pow(2, o))
                ctx.beginPath(); ctx.moveTo(x, 12); ctx.lineTo(x, plotBottom); ctx.stroke()
                ctx.fillText((o < 10 ? Math.pow(2, o) : "2^" + o) + "×", x - 8, height - 6)
            }
            for (let i = 0; i < root.partials.length; ++i) {
                const p = root.partials[i]
                ctx.globalAlpha = 0.35
                ctx.strokeStyle = SharedUi.Theme.secondaryText
                ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(px(i + 1), plotBottom); ctx.lineTo(px(i + 1), py(p.gain)); ctx.stroke()
                const x = px(p.ratio), y = py(p.gain)
                ctx.globalAlpha = 0.2
                ctx.strokeStyle = SharedUi.Theme.accent
                ctx.lineWidth = 7
                ctx.beginPath(); ctx.moveTo(x, py(p.minGain)); ctx.lineTo(x, py(p.maxGain)); ctx.stroke()
                ctx.globalAlpha = 0.8
                ctx.lineWidth = 2
                ctx.strokeStyle = "#63b3d5"
                ctx.beginPath(); ctx.moveTo(x - 5, py(p.gainA)); ctx.lineTo(x, py(p.gainA)); ctx.stroke()
                ctx.strokeStyle = "#b49ae8"
                ctx.beginPath(); ctx.moveTo(x, py(p.gainB)); ctx.lineTo(x + 5, py(p.gainB)); ctx.stroke()
                ctx.globalAlpha = i === root.selected ? 1 : 0.65
                ctx.strokeStyle = SharedUi.Theme.accent
                ctx.lineWidth = i === root.selected ? 3 : 2
                ctx.beginPath(); ctx.moveTo(x, plotBottom); ctx.lineTo(x, y); ctx.stroke()
                ctx.fillStyle = ctx.strokeStyle
                ctx.beginPath(); ctx.arc(x, y, i === root.selected ? 5 : 3, 0, 2 * Math.PI); ctx.fill()
            }
            ctx.globalAlpha = 1
        }
        MouseArea {
            anchors.fill: parent
            onClicked: mouse => {
                let distance = Infinity, chosen = 0
                for (let i = 0; i < root.partials.length; ++i) {
                    const p = root.partials[i]
                    const d = Math.hypot(mouse.x - graph.px(p.ratio), mouse.y - graph.py(p.gain))
                    if (d < distance) { distance = d; chosen = i }
                }
                root.selected = chosen
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Partial"); color: SharedUi.Theme.text }
        SpinBox {
            objectName: "synthPartialSelector"
            from: 1; to: Math.max(1, root.partials.length); value: root.selected + 1
            Accessible.name: qsTr("Selected partial")
            onValueModified: root.selected = value - 1
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: root.current ? Number(root.current.hz).toFixed(2) + " Hz  ·  "
                + Number(root.current.ratio).toFixed(4) + "×  ·  "
                + qsTr("Level %1%").arg((root.current.gain * 100).toFixed(2)) : ""
            color: SharedUi.Theme.text
        }
    }
    Label {
        objectName: "synthPartialDetails"
        Layout.fillWidth: true; wrapMode: Text.WordWrap
        color: SharedUi.Theme.secondaryText
        text: root.current ? qsTr("Nearest EDO step: %1 · Δ grid: %2 cents · Δ natural: %3 cents")
            .arg(root.current.degree).arg(Number(root.current.gridCents).toFixed(2))
            .arg(Number(root.current.naturalCents).toFixed(2))
            + (root.current.inverted ? qsTr(" · Inverted phase") : "")
            + qsTr(" · Phase: %1°").arg(Number(root.current.phaseDegrees).toFixed(1))
            + qsTr(" · A: %1% · B: %2% · Range: %3–%4%")
                .arg((root.current.gainA * 100).toFixed(2)).arg((root.current.gainB * 100).toFixed(2))
                .arg((root.current.minGain * 100).toFixed(2)).arg((root.current.maxGain * 100).toFixed(2))
            + (root.current.bandLimited ? qsTr(" · Attenuated at audio band limit") : "") : ""
    }
    Label {
        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: SharedUi.Theme.secondaryText
        text: qsTr("Relative level in dB at the preview position, with the movement range shaded. Includes partial amplitudes, spectral envelopes and band limiting at 48 kHz.")
    }
}
