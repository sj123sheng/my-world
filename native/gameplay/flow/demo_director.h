#pragma once

#include "native/engine/core/tick_clock.h"

#include <cstdint>

enum class DemoPhase : uint8_t {
  Intro,
  Explore,
  Encounter,
  Resonance,
  BossIntro,
  BossFight,
  Outro,
};

enum class BossSourceColor : uint8_t {
  Radiance,
  Current,
  Corruption,
};

struct BossCinematicState {
  float ringProgress = 0.0f;
  uint8_t shardCount = 3;
  BossSourceColor sourceColor = BossSourceColor::Radiance;
  bool broken = false;
  bool readyForFight = false;
  Tick elapsedMs = 0;

  BossCinematicState tick(Tick deltaMs) const;
  static BossCinematicState fromBossHp(float hpRatio);
  static float healthRatio(int64_t currentHp, int64_t maxHp);
};

struct DemoSignals {
  bool introComplete = false;
  bool reachedCombatAnchor = false;
  bool encounterComplete = false;
  bool allSourcesActive = false;
  bool cinematicComplete = false;
  bool bossDefeated = false;
};

struct DemoDirectorSnapshot {
  DemoPhase phase = DemoPhase::Intro;
  bool usedTimeoutFallback = false;
  bool inputRestored = true;
  bool cameraRestored = true;
  Tick phaseElapsedMs = 0;
  BossCinematicState bossCinematic;
};

class DemoDirector {
 public:
  void tick(Tick now, const DemoSignals& signals);
  void skipTo(DemoPhase phase);

  DemoPhase phase() const { return snapshot_.phase; }
  const DemoDirectorSnapshot& snapshot() const { return snapshot_; }
  const BossCinematicState& bossCinematic() const {
    return snapshot_.bossCinematic;
  }

 private:
  void advanceTo(DemoPhase phase, Tick now, bool timeoutFallback);

  DemoDirectorSnapshot snapshot_;
  Tick phaseStartedAt_ = 0;
};
