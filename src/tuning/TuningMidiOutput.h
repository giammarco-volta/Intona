#pragma once

#include "../Config.hpp"

#include <cstdint>

class IMidiOut;

namespace Intona::Tuning
{

void sendRpnCoarseFineTuning(
  IMidiOut& out,
  uint16_t channelMask,
  double cents);

void sendTuningSysEx(
  IMidiOut& out,
  uint16_t channelMask,
  uint8_t edo,
  uint8_t fifthStep,
  const Config& config,
  double globalOffsetCents);

void sendNoteOn(
  IMidiOut& out, uint8_t channel,
  uint8_t note, uint8_t velocity);

void sendNoteOff(
  IMidiOut& out, uint8_t channel,
  uint8_t note, uint8_t velocity);

void sendChannelMessage(
  IMidiOut& out, uint8_t channel,
  uint8_t code, uint8_t data1, uint8_t data2);

void sendAllNotesOff(IMidiOut& out, uint8_t channel);

} // namespace Intona::Tuning
