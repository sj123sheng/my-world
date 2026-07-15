#pragma once

#include "action_state_machine.h"
#include "training_target.h"

struct DamageOutcome {
  FixedPoint hpDamage = 0;
  FixedPoint poiseDamage = 0;
  bool poiseBroken = false;
  bool killed = false;
};

class DamageResolver {
 public:
  explicit DamageResolver(CombatConfig config);
  DamageOutcome resolve(TrainingTarget& target, const HitRequest& hit) const;

 private:
  CombatConfig config_;
};
