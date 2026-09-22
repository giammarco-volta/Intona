#include "ScaleTriadAdapting.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Intona::Tuning
{
namespace
{
constexpr std::array<uint16_t, 5> baseScales = {0b101010110101, 0b100110110101, 0b010110101101,
                                                0b101010101101, 0b100110101101};
uint16_t rotate(uint16_t mask, int steps)
{
  steps = mod(steps, 12);
  return uint16_t(((unsigned(mask) << steps) | (unsigned(mask) >> (12 - steps))) & 0xfff);
}
struct Triad
{
  int root, third, fifth, thirdFifths;
};
std::vector<Triad> triads(uint16_t keys)
{
  std::vector<Triad> result;
  for (int r = 0; r < 12; ++r)
    for (int third : {4, 3})
    {
      const int t = (r + third) % 12, f = (r + 7) % 12;
      if ((keys & ((1 << r) | (1 << t) | (1 << f))) == ((1 << r) | (1 << t) | (1 << f)))
        result.push_back({r, t, f, third == 4 ? 4 : -3});
    }
  return result;
}
bool triadsFit(const Config &cfg, const std::vector<Triad> &ts)
{
  for (const auto &t : ts)
    if (cfg.valueForKey[t.third] - cfg.valueForKey[t.root] != t.thirdFifths ||
        cfg.valueForKey[t.fifth] - cfg.valueForKey[t.root] != 1)
      return false;
  return true;
}
} // namespace

void ScaleTriadAdapting::reset(const Config &cfg)
{
  initial_ = cfg;
  const auto anchor = relativeKeyboardAnchor(cfg);
  valid_ = anchor.has_value();
  anchor_ = anchor.value_or(0);
  center_ = stable_ = valid_ ? cfg.tuningCenter : 0;
  time_ = 0;
  episode_.reset();
  notes_.clear();
  transactions_.clear();
  triadPending_.clear();
  triads_.clear();
}
Config ScaleTriadAdapting::config() const
{
  return valid_ ? relativeKeyboardConfig(center_, anchor_) : initial_;
}
void ScaleTriadAdapting::seed(Id id, int key, double time)
{
  Note n;
  n.id = id;
  n.key = key;
  n.on = time;
  n.reading = {Status::Confirmed, config().valueForKey[key], {}};
  notes_[id] = n;
}
void ScaleTriadAdapting::noteOn(Id id, int key, double time)
{
  time_ = time;
  // A new gesture reuses the surviving part of a previously sounding triad.
  // Acquire it before evaluating the new note, never on the release itself.
  acquireTriadPivots();
  Note n;
  n.id = id;
  n.key = key;
  n.on = time;
  n.reading.value = config().valueForKey[key];
  notes_[id] = n;
  if (valid_)
    evaluate(id);
  rememberTriads();
  prune();
}
void ScaleTriadAdapting::noteOff(Id id, double time)
{
  time_ = time;
  auto it = notes_.find(id);
  if (it == notes_.end())
    return;
  auto &n = it->second;
  n.off = time;
  if (time - n.on < verificationMs)
  {
    n.reading.status = Status::Dirty;
    std::set<Id> invalid{id};
    bool changed = true;
    while (changed)
    {
      changed = false;
      for (auto &[i, note] : notes_)
        if (note.reading.status == Status::Confirmed &&
            std::any_of(note.reading.supports.begin(), note.reading.supports.end(),
                        [&](Id j) { return invalid.count(j) != 0; }))
        {
          note.reading.status = Status::Pending;
          invalid.insert(i);
          changed = true;
        }
    }
  }
  rememberTriads();
  prune();
}
ScaleTriadAdapting::Ids ScaleTriadAdapting::context(Id trigger, const std::set<Id> *allowed,
                                                    const std::set<Id> &excluded,
                                                    const std::set<Id> *held) const
{
  Ids sounding, released;
  for (const auto &[id, n] : notes_)
  {
    if (id == trigger || excluded.count(id) || n.reading.status == Status::Dirty ||
        (allowed && !allowed->count(id)))
      continue;
    if (held ? held->count(id) != 0 : !n.off)
      sounding.push_back(id);
    else
      released.push_back(id);
  }
  std::sort(sounding.begin(), sounding.end(), [&](Id a, Id b) {
    return std::make_pair(notes_.at(a).on, a) > std::make_pair(notes_.at(b).on, b);
  });
  std::sort(released.begin(), released.end(), [&](Id a, Id b) {
    return std::make_pair(notes_.at(a).off.value_or(-1), a) >
           std::make_pair(notes_.at(b).off.value_or(-1), b);
  });
  uint16_t seen = 1 << notes_.at(trigger).key;
  Ids result;
  const auto add = [&](Id id) {
    int key = notes_.at(id).key;
    if (!(seen & (1 << key)))
    {
      seen |= 1 << key;
      result.push_back(id);
    }
  };
  for (Id i : sounding)
    add(i);
  for (Id i : released)
  {
    if (result.size() >= 2)
      break;
    add(i);
  }
  return result;
}
bool ScaleTriadAdapting::matches(int c, const Ids &cohort, int count,
                                 const std::map<Id, int> *fixed, bool pivots) const
{
  const auto cfg = relativeKeyboardConfig(c, anchor_);
  if (admissible && !admissible(cfg))
    return false;
  if (pivots)
    for (const auto &[id, n] : notes_)
      if (!n.off && n.reading.status == Status::Confirmed &&
          cfg.valueForKey[n.key] != n.reading.value)
        return false;
  uint16_t mask = 0;
  for (Id i : cohort)
  {
    const auto &n = notes_.at(i);
    mask |= 1 << n.key;
    if (fixed)
    {
      auto it = fixed->find(i);
      if (it != fixed->end() && it->second != cfg.valueForKey[n.key])
        return false;
    }
    else if (n.reading.status == Status::Confirmed && n.reading.value != cfg.valueForKey[n.key])
      return false;
  }
  if (popcount(mask) < 3)
    return false;
  for (int j = 0; j < count; ++j)
  {
    const int offset = j < 5 ? 0 : j < 10 ? 7 : 5;
    auto scale = rotate(baseScales[j % 5], anchor_ + 7 * mod(c, 12) + offset);
    if ((mask & scale) == mask)
      return true;
  }
  return false;
}
ScaleTriadAdapting::Selection ScaleTriadAdapting::select(const Ids &cohort)
{
  std::vector<std::pair<int, int>> candidates{{center_, episode_ ? 5 : 15}};
  if (episode_ && stable_ != center_)
    candidates.push_back({stable_, 15});
  std::vector<int> fixed;
  for (const auto &[i, n] : notes_)
    if (!n.off && n.reading.status == Status::Confirmed)
      fixed.push_back(n.reading.value);
  for (Id i : cohort)
    if (notes_.at(i).reading.status == Status::Confirmed)
      fixed.push_back(notes_.at(i).reading.value);
  int lo = center_ - 6, hi = center_ + 6;
  if (!fixed.empty())
  {
    lo = *std::max_element(fixed.begin(), fixed.end()) - 6;
    hi = *std::min_element(fixed.begin(), fixed.end()) + 5;
  }
  for (int d = 1; d <= std::max(std::abs(lo - center_), std::abs(hi - center_)); ++d)
    for (int c : {center_ + d, center_ - d})
      if (c >= lo && c <= hi &&
          std::none_of(candidates.begin(), candidates.end(), [&](auto p) { return p.first == c; }))
        candidates.push_back({c, 5});
  uint16_t mask = 0;
  for (Id i : cohort)
    mask |= 1 << notes_.at(i).key;
  auto ts = triads(mask);
  Selection result;
  for (auto [c, count] : candidates)
    if (matches(c, cohort, count))
    {
      if (!result.baseChoice)
        result.baseChoice = c;
      if (!result.center && triadsFit(relativeKeyboardConfig(c, anchor_), ts))
        result.center = c;
    }
  if (!result.center)
    result.center = result.baseChoice; // Conflicting constraints: scale-only fallback.
  result.triadOverride = result.center && result.center != result.baseChoice;
  if (result.triadOverride)
    for (Id i : cohort)
    {
      auto &n = notes_.at(i);
      if (n.reading.status == Status::Pending &&
          std::any_of(ts.begin(), ts.end(), [&](const auto &t) {
            return n.key == t.root || n.key == t.third || n.key == t.fifth;
          }))
      {
        triadPending_.insert(i);
        n.triadSeen = true;
      }
    }
  return result;
}
ScaleTriadAdapting::Snapshot ScaleTriadAdapting::snapshot() const
{
  Snapshot s{center_, stable_, episode_, {}, triadPending_, triads_};
  for (const auto &[i, n] : notes_)
    s.readings[i] = n.reading;
  return s;
}
bool ScaleTriadAdapting::mature(const Note &n, const Ids &cohort) const
{
  if (time_ - n.on < verificationMs)
    return false;
  std::set<int> later;
  for (Id i : cohort)
  {
    const auto &o = notes_.at(i);
    if (o.key != n.key && std::make_pair(o.on, i) > std::make_pair(n.on, n.id))
      later.insert(o.key);
  }
  return later.size() >= 2;
}
void ScaleTriadAdapting::accept(const Ids &cohort, int c)
{
  const auto cfg = relativeKeyboardConfig(c, anchor_);
  for (Id i : cohort)
  {
    auto &n = notes_.at(i);
    if (n.reading.status == Status::Pending)
    {
      n.reading.value = cfg.valueForKey[n.key];
      n.reading.supports = cohort;
      n.reading.supports.erase(std::remove(n.reading.supports.begin(), n.reading.supports.end(), i),
                               n.reading.supports.end());
    }
  }
  if (episode_)
  {
    auto &n = notes_.at(episode_->anchor);
    if (std::any_of(cohort.begin(), cohort.end(),
                    [&](Id i) { return notes_.at(i).key == n.key; }) &&
        mature(n, cohort))
    {
      if (n.reading.status != Status::Dirty)
      {
        n.reading.value = cfg.valueForKey[n.key];
        n.reading.status = Status::Confirmed;
      }
      stable_ = c;
      episode_.reset();
    }
  }
  for (Id i : cohort)
  {
    auto &n = notes_.at(i);
    if (n.reading.status == Status::Pending && (mature(n, cohort) || !episode_))
      n.reading.status = Status::Confirmed;
  }
  // Octave doubling shares a reading, never adds an independent confirmation.
  for (Id i : cohort)
  {
    const auto &proof = notes_.at(i);
    if (proof.reading.status != Status::Confirmed)
      continue;
    for (auto &[j, n] : notes_)
      if (n.reading.status == Status::Pending && n.key == proof.key &&
          std::max(n.on, proof.on) <
              std::min(n.off.value_or(time_ + 1), proof.off.value_or(time_ + 1)))
        n.reading = proof.reading;
  }
  for (Id i : cohort)
    if (triadPending_.count(i))
    {
      auto &n = notes_.at(i);
      if (mature(n, cohort))
        triadPending_.erase(i);
      else
        n.reading.status = Status::Pending;
    }
}
void ScaleTriadAdapting::retainPending()
{
  for (Id i : triadPending_)
    if (notes_.count(i) && notes_.at(i).reading.status != Status::Dirty)
      notes_.at(i).reading.status = Status::Pending;
}
void ScaleTriadAdapting::rememberTriads()
{
  const auto cfg = config();
  std::array<std::optional<Id>, 12> held{};
  uint16_t mask = 0;
  for (const auto &[i, n] : notes_)
    if (!n.off && n.reading.status != Status::Dirty)
    {
      auto &oldest = held[n.key];
      if (!oldest || std::make_pair(n.on, i) <
                         std::make_pair(notes_.at(*oldest).on, *oldest))
        oldest = i;
      mask |= 1 << n.key;
    }
  // Releasing an octave doubling does not break a still-sounding triad.
  for (auto &t : triads_)
    for (size_t j = 0; j < t.notes.size(); ++j)
    {
      const auto &n = notes_.at(t.notes[j]);
      if (n.off && held[n.key] && cfg.valueForKey[n.key] == t.values[j])
      {
        t.notes[j] = *held[n.key];
        t.since = std::max(t.since, notes_.at(t.notes[j]).on);
      }
    }
  // Keep a released triad only if its exact reading sounded together for 70ms
  // and at least one of those same attacks is still held with that reading.
  triads_.erase(std::remove_if(triads_.begin(), triads_.end(), [&](const auto &t) {
                  double end = time_;
                  bool held = false;
                  for (size_t j = 0; j < t.notes.size(); ++j)
                  {
                    const auto &n = notes_.at(t.notes[j]);
                    if (n.reading.status == Status::Dirty)
                      return true;
                    if (n.off)
                      end = std::min(end, *n.off);
                    else
                    {
                      held = true;
                      if (cfg.valueForKey[n.key] != t.values[j])
                        return true;
                    }
                  }
                  const bool released = std::any_of(t.notes.begin(), t.notes.end(),
                      [&](Id i) { return notes_.at(i).off.has_value(); });
                  return !held || (released && end - t.since < verificationMs);
                }), triads_.end());

  // Use the oldest held occurrence of each class: octave doublings neither
  // multiply the evidence nor let a restruck key inherit the old attack.
  for (const auto &t : triads(mask))
    if (triadsFit(cfg, {t}))
    {
      TriadEvidence evidence{{*held[t.root], *held[t.third], *held[t.fifth]},
                             {cfg.valueForKey[t.root], cfg.valueForKey[t.third],
                              cfg.valueForKey[t.fifth]}, time_};
      if (std::none_of(triads_.begin(), triads_.end(), [&](const auto &old) {
            return old.notes == evidence.notes && old.values == evidence.values;
          }))
        triads_.push_back(evidence);
    }
}
void ScaleTriadAdapting::acquireTriadPivots()
{
  rememberTriads();
  for (const auto &t : triads_)
  {
    if (std::none_of(t.notes.begin(), t.notes.end(),
                     [&](Id i) { return notes_.at(i).off.has_value(); }))
      continue; // A fully held triad can still be reinterpreted by new evidence.
    for (size_t j = 0; j < t.notes.size(); ++j)
    {
      auto &n = notes_.at(t.notes[j]);
      if (!n.off)
      {
        n.reading = {Status::Confirmed, t.values[j], {}};
        triadPending_.erase(n.id);
      }
    }
  }
  triads_.erase(std::remove_if(triads_.begin(), triads_.end(), [&](const auto &t) {
                  return std::any_of(t.notes.begin(), t.notes.end(),
                      [&](Id i) { return notes_.at(i).off.has_value(); });
                }), triads_.end());
}
void ScaleTriadAdapting::evaluate(Id trigger)
{
  Ids cohort{trigger};
  auto others = context(trigger);
  cohort.insert(cohort.end(), others.begin(), others.end());
  if (cohort.size() < 3)
    return;
  uint16_t mask = 0;
  for (Id i : cohort)
    mask |= 1 << notes_.at(i).key;
  std::map<Id, int> originals;
  if (!triads(mask).empty())
    for (auto &[i, n] : notes_)
      if (n.reading.status == Status::Confirmed && !n.off && time_ - n.on < verificationMs)
      {
        originals[i] = n.reading.value;
        n.reading.status = Status::Pending;
      }
  auto choice = select(cohort);
  if (choice.center)
  {
    int c = *choice.center, previous = center_;
    auto before = snapshot();
    if (c != center_)
    {
      center_ = c;
      if (c == stable_)
        episode_.reset();
      else if (!episode_)
        episode_ = Episode{trigger};
    }
    accept(cohort, c);
    if (c != previous)
    {
      Transaction tx{time_ + verificationMs,
                     trigger,
                     c,
                     c == before.stable && before.episode ? 15 : 5,
                     before,
                     {},
                     {},
                     choice.triadOverride,
                     choice.baseChoice};
      for (const auto &[i, n] : notes_)
      {
        if (!n.off)
          tx.held.insert(i);
        tx.values[i] = config().valueForKey[n.key];
      }
      transactions_.push_back(std::move(tx));
    }
  }
  for (auto [i, v] : originals)
  {
    auto &n = notes_.at(i);
    if (n.reading.status == Status::Pending && n.reading.value == v)
      n.reading.status = Status::Confirmed;
  }
  retainPending();
}
std::optional<double> ScaleTriadAdapting::nextDeadline() const
{
  if (transactions_.empty())
    return std::nullopt;
  return transactions_.front().deadline;
}
void ScaleTriadAdapting::advance(double time)
{
  while (!transactions_.empty() && transactions_.front().deadline <= time)
  {
    time_ = transactions_.front().deadline;
    checkFirst();
  }
  time_ = time;
  rememberTriads();
  prune();
}
void ScaleTriadAdapting::checkFirst()
{
  auto tx = transactions_.front();
  std::set<Id> allowed, invalid;
  for (const auto &[i, r] : tx.before.readings)
  {
    allowed.insert(i);
    const auto &n = notes_.at(i);
    if (n.reading.status == Status::Dirty || (tx.held.count(i) && n.off && *n.off < tx.deadline))
      invalid.insert(i);
  }
  Ids cohort{tx.trigger};
  auto other = context(tx.trigger, &allowed, invalid, &tx.held);
  cohort.insert(cohort.end(), other.begin(), other.end());
  bool valid =
      !invalid.count(tx.trigger) && matches(tx.center, cohort, tx.count, &tx.values, false);
  if (valid && tx.triadProof)
  {
    uint16_t mask = 0;
    for (Id i : cohort)
      mask |= 1 << notes_.at(i).key;
    const auto ts = triads(mask);
    valid = !ts.empty() && triadsFit(relativeKeyboardConfig(tx.center, anchor_), ts) &&
            (!tx.baseChoice || !triadsFit(relativeKeyboardConfig(*tx.baseChoice, anchor_), ts));
  }
  if (valid)
  {
    transactions_.erase(transactions_.begin());
    return;
  }
  center_ = tx.before.center;
  stable_ = tx.before.stable;
  episode_ = tx.before.episode;
  triadPending_ = tx.before.triadPending;
  triads_ = tx.before.triads;
  for (auto &[i, n] : notes_)
  {
    if (n.reading.status == Status::Dirty)
      continue;
    auto old = tx.before.readings.find(i);
    if (old != tx.before.readings.end())
      n.reading = old->second;
    else
    {
      n.reading = {Status::Pending, config().valueForKey[n.key], {}};
      if (n.triadSeen)
        triadPending_.insert(i);
    }
  }
  transactions_.clear();
  retainPending();
}
void ScaleTriadAdapting::prune()
{
  // Older released occurrences of a class cannot enter the recent context.
  // Keep all notes needed by a pending rollback, and recent releases that could
  // be its fallback evidence. No unbounded performance history is retained.
  std::set<Id> keep;
  std::array<std::optional<Id>, 12> latest{};
  for (const auto &[i, n] : notes_)
  {
    if (!n.off || time_ - *n.off <= verificationMs)
      keep.insert(i);
    if (n.off && n.reading.status != Status::Dirty)
    {
      auto &last = latest[n.key];
      if (!last || std::make_pair(*n.off, i) > std::make_pair(*notes_.at(*last).off, *last))
        last = i;
    }
  }
  for (auto id : latest)
    if (id)
      keep.insert(*id);
  if (episode_)
    keep.insert(episode_->anchor);
  for (const auto &t : triads_)
    keep.insert(t.notes.begin(), t.notes.end());
  for (const auto &tx : transactions_)
  {
    for (const auto &[i, r] : tx.before.readings)
      keep.insert(i);
    if (tx.before.episode)
      keep.insert(tx.before.episode->anchor);
  }
  for (auto it = notes_.begin(); it != notes_.end();)
    if (!keep.count(it->first))
    {
      triadPending_.erase(it->first);
      it = notes_.erase(it);
    }
    else
      ++it;
}
} // namespace Intona::Tuning
