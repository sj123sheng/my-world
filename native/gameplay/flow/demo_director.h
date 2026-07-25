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
};

class DemoDirector {
 public:
  void tick(Tick now, const DemoSignals& signals);
  void skipTo(DemoPhase phase);

  DemoPhase phase() const { return snapshot_.phase; }
  const DemoDirectorSnapshot& snapshot() const { return snapshot_; }

 private:
  void advanceTo(DemoPhase phase, Tick now, bool timeoutFallback);

  DemoDirectorSnapshot snapshot_;
  Tick phaseStartedAt_ = 0;
};
