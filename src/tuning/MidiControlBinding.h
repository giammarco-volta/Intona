#pragma once
#include <array>
#include <algorithm>

namespace Intona::Tuning {
// Sources: channel pressure, poly pressure, positive/negative bend, then CC 0..119.
// Channel-mode CCs (120..127) are deliberately not assignable.
struct MidiControlBinding {
  int source = 0;
  int action = 0; // Step up/down, next/previous preset.
  int threshold = 10;
  bool enabled = true;
  std::array<int, 124> values{};
  std::array<int, 128> polyValues{};
  std::array<bool, 128> polyArmed{};
  bool armed = true;

  MidiControlBinding() { polyArmed.fill(true); }
  int releaseThreshold() const { return threshold / 2; }
  void rearm() {
    armed = values[source] <= releaseThreshold();
    for (int n = 0; n < 128; ++n) polyArmed[n] = polyValues[n] <= releaseThreshold();
  }
  void clearInput() {
    values.fill(0); polyValues.fill(0); polyArmed.fill(true); armed = true;
  }
  void releaseNote(int note) {
    if (note >= 0 && note < 128) { polyValues[note] = 0; polyArmed[note] = true; }
  }
  bool matches(int code, int data1) const {
    return source == 0 ? code == 0xd0 : source == 1 ? code == 0xa0
      : source < 4 ? code == 0xe0 : code == 0xb0 && data1 == source - 4;
  }
  bool companion(int code, int data1) const {
    // Consume the fine-resolution half too when an MSB controller is reserved.
    return source >= 4 && source < 36 && code == 0xb0 && data1 == source - 4 + 32;
  }
  bool update(int code, int data1, int data2) {
    if (data1 < 0 || data1 > 127 || data2 < 0 || data2 > 127) return false;
    if (code == 0xd0) values[0] = data1;
    else if (code == 0xa0) polyValues[data1] = data2;
    else if (code == 0xb0 && data1 < 120) values[4 + data1] = data2;
    else if (code == 0xe0) {
      const int bend = data1 + 128 * data2 - 8192;
      values[2] = std::max(0, bend) * 127 / 8191;
      values[3] = std::max(0, -bend) * 127 / 8192;
    }
    if (!matches(code, data1)) return false;
    auto& ready = source == 1 ? polyArmed[data1] : armed;
    const int value = source == 1 ? polyValues[data1] : values[source];
    if (value <= releaseThreshold()) ready = true;
    if (value < threshold || !ready) return false;
    ready = false;
    return enabled;
  }
};
} // namespace Intona::Tuning
