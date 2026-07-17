#include "damage_resolver.h"

#include <cmath>

namespace {
FixedPoint multiply(FixedPoint value, FixedPoint multiplier) {
  return static_cast<FixedPoint>((static_cast<__int128>(value) * multiplier) / FP_ONE);
}

FixedPoint directionalHpMultiplier(const DamageResolutionContext& context) {
  if (!context.directionalDefense.has_value()) return FP_ONE;
  const DirectionalDefenseProfile& defense = *context.directionalDefense;
  if (defense.frontalHpDamageMultiplier < 0 ||
      defense.frontalHpDamageMultiplier > FP_ONE ||
      !std::isfinite(defense.minimumFrontDot) || defense.minimumFrontDot < -1.0f ||
      defense.minimumFrontDot > 1.0f || !context.attackerPosition.finite() ||
      !context.defenderPosition.finite() || !context.defenderFacing.finite()) {
    return FP_ONE;
  }

  const Vec2 towardAttacker = context.attackerPosition - context.defenderPosition;
  const float attackerDistance = towardAttacker.length();
  const float facingLength = context.defenderFacing.length();
  if (!std::isfinite(attackerDistance) || !std::isfinite(facingLength) ||
      attackerDistance <= 0.0f || facingLength <= 0.0f) {
    return FP_ONE;
  }

  const float dot = (towardAttacker.x * context.defenderFacing.x +
                     towardAttacker.y * context.defenderFacing.y) /
                    (attackerDistance * facingLength);
  return std::isfinite(dot) && dot >= defense.minimumFrontDot
             ? defense.frontalHpDamageMultiplier
             : FP_ONE;
}
}

DamageResolver::DamageResolver(CombatConfig config) : config_(config.validated()) {}

DamageOutcome DamageResolver::resolve(TrainingTarget& target, const HitRequest& hit) const {
  return resolve(target, hit, {});
}

DamageOutcome DamageResolver::resolve(TrainingTarget& target, const HitRequest& hit,
                                      const DamageResolutionContext& context) const {
  (void)config_;
  target.advance(hit.tick);
  if (!target.alive()) return {};

  DamageOutcome outcome;
  const bool wasAlive = target.alive();
  const FixedPoint defendedHpDamage =
      multiply(hit.baseDamage, directionalHpMultiplier(context));
  const FixedPoint hpDamage =
      multiply(defendedHpDamage, target.hpDamageMultiplier(hit.tick));
  const FixedPoint poiseDamage =
      multiply(hit.poiseDamage, target.poiseDamageMultiplier(hit.tick));
  outcome.hpDamage = target.applyHpDamage(hpDamage, hit.tick);
  const bool hadPoise = target.poise() > 0;
  outcome.poiseDamage = target.applyPoiseDamage(poiseDamage, hit.tick);
  outcome.poiseBroken = hadPoise && target.poise() == 0;
  outcome.killed = wasAlive && !target.alive();
  return outcome;
}
