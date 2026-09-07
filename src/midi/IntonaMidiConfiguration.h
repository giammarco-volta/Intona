#pragma once

#include "MidiMonoIn.h"

inline MidiIn_MonoInterpreter::Configuration
makeIntonaMidiInConfiguration()
{
  MidiIn_MonoInterpreter::Configuration configuration;

  configuration.ignoredControlChanges.set(0);
  configuration.ignoredControlChanges.set(7);
  configuration.ignoredControlChanges.set(10);
  configuration.ignoredControlChanges.set(11);
  configuration.ignoredControlChanges.set(32);
  configuration.ignoredControlChanges.set(71);
  configuration.ignoredControlChanges.set(74);

  configuration.ignoreProgramChanges = true;
  configuration.noteOffStatePolicy =
    MidiIn_MonoInterpreter::NoteOffStatePolicy::ClearCurrentNote;

  return configuration;
}