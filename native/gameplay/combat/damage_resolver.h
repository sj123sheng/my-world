#pragma once

#include "action_state_machine.h"
#include "training_target.h"
#include "engine/math/vec2.h"

#include <optional>

struct DamageOutcome {
  FixedPoint hpDamage = 0;
  FixedPoint poiseDamage = 0;
  bool poiseBroken = false;
  bool killed = false;
};

struct DirectionalDefenseProfile {
  FixedPoint frontalHpDamageMultiplier = FP_ONE;
  float minimumFrontDot = 0.0f;
};

struct DamageResolutionContext {
  Vec2 attackerPosition;
  Vec2 defenderPosition;
  Vec2 defenderFacing = {1.0f, 0.0f};
  std::optional<DirectionalDefenseProfile> directionalDefense;
};

class DamageResolver {
 public:
  explicit DamageResolver(CombatConfig config);
  DamageOutcome resolve(TrainingTarget& target, const HitRequest& hit) const;
  DamageOutcome resolve(TrainingTarget& target, const HitRequest& hit,
                        const DamageResolutionContext& context) const;

 private:
  CombatConfig config_;
};
