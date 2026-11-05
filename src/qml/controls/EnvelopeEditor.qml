import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import NaadaLab.Ui as SharedUi

ColumnLayout {
    id: root
    required property var points
    required property real minimumSpan
    property bool timeAxis: false
    property int axisKind: timeAxis ? 3 : 0
    readonly property string axisLabel: [qsTr("Partial index"), qsTr("Frequency ratio"), qsTr("Frequency (Hz)"), qsTr("Time (s)")][axisKind]
    function formatX(value) {
        const number = axisKind === 0 ? Number(value.toFixed(2)) : Number(value.toPrecision(4))
        return number + (axisKind === 1 ? "×" : axisKind === 2 ? " Hz" : axisKind === 3 ? " s" : "")
    }
    property int selected: 0
    readonly property int index: Math.min(selected, points.length - 1)
    readonly property real span: Math.max(minimumSpan, points[points.length - 1].x * 1.1)
    property real dragSpan: 0
    signal edited(var value)
    Layout.fillWidth: true

    function updatePoint(x, y) {
        const list = Array.from(points)
        const i = index
        const epsilon = Math.max(0.000001, Math.abs(x) * 0.000001)
        const low = i > 0 ? list[i - 1].x + epsilon : 0
        const high = i + 1 < list.length ? list[i + 1].x - epsilon : Infinity
        list[i] = {x: timeAxis && i === 0 ? 0 : Math.max(low, Math.min(high, x)),
                   y: Math.max(0, Math.min(1, y))}
        edited(list)
    }
    onPointsChanged: graph.requestPaint()
    onIndexChanged: graph.requestPaint()
    onSpanChanged: graph.requestPaint()
    onAxisKindChanged: graph.requestPaint()
    Label { text: root.axisLabel; color: SharedUi.Theme.secondaryText }
    Canvas {
        id: graph
        objectName: "envelopeGrid"
        Layout.fillWidth: true; Layout.preferredHeight: 170
        readonly property real padLeft: 40
        readonly property real padRight: 24
        readonly property real padTop: 12
        readonly property real padBottom: 32
        function px(p) { return padLeft + p.x / (root.dragSpan || root.span) * (width - padLeft - padRight) }
        function py(p) { return padTop + (1 - p.y) * (height - padTop - padBottom) }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = SharedUi.Theme.background
            ctx.fillRect(0, 0, width, height)
            ctx.font = "10px sans-serif"
            const span = root.dragSpan || root.span
            const rawStep = span / Math.max(2, Math.floor((width - padLeft - padRight) / 85))
            const power = Math.pow(10, Math.floor(Math.log10(rawStep)))
            const relative = rawStep / power
            let step = (relative <= 1 ? 1 : relative <= 2 ? 2 : relative <= 5 ? 5 : 10) * power
            if (root.axisKind === 0) step = Math.max(1, Math.ceil(step))
            ctx.lineWidth = 1
            ctx.strokeStyle = SharedUi.Theme.secondaryText
            ctx.fillStyle = SharedUi.Theme.secondaryText
            for (let level = 0; level <= 1; level += 0.25) {
                const y = py({y: level})
                ctx.globalAlpha = 0.2
                ctx.beginPath(); ctx.moveTo(padLeft, y); ctx.lineTo(width - padRight, y); ctx.stroke()
                ctx.globalAlpha = 0.85; ctx.fillText(Math.round(level * 100) + "%", 0, y + 3)
            }
            for (let x = 0; x <= span + step * 0.001; x += step) {
                const pixel = px({x: x})
                ctx.globalAlpha = 0.2
                ctx.beginPath(); ctx.moveTo(pixel, padTop); ctx.lineTo(pixel, height - padBottom); ctx.stroke()
                ctx.globalAlpha = 0.85
                const label = root.formatX(x), labelWidth = ctx.measureText(label).width
                ctx.fillText(label, Math.max(0, Math.min(width - labelWidth, pixel - labelWidth / 2)), height - 8)
            }
            ctx.globalAlpha = 1
            ctx.strokeStyle = SharedUi.Theme.accent
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.moveTo(padLeft, py(root.points[0]))
            for (const p of root.points) ctx.lineTo(px(p), py(p))
            ctx.lineTo(width - padRight, py(root.points[root.points.length - 1]))
            ctx.stroke()
            for (let i = 0; i < root.points.length; ++i) {
                ctx.fillStyle = i === root.index ? SharedUi.Theme.accent : SharedUi.Theme.text
                ctx.beginPath()
                ctx.arc(px(root.points[i]), py(root.points[i]), i === root.index ? 6 : 4, 0, 2*Math.PI)
                ctx.fill()
            }
        }
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onPressed: event => {
                let nearest = 0, distance = Infinity
                for (let i = 0; i < root.points.length; ++i) {
                    const d = Math.hypot(graph.px(root.points[i]) - event.x, graph.py(root.points[i]) - event.y)
                    if (d < distance) { nearest = i; distance = d }
                }
                root.selected = nearest
                root.dragSpan = root.span
            }
            onPositionChanged: event => {
                if (pressed) root.updatePoint(root.dragSpan * (event.x - graph.padLeft) / (width - graph.padLeft - graph.padRight),
                                              1 - (event.y - graph.padTop) / (height - graph.padTop - graph.padBottom))
            }
            onReleased: { root.dragSpan = 0; graph.requestPaint() }
            onCanceled: { root.dragSpan = 0; graph.requestPaint() }
        }
    }
    Label {
        color: SharedUi.Theme.secondaryText
        Layout.fillWidth: true; wrapMode: Text.WordWrap
        text: qsTr("Point %1 · %2 · level %3%").arg(root.index + 1)
            .arg(root.formatX(root.points[root.index].x)).arg((root.points[root.index].y * 100).toFixed(1))
    }
    RowLayout {
        Button {
            text: qsTr("Add point"); enabled: root.points.length < 16
            onClicked: {
                const list = Array.from(root.points)
                const i = root.index, a = list[i], b = list[i + 1]
                list.splice(i + 1, 0, {x: b ? (a.x + b.x) / 2 : a.x + root.minimumSpan / 4,
                                       y: b ? (a.y + b.y) / 2 : a.y})
                root.edited(list)
                root.selected = Math.min(i + 1, root.points.length - 1)
            }
        }
        Button {
            text: qsTr("Remove"); enabled: root.points.length > 1 && !(root.timeAxis && root.index === 0)
            onClicked: {
                const list = Array.from(root.points)
                list.splice(root.index, 1)
                root.selected = Math.max(0, root.index - 1)
                root.edited(list)
            }
        }
    }
}
