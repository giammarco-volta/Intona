# Internal additive synth

Open **Synthesizer** using the faders icon on the left rail. **Play internal
additive synth** sends playback to the default audio device and disables MIDI OUT
controls. MIDI IN remains available. Turning internal audio off restores external
MIDI notes and tuning. The obsolete Output explanation panel has been removed;
actual audio errors still appear beside the editor.

The engine renders mono float at 48 kHz. Windows uses event-driven shared WASAPI
stereo output; Android uses AAudio (minimum SDK 28).

## Tuning and voice lifetime

TuningController uses keyboardRawOffset for both the internal fundamental and
the external MTS table. Internal frequency is
440 × 2^((MIDI note − 69 + offset/100)/12) Hz. Each articulation has an independent
voice ID. Adaptive tuning, EDO changes and tuning-preset recall preserve held
voices, oscillator phase and envelope progress, with a 5 ms pitch glide.
External MIDI Note On/Off and MTS routing remain unchanged.

There are 16 voices, each with exactly 48 partial slots. There is no partial-count
control. CC64 sustains releases; CC123 releases keys under the pedal; CC121 lifts
the pedal; CC120 silences voices. Voice stealing prefers released slots and uses
a short declick tail. Repeated Note On creates a new articulation.

## Starting spectra, edited amplitudes and envelopes

A and B each contain 48 signed Fourier amplitudes, phase offsets and an independent spectral
envelope. The starting-spectrum menu loads amplitudes and phases for Impulse, Sine, Triangle,
Square, Sawtooth, Saw/Square, Saw/Triangle, Square/Triangle, Organ and Pulse. The three
mixed shapes are equal linear combinations. Organ uses a drawbar-inspired
distribution over partials 1,2,3,4,5,6,8. Impulse replaces Flat: equal amplitudes
with cosine-aligned phases produce a band-limited impulse train with no DC term.
A flat magnitude spectrum alone does not specify a unique waveform; phase matters.

Pulse is the rectangular-wave generalization of Square. Its high-state duration
is adjustable from 1% to 99% independently for A/B; 50% matches Square exactly.
Width can be edited with the slider or numeric field. Changing it reloads that
endpoint's Pulse amplitudes and phases, retaining the spectral envelope. It is a
manual pulse-width control, not a new automatic PWM LFO. A/B morph remains a
crossfade between endpoint waveforms, not an exact interpolation of duty cycle.

For width D and harmonic n, the signed amplitude is sin(pi*n*D)/n and the phase
offset relative to sine is 1/4 - n*D/2 cycles, modulo one. This removes the DC
component and preserves the high/low level difference without boosting narrow
pulses. With a flat envelope, Natural Harmonic and stretch 1, the finite harmonic
sum approximates the requested duty cycle. Band limiting rounds edges; EDO
redistribution and envelope shaping alter the time-domain waveform.

Selecting a starting spectrum replaces that endpoint's amplitudes and phases while retaining
its envelope. It is not a sound-preset operation. An edited distribution is marked
**Custom spectrum**; the last starting spectrum remains visible in the menu.
Sound-preset saving is not implemented; synth edits are session-local. The fixed
header explicitly labels the existing presets as **Tuning preset**.

Select any partial in the spectrum graph or its numeric selector. Its amplitude
can be edited using the slider or percentage field, including partials initially
zero in a template. Editing A does not alter B. Negative Fourier signs (triangle
phase inversions) are preserved when adjusting magnitude. Copy A/B copies the
amplitude/phase arrays, pulse width, starting-spectrum identity, envelope and envelope axis.

The spectral envelope multiplies these amplitudes. Therefore a nonzero partial
can still be silent because of its envelope or the audio band fade. A poorer
spectrum is made with zeros or envelope attenuation, not a smaller oscillator
count. Triangle coefficients are signed odd 1/n², square odd 1/n and saw all 1/n.

## Distribution and stretch

- **Natural Harmonic:** ratio n for partial n, before stretch.
- **EDO Quantized:** round(EDO × log2(n)) steps above the fundamental.
- **EDO Specific:** round(S × log2(n)) steps, where S is **Partial 2 · EDO steps**.
  Thus S controls every partial proportionally on the logarithmic frequency axis.
  At S=12, partials 2,3,4 are 12,19,24 steps. At S=19 they are 19,30,38.
  The fundamental remains at step zero.

Spectral stretch raises the resulting frequency ratio to an exponent. The editor
uses 0.01 increments over 0.50–2.00, with a directly editable numeric control.
The decoder retains the older 0.25–4.00 range for compatibility. Stretch is applied
after EDO distribution and may move carriers off the EDO grid; the inspector
reports that deviation. Extremely high, inaudible ratios are capped at 2^900
to keep calculations finite.

## Envelope editors and spectrum preview

Both envelope editors show vertical/horizontal grids, amplitude percentages and
axis units. Spectral units are Partial index, Frequency ratio or Frequency (Hz);
the time envelope uses seconds. When changing spectral units, nodes are converted
using the reference note and current partial distribution. Conversion interpolates
between neighboring carrier positions; coincident frequencies merge envelope
nodes. An absolute-frequency envelope retains its Hz positions as notes change.

The graph shows target frequencies on a logarithmic axis, natural-harmonic
references, the EDO grid, A/B levels and the animated morph's complete range.
The selected-partial inspector reports Hz, ratio, effective amplitude, nearest EDO
step and offsets from that step and the natural harmonic. Coincident carriers can
be selected separately with the numeric selector.

This is a static preview at the chosen preview morph position, not a live FFT.
It includes spectral envelopes and carrier band attenuation at 48 kHz. It excludes
intermediate pitch glides, velocity, the temporal envelope, final output limiting
and modulation sidebands. The dB axis is relative to a unit partial, not dBFS.

## Morph per note

The modes are **Manual**, **A → B · one shot** and **A ↔ B · loop**.
Manual uses the morph slider for the audible mix. In animated modes the slider
only selects the spectrum-preview position; playback always starts at A.

Each Note On starts its own morph at phase zero, including repeated MIDI notes
and notes added to a held chord. One shot follows a half-cosine from A to B over
the specified 0.001–30 second duration, then holds B until the note ends.
Loop follows a full cosine cycle A–B–A at 0.01–5 Hz. Its speed is independent
of the fundamental. Voices do not inherit a shared or free-running cycle.

EDO/tuning changes and same-mode patch edits preserve each voice's progress.
Changing morph mode restarts that voice's motion with a decaying offset to
avoid a discontinuity. Manual-position, duration and speed edits are smoothed.
Copying/editing endpoints smoothly changes their gains without restarting notes.
A/B sine and cosine coefficients interpolate linearly, so opposite phases may cancel
in the middle; identical endpoints retain their level. The spectrum preview computes
magnitudes, phase and movement ranges from these two components, including partial
cancellation between endpoints with different phases.

The amplitude envelope remains independent: its final node sustains until
Note Off, then releases over releaseSeconds. Attack duration and spectral morph
duration can therefore be set separately.

## Patch format and compatibility

The complete QML value map is exposed as synthPatch; setSynthPatch validates a
copy before publishing. Format version 3 writes:
- shape/shapeB (starting-spectrum identities, 0–9; Impulse=0, Pulse=9).
- amplitudes/amplitudesB (48 finite signed values in [-1,1]).
- phaseOffsets/phaseOffsetsB (48 values in [0,1], cycles relative to sine).
- pulseWidth/pulseWidthB (0.01–0.99, independent endpoint duty).
- spectral/spectralB and spectralAxis/spectralAxisB (0=index, 1=ratio, 2=Hz).
- spectrum (0=natural, 1=EDO quantized, 3=EDO specific), edoSecondStep, stretch.
- morph, morphMode (0=manual, 1=one shot, 2=loop), morphSeconds, morphLfoHz.
- temporal, releaseSeconds, modulationRatio, amDepth, fmDepth.

Curves contain 1–16 increasing nonnegative x positions and y in [0,1].
The temporal curve must start at x=0. Invalid or incomplete maps are rejected.

Unversioned maps are migrated: old shape coefficients are materialized into
editable amplitudes, and slots above the old count are zeroed. A single endpoint
is copied to B. Selectively Corrected becomes EDO Quantized. Old EDO-designed
degree arrays supply the second-partial step; later degrees are regenerated by
the new proportional rule, so arbitrary old degree patterns are not preserved.
Old nonzero morph-LFO depth selects the new full-range, per-note loop; zero depth
selects Manual. The former shared phase and limited-depth excursion are
intentionally replaced by the new behavior.

Version 2 and unversioned maps retain zero phase offsets, preserving the original
sine-only sound, including old Flat spectra. Such a Flat spectrum is displayed as
a custom Impulse until the user selects Impulse again to load the aligned phases.
Version 3 stores all phase and width fields explicitly; malformed arrays and
out-of-range duties are rejected.

## Realtime boundary and verification

One producer on the tuning worker prepares frequencies and sine/cosine gain arrays
for both endpoints. All phase trigonometry is outside the audio callback.
Three fixed scene buffers transfer the newest complete state to the audio
consumer through a lock-free atomic exchange. Rendering uses fixed arrays,
an interpolated sine table and voice-local morph state, without allocations,
locks, Qt calls or device calls. Output remains linear below the final soft
limiter threshold. The existing voice-local audio-rate AM/FM controls are separate
from the slower A/B morph.

Build with INTONA_BUILD_TESTS=ON and run CTest. Audio tests cover distributions,
editable amplitudes, Pulse Fourier-series accuracy, 50% Pulse/Square equivalence,
Impulse phase alignment, quadrature morphing, migration, per-note morph trajectories, one-shot completion,
retrigger, staggered notes, sustain, tuning changes, callback-size independence
and 16×48 rendering. UI tests exercise controls, unit grids and numeric edits;
Windows and Android native builds are checked separately. Device listening,
latency and dropout verification require an actual output device.
