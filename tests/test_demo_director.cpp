#include "native/gameplay/flow/demo_director.h"

#include <cassert>

namespace {

void testStagesAdvanceFromSignals() {
  DemoDirector director;
  assert(director.phase() == DemoPhase::Intro);

  director.tick(1000, DemoSignals{.introComplete = true});
  assert(director.phase() == DemoPhase::Explore);

  director.tick(2000, DemoSignals{.reachedCombatAnchor = true});
  assert(director.phase() == DemoPhase::Encounter);

  director.tick(3000, DemoSignals{.encounterComplete = true});
  assert(director.phase() == DemoPhase::Resonance);

  director.tick(4000, DemoSignals{.allSourcesActive = true});
  assert(director.phase() == DemoPhase::BossIntro);

  director.tick(12000, DemoSignals{.cinematicComplete = true});
  assert(director.phase() == DemoPhase::BossFight);

  director.tick(13000, DemoSignals{.bossDefeated = true});
  assert(director.phase() == DemoPhase::Outro);
}

void testExploreTimeoutProvidesSafeProgression() {
  DemoDirector director;
  director.tick(1000, DemoSignals{.introComplete = true});
  director.tick(33000, DemoSignals{});
  assert(director.phase() == DemoPhase::Encounter);
  assert(director.snapshot().usedTimeoutFallback);
}

void testSkipRestoresInputAndCamera() {
  DemoDirector director;
  director.skipTo(DemoPhase::BossIntro);
  assert(director.phase() == DemoPhase::BossIntro);
  assert(director.snapshot().inputRestored);
  assert(director.snapshot().cameraRestored);
}

void testBossIntroTimeoutIsRelativeToPhaseEntry() {
  DemoDirector director;
  director.tick(90000, DemoSignals{.introComplete = true});
  director.skipTo(DemoPhase::BossIntro);
  director.tick(97000, DemoSignals{});
  assert(director.phase() == DemoPhase::BossIntro);
  director.tick(98000, DemoSignals{});
  assert(director.phase() == DemoPhase::BossFight);
}

}  // namespace

int main() {
  testStagesAdvanceFromSignals();
  testExploreTimeoutProvidesSafeProgression();
  testSkipRestoresInputAndCamera();
  testBossIntroTimeoutIsRelativeToPhaseEntry();
  return 0;
}
