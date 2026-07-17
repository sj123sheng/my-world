#include "gameplay/ai/tactical_planner.h"

#include <cassert>
#include <vector>

namespace {

EnemyAbilityState attackAbility(EnemyAbilityId id, FixedPoint weight = fp(1.0),
                                FixedPoint range = fp(2.0),
                                Tick cooldownRemainingMs = 0) {
  EnemyAbility ability;
  ability.id = id;
  ability.tag = "slash";
  ability.category = EnemyAbilityCategory::Attack;
  ability.range = range;
  ability.weight = weight;
  ability.targetPolicy = EnemyTargetPolicy::CurrentTarget;
  return {ability, cooldownRemainingMs};
}

EnemyAbilityState supportAbility(EnemyAbilityId id, FixedPoint range = fp(2.0)) {
  EnemyAbility ability;
  ability.id = id;
  ability.tag = "ward";
  ability.category = EnemyAbilityCategory::Support;
  ability.range = range;
  ability.weight = fp(1.0);
  ability.targetPolicy = EnemyTargetPolicy::LowestShieldAlly;
  return {ability, 0};
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

bool samePlan(const EnemyActionPlan& left, const EnemyActionPlan& right) {
  return left.createdAt == right.createdAt && left.intent == right.intent &&
         left.state == right.state && left.phase == right.phase &&
         left.abilityId == right.abilityId && left.targetId == right.targetId &&
         left.desiredPosition == right.desiredPosition &&
         left.fallbackReason == right.fallbackReason && left.movement == right.movement;
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
      attackAbility(10, fp(1.0), fp(2.0), 1),
  };
  const EnemyActionPlan second =
      planner.plan(EnemyIntent::Attack, snapshot, firstOnCooldown);
  assert(second.intent == EnemyIntent::Attack);
  assert(second.abilityId == 20);

  const EnemyActionPlan higherWeight = planner.plan(
      EnemyIntent::Attack, snapshot,
      {attackAbility(10, fp(1.0)), attackAbility(20, fp(2.0))});
  assert(higherWeight.abilityId == 20);

  const EnemyActionPlan closerRange = planner.plan(
      EnemyIntent::Attack, snapshot,
      {attackAbility(10, fp(1.0), fp(2.0)), attackAbility(20, fp(1.0), fp(1.25))});
  assert(closerRange.abilityId == 20);

  const EnemyActionPlan noAttack = planner.plan(EnemyIntent::Attack, snapshot, {});
  assert(noAttack.intent == EnemyIntent::Chase);
  assert(!noAttack.abilityId.has_value());
  assert(noAttack.fallbackReason == EnemyPlanFallbackReason::NoLegalAbility);

  const EnemyActionPlan noSupport = planner.plan(EnemyIntent::Support, snapshot, {});
  assert(noSupport.intent == EnemyIntent::Retreat);
  assert(noSupport.fallbackReason == EnemyPlanFallbackReason::NoLegalAbility);

  snapshot.playerVisible = false;
  snapshot.targetVisible = false;
  snapshot.allies = {
      {20, EnemyArchetype::Guard, fp(90), 0, {1.0f, 0.0f}, 1.0f, true, true},
      {10, EnemyArchetype::RiftClaw, fp(80), 0, {0.5f, 0.0f}, 0.5f, true, true},
      {30, EnemyArchetype::RiftClaw, fp(80), fp(1), {0.4f, 0.0f}, 0.4f, true, true},
      {40, EnemyArchetype::RiftClaw, fp(80), 0, {0.4f, 0.0f}, 0.4f, false, true},
      {50, EnemyArchetype::RiftClaw, fp(80), 0, {0.4f, 0.0f}, 0.4f, true, false},
      {60, EnemyArchetype::RiftClaw, fp(80), 0, {3.0f, 0.0f}, 3.0f, true, true},
  };
  const EnemyActionPlan support =
      planner.plan(EnemyIntent::Support, snapshot, {supportAbility(99)});
  assert(support.intent == EnemyIntent::Support);
  assert(support.abilityId == 99);
  assert(support.targetId == 10);
  assert(support.desiredPosition == (Vec2{0.5f, 0.0f}));
  assert(support.fallbackReason == EnemyPlanFallbackReason::None);

  PerceptionSnapshot selfAndZero = facts();
  selfAndZero.playerVisible = false;
  selfAndZero.targetVisible = false;
  selfAndZero.allies = {
      {1, EnemyArchetype::Priest, fp(80), 0, {0.1f, 0.0f}, 0.1f, true, true},
      {0, EnemyArchetype::RiftClaw, fp(80), 0, {0.2f, 0.0f}, 0.2f, true, true},
      {8, EnemyArchetype::Guard, fp(80), 0, {0.3f, 0.0f}, 0.3f, true, true},
  };
  const EnemyActionPlan excludesSelfAndZero =
      planner.plan(EnemyIntent::Support, selfAndZero, {supportAbility(99)});
  assert(excludesSelfAndZero.targetId == 8);

  selfAndZero.allies.pop_back();
  const EnemyActionPlan onlySelfAndZero =
      planner.plan(EnemyIntent::Support, selfAndZero, {supportAbility(99)});
  assert(onlySelfAndZero.intent == EnemyIntent::Retreat);
  assert(onlySelfAndZero.fallbackReason == EnemyPlanFallbackReason::NoLegalAbility);

  for (const AllyPerception& invalid : std::vector<AllyPerception>{
           snapshot.allies[2], snapshot.allies[3], snapshot.allies[4], snapshot.allies[5]}) {
    PerceptionSnapshot filtered = facts();
    filtered.playerVisible = false;
    filtered.targetVisible = false;
    filtered.allies = {invalid};
    const EnemyActionPlan rejected =
        planner.plan(EnemyIntent::Support, filtered, {supportAbility(99)});
    assert(rejected.intent == EnemyIntent::Retreat);
    assert(rejected.fallbackReason == EnemyPlanFallbackReason::NoLegalAbility);
  }

  EnemyAbilityState selfSupport = supportAbility(100);
  selfSupport.ability.targetPolicy = EnemyTargetPolicy::Self;
  const EnemyActionPlan selfRejected =
      planner.plan(EnemyIntent::Support, snapshot, {selfSupport});
  assert(selfRejected.intent == EnemyIntent::Retreat);
  assert(selfRejected.fallbackReason == EnemyPlanFallbackReason::NoLegalAbility);

  const EnemyActionPlan idle = planner.plan(EnemyIntent::Idle, facts(), {});
  assert(idle.intent == EnemyIntent::Idle);
  assert(idle.state == EnemyAiState::Idle);
  assert(idle.desiredPosition == facts().selfPosition);
  assert(idle.fallbackReason == EnemyPlanFallbackReason::None);

  const EnemyActionPlan breakFree = planner.plan(EnemyIntent::BreakFree, facts(), {});
  assert(breakFree.intent == EnemyIntent::BreakFree);
  assert(breakFree.state == EnemyAiState::Moving);
  assert(breakFree.desiredPosition == facts().safeReturnPosition);
  assert(breakFree.fallbackReason == EnemyPlanFallbackReason::None);

  snapshot = facts();
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
    assert(samePlan(actual, expected));
  }
}
