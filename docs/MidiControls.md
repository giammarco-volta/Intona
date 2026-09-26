# Assignable MIDI control

Settings stores one independent source, action, enabled flag and threshold.
The surface check mark changes only activation; its text reverses direction
within the chosen action family. Settings and surface share the same snapshot
and queued MIDI-thread setters.

Sources use stable IDs: 0 channel pressure, 1 polyphonic key pressure,
2 positive pitch bend, 3 negative pitch bend, 4–123 map to CC 0–119.
Actions are 0 step up, 1 step down, 2 next preset, 3 previous preset.
Channel-mode CCs 120–127 are never assignable. Program changes retain the existing
input filter. This is single-input-channel MIDI 1.0, not an MPE implementation.
Names follow the [MIDI Association CC table](https://midi.org/midi-1-0-control-change-messages)
and [message summary](https://midi.org/summary-of-midi-1-0-messages).

A gesture fires once at or above `controlThreshold` (1–127), then rearms at or
below half the threshold. Bend is normalized from centre to 0–127 separately in
each direction. Poly pressure has independent latches per MIDI note, reset on
note attack/release. Source/action/activation/threshold changes rearm according
to the last observed value, preventing a held controller from firing again merely
because a setting changed. Input-port/channel changes clear controller state.

Step actions compute all affected keys as one update. With `retriggerHeldNotes`
enabled, every affected held MIDI note is stopped first, a single tuning SysEx
is sent, and the notes are restarted with their original velocities on the selected
output channels. With it disabled, only tuning is sent. Unaffected held notes are
not retriggered; poly pressure also retriggers held octave copies because their
MTS pitch class changes together. The gesture latch still prevents repetition. Global sources deduplicate held
pitch classes. Poly pressure selects the addressed held MIDI note, but MTS
scale/octave tuning necessarily shares its change with octave copies. Preset
actions call the existing preset application path, wrap at both ends, use the
first/last entry when no preset is selected, and do nothing with no saved presets.
They do not send an instrument Program Change.

Enabled assignments consume their source. A selected coarse CC also consumes
its fine-resolution companion (CC+32); only the coarse value triggers actions.
Disabled assignments forward the selected source normally, including expression
and aftertouch. For other CCs, legacy routing exclusions (0, 7, 10, 11, 32, 71,
74) remain downstream of the binding. All CCs now reach the interpreter observer
and recorder before routing. Before reserving a previously forwarded sustain,
sostenuto, Hold 2 or pitch bend, its native hold/bend is released to prevent a
captured release from leaving the instrument stuck.

Preferences under `status`: `controlSource`, `controlAction`, `controlEnabled`,
`controlThreshold`. The old `aftertouchBehaviour` and misspelled
`aftertouchThreshol` are migrated once, preserving direction, activation and
threshold; they are then removed. Default remains channel aftertouch, step up,
enabled, threshold 10. Recording context includes all four current values.

Tests exercise real interpreter delivery of CC11/poly pressure, routing,
hysteresis, independent polyphonic notes, octave deduplication, preset wrap and
empty lists, persistence/migration, and the separate QML activation/direction
hit areas. Existing tuning and recording suites remain applicable.

When disabled the surface caption explicitly reads `= off`, preserving the saved action for re-enabling. The caption uses horizontal font fitting only when its measured text exceeds the available width; the hit region stays outside the circle. Settings places control and held-note panels side by side on wide screens, stacking them on narrow ones. Detailed explanations remain in the integrated manual.
