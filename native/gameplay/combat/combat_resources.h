#pragma once

#include "combat_config.h"

class CombatResources {
 public:
  explicit CombatResources(CombatConfig config);

  bool spendStamina(FixedPoint amount, Tick tick);
  void advance(Tick now);
  void grantInsight(Tick tick);
  bool consumeInsight(Tick tick);
  void reset();

  FixedPoint stamina() const { return stamina_; }
  Tick insightRemainingMs() const;

 private:
  CombatConfig config_;
  FixedPoint stamina_ = 0;
  Tick now_ = 0;
  Tick recoveryStartTick_ = 0;
  Tick recoveredMs_ = 0;
  int64_t recoveryRemainder_ = 0;
  Tick insightExpiresAt_ = 0;
  bool insightAvailable_ = false;
};
