import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import NaadaLab.Ui as SharedUi
import "../controls"

Rectangle {
    id: root
    color: SharedUi.Theme.background
    readonly property var patch: TuningController.synthPatch
    property bool editingB: false
    readonly property var endpointAmplitudes: patch[endpointField("amplitudes")]
    readonly property var shapeTemplate: TuningController.synthShapeData(patch[endpointField("shape")], patch[endpointField("pulseWidth")])
    readonly property var templateAmplitudes: shapeTemplate.amplitudes
    readonly property bool modifiedSpectrum: endpointAmplitudes.some((a, i) => Math.abs(a - templateAmplitudes[i]) > 0.000001)
        || patch[endpointField("phaseOffsets")].some((p, i) => Math.abs(p - shapeTemplate.phaseOffsets[i]) > 0.000001)
    function selectShape(index, width) {
        if (width === undefined) width = patch[endpointField("pulseWidth")]
        const template = TuningController.synthShapeData(index, width)
        const next = Object.assign({}, patch)
        next[endpointField("shape")] = index
        next[endpointField("amplitudes")] = template.amplitudes
        next[endpointField("phaseOffsets")] = template.phaseOffsets
        next[endpointField("pulseWidth")] = width
        return TuningController.setSynthPatch(next)
    }
    function editAmplitude(value) {
        const values = Array.from(endpointAmplitudes), i = spectrumView.selected
        const sign = values[i] < 0 || (values[i] === 0 && templateAmplitudes[i] < 0) ? -1 : 1
        values[i] = sign * Math.max(0, Math.min(1, value))
        return edit(endpointField("amplitudes"), values)
    }
    function endpointField(field) { return field + (editingB ? "B" : "") }
    function convertSpectralAxis(axis) {
        const axisField = endpointField("spectralAxis"), curveField = endpointField("spectral")
        const oldAxis = patch[axisField], partials = TuningController.spectrumPreview
        if (oldAxis === axis) return true
        const fundamental = partials[0].hz
        function ratioAt(index) {
            if (index <= 1) return index
            const i = Math.min(46, Math.floor(index) - 1)
            return partials[i].ratio + (index - i - 1) * (partials[i + 1].ratio - partials[i].ratio)
        }
        function indexAt(ratio) {
            if (ratio <= 1) return ratio
            for (let i = 1; i < 48; ++i) {
                const low = partials[i - 1].ratio, high = partials[i].ratio
                if (ratio <= high && high > low) return i + (ratio - low) / (high - low)
            }
            const step = Math.max(0.000001, partials[47].ratio - partials[46].ratio)
            return 48 + (ratio - partials[47].ratio) / step
        }
        const points = []
        for (const point of patch[curveField]) {
            const ratio = oldAxis === 0 ? ratioAt(point.x) : oldAxis === 2 ? point.x / fundamental : point.x
            const x = Math.max(0, Math.min(1e30, axis === 0 ? indexAt(ratio) : axis === 2 ? ratio * fundamental : ratio))
            // Coincident EDO carriers share a single envelope frequency.
            if (points.length && x <= points[points.length - 1].x + Math.max(0.000001, x * 0.000001))
                points[points.length - 1].y = point.y
            else points.push({x: x, y: point.y})
        }
        const next = Object.assign({}, patch)
        next[axisField] = axis; next[curveField] = points
        return TuningController.setSynthPatch(next)
    }
    function copySpectrum() {
        const next = Object.assign({}, patch)
        for (const field of ["shape", "amplitudes", "phaseOffsets", "pulseWidth", "spectralAxis", "spectral"])
            next[field + (editingB ? "" : "B")] = patch[endpointField(field)]
        return TuningController.setSynthPatch(next)
    }

    function edit(field, value) {
        const next = Object.assign({}, patch)
        next[field] = value
        return TuningController.setSynthPatch(next)
    }

    component Parameter: ColumnLayout {
        id: control
        required property string caption
        required property string field
        required property real low
        required property real high
        required property real step
        property int decimals: 2
        property real displayScale: 1
        property string suffix: ""
        property bool precise: false
        Layout.fillWidth: true
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Label { text: control.caption; Layout.fillWidth: true; color: SharedUi.Theme.text }
            Label { text: Number(root.patch[control.field] * control.displayScale).toFixed(control.decimals) + control.suffix; color: SharedUi.Theme.secondaryText }
            SpinBox {
                visible: control.precise
                objectName: control.field + "Value"
                readonly property real factor: Math.pow(10, control.decimals)
                from: Math.round(control.low * factor); to: Math.round(control.high * factor)
                stepSize: Math.max(1, Math.round(control.step * factor)); editable: true
                value: Math.round(root.patch[control.field] * factor)
                textFromValue: (value, locale) => Number(value / factor).toLocaleString(locale, 'f', control.decimals)
                valueFromText: (text, locale) => Math.round(Number.fromLocaleString(locale, text) * factor)
                onValueModified: root.edit(control.field, value / factor)
            }
        }
        Slider {
            objectName: control.field + "Slider"
            Layout.fillWidth: true
            from: control.low; to: control.high; stepSize: control.step
            value: root.patch[control.field]
            Accessible.name: control.caption
            onMoved: root.edit(control.field, value)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: width < 700 ? 10 : SharedUi.Theme.pageMargin
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Synthesizer")
                font.bold: true; font.pixelSize: 22
                Layout.fillWidth: true; color: SharedUi.Theme.text
            }
            CheckBox {
                objectName: "internalAudioSelector"
                text: root.width < 600 ? qsTr("Internal synth") : qsTr("Play internal additive synth")
                checked: TuningController.internalAudio
                onToggled: TuningController.internalAudio = checked
            }
        }
        Label {
            visible: TuningController.audioError.length > 0
            text: TuningController.audioError
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: SharedUi.Theme.text
        }
        Label {
            objectName: "synthTemperamentStatus"
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            color: SharedUi.Theme.accent
            readonly property int centerKey: TuningController.keyValues.indexOf(TuningController.tuningCenter)
            text: qsTr("%1 EDO · Center: %2 · Adaptation: %3 · Tuning preset: %4")
                .arg(TuningController.edo)
                .arg(centerKey >= 0 ? TuningController.keyNames[centerKey] : "—")
                .arg(TuningController.adaptingEnabled ? qsTr("On") : qsTr("Off"))
                .arg(TuningController.currentPresetIndex >= 0 ? String(TuningController.currentPresetIndex + 1) : qsTr("Custom"))
        }
    ScrollView {
        id: scroll
        objectName: "synthEditorScroll"
        Layout.fillWidth: true; Layout.fillHeight: true
        contentWidth: availableWidth
        ColumnLayout {
            width: scroll.availableWidth
            spacing: SharedUi.Theme.pageSpacing
            SharedUi.FormSection {
                title: qsTr("Spectrum")
                Layout.fillWidth: true
                GridLayout {
                    Layout.fillWidth: true
                    columns: scroll.availableWidth >= 740 ? 3 : 1
                    ComboBox {
                        objectName: "synthEndpointSelector"
                        Layout.fillWidth: true
                        model: [qsTr("Edit spectrum A"), qsTr("Edit spectrum B")]
                        Accessible.name: qsTr("Spectrum to edit")
                        currentIndex: root.editingB ? 1 : 0
                        onActivated: index => root.editingB = index === 1
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        objectName: "synthSpectrumSelector"
                        Accessible.name: qsTr("Partial spectrum")
                        model: [qsTr("Natural Harmonic"), qsTr("EDO Quantized"), qsTr("EDO Specific")]
                        currentIndex: [0, 1, 3].indexOf(root.patch.spectrum)
                        onActivated: index => root.edit("spectrum", [0, 1, 3][index])
                    }
                    ComboBox {
                        objectName: "synthShapeSelector"
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Starting spectrum")
                        model: [qsTr("Impulse"), qsTr("Sine"), qsTr("Triangle"), qsTr("Square"), qsTr("Sawtooth"),
                            qsTr("Saw / Square"), qsTr("Saw / Triangle"), qsTr("Square / Triangle"), qsTr("Organ"), qsTr("Pulse")]
                        currentIndex: root.patch[root.endpointField("shape")]
                        onActivated: index => root.selectShape(index)
                    }
                }
                ColumnLayout {
                    visible: root.patch[root.endpointField("shape")] === 9
                    Layout.fillWidth: true
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; color: SharedUi.Theme.text; text: qsTr("Pulse width · high portion (%)") }
                        SpinBox {
                            objectName: "synthPulseWidthValue"
                            from: 10; to: 990; stepSize: 1; editable: true
                            value: Math.round(root.patch[root.endpointField("pulseWidth")] * 1000)
                            textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, 'f', 1)
                            valueFromText: (text, locale) => Math.round(Number.fromLocaleString(locale, text) * 10)
                            onValueModified: root.selectShape(9, value / 1000)
                        }
                    }
                    Slider {
                        objectName: "synthPulseWidthSlider"
                        Layout.fillWidth: true; from: 0.01; to: 0.99; stepSize: 0.001
                        value: root.patch[root.endpointField("pulseWidth")]
                        Accessible.name: qsTr("Pulse width")
                        onMoved: root.selectShape(9, value)
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: SharedUi.Theme.secondaryText
                        text: qsTr("50% = square. Changing width reloads Pulse amplitudes and phases; the envelope remains.")
                    }
                }
                Label {
                    objectName: "synthSpectrumModified"
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: SharedUi.Theme.secondaryText
                    text: qsTr("48 partials · %1")
                        .arg(root.modifiedSpectrum ? qsTr("Custom spectrum") : qsTr("Starting spectrum"))
                }
                Parameter { caption: root.patch.morphMode === 0 ? qsTr("Morph A ↔ B") : qsTr("Spectrum preview A ↔ B"); field: "morph"; low: 0; high: 1; step: 0.001; displayScale: 100; decimals: 1; suffix: "% B" }
                SpectrumView {
                    id: spectrumView
                    objectName: "synthSpectrumView"
                    partials: TuningController.spectrumPreview
                    edo: TuningController.edo
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: SharedUi.Theme.text
                        text: qsTr("Partial %1 · amplitude %2 before envelope (%)").arg(spectrumView.selected + 1).arg(root.editingB ? "B" : "A")
                    }
                    SpinBox {
                        objectName: "synthPartialAmplitudeValue"
                        from: 0; to: 10000; editable: true
                        value: Math.round(Math.abs(root.endpointAmplitudes[spectrumView.selected]) * 10000)
                        textFromValue: (value, locale) => Number(value / 100).toLocaleString(locale, 'f', 2)
                        valueFromText: (text, locale) => Math.round(Number.fromLocaleString(locale, text) * 100)
                        onValueModified: root.editAmplitude(value / 10000)
                    }
                }
                Slider {
                    objectName: "synthPartialAmplitude"
                    Layout.fillWidth: true; from: 0; to: 1; stepSize: 0.001
                    value: Math.abs(root.endpointAmplitudes[spectrumView.selected])
                    Accessible.name: qsTr("Selected partial amplitude")
                    onMoved: root.editAmplitude(value)
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Reference MIDI note"); color: SharedUi.Theme.text }
                    SpinBox {
                        objectName: "synthReferenceNote"
                        from: 0; to: 127; value: TuningController.spectrumNote
                        Accessible.name: qsTr("Reference MIDI note")
                        onValueModified: TuningController.spectrumNote = value
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: TuningController.keyNames[TuningController.spectrumNote % 12]
                            + " · " + qsTr("Follows current tuning")
                        color: SharedUi.Theme.secondaryText
                    }
                }
            }
            SharedUi.FormSection {
                title: qsTr("Movement")
                Layout.fillWidth: true
                ComboBox {
                    objectName: "synthMorphModeSelector"
                    Layout.fillWidth: true
                    model: [qsTr("Manual"), qsTr("A → B · one shot"), qsTr("A ↔ B · loop")]
                    currentIndex: root.patch.morphMode
                    onActivated: index => root.edit("morphMode", index)
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: scroll.availableWidth >= 600 ? 2 : 1
                    columnSpacing: SharedUi.Theme.pageSpacing
                    Parameter { visible: root.patch.morphMode === 2; caption: qsTr("Loop speed (Hz)"); field: "morphLfoHz"; low: 0.01; high: 5; step: 0.01 }
                    Parameter { visible: root.patch.morphMode === 1; caption: qsTr("A → B duration (seconds)"); field: "morphSeconds"; low: 0.001; high: 30; step: 0.001; decimals: 3; precise: true }
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: root.patch.morphMode === 0 ? qsTr("The morph slider sets the audible mix.")
                        : root.patch.morphMode === 1 ? qsTr("Each note starts at A, reaches B and stays there.")
                        : qsTr("Each note starts at A · its own A–B–A cycle lasts %1 s.").arg((1 / root.patch.morphLfoHz).toFixed(2))
                    color: SharedUi.Theme.secondaryText
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: scroll.availableWidth >= 740 ? 2 : 1
                columnSpacing: SharedUi.Theme.pageSpacing
                rowSpacing: SharedUi.Theme.pageSpacing
                SharedUi.FormSection {
                    title: qsTr("Partial spectrum")
                    Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.alignment: Qt.AlignTop
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: qsTr("48 editable partial amplitudes × spectral envelope. Stretch is applied after EDO distribution.")
                        color: SharedUi.Theme.secondaryText
                    }
                    Parameter { caption: qsTr("Spectral stretch"); field: "stretch"; low: 0.5; high: 2; step: 0.01; precise: true }
                    RowLayout {
                        visible: root.patch.spectrum === 3
                        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Partial 2 · EDO steps (scales all partials)"); color: SharedUi.Theme.text }
                        SpinBox {
                            objectName: "synthEdoSecondStep"
                            from: 1; to: 256; value: root.patch.edoSecondStep; editable: true
                            onValueModified: root.edit("edoSecondStep", value)
                        }
                    }
                }
                SharedUi.FormSection {
                    title: qsTr("Voice modulation")
                    Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.alignment: Qt.AlignTop
                    Parameter { caption: qsTr("Modulator / fundamental"); field: "modulationRatio"; low: 0; high: 16; step: 0.01 }
                    Parameter { caption: qsTr("AM depth"); field: "amDepth"; low: 0; high: 1; step: 0.01 }
                    Parameter { caption: qsTr("FM depth"); field: "fmDepth"; low: 0; high: 1; step: 0.01 }
                }
                SharedUi.FormSection {
                    title: root.editingB ? qsTr("Spectral envelope B") : qsTr("Spectral envelope A")
                    Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.alignment: Qt.AlignTop
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            objectName: "synthCopySpectrum"
                            text: root.editingB ? qsTr("Copy B → A") : qsTr("Copy A → B")
                            onClicked: root.copySpectrum()
                        }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: qsTr("Copy amplitudes and envelope. A and B share tuning.")
                            color: SharedUi.Theme.secondaryText
                        }
                    }
                    RowLayout {
                        Button {
                            text: qsTr("Flat envelope")
                            onClicked: root.edit(root.endpointField("spectral"), [{x: 0, y: 1}])
                        }
                        Button {
                            text: root.editingB ? qsTr("Edit A") : qsTr("Edit B")
                            onClicked: root.editingB = !root.editingB
                        }
                    }
                    ComboBox {
                        objectName: "synthSpectralAxisSelector"
                        Layout.fillWidth: true; Layout.maximumWidth: 360
                        model: [qsTr("Partial index"), qsTr("Frequency ratio"), qsTr("Absolute frequency (Hz)")]
                        currentIndex: root.patch[root.endpointField("spectralAxis")]
                        Accessible.name: qsTr("Spectral envelope units")
                        onActivated: index => root.convertSpectralAxis(index)
                    }
                    EnvelopeEditor {
                        objectName: "synthSpectralEnvelopeEditor"
                        points: root.patch[root.endpointField("spectral")]
                        axisKind: root.patch[root.endpointField("spectralAxis")]
                        minimumSpan: axisKind === 2 ? Math.min(24000, TuningController.spectrumPreview[47].hz) : axisKind === 1 ? TuningController.spectrumPreview[47].ratio : 48
                        onEdited: points => root.edit(root.endpointField("spectral"), points)
                    }
                }
                SharedUi.FormSection {
                    title: qsTr("Amplitude envelope")
                    Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.alignment: Qt.AlignTop
                    Label { text: qsTr("Time since attack (seconds)"); color: SharedUi.Theme.secondaryText }
                    EnvelopeEditor {
                        objectName: "timeEnvelopeEditor"
                        points: root.patch.temporal; timeAxis: true; axisKind: 3; minimumSpan: 0.25
                        onEdited: points => root.edit("temporal", points)
                    }
                    Parameter { caption: qsTr("Release (seconds)"); field: "releaseSeconds"; low: 0.001; high: 3; step: 0.001; decimals: 3 }
                }
            }
        }
    }
    }
}
