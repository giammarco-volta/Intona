# Intona regression tests

Configure with `-DINTONA_BUILD_TESTS=ON`, build, then run
`ctest --test-dir <build-directory> --output-on-failure`.
On Windows, put the matching Qt `bin` directory on PATH before running tests.
All controller tests use temporary INI settings. MIDI output is simulated.

## Naming and keyboard geometry

`IntonaNoteNaming` verifies note spelling across every supported EDO and center,
relative interval spelling, simplified names, preset persistence and queued UI
updates. Keyboard tests enumerate all twelve rotations to verify minimum tuning
error, pitch order, MIDI encoding, old preset compatibility.
`RelativeKeyboardTests` verifies the fixed keyboard anchor, preservation of the
eleven common notes, octave continuity and MIDI encoding across long fifth paths.
The old harmonic-cost/history experiment and chord-driven tuning tests are removed.

## Scale and triad adaptation

`IntonaScaleTriads` replays 541 independent simulator-reference cases at four
keyboard anchors, covering attack orders, progressive interpretation, conflicting
triads, dirty notes and rollback. It accepts an optional JSON fixture path for
full performance replays; personal recordings stay outside the repository.

Controller tests cover immediate MIDI sound, NoteOff/tuning/NoteOn ordering,
original velocities and channels, the real 70 ms timer, triad-derived pivots,
manual and aftertouch edits, RT off, removal of obsolete preferences, and scale
adaptation for both existing and fresh installations. UI notification tests
ensure keyboard highlights do not unnecessarily refresh the circles.

## Settings, MIDI and startup

`IntonaSettingsPage` loads the real QML page, verifies that the algorithm selector
and obsolete timing/history controls are absent, checks the note-name setting,
recording controls, responsive layouts and the Settings icon in software rendering.
Set `INTONA_TEST_SCREENSHOT_DIR` to save screenshots.

`IntonaMidiStartup` checks saved port/channel selection, reopening and reconnecting
by name with simulated devices. `IntonaMidiRecording` checks raw event recording.
`IntonaStartup` runs the real app with `--startup-check -platform offscreen`, using
temporary INI settings and unavailable port names so no MIDI hardware is opened.
Build Intona as well as the tests before running the full suite.

Held-note output tests verify the saved retrigger preference, identical adaptive
interpretations with retrigger disabled, and the two-second Settings probe:
real-time MTS bytes, selected channels, midpoint pitch change without a second
attack, final Note Off and exact table restoration. Cancellation, incoming notes,
output changes, shutdown and send failure paths use injected output only.

Assignable-control tests cover migration of aftertouch preferences, CC11 input delivery, source consumption/passthrough, pressure and bend thresholds, octave deduplication, polyphonic target selection, native sustain release when reserving a pedal, and preset navigation. See [MIDI controls](../docs/MidiControls.md).

The shared MIDI port selector regression starts with a saved port name and an empty list, then enumerates ports asynchronously. It checks the displayed name after enumeration, reordering, disappearance/reconnection, backend selection changes, and user selection followed by refresh. Model updates must never emit a port-selection command.
