#include "TuningMidiOutput.h"

#include "../CentsUtilities.hpp"
#include "IMidiOut.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace Intona::Tuning
{

void sendRpnCoarseFineTuning(
  IMidiOut& out,
  uint16_t channelMask,
  double cents)
{
  const int coarse = std::lround(cents / 100.0);
  const double fine = cents - coarse * 100.0;
  assert(fine >= -50.0 && fine <= 50.0);

  int fine14 = std::lround((fine + 64.0) * 128.0);
  fine14 = std::clamp(fine14, 0, 16383);

  const uint8_t fineMsb = fine14 / 128;
  const uint8_t fineLsb = fine14 % 128;

  for (uint8_t channel = 0; channel < 16; ++channel)
  {
    if ((channelMask & (uint16_t(1) << channel)) == 0)
      continue;

    out.sendShort(0xB0 | channel, 101, 0);
    out.sendShort(0xB0 | channel, 100, 2);
    out.sendShort(0xB0 | channel, 6, 64 + coarse);
    out.sendShort(0xB0 | channel, 100, 1);
    out.sendShort(0xB0 | channel, 6, fineMsb);
    out.sendShort(0xB0 | channel, 38, fineLsb);
    out.sendShort(0xB0 | channel, 101, 127);
    out.sendShort(0xB0 | channel, 100, 127);
  }
}

void sendTuningSysEx(
  IMidiOut& out,
  uint16_t channelMask,
  uint8_t edo,
  uint8_t fifthStep,
  const Config& config,
  double globalOffsetCents)
{
  uint8_t ff = 0;
  uint8_t gg = 0;
  uint8_t hh = 0;

  for (int channel = 0; channel < 16; ++channel)
  {
    if ((channelMask & (uint16_t(1) << channel)) == 0)
      continue;

    const int channelOneBased = channel + 1;
    if (channelOneBased <= 7)
      hh |= uint8_t(1u << (channelOneBased - 1));
    else if (channelOneBased <= 14)
      gg |= uint8_t(1u << (channelOneBased - 8));
    else
      ff |= uint8_t(1u << (channelOneBased - 15));
  }

  const auto table = computeMtsTable(
    edo, fifthStep, config, globalOffsetCents);

  std::vector<uint8_t> sysex;
  sysex.reserve(33);
  sysex.push_back(0xF0);
  sysex.push_back(0x7F);
  sysex.push_back(0x7F);
  sysex.push_back(0x08);
  sysex.push_back(0x09);
  sysex.push_back(ff & 0x03);
  sysex.push_back(gg & 0x7F);
  sysex.push_back(hh & 0x7F);

  for (const uint16_t value : table)
  {
    sysex.push_back(uint8_t((value >> 7) & 0x7F));
    sysex.push_back(uint8_t(value & 0x7F));
  }

  sysex.push_back(0xF7);
  out.sendSysEx(sysex);
}

void sendNoteOn(
  IMidiOut& out, uint8_t channel,
  uint8_t note, uint8_t velocity)
{
  out.sendShort(0x90 | (channel & 0x0F), note, velocity);
}

void sendNoteOff(
  IMidiOut& out, uint8_t channel,
  uint8_t note, uint8_t velocity)
{
  out.sendShort(0x80 | (channel & 0x0F), note, velocity);
}

void sendChannelMessage(
  IMidiOut& out, uint8_t channel,
  uint8_t code, uint8_t data1, uint8_t data2)
{
  out.sendShort(code | (channel & 0x0F), data1, data2);
}

void sendAllNotesOff(IMidiOut& out, uint8_t channel)
{
  out.sendShort(0xB0 | (channel & 0x0F), 123, 0);
}

} // namespace Intona::Tuning
