#pragma once

#include "damage_resolver.h"
#include "resonance.h"
#include <optional>

struct ReactionOutcome {
  std::optional<ResonanceType> type;
  FixedPoint hpDamage = 0;
  FixedPoint poiseDamage = 0;
  FixedPoint resonanceGain = 0;
  bool poiseBroken = false;
};

class SourceReactionSystem {
 public:
  explicit SourceReactionSystem(CombatConfig config);
  ReactionOutcome apply(TrainingTarget& target,
                        SourceType source,
                        FixedPoint amount,
                        Tick now,
                        EntityId applier);

 private:
  CombatConfig config_;
  DamageResolver resolver_;
};
