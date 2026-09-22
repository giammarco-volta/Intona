#include "HarmonicCostAdapting.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>

namespace Intona::Tuning {

std::optional<int> relativeKeyboardAnchor(const Config& config)
{
  if (config.tuningCenter == Config::invalid) return std::nullopt;
  for (int anchor = 0; anchor < 12; ++anchor)
    if (relativeKeyboardConfig(config.tuningCenter, anchor).valueForKey == config.valueForKey)
      return anchor;
  return std::nullopt;
}

Config relativeKeyboardConfig(int center, int anchor)
{
  Config result{};
  result.tuningCenter = center;
  result.relativeKeyboard = true;
  for (int i = -5; i <= 6; ++i)
  {
    const int value = center + i;
    result.valueForKey[mod(anchor + 7 * mod(value, 12), 12)] = value;
    if (value >= kConfigMaskMin && value <= kConfigMaskMax)
      result.mask |= valueToPoolBit(value);
  }
  return result;
}

double harmonicDistance(int a, int b)
{
  return std::abs(double(a) - double(b));
}

double harmonicGroupCost(const Config& config, uint16_t group,
  const std::vector<HarmonicWeight>& history, uint16_t held)
{
  double external = 0, internal = 0;
  if (!group) return 0;
  const uint16_t coherenceGroup = group | held;
  for (int key = 0; key < 12; ++key) if (hasKey12(group, key))
  {
    const int value = config.valueForKey[key];
    for (const auto& old : history)
      external += old.weight * harmonicDistance(value, old.value);
  }
  for (int key = 0; key < 12; ++key) if (hasKey12(coherenceGroup, key))
    for (int other = key + 1; other < 12; ++other) if (hasKey12(coherenceGroup, other))
      internal += harmonicDistance(config.valueForKey[key], config.valueForKey[other]);
  return external + internal;
}

HarmonicCostResult chooseHarmonicCostConfig(const Config& current, int anchor,
  const NtetMapping& edo, uint16_t group, uint16_t pivots,
  const std::vector<HarmonicWeight>& history,
  const std::function<bool(const Config&)>& admissible, uint16_t held)
{
  HarmonicCostResult best{current, harmonicGroupCost(current, group, history, held), 0};
  if (!group) return best;
  const uint16_t coherenceGroup = group | held;
  double mass = 0;
  int lowest = current.tuningCenter, highest = current.tuningCenter;
  for (const auto& note : history) if (note.weight > 0)
  { mass += note.weight; lowest = std::min(lowest, note.value); highest = std::max(highest, note.value); }

  // Outside [lowest-6, highest+5], every new value is on the same side
  // of all historical notes. Even ignoring internal costs, the
  // external cost exceeds best once the extra distance exceeds best/mass/m.
  // This is an admissible bound, not the invalid first-worsening heuristic.
  int low, high;
  if (mass > 0)
  {
    const double radius = std::ceil(best.cost / (mass * popcount(group))) + 1;
    low = static_cast<int>(std::max(double(std::numeric_limits<int>::min()) + 1024,
      double(lowest) - 6 - radius));
    high = static_cast<int>(std::min(double(std::numeric_limits<int>::max()) - 1024,
      double(highest) + 5 + radius));
    // A second bound remains small even when the last historical weight is
    // nearly zero. Beyond this corridor an lcm(N,12) translation toward the
    // history strictly improves the external score, keeps internal intervals
    // and physical pitches/pivots.
    const int period = std::lcm(int(edo.N), 12);
    low = std::max(low, lowest - period - 20);
    high = std::min(high, highest + period + 20);
  }
  else
  {
    // With no history the score is 12-periodic. Actual pitches, retune counts
    // and pivot constraints repeat after lcm(N,12); one period each way also
    // contains a nearest representative of every tied state.
    const int period = std::lcm(int(edo.N), 12);
    low = current.tuningCenter - period;
    high = current.tuningCenter + period;
  }
  std::map<std::vector<int>, double> costCache;
  const auto consider = [&](int center)
  {
    Config candidate = relativeKeyboardConfig(center, anchor);
    bool changedGroup = false;
    int retuned = 0;
    std::vector<int> signature;
    for (int key = 0; key < 12; ++key)
    {
      const bool changedPitch = mod(candidate.valueForKey[key] - current.valueForKey[key], edo.N) != 0;
      if (changedPitch && hasKey12(pivots, key)) return;
      retuned += changedPitch;
      if (hasKey12(group, key))
        changedGroup |= candidate.valueForKey[key] != current.valueForKey[key];
      // Held assignments also affect internal coherence, so equal new-note
      // interpretations alone are no longer enough to share a cached score.
      if (hasKey12(coherenceGroup, key))
        signature.push_back(candidate.valueForKey[key]);
    }
    if (!changedGroup || (admissible && !admissible(candidate))) return;
    auto cached = costCache.find(signature);
    const double cost = cached == costCache.end()
      ? costCache.emplace(signature, harmonicGroupCost(candidate, group, history, held)).first->second
      : cached->second;
    const double tolerance = 1e-10 * std::max({1.0, std::abs(best.cost), std::abs(cost)});
    // Visit order is the final tie-break: +1,-1,+2,-2,... from the anchor state.
    if (cost < best.cost - tolerance || (std::abs(cost - best.cost) <= tolerance && retuned < best.retunedKeys))
      best = {candidate, cost, retuned};
  };
  const int64_t extent = std::max(int64_t(high) - current.tuningCenter, int64_t(current.tuningCenter) - low);
  for (int64_t distance = 1; distance <= extent; ++distance)
  {
    const int64_t up = int64_t(current.tuningCenter) + distance;
    const int64_t down = int64_t(current.tuningCenter) - distance;
    if (up <= high) consider(static_cast<int>(up));
    if (down >= low) consider(static_cast<int>(down));
  }
  return best;
}

void HarmonicHistory::reset()
{
  notes_.clear(); interval_ = defaultIntervalMs;
  lastAttack_.reset(); lastRelease_.reset(); lastGroup_ = 0; estimated_ = false;
}

void HarmonicHistory::setWindowIntervals(int intervals)
{
  if (intervals >= 1 && intervals <= maxWindowIntervals)
    windowIntervals_ = intervals;
}

void HarmonicHistory::release(uint64_t id, double time)
{
  bool found = false;
  for (auto& note : notes_) if (note.id == id && !note.end)
  { note.end = std::max(time, note.start); found = true; }
  if (found && std::none_of(notes_.begin(), notes_.end(), [](const auto& n) { return !n.end; }))
    lastRelease_ = time;
}

void HarmonicHistory::retune(uint64_t id, int value, double time)
{
  for (auto& note : notes_) if (note.id == id && !note.end && note.value != value)
  {
    const auto next = HarmonicHistoryNote{id, note.key, value, time, std::nullopt};
    note.end = time;
    notes_.push_back(next);
    return;
  }
}

void HarmonicHistory::beginGroup(double attack, uint16_t keys)
{
  if (lastRelease_ && attack - *lastRelease_ > silenceResetMs
    && std::none_of(notes_.begin(), notes_.end(), [](const auto& n) { return !n.end; })) reset();
  // Rearticulations do not create extra clock ticks. Their actual sound
  // durations are still integrated into history by admit()/release().
  if (lastAttack_ && (keys & ~lastGroup_) == 0) return;
  if (lastAttack_ && attack > *lastAttack_)
  {
    const double interval = attack - *lastAttack_;
    if (!estimated_) { interval_ = interval; estimated_ = true; }
    else interval_ = .75 * interval_ + .25 * std::clamp(interval, interval_ / 4, interval_ * 4);
  }
  lastAttack_ = attack;
  lastGroup_ = keys;
}

void HarmonicHistory::admit(const HarmonicHistoryNote& note)
{
  notes_.push_back(note);
  if (note.end && std::none_of(notes_.begin(), notes_.end(), [](const auto& n) { return !n.end; }))
    lastRelease_ = lastRelease_ ? std::max(*lastRelease_, *note.end) : *note.end;
}

std::vector<HarmonicWeight> HarmonicHistory::weights(double now, double slope)
{
  const double window = windowMs();
  notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [=](const auto& n) {
    return n.end && *n.end <= now - window;
  }), notes_.end());
  // Union overlapping instances of the same interpreted pitch class, so
  // octave doublings do not multiply its harmonic evidence.
  std::map<int, std::vector<std::pair<double,double>>> intervals;
  for (const auto& note : notes_)
  {
    const double start = std::max(note.start, now - window);
    const double end = std::min(note.end.value_or(now), now);
    if (end > start) intervals[note.value].push_back({start,end});
  }
  std::vector<HarmonicWeight> result;
  for (auto& [value, ranges] : intervals)
  {
    std::sort(ranges.begin(), ranges.end());
    double weight = 0, start = ranges.front().first, end = ranges.front().second;
    const auto integral = [&](double a, double b) {
      return window / ((slope + 1) * interval_) *
        (std::pow(std::clamp(1-(now-b)/window,0.0,1.0), slope+1)
        - std::pow(std::clamp(1-(now-a)/window,0.0,1.0), slope+1));
    };
    for (size_t i = 1; i < ranges.size(); ++i)
      if (ranges[i].first <= end) end = std::max(end, ranges[i].second);
      else { weight += integral(start,end); start=ranges[i].first; end=ranges[i].second; }
    weight += integral(start,end);
    if (weight > 0) result.push_back({value,weight});
  }
  return result;
}

} // namespace Intona::Tuning
