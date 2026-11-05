#include "synth/PatchData.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <chrono>
#include <limits>
#include <memory>

using namespace Intona::Synth;
namespace {
void check(bool condition, const char* reason) {
  if (!condition) throw std::runtime_error(reason);
}
std::vector<float> block(AdditiveSynth& synth, int frames) {
  std::vector<float> x(frames);
  synth.render(x.data(), frames);
  for (float s : x) check(std::isfinite(s) && std::abs(s) <= 1, "invalid PCM");
  return x;
}
float peak(const std::vector<float>& x) {
  float p = 0;
  for (float s : x) p = std::max(p, std::abs(s));
  return p;
}
int cycles(const std::vector<float>& x) {
  int n = 0;
  for (size_t i = 1; i < x.size(); ++i) if (x[i-1] < 0 && x[i] >= 0) ++n;
  return n;
}
Patch sinePatch() {
  Patch p;
  p.setShape(Shape::Sine);
  p.setShape(Shape::Sine, true);
  p.temporal = {{{Point{0, 1}}}, 1};
  return p;
}
void spectra() {
  Patch p;
  constexpr int i = 4;
  check(std::abs(p.ratio(i, 19)-5) < 1e-9, "natural ratio");
  p.spectrum = Spectrum::EdoQuantized;
  const double snapped = std::exp2(std::round(19*std::log2(5.))/19);
  check(std::abs(p.ratio(i, 19)-snapped) < 1e-9, "EDO grid");
  p.spectrum = Spectrum::EdoSpecific;
  p.edoSecondStep = 19;
  check(std::abs(p.ratio(1, 19)-2) < 1e-9, "EDO second partial");
  check(std::abs(p.ratio(2, 19)-std::exp2(30./19)) < 1e-9, "EDO proportional third");
  p.edoSecondStep = 12;
  for (int k = 0; k < 48; ++k)
    check(std::abs(19 * std::log2(p.ratio(k, 19)) - std::round(12 * std::log2(k+1.))) < 1e-9, "EDO scalar changes every partial");
  p.stretch = 2;
  check(std::abs(p.ratio(1, 19)-std::exp2(24./19)) < 1e-9 && p.ratio(0, 19) == 1, "EDO stretch preserves fundamental");
  auto data = patchData(p);
  const auto copy = readPatch(data);
  check(copy && patchData(*copy) == data, "QML patch round trip");
  data["temporal"] = QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                   QVariantMap{{"x", 0}, {"y", 1}}};
  check(!readPatch(data), "reject coincident envelope nodes");
  check(std::abs(p.spectral.at(3)-.5f) < 1e-6f, "spectral interpolation");
}
void glideAndPedal() {
  AdditiveSynth a(48000), reference(48000);
  for (auto* synth : {&a, &reference}) {
    check(synth->setPatch(sinePatch()), "valid sine patch");
    synth->noteOn(69, 100);
    block(*synth, 777);
  }
  std::array<double, 12> detunes{};
  detunes[9] = 1200;
  a.setTuning(31, detunes);
  const auto changed = block(a, 20), old = block(reference, 20);
  check(changed[0] == old[0] && std::abs(changed[10]-old[10]) < .003, "phase-continuous glide");
  block(a, 4000);
  check(std::abs(cycles(block(a, 4800))-88) <= 1, "retuned 880 Hz");
  a.sustain(true);
  a.noteOff(69);
  check(peak(block(a, 9000)) > .02, "sustain holds");
  a.sustain(false);
  block(a, 7000);
  check(peak(block(a, 128)) == 0, "release reaches silence");
  a.noteOn(60, 80);
  a.noteOff(60);
  check(peak(block(a, 300)) > .01, "short tap within one block still articulates");
}
void modulationAndAxes() {
  for (bool fm : {true, false}) {
    AdditiveSynth a(48000), b(48000);
    auto p = sinePatch();
    a.setPatch(p);
    if (fm) p.fmDepth = .3f; else p.amDepth = .8f;
    b.setPatch(p);
    a.noteOn(69, 96); b.noteOn(69, 96);
    check(block(a, 200) != block(b, 200), "AM/FM audible");
  }
  for (auto axis : {Axis::PartialIndex, Axis::RelativeFrequency, Axis::AbsoluteHz}) {
    AdditiveSynth a(48000);
    auto p = sinePatch();
    p.spectralAxis = axis;
    p.spectral = {{{Point{1, 0}, Point{440, 1}}}, 2};
    a.setPatch(p);
    a.noteOn(69, 127);
    check((peak(block(a, 240)) > .05) == (axis == Axis::AbsoluteHz), "spectral axis units");
  }
  AdditiveSynth a(48000);
  auto p = sinePatch();
  p.setShape(Shape::Impulse);
  p.spectral = {{{Point{1, 0}, Point{2, 1}}}, 2};
  a.setPatch(p);
  a.noteOn(127, 127);
  check(peak(block(a, 1000)) == 0, "partials above Nyquist excluded");
}
void cleanContinuousTone() {
  AdditiveSynth whole(48000), split(48000);
  for (auto* synth : {&whole, &split}) {
    synth->setPatch(sinePatch());
    synth->noteOn(69, 127);
  }
  const auto original = block(whole, 24000);
  size_t pos = 0;
  for (int size : {1, 17, 256, 480, 1024, 4096, 18126}) {
    const auto packet = block(split, size);
    check(std::equal(packet.begin(), packet.end(), original.begin() + pos), "sample discontinuity across packets");
    pos += size;
  }
  check(pos == original.size(), "packet coverage");
  // Frequency and amplitude on a single unmodulated partial, including buffer seams.
  for (size_t i = 0; i < pos; ++i) {
    const double expected = .14 * std::sin(2 * 3.141592653589793 * i * 440 / 48000);
    check(std::abs(original[i] - expected) < .0005, "single partial distorted or interrupted");
  }
}

void shapesAndPreview() {
  Patch p;

  p.temporal = {{{Point{0, 1}}}, 1};
  p.spectral = {{{Point{0, 1}}}, 1};
  p.setShape(Shape::Triangle);
  check(p.coefficient(1) == 0 && std::abs(p.coefficient(2) + 1.f/9) < 1e-7, "triangle odd signed coefficients");
  p.setShape(Shape::Square);
  check(p.coefficient(1) == 0 && std::abs(p.coefficient(2) - 1.f/3) < 1e-7, "square odd coefficients");
  for (auto shape : {Shape::Sine, Shape::Triangle, Shape::Square, Shape::Saw}) {
    p.setShape(shape);
    check(readPatch(patchData(p))->shape == shape, "shape round trip");
    AdditiveSynth synth(48000);
    synth.setPatch(p); synth.noteOn(45, 127); // 110 Hz: all 48 carriers below the band fade.
    const auto pcm = block(synth, 4096);
    for (int t = 0; t < int(pcm.size()); ++t) {
      double expected = 0;
      for (int i = 0; i < 48; ++i)
        expected += .14 * p.coefficient(i) * std::sin(2 * 3.141592653589793 * (i+1) * t * 110 / 48000);
      check(std::abs(pcm[t] - expected) < .0006, "waveform PCM matches Fourier series");
    }
    p.spectrum = Spectrum::EdoQuantized;
    const auto preview = spectrumData(p, 31, 440);
    check(preview.size() == 48, "48 preview partials");
    for (int i = 0; i < 48; ++i) {
      const auto row = preview[i].toMap();
      check(std::abs(row.value("gridCents").toDouble()) < 1e-8, "shape follows EDO grid");
      check(row.value("gain").toFloat() == std::abs(p.gain(i, 31, 440, 48000)), "preview matches engine gain");
    }
    p.stretch = 1.1f;
    check(std::abs(spectrumData(p, 31, 440)[4].toMap().value("gridCents").toDouble()) > .1, "preview shows stretch off grid");
    p.stretch = 1; p.spectrum = Spectrum::NaturalHarmonic;
  }
  // Every oscillator keeps running when its coefficient is zero: shape edits on
  // held voices must converge to the same phase as a voice started in that shape.
  AdditiveSynth changed(48000), reference(48000);
  p.setShape(Shape::Sine); changed.setPatch(p);
  p.setShape(Shape::Triangle); reference.setPatch(p);
  changed.noteOn(45, 127); reference.noteOn(45, 127);
  block(changed, 777); block(reference, 777);
  changed.setPatch(p);
  block(changed, 4000); block(reference, 4000);
  const auto a = block(changed, 1000), b = block(reference, 1000);
  for (int i = 0; i < 1000; ++i) check(std::abs(a[i] - b[i]) < .00001, "held shape edit preserves oscillator phase");
  p.setShape(Shape::Sine); changed.setPatch(p);
  check(std::abs(block(changed, 1)[0] - block(reference, 1)[0]) < .001, "negative triangle partial fades smoothly");

  AdditiveSynth full(48000);
  p.setShape(Shape::Saw); full.setPatch(p);
  for (int i = 0; i < 16; ++i) full.noteOn(24 + i, 60);
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100; ++i) block(full, 480);
  std::cout << "16 voices x 48 partials, 1 second PCM: "
    << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() << " ms\n";
}

void pressure() {
  AdditiveSynth a(48000);
  std::atomic<bool> stop{false};
  std::thread producer([&]() {
    for (int n = 0; n < 4000; ++n) {
      a.noteOn(36+n%60, 100);
      if (n%3 == 0) a.noteOff(36+n%60);
    }
    a.silence();
    stop.store(true);
  });
  do { block(a, 128); } while (!stop.load());
  producer.join();
  check(peak(block(a, 128)) == 0, "latest state survives overload");
}

void morphAndSlowLfo() {
  Patch p = sinePatch();
  p.spectral = p.spectralB = {{{Point{0, 1}}}, 1};
  p.amplitudesB.fill(0); p.amplitudesB[1] = 1;
  p.morph = .4f; p.morphLfoHz = .5f; p.morphSeconds = .25f;
  check(patchData(*readPatch(patchData(p))) == patchData(p), "new patch round trip");
  for (auto mode : {MorphMode::Manual, MorphMode::OneShot, MorphMode::Loop}) {
    p.morphMode = mode;
    // Deliberately leave the engine rendering silence before each onset.
    auto movingOwner = std::make_unique<AdditiveSynth>(48000); auto& moving = *movingOwner;
    auto aOwner = std::make_unique<AdditiveSynth>(48000); auto& a = *aOwner;
    auto bOwner = std::make_unique<AdditiveSynth>(48000); auto& b = *bOwner;
    moving.setPatch(p); block(moving, 777);
    auto ep = p; ep.morphMode = MorphMode::Manual; ep.morph = 0; a.setPatch(ep);
    ep.amplitudes = ep.amplitudesB; b.setPatch(ep);
    for (auto* synth : {&moving, &a, &b}) synth->noteOn(69, 100);
    const int frames = 96000;
    const auto sound = block(moving, frames), left = block(a, frames), right = block(b, frames);
    for (int i = 0; i < frames; ++i) {
      const double position = mode == MorphMode::Manual ? p.morph :
        mode == MorphMode::OneShot ? .5 - .5*std::cos(3.141592653589793 * std::min(1., i/(48000.*p.morphSeconds))) :
        .5 - .5*std::cos(2*3.141592653589793 * p.morphLfoHz * i/48000);
      check(std::abs(sound[i] - ((1-position)*left[i] + position*right[i])) < 1e-6, "morph trajectory starts at A per note; one-shot holds B");
    }
  }
  // Staggered notes each have their own phase-zero motion timeline.
  for (auto mode : {MorphMode::OneShot, MorphMode::Loop}) {
    p.morphMode = mode;
    auto combinedOwner = std::make_unique<AdditiveSynth>(48000); auto& combined = *combinedOwner;
    auto firstOwner = std::make_unique<AdditiveSynth>(48000); auto& first = *firstOwner;
    auto secondOwner = std::make_unique<AdditiveSynth>(48000); auto& second = *secondOwner;
    for (auto* synth : {&combined, &first, &second}) synth->setPatch(p);
    combined.noteOn(57, 60); first.noteOn(57, 60);
    block(combined, 12345); block(first, 12345); block(second, 12345);
    combined.noteOn(64, 60); second.noteOn(64, 60);
    const auto sum = block(combined, 4096), one = block(first, 4096), two = block(second, 4096);
    for (int i = 0; i < 4096; ++i) check(std::abs(sum[i]-one[i]-two[i]) < 1e-7, "new note starts its own cycle without restarting existing voices");
  }
  p.morphMode = MorphMode::Loop;
  auto wholeOwner = std::make_unique<AdditiveSynth>(48000); auto& whole = *wholeOwner;
    auto splitOwner = std::make_unique<AdditiveSynth>(48000); auto& split = *splitOwner;
  for (auto* synth : {&whole, &split}) { synth->setPatch(p); synth->noteOn(57, 80); synth->noteOn(64, 80); }
  const auto expected = block(whole, 17001);
  int offset = 0;
  for (int size : {1, 17, 128, 513, 8192, 8150}) {
    const auto packet = block(split, size);
    check(std::equal(packet.begin(), packet.end(), expected.begin()+offset), "motion independent of callback sizes");
    offset += size;
  }
  split.setPatch(p);
  std::array<double, 12> detunes{}; detunes[9] = 37; detunes[4] = -23;
  whole.setTuning(19, detunes); split.setTuning(19, detunes);
  check(block(whole, 1024) == block(split, 1024), "retuning and patch publication preserve note motion");
  split.sustain(true); split.allNotesOff();
  check(peak(block(split, 8000)) > .02, "motion continues under sustain");
  split.sustain(false); block(split, 8000);
  check(peak(block(split, 128)) == 0, "motion releases fully");
  // Same MIDI note rearticulated after silence must reproduce the original attack.
  auto repeatOwner = std::make_unique<AdditiveSynth>(48000); auto& repeat = *repeatOwner; p.morphMode = MorphMode::OneShot; repeat.setPatch(p);
  repeat.noteOn(69, 80); const auto attack = block(repeat, 2048);
  repeat.noteOff(69); block(repeat, 16000);
  repeat.noteOn(69, 80); check(block(repeat, 2048) == attack, "retrigger restarts one-shot at phase zero");

  p.morphMode = MorphMode::Manual; p.morph = .6f;
  auto editOwner = std::make_unique<AdditiveSynth>(48000); auto& edit = *editOwner;
    auto unchangedOwner = std::make_unique<AdditiveSynth>(48000); auto& unchanged = *unchangedOwner;
  for (auto* synth : {&edit, &unchanged}) { synth->setPatch(p); synth->noteOn(69, 100); block(*synth, 777); }
  p.morphMode = MorphMode::OneShot; edit.setPatch(p);
  check(std::abs(block(edit, 1)[0]-block(unchanged, 1)[0]) < .001, "mode edits are continuous on held notes");
}

void editableSpectraAndMigration() {
  {
    auto editedPatch = sinePatch();
    AdditiveSynth edited(48000), reference(48000);
    edited.setPatch(editedPatch);
    editedPatch.amplitudes[1] = .7f;
    reference.setPatch(editedPatch);
    for (auto* synth : {&edited, &reference}) { synth->noteOn(57, 80); block(*synth, 777); }
    edited.setPatch(editedPatch);
    block(edited, 4000); block(reference, 4000);
    const auto changed = block(edited, 1024), expected = block(reference, 1024);
    for (int i = 0; i < 1024; ++i) check(std::abs(changed[i]-expected[i]) < .00001, "edited zero partial becomes audible with continuous phase");
  }
  Patch p;
  for (int shape = 0; shape <= int(Shape::Pulse); ++shape) {
    p.setShape(Shape(shape));
    check(p.valid() && shapeAmplitudes(shape).size() == 48, "all starting spectra valid");
  }
  p.setShape(Shape::Square);
  check(p.coefficient(1) == 0, "square even partial initially absent");
  p.amplitudes[1] = .7f;
  check(std::abs(p.endpointGain(1, 19, 440, 48000, false) - .7f*p.spectral.at(2)) < 1e-7, "zero template partial can be edited");
  p.amplitudes[47] = .3f;
  check(patchData(*readPatch(patchData(p))) == patchData(p), "all 48 edited amplitudes round trip");
  auto bad = patchData(p);
  auto levels = bad["amplitudes"].toList(); levels[0] = 1.1; bad["amplitudes"] = levels;
  check(!readPatch(bad), "reject amplitude out of range");
  bad = patchData(p); bad["spectrum"] = 2; check(!readPatch(bad), "removed mode rejected in current format");
  bad = patchData(p); bad["morphSeconds"] = 0; check(!readPatch(bad), "reject zero morph duration");
  bad = patchData(p); bad["morphMode"] = 3; check(!readPatch(bad), "reject unknown motion mode");
  bad = patchData(p); bad["morphLfoHz"] = std::numeric_limits<double>::quiet_NaN(); check(!readPatch(bad), "reject invalid motion rate");
  QVariantList degrees;
  for (int i = 0; i < 16; ++i) degrees.append(int(std::round(12*std::log2(i+1.))));
  auto legacy = patchData(p);
  for (const auto key : {"version", "amplitudes", "amplitudesB", "edoSecondStep", "morphMode", "morphSeconds"}) legacy.remove(key);
  legacy["count"] = 12; legacy["degrees"] = degrees; legacy["correctionCents"] = 16; legacy["morphLfoDepth"] = .6;
  legacy["spectrum"] = 2;
  auto migrated = readPatch(legacy);
  check(migrated && migrated->spectrum == Spectrum::EdoQuantized && migrated->morphMode == MorphMode::Loop &&
    migrated->amplitudes[12] == 0 && migrated->edoSecondStep == 12, "legacy count, selective mode and movement migrate");
  for (const auto key : {"shapeB", "spectralAxisB", "spectralB", "morph", "morphLfoHz", "morphLfoDepth"}) legacy.remove(key);
  migrated = readPatch(legacy);
  check(migrated && migrated->amplitudes == migrated->amplitudesB && migrated->morphMode == MorphMode::Manual, "old single spectrum clones into B");
  p.spectrum = Spectrum::EdoSpecific; p.edoSecondStep = 256; p.stretch = 4;
  for (const auto& v : spectrumData(p, 1, 440)) check(std::isfinite(v.toMap()["hz"].toDouble()), "extreme EDO preview remains finite");
}

}
void pulseAndImpulse() {
  using namespace Intona::Synth;
  constexpr double pi = 3.141592653589793;
  Patch p;
  p.spectral = p.spectralB = {{{Point{0, 1}}}, 1};
  p.temporal = {{{Point{0, 1}}}, 1};
  for (float duty : {.01f, .125f, .25f, .5f, .75f, .99f}) {
    p.pulseWidth = duty; p.setShape(Shape::Pulse);
    AdditiveSynth synth(48000); synth.setPatch(p); synth.noteOn(45, 127);
    const auto pcm = block(synth, 4096);
    for (int t = 0; t < 4096; ++t) {
      double expected = 0;
      for (int n = 1; n <= 48; ++n) {
        const double angle = 2*pi*n*110*t/48000;
        expected += (std::sin(2*pi*n*duty)*std::cos(angle) + (1-std::cos(2*pi*n*duty))*std::sin(angle)) / (2*n);
      }
      check(std::abs(pcm[t] - .14*expected) < .0003, "Pulse PCM matches rectangular-wave Fourier integral including phase");
    }
    check(patchData(*readPatch(patchData(p))) == patchData(p), "pulse duty and phases round trip");
  }
  p.pulseWidth = .5f; p.setShape(Shape::Pulse);
  AdditiveSynth pulse(48000), square(48000);
  pulse.setPatch(p); p.setShape(Shape::Square); square.setPatch(p);
  pulse.noteOn(45, 100); square.noteOn(45, 100);
  check(block(pulse, 4096) == block(square, 4096), "50 percent pulse exactly equals square");

  p.setShape(Shape::Impulse);
  AdditiveSynth impulse(48000); impulse.setPatch(p); impulse.noteOn(45, 1);
  const auto pcm = block(impulse, 4096);
  for (int t = 0; t < 4096; ++t) {
    double expected = 0;
    for (int n = 1; n <= 48; ++n) expected += std::cos(2*pi*n*110*t/48000);
    check(std::abs(pcm[t] - .14/127*expected) < .0001, "flat spectrum has aligned cosine phases for band-limited impulse");
  }
  p.setShape(Shape::Sine, true); p.morphMode = MorphMode::Loop; p.morph = .5f;
  const auto preview = spectrumData(p, 19, 110).first().toMap();
  check(std::abs(preview["gain"].toDouble()-std::sqrt(.5)) < 1e-6 &&
        std::abs(preview["minGain"].toDouble()-std::sqrt(.5)) < 1e-6 &&
        std::abs(preview["phaseDegrees"].toDouble()-45) < .001, "preview accounts for quadrature morph instead of false cancellation");
  p.morphMode = MorphMode::Manual; p.morph = .3f;
  p.pulseWidthB = .25f; p.setShape(Shape::Pulse, true);
  AdditiveSynth mixed(48000), a(48000), b(48000);
  mixed.setPatch(p);
  auto endpoint = p; endpoint.morph = 0; a.setPatch(endpoint);
  endpoint.amplitudes = endpoint.amplitudesB; endpoint.phaseOffsets = endpoint.phaseOffsetsB; b.setPatch(endpoint);
  for (auto* synth : {&mixed, &a, &b}) synth->noteOn(45, 1);
  const auto blend = block(mixed, 3000), left = block(a, 3000), right = block(b, 3000);
  for (int i = 0; i < 3000; ++i) check(std::abs(blend[i] - .7*left[i] - .3*right[i]) < 1e-7, "morph includes both quadratures");
  auto legacy = patchData(p); legacy["version"] = 2;
  for (const auto key : {"phaseOffsets", "phaseOffsetsB", "pulseWidth", "pulseWidthB"}) legacy.remove(key);
  const auto migrated = readPatch(legacy);
  check(migrated && migrated->phaseOffsets[0] == 0 && migrated->phaseOffsetsB[0] == 0, "old sine-only spectra keep their phases");
  auto bad = patchData(p); bad["pulseWidth"] = 0; check(!readPatch(bad), "reject zero duty");
  bad["pulseWidth"] = 1; check(!readPatch(bad), "reject full duty");
  bad = patchData(p); auto phases = bad["phaseOffsets"].toList(); phases[0] = -.1; bad["phaseOffsets"] = phases;
  check(!readPatch(bad), "reject invalid phases");
}

int main() {
  try { spectra(); glideAndPedal(); modulationAndAxes(); cleanContinuousTone(); shapesAndPreview(); morphAndSlowLfo(); editableSpectraAndMigration(); pulseAndImpulse(); pressure(); }
  catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  return 0;
}
