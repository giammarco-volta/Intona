#include "PatchData.h"
#include <cmath>
#include <algorithm>

namespace Intona::Synth {
namespace {
QVariantList points(const Curve& curve) {
  QVariantList result;
  for (int i = 0; i < curve.count; ++i)
    result.append(QVariantMap{{"x", curve.points[i].x}, {"y", curve.points[i].y}});
  return result;
}
QVariantList values(const std::array<float, Patch::partials>& amplitudes) {
  QVariantList result;
  for (float a : amplitudes) result.append(a);
  return result;
}
std::optional<Curve> curveFrom(const QVariant& value) {
  const auto list = value.toList();
  Curve curve;
  if (list.empty() || list.size() > int(curve.points.size())) return {};
  for (const auto& entry : list) {
    const auto point = entry.toMap();
    bool xOk, yOk;
    const float x = point.value("x").toFloat(&xOk), y = point.value("y").toFloat(&yOk);
    if (!xOk || !yOk) return {};
    curve.points[curve.count++] = {x, y};
  }
  return curve;
}
bool integer(const QVariant& value, int& result) {
  bool ok;
  const double d = value.toDouble(&ok);
  if (!ok || !(d >= 0 && d <= 256) || std::trunc(d) != d) return false;
  result = int(d);
  return true;
}
bool number(const QVariantMap& data, const char* key, float& result) {
  bool ok;
  result = data.value(key).toFloat(&ok);
  return ok && std::isfinite(result);
}
bool readAmplitudes(const QVariant& value, std::array<float, Patch::partials>& result) {
  const auto list = value.toList();
  if (list.size() != Patch::partials) return false;
  for (int i = 0; i < Patch::partials; ++i) {
    bool ok; result[i] = list[i].toFloat(&ok);
    if (!ok || !(result[i] >= -1 && result[i] <= 1)) return false;
  }
  return true;
}
}

QVariantList shapeAmplitudes(int shape) {
  if (shape < int(Shape::Impulse) || shape > int(Shape::Pulse)) return {};
  Patch p; p.setShape(Shape(shape));
  return values(p.amplitudes);
}

QVariantMap shapeData(int shape, float width) {
  if (shape < int(Shape::Impulse) || shape > int(Shape::Pulse) || !(width >= .01f && width <= .99f)) return {};
  Patch p; p.pulseWidth = width; p.setShape(Shape(shape));
  return {{"amplitudes", values(p.amplitudes)}, {"phaseOffsets", values(p.phaseOffsets)}};
}

QVariantMap patchData(const Patch& p) {
  return {{"version", 3}, {"shape", int(p.shape)}, {"spectrum", int(p.spectrum)},
    {"spectralAxis", int(p.spectralAxis)}, {"edoSecondStep", p.edoSecondStep}, {"stretch", p.stretch},
    {"amplitudes", values(p.amplitudes)}, {"amplitudesB", values(p.amplitudesB)},
    {"phaseOffsets", values(p.phaseOffsets)}, {"phaseOffsetsB", values(p.phaseOffsetsB)},
    {"pulseWidth", p.pulseWidth}, {"pulseWidthB", p.pulseWidthB},
    {"spectral", points(p.spectral)}, {"shapeB", int(p.shapeB)},
    {"spectralAxisB", int(p.spectralAxisB)}, {"spectralB", points(p.spectralB)},
    {"morph", p.morph}, {"morphLfoHz", p.morphLfoHz}, {"morphMode", int(p.morphMode)},
    {"morphSeconds", p.morphSeconds}, {"temporal", points(p.temporal)}, {"releaseSeconds", p.releaseSeconds},
    {"modulationRatio", p.modulationRatio}, {"amDepth", p.amDepth}, {"fmDepth", p.fmDepth}};
}

std::optional<Patch> readPatch(const QVariantMap& data) {
  Patch p;
  int version = 0;
  if (data.contains("version") && (!integer(data.value("version"), version) || (version != 2 && version != 3))) return {};
  const bool legacy = version == 0;
  int shape = 0, spectrum, axis;
  if ((data.contains("shape") || !legacy) && !integer(data.value("shape"), shape)) return {};
  if (!integer(data.value("spectrum"), spectrum) || !integer(data.value("spectralAxis"), axis)) return {};
  p.setShape(Shape(shape)); p.spectrum = Spectrum(legacy && spectrum == 2 ? 1 : spectrum);
  p.spectralAxis = Axis(axis);
  const auto spectral = curveFrom(data.value("spectral")), temporal = curveFrom(data.value("temporal"));
  if (!spectral || !temporal) return {};
  p.spectral = *spectral; p.temporal = *temporal;
  for (const auto field : {std::pair{"stretch", &p.stretch}, {"releaseSeconds", &p.releaseSeconds},
      {"modulationRatio", &p.modulationRatio}, {"amDepth", &p.amDepth}, {"fmDepth", &p.fmDepth}})
    if (!number(data, field.first, *field.second)) return {};
  int morphFields = 0;
  for (const auto key : {"shapeB", "spectralAxisB", "spectralB", "morph", "morphLfoHz"}) morphFields += data.contains(key);
  if ((!legacy || morphFields) && morphFields != 5) return {};
  if (morphFields) {
    int shapeB, axisB;
    if (!integer(data.value("shapeB"), shapeB) || !integer(data.value("spectralAxisB"), axisB)) return {};
    p.setShape(Shape(shapeB), true); p.spectralAxisB = Axis(axisB);
    const auto curve = curveFrom(data.value("spectralB"));
    if (!curve || !number(data, "morph", p.morph) || !number(data, "morphLfoHz", p.morphLfoHz)) return {};
    p.spectralB = *curve;
  } else {
    p.setShape(p.shape, true); p.spectralAxisB = p.spectralAxis; p.spectralB = p.spectral;
  }
  if (legacy) {
    int count;
    if (!integer(data.value("count"), count) || count < 1 || count > Patch::partials) return {};
    const auto degrees = data.value("degrees").toList();
    if (degrees.size() != 16 && degrees.size() != Patch::partials) return {};
    for (int i = 0; i < degrees.size(); ++i) {
      int degree;
      if (!integer(degrees[i], degree)) return {};
      if (i == 1) p.edoSecondStep = std::max(1, degree);
    }
    for (int i = count; i < Patch::partials; ++i) p.amplitudes[i] = p.amplitudesB[i] = 0;
    if (morphFields) {
      float depth;
      if (!number(data, "morphLfoDepth", depth) || depth < 0 || depth > 1) return {};
      p.morphMode = depth > 0 ? MorphMode::Loop : MorphMode::Manual;
    } else if (data.contains("morphLfoDepth")) return {};
  } else {
    int mode;
    if (!integer(data.value("edoSecondStep"), p.edoSecondStep) ||
        !integer(data.value("morphMode"), mode) || !number(data, "morphSeconds", p.morphSeconds) ||
        !readAmplitudes(data.value("amplitudes"), p.amplitudes) ||
        !readAmplitudes(data.value("amplitudesB"), p.amplitudesB)) return {};
    p.morphMode = MorphMode(mode);
  }
  if (version == 3) {
    if (!readAmplitudes(data.value("phaseOffsets"), p.phaseOffsets) ||
        !readAmplitudes(data.value("phaseOffsetsB"), p.phaseOffsetsB) ||
        !number(data, "pulseWidth", p.pulseWidth) || !number(data, "pulseWidthB", p.pulseWidthB)) return {};
  } else {
    // Previous formats summed sine components, including the old Flat template.
    p.phaseOffsets.fill(0); p.phaseOffsetsB.fill(0);
  }
  return p.valid() ? std::optional<Patch>{p} : std::nullopt;
}

QVariantList spectrumData(const Patch& p, int edo, double fundamental) {
  QVariantList result;
  if (!p.valid() || edo < 1 || edo > 256 || !std::isfinite(fundamental) || fundamental <= 0) return result;
  for (int i = 0; i < Patch::partials; ++i) {
    const double r = p.ratio(i, edo), cents = 1200 * std::log2(r);
    const int degree = int(std::round(cents * edo / 1200));
    const float gain = p.gain(i, edo, fundamental, 48000);
    const float a = p.endpointGain(i, edo, fundamental, 48000, false);
    const float b = p.endpointGain(i, edo, fundamental, 48000, true);
    const auto ac = p.endpointComponents(i, edo, fundamental, 48000, false);
    const auto bc = p.endpointComponents(i, edo, fundamental, 48000, true);
    const float ds = bc.sine - ac.sine, dc = bc.cosine - ac.cosine;
    const float length2 = ds * ds + dc * dc;
    const float closest = length2 > 0 ? std::clamp(-(ac.sine*ds + ac.cosine*dc)/length2, 0.f, 1.f) : 0;
    const float minimum = std::hypot(ac.sine + closest*ds, ac.cosine + closest*dc);
    const float ms = (1-p.morph)*ac.sine + p.morph*bc.sine;
    const float mc = (1-p.morph)*ac.cosine + p.morph*bc.cosine;
    const bool animated = p.morphMode != MorphMode::Manual;
    result.append(QVariantMap{{"index", i + 1}, {"ratio", r}, {"hz", fundamental * r},
      {"gain", gain}, {"inverted", ms < 0 && std::abs(mc) < 1.e-7f}, {"degree", degree},
      {"phaseDegrees", gain > 0 ? std::atan2(mc, ms) * 180 / 3.141592653589793 : 0},
      {"gainA", std::abs(a)}, {"gainB", std::abs(b)}, {"minGain", animated ? minimum : std::abs(gain)},
      {"maxGain", animated ? std::max(std::abs(a), std::abs(b)) : std::abs(gain)},
      {"naturalCents", cents - 1200 * std::log2(i + 1.)}, {"gridCents", cents - degree * 1200. / edo},
      {"bandLimited", (p.coefficient(i) != 0 || p.coefficient(i, true) != 0) &&
        fundamental * r * (1 + p.fmDepth) > .45 * 48000}});
  }
  return result;
}
} // namespace Intona::Synth
