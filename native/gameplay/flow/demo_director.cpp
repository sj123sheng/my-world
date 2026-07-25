#include "demo_director.h"

namespace {

constexpr Tick kExploreTimeoutMs = 30000;
constexpr Tick kBossIntroTimeoutMs = 8000;

}  // namespace

void DemoDirector::advanceTo(DemoPhase phase, Tick now,
                             bool timeoutFallback) {
  snapshot_.phase = phase;
  snapshot_.usedTimeoutFallback = timeoutFallback;
  snapshot_.inputRestored = true;
  snapshot_.cameraRestored = true;
  snapshot_.phaseElapsedMs = 0;
  phaseStartedAt_ = now;
}

void DemoDirector::tick(Tick now, const DemoSignals& signals) {
  snapshot_.phaseElapsedMs = now >= phaseStartedAt_ ? now - phaseStartedAt_ : 0;
  switch (snapshot_.phase) {
    case DemoPhase::Intro:
      if (signals.introComplete) advanceTo(DemoPhase::Explore, now, false);
      break;
    case DemoPhase::Explore:
      if (signals.reachedCombatAnchor) {
        advanceTo(DemoPhase::Encounter, now, false);
      } else if (snapshot_.phaseElapsedMs >= kExploreTimeoutMs) {
        advanceTo(DemoPhase::Encounter, now, true);
      }
      break;
    case DemoPhase::Encounter:
      if (signals.encounterComplete) {
        advanceTo(DemoPhase::Resonance, now, false);
      }
      break;
    case DemoPhase::Resonance:
      if (signals.allSourcesActive) {
        advanceTo(DemoPhase::BossIntro, now, false);
      }
      break;
    case DemoPhase::BossIntro:
      if (signals.cinematicComplete) {
        advanceTo(DemoPhase::BossFight, now, false);
      } else if (snapshot_.phaseElapsedMs >= kBossIntroTimeoutMs) {
        advanceTo(DemoPhase::BossFight, now, true);
      }
      break;
    case DemoPhase::BossFight:
      if (signals.bossDefeated) advanceTo(DemoPhase::Outro, now, false);
      break;
    case DemoPhase::Outro:
      break;
  }
}

void DemoDirector::skipTo(DemoPhase phase) {
  advanceTo(phase, phaseStartedAt_, false);
}
