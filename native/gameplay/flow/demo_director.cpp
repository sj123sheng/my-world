#include "demo_director.h"

#include <algorithm>

namespace {

constexpr Tick kExploreTimeoutMs = 30000;
constexpr Tick kBossIntroTimeoutMs = 8000;
constexpr Tick kBossCinematicDurationMs = 7000;
constexpr Tick kSourceColorIntervalMs = 1500;

}  // namespace

BossCinematicState BossCinematicState::tick(Tick deltaMs) const {
  BossCinematicState next = *this;
  next.elapsedMs = std::min(kBossCinematicDurationMs, elapsedMs + deltaMs);
  next.ringProgress = static_cast<float>(next.elapsedMs) /
                      static_cast<float>(kBossCinematicDurationMs);
  next.readyForFight = next.elapsedMs >= kBossCinematicDurationMs;
  next.sourceColor = static_cast<BossSourceColor>(
      (next.elapsedMs / kSourceColorIntervalMs) % 3);
  return next;
}

BossCinematicState BossCinematicState::fromBossHp(float hpRatio) {
  BossCinematicState state;
  state.broken = hpRatio < 0.5f;
  return state;
}

float BossCinematicState::healthRatio(int64_t currentHp, int64_t maxHp) {
  if (maxHp <= 0) return 0.0f;
  return std::clamp(static_cast<float>(currentHp) / static_cast<float>(maxHp),
                    0.0f, 1.0f);
}

void DemoDirector::advanceTo(DemoPhase phase, Tick now,
                             bool timeoutFallback) {
  snapshot_.phase = phase;
  snapshot_.usedTimeoutFallback = timeoutFallback;
  snapshot_.inputRestored = true;
  snapshot_.cameraRestored = true;
  snapshot_.phaseElapsedMs = 0;
  if (phase == DemoPhase::BossIntro) {
    snapshot_.bossCinematic = {};
  }
  phaseStartedAt_ = now;
}

void DemoDirector::tick(Tick now, const DemoSignals& signals) {
  snapshot_.phaseElapsedMs = now >= phaseStartedAt_ ? now - phaseStartedAt_ : 0;
  if (snapshot_.phase == DemoPhase::BossIntro) {
    snapshot_.inputRestored = false;
    snapshot_.cameraRestored = false;
    if (snapshot_.phaseElapsedMs > snapshot_.bossCinematic.elapsedMs) {
      snapshot_.bossCinematic = snapshot_.bossCinematic.tick(
          snapshot_.phaseElapsedMs - snapshot_.bossCinematic.elapsedMs);
    }
  }
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
      if (signals.cinematicComplete || snapshot_.bossCinematic.readyForFight) {
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
