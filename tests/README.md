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
