#include "native/engine/presentation/performance_guard.h"

#include <cassert>

namespace {

void testLevelBoundaries() {
  PerformanceGuard guard;
  // 2-second window at 16ms per sample = 125 samples
  for (int i = 0; i < 125; i++) guard.sample(16 * i, 16, 60.0f);
  assert(guard.level() == 0);

  guard = {};
  for (int i = 0; i < 125; i++) guard.sample(16 * i, 16, 48.0f);
  assert(guard.level() == 1);

  guard = {};
  for (int i = 0; i < 125; i++) guard.sample(16 * i, 16, 35.0f);
  assert(guard.level() == 2);

  guard = {};
  for (int i = 0; i < 125; i++) guard.sample(16 * i, 16, 25.0f);
  assert(guard.level() == 3);
}

void testRecoveryUpgrades() {
  PerformanceGuard guard;
  for (int i = 0; i < 125; i++) guard.sample(16 * i, 16, 25.0f);
  assert(guard.level() == 3);
  for (int i = 0; i < 125; i++) guard.sample(16 * (125 + i), 16, 60.0f);
  assert(guard.level() == 0);
}

void testSingleSpikeDoesNotChangeLevel() {
  PerformanceGuard guard;
  for (int i = 0; i < 125; i++) guard.sample(16 * i, 16, 60.0f);
  assert(guard.level() == 0);
  // Single low-FPS sample should not immediately drop the level
  guard.sample(16 * 125, 16, 10.0f);
  assert(guard.level() == 0);
}

void testInitialLevelIsZero() {
  PerformanceGuard guard;
  assert(guard.level() == 0);
}

}  // namespace

int main() {
  testInitialLevelIsZero();
  testLevelBoundaries();
  testRecoveryUpgrades();
  testSingleSpikeDoesNotChangeLevel();
  return 0;
}
