#pragma once

#include "MidiMonoIn.h"

inline MidiIn_MonoInterpreter::Configuration
makeIntonaMidiInConfiguration()
{
  MidiIn_MonoInterpreter::Configuration configuration;

  // All CCs must reach the configurable action binding. Legacy forwarding
  // exclusions are applied by TuningController after binding dispatch.

  configuration.ignoreProgramChanges = true;
  configuration.noteOffStatePolicy =
    MidiIn_MonoInterpreter::NoteOffStatePolicy::ClearCurrentNote;

  return configuration;
}