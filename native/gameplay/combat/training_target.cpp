#include "training_target.h"

#include <algorithm>

TrainingTarget::TrainingTarget(CombatConfig config) : config_(config.validated()) { reset(); }

TrainingTarget TrainingTarget::defaults() { return TrainingTarget(CombatConfig::defaults()); }

void TrainingTarget::advance(Tick now) {
  if (!alive() && deathResetAt_ > 0 && now >= deathResetAt_) {
    reset();
    return;
  }
  sourceAuras_.decay(now);
  if (reactionWeakUntil_ > 0 && now >= reactionWeakUntil_) {
    reactionWeakUntil_ = 0;
  }
  if (breakUntil_ > 0 && now >= breakUntil_) {
    breakUntil_ = 0;
    poise_ = config_.trainingTargetPoise;
  }
  if (stagnationUntil_ > 0 && now >= stagnationUntil_) {
    stagnationUntil_ = 0;
  }
}

void TrainingTarget::reset() {
  hp_ = config_.trainingTargetHp;
  poise_ = config_.trainingTargetPoise;
  reactionWeakUntil_ = 0;
  breakUntil_ = 0;
  stagnationUntil_ = 0;
  deathResetAt_ = 0;
  sourceAuras_.clear();
}

Tick TrainingTarget::weakUntil() const { return std::max(reactionWeakUntil_, breakUntil_); }

FixedPoint TrainingTarget::hpDamageMultiplier(Tick now) const {
  FixedPoint multiplier = FP_ONE;
  if (reactionWeakUntil_ > now) multiplier = config_.weakDamageMultiplier;
  if (breakUntil_ > now) multiplier = std::max(multiplier, config_.trainingBreakDamageMultiplier);
  return multiplier;
}

FixedPoint TrainingTarget::poiseDamageMultiplier(Tick now) const {
  return stagnationUntil_ > now ? config_.stagnationPoiseMultiplier : FP_ONE;
}

FixedPoint TrainingTarget::applyHpDamage(FixedPoint amount, Tick now) {
  if (!alive() || amount <= 0) return 0;
  const FixedPoint applied = std::min(amount, hp_);
  hp_ -= applied;
  if (hp_ == 0) deathResetAt_ = now + config_.trainingDeathResetMs;
  return applied;
}

FixedPoint TrainingTarget::applyPoiseDamage(FixedPoint amount, Tick now) {
  if (!alive() || poise_ <= 0 || amount <= 0) return 0;
  const FixedPoint applied = std::min(amount, poise_);
  poise_ -= applied;
  if (poise_ == 0) {
    breakUntil_ = std::max(breakUntil_, now + config_.trainingBreakDurationMs);
  }
  return applied;
}

void TrainingTarget::applyWeakness(Tick until, FixedPoint multiplier) {
  (void)multiplier;
  reactionWeakUntil_ = std::max(reactionWeakUntil_, until);
}

void TrainingTarget::applyStagnation(Tick until) {
  stagnationUntil_ = std::max(stagnationUntil_, until);
}
