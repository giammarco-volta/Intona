Note naming regression tests
============================

Configure with `-DINTONA_BUILD_TESTS=ON`, build, then run
`ctest --test-dir <build-directory> --output-on-failure`.
On Windows, put the matching Qt `bin` directory on PATH before running tests.

The test executable uses a temporary INI settings directory and does not open
MIDI devices. It checks all supported EDOs and tuning centers, preservation of
pitch and interaction state, the two-accidental limit, nearest-anchor selection,
octave wrapping, tie breaking, repeated step modifiers, legacy spelling,
settings persistence, preset labels and the queued QML view model update.

For the selected twelve notes, an independent letter/semitone/octave oracle
verifies each interval relative to the simplified tuning center in every EDO.
Exact interval spelling may exceed two accidentals; all degrees inherit the
center's step modifier. Keyboard and circle labels must agree, and preset labels
must retain their own center context when the active tuning center changes.

A separate offscreen QML test loads the actual Settings page and checks its
selector and property binding. Set `INTONA_TEST_SCREENSHOT_DIR` to save desktop
and mobile renderings while running that test.

Keyboard mapping checks independently enumerate all twelve rotations for every
supported EDO and tuning center. They verify minimum absolute error, deterministic
ties, pitch order, MIDI detune limits and encoding, adaptive chord/leading-tone
recognition after remapping, held-key preservation during adaptive selection,
and loading presets saved with the previous keyboard assignments.

Adaptive timing tests use an injected recording MIDI output: no hardware is
enumerated or opened and MIDI preferences are not accessed. They independently
score major/minor interpretations for every center, exercise all six orders of
E-G#/Ab-B in 19-, 31-, 43- and 53-EDO within one timer window, and verify NoteOff ->
tuning -> NoteOn ordering, original velocities, channel selection, absence of
unnecessary retriggers, automatic pivots, RT off, persistence and queued settings.

All six release orders are crossed with the six press orders.
Every Note Off must preserve the mapping, center and inferred key and send only
the corresponding MIDI Note Off, with no tuning messages or other retriggers.
Only timer expiry can evaluate a group. Tests cover restarting the timer without
changing pivots, releasing pivots, dirty notes leaving no harmonic or melodic
trace, clean notes released during a pending window, threshold changes and zero,
cancellation on explicit context changes, destruction, and the actual worker
thread. Sound must remain immediate, with one evaluation per stable group.

Harmony tests independently enumerate spellings and roots for German/French
sixths, rootless minor sixths, diminished sevenths and augmented triads across
all transpositions at three reference centers in every supported EDO. They
verify previous-chord membership, ambiguous membership falling back to distance,
leading-tone preference, pivot precedence and inversion stability. Melodic tests
vary only center metadata for all 1126 mappings, then exercise matching preset
assignments with different centers through complete MIDI sequences. Additional
sequences verify the E–F#–G# correction and rejected chords leaving no context.
