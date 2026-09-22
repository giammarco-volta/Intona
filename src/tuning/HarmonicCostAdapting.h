#pragma once

#include "../Config.hpp"
#include <optional>
#include <functional>
#include <map>

namespace Intona::Tuning {

struct HarmonicWeight { int value; double weight; };
struct HarmonicCostResult { Config config; double cost = 0; int retunedKeys = 0; };

// The keyboard anchor is independent of the pitch-class representative of the
// center. One fifth replaces one assignment; twelve fifths translate all values.
std::optional<int> relativeKeyboardAnchor(const Config& config);
Config relativeKeyboardConfig(int center, int anchor);
double harmonicDistance(int a, int b);
// History is compared only with new notes. Internal coherence includes their
// union with validated notes still held; overlapping keys count just once.
double harmonicGroupCost(const Config& config, uint16_t group,
  const std::vector<HarmonicWeight>& history, uint16_t held = 0);
HarmonicCostResult chooseHarmonicCostConfig(const Config& current, int anchor,
  const NtetMapping& edo, uint16_t group, uint16_t pivots,
  const std::vector<HarmonicWeight>& history,
  const std::function<bool(const Config&)>& admissible = {}, uint16_t held = 0);

struct HarmonicHistoryNote {
  uint64_t id;
  int key;
  int value;
  double start;
  std::optional<double> end;
};

class HarmonicHistory {
public:
  static constexpr double defaultIntervalMs = 250;
  static constexpr int defaultWindowIntervals = 16;
  static constexpr int maxWindowIntervals = 128;
  static constexpr double silenceResetMs = 10000;
  void reset();
  void setWindowIntervals(int intervals);
  int windowIntervals() const { return windowIntervals_; }
  void release(uint64_t id, double time);
  void retune(uint64_t id, int value, double time);
  // Called only for an admitted batch; dirty attacks never affect the clock.
  void beginGroup(double attack, uint16_t keys);
  void admit(const HarmonicHistoryNote& note);
  std::vector<HarmonicWeight> weights(double now, double slope);
  double intervalMs() const { return interval_; }
  double windowMs() const { return windowIntervals_ * interval_; }
  size_t size() const { return notes_.size(); }
private:
  std::vector<HarmonicHistoryNote> notes_;
  double interval_ = defaultIntervalMs;
  int windowIntervals_ = defaultWindowIntervals;
  std::optional<double> lastAttack_;
  std::optional<double> lastRelease_;
  uint16_t lastGroup_ = 0;
  bool estimated_ = false;
};

} // namespace Intona::Tuning
