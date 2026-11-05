#include "AdditiveSynth.h"
#include <algorithm>
#include <cmath>

namespace Intona::Synth {
namespace {
bool between(float x, float lo, float hi) { return x >= lo && x <= hi; }

float advance(float phase, float step) noexcept {
  phase += step;
  return phase >= 1 ? phase - 1 : phase;
}

void smoothControl(float& value, float target, float slew) noexcept {
  const float previous = value;
  value += slew * (target - value);
  // Reach exact endpoints once float precision is exhausted; avoid denormal
  // tails when movement is disabled.
  if (value == previous || std::abs(target - value) < 1.e-8f) value = target;
}
}

bool Curve::valid() const {
  if (count < 1 || count > int(points.size())) return false;
  for (int i = 0; i < count; ++i) {
    const auto p = points[i];
    if (!std::isfinite(p.x) || p.x < 0 || !between(p.y, 0, 1) ||
        (i && p.x <= points[i-1].x)) return false;
  }
  return true;
}

float Curve::at(float x) const noexcept {
  if (x <= points[0].x) return points[0].y;
  for (int i = 1; i < count; ++i) {
    const auto a = points[i-1], b = points[i];
    if (x <= b.x) return a.y + (b.y-a.y) * ((x-a.x)/(b.x-a.x));
  }
  return points[count-1].y;
}

Patch::Patch() {
  setShape(Shape::Impulse); setShape(Shape::Impulse, true);
}

bool Patch::valid() const {
  return between(stretch, .25f, 4) && between(releaseSeconds, .001f, 30) &&
    between(modulationRatio, 0, 16) && between(amDepth, 0, 1) && between(fmDepth, 0, 1) &&
    spectral.valid() && spectralB.valid() && temporal.valid() && temporal.points[0].x == 0 &&
    between(morph, 0, 1) && between(morphLfoHz, .01f, 5) && between(morphSeconds, .001f, 30) &&
    morphMode >= MorphMode::Manual && morphMode <= MorphMode::Loop &&
    edoSecondStep >= 1 && edoSecondStep <= 256 &&
    std::all_of(amplitudes.begin(), amplitudes.end(), [](float a) { return between(a, -1, 1); }) &&
    std::all_of(amplitudesB.begin(), amplitudesB.end(), [](float a) { return between(a, -1, 1); }) &&
    std::all_of(phaseOffsets.begin(), phaseOffsets.end(), [](float a) { return between(a, 0, 1); }) &&
    std::all_of(phaseOffsetsB.begin(), phaseOffsetsB.end(), [](float a) { return between(a, 0, 1); }) &&
    between(pulseWidth, .01f, .99f) && between(pulseWidthB, .01f, .99f) &&
    (spectrum == Spectrum::NaturalHarmonic || spectrum == Spectrum::EdoQuantized || spectrum == Spectrum::EdoSpecific) &&
    spectralAxis >= Axis::PartialIndex && spectralAxis <= Axis::AbsoluteHz &&
    shape >= Shape::Impulse && shape <= Shape::Pulse &&
    spectralAxisB >= Axis::PartialIndex && spectralAxisB <= Axis::AbsoluteHz &&
    shapeB >= Shape::Impulse && shapeB <= Shape::Pulse;
}

float Patch::coefficient(int index, bool endpointB) const noexcept {
  return (endpointB ? amplitudesB : amplitudes)[index];
}

void Patch::setShape(Shape value, bool endpointB) {
  (endpointB ? shapeB : shape) = value;
  auto& values = endpointB ? amplitudesB : amplitudes;
  auto& phases = endpointB ? phaseOffsetsB : phaseOffsets;
  const float width = endpointB ? pulseWidthB : pulseWidth;
  for (int i = 0; i < partials; ++i) {
    values[i] = shapeCoefficient(value, i, width);
    const double phase = value == Shape::Impulse ? .25 : value == Shape::Pulse ? .25 - .5 * (i+1) * width : 0;
    phases[i] = float(phase - std::floor(phase));
  }
}

float Patch::shapeCoefficient(Shape shape, int index, float width) noexcept {
  const int n = index + 1;
  switch (shape) {
  case Shape::Sine: return index == 0 ? 1.f : 0.f;
  case Shape::Triangle: return n % 2 ? (n % 4 == 1 ? 1.f : -1.f) / (n * n) : 0.f;
  case Shape::Square: return n % 2 ? 1.f / n : 0.f;
  case Shape::Saw: return 1.f / n;
  case Shape::Pulse: {
    // Zero-mean rectangular pulse. At 50% this exactly matches Square;
    // keep its high/low difference constant rather than boosting narrow pulses.
    const double coefficient = std::sin(3.141592653589793 * n * width) / n;
    return std::abs(coefficient) < 1.e-7 ? 0 : float(coefficient);
  }
  case Shape::SawSquare: return .5f * (shapeCoefficient(Shape::Saw, index) + shapeCoefficient(Shape::Square, index));
  case Shape::SawTriangle: return .5f * (shapeCoefficient(Shape::Saw, index) + shapeCoefficient(Shape::Triangle, index));
  case Shape::SquareTriangle: return .5f * (shapeCoefficient(Shape::Square, index) + shapeCoefficient(Shape::Triangle, index));
  case Shape::Organ: {
    constexpr float stops[] = {1, .5f, .7f, .25f, .2f, .15f, 0, .1f};
    return index < 8 ? stops[index] : 0;
  }
  default: return 1.f;
  }
}

float Patch::endpointGain(int index, int edo, double fundamental, int sampleRate, bool endpointB) const {
  const double r = ratio(index, edo);
  const auto axis = endpointB ? spectralAxisB : spectralAxis;
  const auto& curve = endpointB ? spectralB : spectral;
  const float x = axis == Axis::AbsoluteHz ? float(fundamental * r) :
    axis == Axis::RelativeFrequency ? float(r) : float(index + 1);
  const double step = fundamental * r / sampleRate;
  const float band = std::clamp(float((.49 - step * (1 + fmDepth)) / .04), 0.f, 1.f);
  return coefficient(index, endpointB) * curve.at(x) * band;
}

float Patch::gain(int index, int edo, double fundamental, int sampleRate) const {
  const auto a = endpointComponents(index, edo, fundamental, sampleRate, false);
  const auto b = endpointComponents(index, edo, fundamental, sampleRate, true);
  return std::hypot((1 - morph) * a.sine + morph * b.sine, (1 - morph) * a.cosine + morph * b.cosine);
}

Harmonic Patch::endpointComponents(int index, int edo, double fundamental, int sampleRate, bool endpointB) const {
  const float gain = endpointGain(index, edo, fundamental, sampleRate, endpointB);
  const float phase = (endpointB ? phaseOffsetsB : phaseOffsets)[index];
  if (phase == 0 || phase == 1) return {gain, 0};
  if (phase == .25f) return {0, gain};
  if (phase == .5f) return {-gain, 0};
  if (phase == .75f) return {0, -gain};
  return {gain * float(std::cos(6.283185307179586 * phase)), gain * float(std::sin(6.283185307179586 * phase))};
}

double Patch::ratio(int index, int edo) const {
  const double octaves = std::log2(index + 1.);
  const double nearest = std::round(octaves * edo) / edo;
  double pitch = octaves;
  if (spectrum == Spectrum::EdoSpecific) pitch = std::round(octaves * edoSecondStep) / edo;
  if (spectrum == Spectrum::EdoQuantized) pitch = nearest;
  // Keep even extreme, inaudible ratios finite for the preview and band fade.
  return std::exp2(std::min(900., pitch * stretch));
}

AdditiveSynth::AdditiveSynth(int sampleRate)
  : rate_(std::clamp(sampleRate, 8000, 384000)), slew_(1 - std::exp(-1.f / (.005f * rate_))) {
  for (size_t i = 0; i < sineTable_.size(); ++i)
    sineTable_[i] = float(std::sin(6.283185307179586 * i / 2048));
}

void AdditiveSynth::publish() {
  scenes_[writer_] = desired_;
  writer_ = middle_.exchange(writer_ | dirty, std::memory_order_acq_rel) & ~dirty;
}

void AdditiveSynth::prepare(Target& target) {
  const auto& patch = desired_.patch;
  const double hz = 440 * std::exp2((target.note - 69 + detunes_[target.note % 12] / 100.) / 12.);
  target.modulationStep = float(std::min(.49, hz * patch.modulationRatio / rate_));
  for (int i = 0; i < Patch::partials; ++i) {
    const double ratio = patch.ratio(i, edo_);
    const double step = hz * ratio / rate_;
    target.steps[i] = float(std::min(.49, step));
    const auto a = patch.endpointComponents(i, edo_, hz, rate_, false);
    const auto b = patch.endpointComponents(i, edo_, hz, rate_, true);
    target.gains[i] = a.sine; target.cosGains[i] = a.cosine;
    target.gainsB[i] = b.sine; target.cosGainsB[i] = b.cosine;
  }
}

void AdditiveSynth::setTuning(int edo, const std::array<double, 12>& detunes) {
  if (edo < 1 || edo > 256 || !std::all_of(detunes.begin(), detunes.end(),
      [](double c) { return std::isfinite(c) && std::abs(c) <= 24000; })) return;
  edo_ = edo;
  detunes_ = detunes;
  for (auto& target : desired_.notes) if (target.id) prepare(target);
  publish();
}

bool AdditiveSynth::setPatch(const Patch& patch) {
  if (!patch.valid()) return false;
  desired_.patch = patch;
  for (auto& target : desired_.notes) if (target.id) prepare(target);
  publish();
  return true;
}

void AdditiveSynth::noteOn(int note, int velocity) {
  if (note < 0 || note > 127 || velocity < 0 || velocity > 127) return;
  if (!velocity) { noteOff(note); return; }
  for (auto& t : desired_.notes) if (t.held && t.note == note) {
    t.held = false;
    t.gated = pedal_;
  }
  // Prefer free/released slots, then steal the oldest sounding voice.
  auto& target = *std::min_element(desired_.notes.begin(), desired_.notes.end(),
    [](const Target& a, const Target& b) {
      return a.gated != b.gated ? !a.gated : a.id < b.id;
    });
  target = {};
  target.id = ++serial_;
  target.note = note;
  target.velocity = velocity;
  target.held = target.gated = true;
  prepare(target);
  publish();
}

void AdditiveSynth::noteOff(int note) {
  for (auto& target : desired_.notes) if (target.note == note && target.held) {
    target.held = false;
    target.gated = pedal_;
  }
  publish();
}

void AdditiveSynth::sustain(bool down) {
  pedal_ = down;
  if (!down) for (auto& t : desired_.notes) if (!t.held) t.gated = false;
  publish();
}

void AdditiveSynth::allNotesOff() {
  for (auto& t : desired_.notes) {
    t.held = false;
    if (!pedal_) t.gated = false;
  }
  publish();
}

void AdditiveSynth::silence() {
  pedal_ = false;
  desired_.notes = {};
  publish();
}

float AdditiveSynth::sine(float phase) const noexcept {
  const float index = phase * 2048;
  const int i = int(index);
  return sineTable_[i] + (sineTable_[i+1]-sineTable_[i]) * (index-i);
}

void AdditiveSynth::render(float* samples, int frames) noexcept {
  if (!samples || frames <= 0) return;
  if (middle_.load(std::memory_order_acquire) & dirty)
    reader_ = middle_.exchange(reader_, std::memory_order_acq_rel) & ~dirty;
  const auto& scene = scenes_[reader_];
  std::fill_n(samples, frames, 0.f);
  renderVoices(samples, frames, scene);
  // Leave quiet signals linear. Compress only the last 20% of headroom.
  for (int j = 0; j < frames; ++j) {
    const float excess = std::abs(samples[j]) - .8f;
    if (excess > 0) samples[j] = std::copysign(.8f + .2f * (excess / (.2f + excess)), samples[j]);
  }
}

void AdditiveSynth::renderVoices(float* samples, int frames, const Scene& scene) noexcept {
  const auto& patch = scene.patch;
  const float dt = 1.f / rate_;
  for (size_t slot = 0; slot < voices_.size(); ++slot) {
    const auto& target = scene.notes[slot];
    auto& v = voices_[slot];
    if (!target.id) { v.done = true; continue; }
    if (v.id != target.id) {
      const float tail = v.done ? 0 : v.previousSample;
      v = {};
      v.stolenTail = tail;
      v.attackFrames = rate_ / 500; // Retain a short attack for a tap inside one audio block.
      v.id = target.id;
      v.steps = target.steps;
      v.gains = target.gains;
      v.gainsB = target.gainsB;
      v.cosGains = target.cosGains; v.cosGainsB = target.cosGainsB;
      v.modStep = target.modulationStep;
      v.morphMode = patch.morphMode;
      v.manualMorph = patch.morph;
      v.morphPosition = patch.morphMode == MorphMode::Manual ? patch.morph : 0;
      v.morphHz = patch.morphLfoHz; v.morphSeconds = patch.morphSeconds;
    }
    if (v.done) continue;
    const float volume = .14f * target.velocity / 127;
    for (int j = 0; j < frames; ++j) {
      if (!target.gated && !v.releasing && !v.attackFrames) {
        v.releasing = true;
        v.releaseLevel = v.level;
      }
      if (v.attackFrames) --v.attackFrames;
      if (v.releasing) {
        if (v.releaseAge >= patch.releaseSeconds) { v.done = true; break; }
        v.level = v.releaseLevel * (1 - v.releaseAge / patch.releaseSeconds);
        v.releaseAge += dt;
      } else {
        v.level = patch.temporal.at(v.age);
        v.age = std::min(v.age + dt, patch.temporal.points[patch.temporal.count-1].x);
      }
      const float mod = sine(v.modPhase);
      v.modStep += slew_ * (target.modulationStep - v.modStep);
      v.modPhase = advance(v.modPhase, v.modStep);
      smoothControl(v.manualMorph, patch.morph, slew_);
      smoothControl(v.morphHz, patch.morphLfoHz, slew_);
      smoothControl(v.morphSeconds, patch.morphSeconds, slew_);
      if (v.morphMode != patch.morphMode) {
        v.morphMode = patch.morphMode; v.morphPhase = 0;
        v.morphOffset = v.morphPosition - (v.morphMode == MorphMode::Manual ? v.manualMorph : 0);
      }
      float position = v.manualMorph;
      if (v.morphMode != MorphMode::Manual) {
        float phase = float(v.morphPhase * (v.morphMode == MorphMode::OneShot ? .5 : 1) + .25);
        if (phase >= 1) phase -= 1;
        position = .5f - .5f * sine(phase);
      }
      v.morphPosition = std::clamp(position + v.morphOffset, 0.f, 1.f);
      smoothControl(v.morphOffset, 0, slew_);
      if (v.morphMode == MorphMode::OneShot)
        v.morphPhase = std::min(1., v.morphPhase + 1. / (rate_ * double(v.morphSeconds)));
      else if (v.morphMode == MorphMode::Loop) {
        v.morphPhase += double(v.morphHz) / rate_;
        if (v.morphPhase >= 1) v.morphPhase -= 1;
      }
      float sum = 0;
      for (int i = 0; i < Patch::partials; ++i) {
        v.steps[i] += slew_ * (target.steps[i] - v.steps[i]);
        v.gains[i] += slew_ * (target.gains[i] - v.gains[i]);
        v.gainsB[i] += slew_ * (target.gainsB[i] - v.gainsB[i]);
        v.cosGains[i] += slew_ * (target.cosGains[i] - v.cosGains[i]);
        v.cosGainsB[i] += slew_ * (target.cosGainsB[i] - v.cosGainsB[i]);
        if (target.gains[i] == 0 && std::abs(v.gains[i]) < 1.e-8f) v.gains[i] = 0;
        if (target.gainsB[i] == 0 && std::abs(v.gainsB[i]) < 1.e-8f) v.gainsB[i] = 0;
        if (target.cosGains[i] == 0 && std::abs(v.cosGains[i]) < 1.e-8f) v.cosGains[i] = 0;
        if (target.cosGainsB[i] == 0 && std::abs(v.cosGainsB[i]) < 1.e-8f) v.cosGainsB[i] = 0;
        const float gain = (1 - v.morphPosition) * v.gains[i] + v.morphPosition * v.gainsB[i];
        if (gain != 0) sum += sine(v.phases[i]) * gain;
        const float cosineGain = (1 - v.morphPosition) * v.cosGains[i] + v.morphPosition * v.cosGainsB[i];
        if (cosineGain != 0) sum += sine(advance(v.phases[i], .25f)) * cosineGain;
        v.phases[i] = advance(v.phases[i], v.steps[i] * (1 + mod * patch.fmDepth));
      }
      v.previousSample = v.stolenTail + volume * v.level * sum * (1 - patch.amDepth * (.5f - .5f*mod));
      v.stolenTail -= 4 * slew_ * v.stolenTail;
      if (std::abs(v.stolenTail) < 1.e-8f) v.stolenTail = 0;
      samples[j] += v.previousSample;
    }
  }
}
} // namespace Intona::Synth
