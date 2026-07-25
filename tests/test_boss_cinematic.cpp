#include "native/gameplay/flow/demo_director.h"

#include <cassert>

namespace {

void testIntroCompletesWithinEightSeconds() {
  BossCinematicState state;
  for (int i = 0; i < 8; ++i) {
    state = state.tick(1000);
  }

  assert(state.readyForFight);
  assert(state.shardCount == 3);
}

void testHalfHealthBreaksOuterRing() {
  assert(BossCinematicState::fromBossHp(0.49f).broken);
  assert(!BossCinematicState::fromBossHp(0.50f).broken);
}

}  // namespace

int main() {
  testIntroCompletesWithinEightSeconds();
  testHalfHealthBreaksOuterRing();
  return 0;
}
