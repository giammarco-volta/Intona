#pragma once
#include "AdditiveSynth.h"
#include <QVariantMap>
#include <optional>

namespace Intona::Synth {
// Full value snapshot for a QML editor. A failed edit preserves the last good patch.
QVariantMap patchData(const Patch& patch);
QVariantList shapeAmplitudes(int shape);
QVariantMap shapeData(int shape, float pulseWidth);
std::optional<Patch> readPatch(const QVariantMap& data);
// Target carriers at the engine's 48 kHz output rate; excludes modulation sidebands.
QVariantList spectrumData(const Patch& patch, int edo, double fundamental);
}
