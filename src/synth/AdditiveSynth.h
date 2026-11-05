#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace Intona::Synth {

enum class Spectrum { NaturalHarmonic = 0, EdoQuantized = 1, EdoSpecific = 3 };
enum class Axis { PartialIndex, RelativeFrequency, AbsoluteHz };
enum class Shape { Impulse, Sine, Triangle, Square, Saw, SawSquare, SawTriangle, SquareTriangle, Organ, Pulse };
struct Harmonic { float sine = 0, cosine = 0; };
enum class MorphMode { Manual, OneShot, Loop };

struct Point { float x, y; };
// Increasing nonnegative x, linear interpolation, constant beyond either end.
struct Curve {
  std::array<Point, 16> points{};
  int count = 0;
  bool valid() const;
  float at(float x) const noexcept;
};

struct Patch {
  static constexpr int partials = 48;
  Patch();
  Shape shape = Shape::Impulse;
  Spectrum spectrum = Spectrum::NaturalHarmonic;
  Axis spectralAxis = Axis::PartialIndex;
  float stretch = 1; // exponent on the partial ratio; fundamental stays fixed
  int edoSecondStep = 12;
  std::array<float, partials> amplitudes{}, amplitudesB{};
  std::array<float, partials> phaseOffsets{}, phaseOffsetsB{}; // cycles relative to sine
  float pulseWidth = .5f, pulseWidthB = .5f;
  Curve spectral{{Point{1, 1}, Point{4, .25f}, Point{16, .03f}}, 3};
  // A uses the original shape/axis/curve fields for patch compatibility.
  Shape shapeB = Shape::Impulse;
  Axis spectralAxisB = Axis::PartialIndex;
  Curve spectralB = spectral;
  float morph = 0; // 0 = A, 1 = B; signed Fourier coefficients interpolate linearly
  MorphMode morphMode = MorphMode::Manual;
  float morphLfoHz = .2f, morphSeconds = .5f;
  // Seconds since attack. The last level is held until release.
  Curve temporal{{Point{0, 0}, Point{.008f, 1}, Point{.12f, .75f}}, 3};
  float releaseSeconds = .12f;
  float modulationRatio = 1; // voice-local oscillator, relative to fundamental
  float amDepth = 0, fmDepth = 0; // unipolar amplitude and fractional frequency
  bool valid() const;
  double ratio(int index, int edo) const;
  float coefficient(int index, bool endpointB = false) const noexcept;
  static float shapeCoefficient(Shape shape, int index, float width = .5f) noexcept;
  void setShape(Shape value, bool endpointB = false);
  float endpointGain(int index, int edo, double fundamental, int sampleRate, bool endpointB) const;
  Harmonic endpointComponents(int index, int edo, double fundamental, int sampleRate, bool endpointB) const;
  float gain(int index, int edo, double fundamental, int sampleRate) const;
};

class AdditiveSynth {
public:
  static constexpr int polyphony = 16;
  explicit AdditiveSynth(int sampleRate);
  // Single producer (tuning worker). Detunes include the global offset, in cents.
  void setTuning(int edo, const std::array<double, 12>& detunes);
  bool setPatch(const Patch& patch);
  void noteOn(int note, int velocity);
  void noteOff(int note);
  void sustain(bool down);
  void allNotesOff();
  void silence();
  // Single audio consumer, mono float PCM. No heap, locks, Qt or device calls.
  void render(float* samples, int frames) noexcept;

private:
  struct Target {
    uint64_t id = 0;
    int note = 0, velocity = 0;
    bool held = false, gated = false;
    float modulationStep = 0;
    std::array<float, Patch::partials> steps{}, gains{}, gainsB{}, cosGains{}, cosGainsB{};
  };
  struct Scene {
    Patch patch;
    std::array<Target, polyphony> notes{};
  };
  struct Voice {
    uint64_t id = 0;
    float age = 0, releaseAge = 0, releaseLevel = 0, level = 0;
    bool releasing = false, done = false;
    int attackFrames = 0;
    float previousSample = 0, stolenTail = 0;
    float modPhase = 0, modStep = 0;
    MorphMode morphMode = MorphMode::Manual;
    double morphPhase = 0;
    float morphPosition = 0, morphOffset = 0, manualMorph = 0;
    float morphHz = .2f, morphSeconds = .5f;
    std::array<float, Patch::partials> phases{}, steps{}, gains{}, gainsB{}, cosGains{}, cosGainsB{};
  };
  void prepare(Target& target);
  void publish();
  float sine(float phase) const noexcept;
  void renderVoices(float* samples, int frames, const Scene& scene) noexcept;

  int rate_, edo_ = 12;
  float slew_;
  bool pedal_ = false;
  uint64_t serial_ = 0;
  std::array<double, 12> detunes_{};
  Scene desired_;
  // Triple buffer. Producer, consumer and exchange slot have exclusive ownership.
  std::array<Scene, 3> scenes_{};
  static constexpr unsigned dirty = 4;
  static_assert(std::atomic<unsigned>::is_always_lock_free);
  std::atomic<unsigned> middle_{1};
  unsigned writer_ = 0, reader_ = 2;
  std::array<Voice, polyphony> voices_{};
  std::array<float, 2049> sineTable_{};
};
} // namespace Intona::Synth
