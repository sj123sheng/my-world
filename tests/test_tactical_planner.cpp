#include "gameplay/ai/tactical_planner.h"

#include <cassert>
#include <vector>

namespace {

EnemyAbilityState attackAbility(EnemyAbilityId id, Tick cooldownRemainingMs = 0) {
  EnemyAbility ability;
  ability.id = id;
  ability.tag = "attack";
  ability.range = fp(2.0);
  ability.weight = fp(1.0);
  ability.targetPolicy = EnemyTargetPolicy::CurrentTarget;
  return {ability, cooldownRemainingMs};
}

PerceptionSnapshot facts() {
  PerceptionSnapshot snapshot;
  snapshot.tick = 42;
  snapshot.selfId = 1;
  snapshot.selfPosition = {0.0f, 0.0f};
  snapshot.targetId = 7;
  snapshot.targetPosition = {1.0f, 0.0f};
  snapshot.targetDistance = 1.0f;
  snapshot.targetVisible = true;
  snapshot.safeReturnPosition = {-2.0f, 0.0f};
  return snapshot;
}

}  // namespace

int main() {
  TacticalPlanner planner;
  PerceptionSnapshot snapshot = facts();
  const std::vector<EnemyAbilityState> equalWeightAttacks = {
      attackAbility(20),
      attackAbility(10),
  };

  const EnemyActionPlan first =
      planner.plan(EnemyIntent::Attack, snapshot, equalWeightAttacks);
  assert(first.intent == EnemyIntent::Attack);
  assert(first.abilityId == 10);
  assert(first.targetId == 7);
  assert(first.desiredPosition == snapshot.targetPosition);
  assert(first.fallbackReason == EnemyPlanFallbackReason::None);

  const std::vector<EnemyAbilityState> firstOnCooldown = {
      attackAbility(20),
      attackAbility(10, 1),
  };
  const EnemyActionPlan second =
      planner.plan(EnemyIntent::Attack, snapshot, firstOnCooldown);
  assert(second.intent == EnemyIntent::Attack);
  assert(second.abilityId == 20);

  const EnemyActionPlan noAttack = planner.plan(EnemyIntent::Attack, snapshot, {});
  assert(noAttack.intent == EnemyIntent::Chase);
  assert(!noAttack.abilityId.has_value());
  assert(noAttack.fallbackReason == EnemyPlanFallbackReason::NoLegalAbility);

  const EnemyActionPlan noSupport = planner.plan(EnemyIntent::Support, snapshot, {});
  assert(noSupport.intent == EnemyIntent::Retreat);
  assert(noSupport.fallbackReason == EnemyPlanFallbackReason::NoLegalAbility);

  const EnemyActionPlan retreat = planner.plan(EnemyIntent::Retreat, snapshot, {});
  assert(retreat.intent == EnemyIntent::Retreat);
  assert(retreat.state == EnemyAiState::Moving);
  assert(retreat.desiredPosition == (Vec2{-1.0f, 0.0f}));
  assert(retreat.fallbackReason == EnemyPlanFallbackReason::None);

  snapshot.selfInsideRegion = false;
  const EnemyActionPlan outside =
      planner.plan(EnemyIntent::Attack, snapshot, equalWeightAttacks);
  assert(outside.intent == EnemyIntent::ReturnToArea);
  assert(!outside.abilityId.has_value());
  assert(outside.desiredPosition == snapshot.safeReturnPosition);
  assert(outside.fallbackReason == EnemyPlanFallbackReason::OutsideRegion);

  const EnemyActionPlan expected = first;
  for (int iteration = 0; iteration < 100; ++iteration) {
    const EnemyActionPlan actual =
        planner.plan(EnemyIntent::Attack, facts(), equalWeightAttacks);
    assert(actual.intent == expected.intent);
    assert(actual.abilityId == expected.abilityId);
    assert(actual.targetId == expected.targetId);
    assert(actual.desiredPosition == expected.desiredPosition);
    assert(actual.fallbackReason == expected.fallbackReason);
  }
}
