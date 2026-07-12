#pragma once
#include "event.h"
#include <vector>
struct DecisionLog {
  std::vector<HitEvent> inputs; std::vector<CombatResult> results;
  void record(const HitEvent& i, const CombatResult& r){ inputs.push_back(i); results.push_back(r); }
  bool replayEquals(const DecisionLog& other) const { return inputs==other.inputs && results==other.results; }
};
