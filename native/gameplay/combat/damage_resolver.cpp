#include "damage_resolver.h"

namespace {
FixedPoint multiply(FixedPoint value, FixedPoint multiplier) {
  return static_cast<FixedPoint>((static_cast<__int128>(value) * multiplier) / FP_ONE);
}
}

DamageResolver::DamageResolver(CombatConfig config) : config_(config.validated()) {}

DamageOutcome DamageResolver::resolve(TrainingTarget& target, const HitRequest& hit) const {
  (void)config_;
  target.advance(hit.tick);
  if (!target.alive()) return {};

  DamageOutcome outcome;
  const bool wasAlive = target.alive();
  const FixedPoint hpDamage = multiply(hit.baseDamage, target.hpDamageMultiplier(hit.tick));
  const FixedPoint poiseDamage =
      multiply(hit.poiseDamage, target.poiseDamageMultiplier(hit.tick));
  outcome.hpDamage = target.applyHpDamage(hpDamage, hit.tick);
  const bool hadPoise = target.poise() > 0;
  outcome.poiseDamage = target.applyPoiseDamage(poiseDamage, hit.tick);
  outcome.poiseBroken = hadPoise && target.poise() == 0;
  outcome.killed = wasAlive && !target.alive();
  return outcome;
}
