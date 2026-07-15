#include "source_reaction_system.h"

SourceReactionSystem::SourceReactionSystem(CombatConfig config)
    : config_(config.validated()), resolver_(config_) {}

ReactionOutcome SourceReactionSystem::apply(TrainingTarget& target,
                                            SourceType source,
                                            FixedPoint amount,
                                            Tick now,
                                            EntityId applier) {
  target.advance(now);
  auras_.decay(now);

  ReactionOutcome outcome;
  for (const SourceAura& aura : auras_.active()) {
    const std::optional<ResonanceType> reaction = resolveResonance(aura.type, source);
    if (!reaction) continue;
    outcome.type = reaction;
    break;
  }

  if (outcome.type) {
    auras_.clear();
    HitRequest reactionHit;
    reactionHit.tick = now;
    switch (*outcome.type) {
      case ResonanceType::Refraction:
        reactionHit.baseDamage = config_.refractionDamage;
        break;
      case ResonanceType::Stasis:
        target.applyStagnation(now + config_.stagnationDurationMs);
        break;
      case ResonanceType::Collapse:
        reactionHit.poiseDamage = config_.disintegrationPoiseDamage;
        break;
      case ResonanceType::Burst:
        break;
    }
    const DamageOutcome damage = resolver_.resolve(target, reactionHit);
    outcome.hpDamage = damage.hpDamage;
    outcome.poiseDamage = damage.poiseDamage;
    outcome.poiseBroken = damage.poiseBroken;
    if (*outcome.type == ResonanceType::Refraction) {
      target.applyWeakness(now + config_.weakDurationMs, config_.weakDamageMultiplier);
    }
    if (*outcome.type == ResonanceType::Collapse && damage.poiseBroken) {
      outcome.hpDamage += target.applyHpDamage(config_.disintegrationBreakDamage, now);
    }
  }

  auras_.apply({source, amount, now + config_.sourceAuraDurationMs, applier});
  return outcome;
}
