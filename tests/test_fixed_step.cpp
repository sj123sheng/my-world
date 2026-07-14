#include "../native/engine/core/fixed_step.h"
#include <cassert>
#include <limits>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
void expectAssertion(void (*construct)()) {
  pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    close(STDERR_FILENO);
    construct();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

void constructWithZeroStep() { FixedStep step(0, 4); }
void constructWithNegativeMaxSteps() { FixedStep step(16, -1); }
}  // namespace

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
  assert(step.advance(0, [&](Tick, int64_t) { assert(false); }) == 0);

  FixedStep negative(16, 4);
  assert(negative.advance(-1, [&](Tick, int64_t) { assert(false); }) == 0);
  assert(negative.tick() == 0);

  FixedStep huge(16, 4);
  assert(huge.advance(std::numeric_limits<int64_t>::max(),
                      [&](Tick, int64_t dt) { assert(dt == 16); }) == 4);
  assert(huge.tick() == 4);
  assert(huge.droppedFrames() ==
         static_cast<uint64_t>(std::numeric_limits<int64_t>::max() / 16) - 4);
  assert(huge.advance(0, [&](Tick, int64_t) { assert(false); }) == 0);

  FixedStep overflowingAccumulator(16, 4);
  assert(overflowingAccumulator.advance(15, [&](Tick, int64_t) { assert(false); }) == 0);
  assert(overflowingAccumulator.advance(
             std::numeric_limits<int64_t>::max(),
             [&](Tick, int64_t dt) { assert(dt == 16); }) == 4);
  const uint64_t total = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 15;
  assert(overflowingAccumulator.droppedFrames() == total / 16 - 4);
  assert(overflowingAccumulator.advance(2, [&](Tick tick, int64_t dt) {
           assert(tick == 5 && dt == 16);
         }) == 1);

  expectAssertion(constructWithZeroStep);
  expectAssertion(constructWithNegativeMaxSteps);
  return 0;
}
