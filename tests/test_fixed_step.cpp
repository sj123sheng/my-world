#include "../native/engine/core/fixed_step.h"
#include <cassert>
#include <vector>

int main() {
  FixedStep step(16, 4);
  std::vector<Tick> ticks;
  assert(step.advance(15, [&](Tick tick, int64_t dt) { ticks.push_back(tick); }) == 0);
  assert(step.advance(1, [&](Tick tick, int64_t dt) {
    assert(dt == 16);
    ticks.push_back(tick);
  }) == 1);
  assert(ticks.size() == 1 && ticks[0] == 1);

  assert(step.advance(80, [&](Tick tick, int64_t dt) { ticks.push_back(tick); }) == 4);
  assert(step.tick() == 5);
  assert(step.droppedFrames() == 1);
  return 0;
}
