#pragma once

#include "Config.hpp"
#include "Chords.h"

enum class KeepOldNotes : uint8_t { No, Yes };

extern const Config* FindConfig(Chord& chord, const std::vector<int8_t>& oldNotes, const NtetMapping& m, const Config& currentCfg, KeepOldNotes keep);
extern bool TestChordConfig(const Chord& chord, const Config& currentCfg);
extern ConfigMask ChordMask(const Chord& chord, const Config& currentCfg);
