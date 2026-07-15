#include "source_reaction_system.h"

SourceReactionSystem::SourceReactionSystem(CombatConfig config)
    : config_(config.validated()), resolver_(config_) {}

ReactionOutcome SourceReactionSystem::apply(TrainingTarget& target,
                                            SourceType source,
                                            FixedPoint amount,
                                            Tick now,
                                            EntityId applier) {
  target.advance(now);
  SourceAuraContainer& auras = target.sourceAuras();

  ReactionOutcome outcome;
  constexpr SourceType kPriority[] = {
      SourceType::Radiance, SourceType::Current, SourceType::Corruption};
  for (const SourceType candidate : kPriority) {
    for (const SourceAura& aura : auras.active()) {
      if (aura.type != candidate) continue;
      const std::optional<ResonanceType> reaction = resolveResonance(candidate, source);
      if (reaction) outcome.type = reaction;
      break;
    }
    if (outcome.type) break;
  }

  if (outcome.type) {
    auras.clear();
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

  auras.apply({source, amount, now + config_.sourceAuraDurationMs, applier});
  return outcome;
}
