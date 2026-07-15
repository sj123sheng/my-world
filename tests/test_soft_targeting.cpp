#include "native/gameplay/targeting/soft_targeting.h"

#include <cassert>
#include <limits>

int main() {
  SoftTargeting targeting({1.0f, 1.0471976f});
  std::vector<TargetCandidate> candidates{{3, {0.5f, 0.8f}},
                                          {2, {0.5f, 0.8f}}};
  auto selected = targeting.select({0.5f, 0.5f}, 0.0f, candidates);
  assert(selected && selected->id == 2);

  candidates.push_back({1, {0.5f, -0.5f}});
  assert(targeting.select({0.5f, 0.5f}, 0.0f, candidates)->id == 2);

  candidates = {
      {4, {std::numeric_limits<float>::infinity(), 0.0f}}};
  assert(!targeting.select({0.5f, 0.5f}, 0.0f, candidates));
}
