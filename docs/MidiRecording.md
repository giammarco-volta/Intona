# Recording a performance

In **Settings → Record a performance**, name the take (for example `Bach slow`), press **Start recording**, and play. Release all keys and the pedal before pressing **Stop recording**. Repeat with a different name for the fast take. Start with no keys held so every note has an attack and a release in the file.

Each take creates a separate `.jsonl` file under **Documents/Intona/Recordings** (the system Documents location, including redirection). Settings shows the full path; **Open folder** opens the directory. Copy the files from another device when necessary. Recording is off at startup, does not change tuning settings, and records MIDI rather than audio.

The log includes input note numbers and velocities, all note durations including those rejected by the tuning filter, equal timestamps for simultaneous events, and other messages forwarded by the selected input channel, including sustain pedal CC64 and pressure. The input configuration still suppresses program changes and some other CCs; other channels are not recorded.

Disk operations run on the GUI thread, outside the MIDI/tuning worker. Data is flushed every second, on Stop, and on normal application shutdown. File errors stop recording and appear in Settings. Abrupt termination may lose buffered data; a missing `session_end` identifies an incomplete take.

## JSON Lines schema 1

Each line is a JSON object. `sequence` orders the records. Types are `session_start`, `note_on`, `note_off`, `channel_message`, `context_snapshot`, and `session_end`.

Input fields:

- `midi_time_ms`: original unsigned 32-bit backend timestamp. **Use this for duration, attack intervals and overlap measurements**. On Windows it is relative to starting the MIDI input device; it may wrap at 2^32 milliseconds or restart if the device is reopened. Avoid changing MIDI connections during a take.
- `capture_ms`: monotonic elapsed time at the recording observer. GUI delivery can lag behind input; this is a diagnostic annotation, not a performance timing source.
- `status`: message kind without the channel nibble; `channel`: 1–16; `data1`, `data2`: original data.
- Note records also contain `note` (MIDI 0–127) and `velocity`. A zero-velocity Note On is classified as `note_off` while retaining its original status and data.

Session start includes UTC time, label and settings. Context snapshots include the MIDI source, EDO, center, twelve fifth-coordinate assignments, RT Adapting status, the scales/triads algorithm identifier and its 70 ms verification interval. These snapshots come from the coalesced UI state: they are not an exhaustive or precisely synchronized trace of tuning decisions. Session end includes the number of input events.

Pair notes by channel and MIDI note, preserve event order for equal timestamps, and report unmatched releases, unfinished notes and repeated attacks explicitly. Pedal events distinguish physical key release from sustained sound. The recorder preserves raw evidence rather than classifying notes as dirty or overlaps as intentional.

`IntonaMidiRecording` tests short notes, equal timestamps, overlaps, velocity-zero releases, pedal timestamps, wrap preservation, separate takes, shutdown and file errors. `IntonaSettingsPage` exercises Start/Stop using a temporary directory.

The context also records `retrigger_held_notes`. The Settings instrument test sends output directly and does not create recorded input events.

Assignable-control context includes `control_source`, `control_action`, `control_enabled`, and `control_threshold` (see [MIDI controls](MidiControls.md)). All CCs now reach recording before the action-aware output routing, including CC11 expression previously discarded by the input interpreter.
