#pragma once

#include "combat_config.h"

class TrainingTarget {
 public:
  explicit TrainingTarget(CombatConfig config);

  static TrainingTarget defaults();
  void advance(Tick now);
  void reset();

  FixedPoint hp() const { return hp_; }
  FixedPoint poise() const { return poise_; }
  bool alive() const { return hp_ > 0; }
  Tick weakUntil() const;
  Tick stagnationUntil() const { return stagnationUntil_; }
  Tick deathResetAt() const { return deathResetAt_; }
  FixedPoint hpDamageMultiplier(Tick now) const;
  FixedPoint poiseDamageMultiplier(Tick now) const;

  FixedPoint applyHpDamage(FixedPoint amount, Tick now);
  FixedPoint applyPoiseDamage(FixedPoint amount, Tick now);
  void applyWeakness(Tick until, FixedPoint multiplier);
  void applyStagnation(Tick until);

 private:
  CombatConfig config_;
  FixedPoint hp_ = 0;
  FixedPoint poise_ = 0;
  Tick reactionWeakUntil_ = 0;
  Tick breakUntil_ = 0;
  Tick stagnationUntil_ = 0;
  Tick deathResetAt_ = 0;
};
