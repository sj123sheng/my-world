#include "decision_policy.h"

#include "engagement_spacing.h"

#include <cmath>

namespace {

// 支援距离独立于交战留白：Priest 为盟友驱散/护盾的探测半径，
// 与对主角的交战距离解耦，保留既有支援口径。
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

// 统一留白参数（Plan 2 Task 6）：优先消费快照注入的交战距离；未注入时
// 按原型推导，保证接线完成前决策仍有合理口径。
EngagementRange effectiveRange(const PerceptionSnapshot& facts,
                               EnemyArchetype archetype) {
  if (facts.engagementRange.ideal > 0.0f) return facts.engagementRange;
  return EngagementRangeFor(archetype, 0.0f, false);
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

  // 交战留白统一决策：低于 minimum 后撤、能力射程内攻击、超出射程追向槽位。
  // ideal 是环形槽位半径（走位目标），不收紧攻击判定，避免缩短攻击有效性。
  const EngagementRange range = effectiveRange(facts, archetype);
  if (facts.playerDistance < range.minimum) return EnemyIntent::Retreat;
  if (facts.playerDistance <= range.attack) return EnemyIntent::Attack;
  return EnemyIntent::Chase;
}
