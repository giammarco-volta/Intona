#pragma once
#include "HarmonicCostAdapting.h" // Relative keyboard geometry, not the cost engine.
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace Intona::Tuning
{

// Event-driven musical state. The caller owns the clock, timer and MIDI output.
// Note identity is per attack, so a release/restrike cannot inherit a pivot.
class ScaleTriadAdapting
{
public:
  using Id = uint64_t;
  static constexpr double verificationMs = 70;
  enum class Status
  {
    Pending,
    Confirmed,
    Dirty
  };
  struct Reading
  {
    Status status = Status::Pending;
    int value = 0;
    std::vector<Id> supports;
  };
  struct Note
  {
    Id id = 0;
    int key = 0;
    double on = 0;
    std::optional<double> off;
    Reading reading;
    bool triadSeen = false;
  };
  void reset(const Config &config);
  void seed(Id id, int key, double time); // Explicit context reset: held notes are pivots.
  void noteOn(Id id, int key, double time);
  void noteOff(Id id, double time);
  void advance(double time); // Check deadlines <= time; never makes a new harmonic guess.
  std::optional<double> nextDeadline() const;
  Config config() const;
  const std::map<Id, Note> &notes() const
  {
    return notes_;
  }
  int stableCenter() const
  {
    return stable_;
  }
  bool provisional() const
  {
    return episode_.has_value();
  }
  bool valid() const
  {
    return valid_;
  }
  std::function<bool(const Config &)> admissible;

private:
  using Ids = std::vector<Id>;
  struct Episode
  {
    Id anchor;
  };
  struct Snapshot
  {
    int center, stable;
    std::optional<Episode> episode;
    std::map<Id, Reading> readings;
    std::set<Id> triadPending;
  };
  struct Transaction
  {
    double deadline;
    Id trigger;
    int center, count;
    Snapshot before;
    std::set<Id> held;
    std::map<Id, int> values;
    bool triadProof = false;
    std::optional<int> baseChoice;
  };
  struct Selection
  {
    std::optional<int> center;
    bool triadOverride = false;
    std::optional<int> baseChoice;
  };
  int center_ = 0, stable_ = 0, anchor_ = 0;
  double time_ = 0;
  bool valid_ = false;
  Config initial_{};
  std::optional<Episode> episode_;
  std::map<Id, Note> notes_;
  std::vector<Transaction> transactions_;
  std::set<Id> triadPending_;
  Ids context(Id trigger, const std::set<Id> *allowed = nullptr, const std::set<Id> &excluded = {},
              const std::set<Id> *held = nullptr) const;
  bool matches(int center, const Ids &cohort, int count, const std::map<Id, int> *fixed = nullptr,
               bool pivots = true) const;
  Selection select(const Ids &cohort);
  void evaluate(Id trigger);
  void accept(const Ids &cohort, int center);
  bool mature(const Note &note, const Ids &cohort) const;
  Snapshot snapshot() const;
  void retainPending();
  void checkFirst();
  void prune();
};

} // namespace Intona::Tuning
