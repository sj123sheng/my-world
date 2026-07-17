#include "decision_policy.h"

#include <cmath>

namespace {

constexpr float kMeleeAttackDistance = 0.25f;
constexpr float kPriestRetreatDistance = 1.0f;
constexpr float kPriestAttackDistance = 4.0f;
constexpr float kPriestSupportDistance = 4.0f;

bool hasSupportTarget(const PerceptionSnapshot& facts) {
  for (const AllyPerception& ally : facts.allies) {
    if (ally.id != 0 && ally.id != facts.selfId && ally.alive && ally.insideRegion &&
        ally.shield <= 0 && std::isfinite(ally.distanceToSelf) &&
        ally.distanceToSelf >= 0.0f && ally.distanceToSelf <= kPriestSupportDistance) {
      return true;
    }
  }
  return false;
}

}  // namespace

EnemyIntent DecisionPolicy::choose(const PerceptionSnapshot& facts,
                                   EnemyArchetype archetype) const {
  if (!facts.selfAlive) return EnemyIntent::Idle;
  if (!facts.selfInsideRegion || !facts.playerInsideRegion) return EnemyIntent::ReturnToArea;
  if (!facts.playerReachable) return EnemyIntent::BreakFree;
  if (facts.staggered) return EnemyIntent::Idle;
  if (archetype == EnemyArchetype::Priest && hasSupportTarget(facts)) {
    return EnemyIntent::Support;
  }
  if (!facts.playerVisible) return EnemyIntent::Idle;

  switch (archetype) {
    case EnemyArchetype::RiftClaw:
    case EnemyArchetype::Guard:
      return facts.playerDistance <= kMeleeAttackDistance ? EnemyIntent::Attack
                                                          : EnemyIntent::Chase;
    case EnemyArchetype::Priest:
      if (facts.playerDistance <= kPriestRetreatDistance) return EnemyIntent::Retreat;
      return facts.playerDistance <= kPriestAttackDistance ? EnemyIntent::Attack
                                                            : EnemyIntent::Chase;
  }
  return EnemyIntent::Idle;
}
