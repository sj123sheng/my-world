#pragma once

#include "engine/core/tick_clock.h"

#include <array>
#include <cstdint>

enum class BossPhase : uint8_t {
  RadianceLockdown = 1,
  CurrentStorm = 2,
  CorruptionCollapse = 3,
};

enum class BossMechanic : uint8_t {
  None = 0,
  JudgmentBeam = 1,
  CurrentNodes = 2,
  FinalForge = 3,
};

struct BossConfig {
  FixedPoint maxHp = fp(1000);
  FixedPoint maxPoise = fp(300);
  FixedPoint phaseTwoHp = fp(700);
  FixedPoint phaseThreeHp = fp(350);
  Tick finalForgeCastMs = 5000;
  uint8_t currentNodeCount = 2;

  static BossConfig karounDefaults();
  bool valid() const;
};

struct BossFrameInput {
  Tick tick = 0;
  int64_t dtMs = 0;
  bool resonanceAvailable = false;
  bool ultimateUsed = false;
  uint64_t sequence = 0;
};

struct BossSnapshot {
  BossPhase phase = BossPhase::RadianceLockdown;
  BossMechanic mechanic = BossMechanic::None;
  FixedPoint hp = fp(1000);
  FixedPoint poise = fp(300);
  bool vulnerable = true;
  bool failedMechanic = false;
  bool defeated = false;
  uint8_t transitionCount = 0;
  uint8_t nodesBroken = 0;
  Tick castRemainingMs = 0;
  Tick lastTransitionTick = 0;
  Tick retryTick = 0;

  bool operator==(const BossSnapshot& other) const {
    return phase == other.phase && mechanic == other.mechanic &&
           hp == other.hp && poise == other.poise &&
           vulnerable == other.vulnerable &&
           failedMechanic == other.failedMechanic &&
           defeated == other.defeated &&
           transitionCount == other.transitionCount &&
           nodesBroken == other.nodesBroken &&
           castRemainingMs == other.castRemainingMs &&
           lastTransitionTick == other.lastTransitionTick &&
           retryTick == other.retryTick;
  }
};

class BossController {
 public:
  bool start(const BossConfig& config);
  bool retry(Tick tick);
  void applyDamage(FixedPoint hpDamage, FixedPoint poiseDamage, Tick tick);
  void update(const BossFrameInput& input);
  bool breakCurrentNode(uint8_t nodeIndex, Tick tick);

  const BossSnapshot& snapshot() const { return snapshot_; }
  const BossConfig& config() const { return config_; }

 private:
  void resetRuntime(Tick retryTick);
  void checkPhaseTransitions(Tick tick);
  void enterPhase(BossPhase phase, Tick tick);
  void refreshVulnerability();
  bool currentNodeBroken(uint8_t nodeIndex) const;

  BossConfig config_ = BossConfig::karounDefaults();
  BossSnapshot snapshot_;
  bool running_ = false;
  bool phaseTwoTriggered_ = false;
  bool phaseThreeTriggered_ = false;
  std::array<bool, 2> currentNodesBroken_{{false, false}};
};
