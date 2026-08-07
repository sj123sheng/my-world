#include "decision_policy.h"

#include <cmath>

namespace {

constexpr float kMeleeAttackDistance = 0.25f;
// 远程原型（Priest/Caster）共用保持距离参数。
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
    case EnemyArchetype::Bruiser:  // 重甲近战：与利爪/守卫同用近战追击逻辑
    case EnemyArchetype::Elite:    // 精英：近战压制
      return facts.playerDistance <= kMeleeAttackDistance ? EnemyIntent::Attack
                                                          : EnemyIntent::Chase;
    case EnemyArchetype::Priest:
    case EnemyArchetype::Caster:  // 远程：近身后拉开距离再施法
      if (facts.playerDistance <= kPriestRetreatDistance) return EnemyIntent::Retreat;
      return facts.playerDistance <= kPriestAttackDistance ? EnemyIntent::Attack
                                                            : EnemyIntent::Chase;
  }
  return EnemyIntent::Idle;
}
