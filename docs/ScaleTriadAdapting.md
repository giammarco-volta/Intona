# Scale and triad adaptation (v1)

`ScaleTriadAdapting` is the only adaptive tuning engine. RT Adapting on the tuning
surface enables or disables it; Settings has no algorithm selector. Its only
timing constant is 70 ms. Old algorithm and threshold preferences are removed
at startup. There is no general chord recognizer or inferred-key analysis. The tuning center
is shown only by its gold disc on the circle; the former labels and outer dots
have been removed.

## Decisions

Each incoming attack is evaluated before it is forwarded. The context contains
distinct keyboard classes: the new class, all other held classes, then the most
recently released distinct classes if fewer than two others are held. Octave
doublings and repeated attacks of a class never supply extra evidence.

The five masks are major, harmonic major, natural minor, melodic minor and
harmonic minor. An established/current center uses these scales at its tonic,
dominant and subdominant (15); provisional/alternative centers use only their own
five. During a transition the old established center still has 15 for recovery.
Candidate order is current, previous established center during transition, then
alternating +1/-1/+2/-2 fifths from current. Keyboard geometry preserves the eleven
common assignments between neighboring centers; it is not re-optimized in cents.

All major/minor keyboard triads found as subsets of the context must have their
actual major/minor spellings, alongside scale compatibility and fixed readings.
No distance score is used. If no candidate satisfies all triads, scales and fixed
readings, fall back to the scale-only decision. If no scale candidate exists,
keep the current tuning and wait for subsequent evidence.

## Acquisition and rollback

Compatible readings at an established center are acquired. Acquired held notes
younger than 70 ms may reopen as a triad completes; older acquired held notes are
pivots. Changed readings remain provisional, including after their physical
verification. Musical acquisition requires a supporting context containing two
distinct later classes and at least 70 ms age. Released provisional readings may
be revised while still in the local context; released acquired readings are fixed.

A change saves an interpretation snapshot and an independent deadline 70 ms
later. New notes do not restart this deadline. Verification excludes dirty notes
and held confirmations released before the deadline, and looks for replacement
evidence that already existed when the change occurred. A triad-only change must
also retain its triad justification. Losing proof restores the snapshot and
cancels dependent changes; later input events remain available as pending notes.
Note Off records evidence; it does not initiate a new harmonic search.

The core takes explicit times and never sees future releases. The controller
owns a precise Qt timer and sends Note Off / tuning / Note On for still-sounding
notes whose EDO pitch changes, preserving velocity and channel selection. Newly
arriving notes are forwarded once, after tuning; released notes are never
restarted. Manual context changes and toggling RT Adapting cancel outstanding deadlines.

`prune()` retains held notes, the newest released occurrence of each class,
recent releases and any notes referenced by a pending rollback or transition.
Thus the engine does not retain an ever-growing performance history.

## Validation and diagnosis

`IntonaScaleTriads` runs the 541 synthetic simulator-reference cases in
`tests/data/ScaleTriadCases.json` at four keyboard anchors. It compares every
attack decision and the final post-verification center, including chord attack
orders, brief overlaps, dirty supporting notes, ambiguous scale passages and
conflicting triads. Controller tests check actual message ordering and the Qt
rollback timer using injected MIDI output and isolated preferences.

The same executable accepts a JSON fixture path for full recorded performances.
Both complete Bach recordings were replayed, totaling 2,205 attacks (8,820 checks
at four anchors), with matching Python/C++ decisions. Recordings remain outside
the repository. The recorder still captures original input events upstream of
tuning; its context identifies `scales_triads_v1`.

A provisional spelling later corrected by new evidence is not an algorithmic
failure merely because it sounded before the evidence arrived. Diagnose whether
the later decision and any dependent interpretations are corrected, and whether
only still-held notes are retriggered. The core's `notes()`, `stableCenter()`,
`provisional()` and `nextDeadline()` expose the useful state for a replay/debugger.
