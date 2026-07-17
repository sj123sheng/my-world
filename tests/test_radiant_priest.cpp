#include "gameplay/ai/enemy_archetypes.h"

#include <cassert>

namespace {

PerceptionSnapshot priestFacts() {
  PerceptionSnapshot facts;
  facts.selfId = 10;
  facts.selfPosition = {0.0f, 0.0f};
  facts.playerPosition = {2.0f, 0.0f};
  facts.playerDistance = 2.0f;
  facts.targetId = 7;
  facts.targetPosition = facts.playerPosition;
  facts.targetDistance = facts.playerDistance;
  facts.targetVisible = true;
  facts.playerVisible = true;
  facts.selfInsideRegion = true;
  facts.playerInsideRegion = true;
  return facts;
}

void testDefaultConfigHasTypedYellowInterruptibleCasts() {
  const EnemyAiConfig config = radiantPriestDefaults();
  assert(config.validated().has_value());
  assert(config.abilities.size() == 2);
  for (const EnemyAbility& ability : config.abilities) {
    assert(ability.telegraph == EnemyAbilityTelegraph::WarningYellow);
    assert(ability.cancelPolicy == EnemyAbilityCancelPolicy::WindupOnly);
    assert(ability.interruptThreshold > 0);
    assert(ability.windupMs > 0);
  }
}

void testMissingShieldAllyWinsAndSelfRefreshIsForbidden() {
  const EnemyAiConfig config = radiantPriestDefaults();
  std::vector<EnemyAbilityState> abilities;
  for (const EnemyAbility& ability : config.abilities) abilities.push_back({ability, 0});

  PerceptionSnapshot facts = priestFacts();
  facts.allies = {
      {10, EnemyArchetype::Priest, fp(100), 0, {0.0f, 0.0f}, 0.0f, true, true},
      {20, EnemyArchetype::RiftClaw, fp(100), fp(10), {1.0f, 0.0f}, 1.0f, true, true},
      {30, EnemyArchetype::Guard, fp(100), 0, {1.5f, 0.0f}, 1.5f, true, true},
  };
  DecisionPolicy policy;
  TacticalPlanner planner;
  assert(policy.choose(facts, EnemyArchetype::Priest) == EnemyIntent::Support);
  const EnemyActionPlan support = planner.plan(EnemyIntent::Support, facts, abilities);
  assert(support.targetId == 30);
  assert(support.ability->category == EnemyAbilityCategory::Support);
  assert(support.ability->targetPolicy == EnemyTargetPolicy::LowestShieldAlly);
  assert(support.ability->effect == EnemyAbilityEffect::Shield);

  facts.allies.resize(1);
  assert(policy.choose(facts, EnemyArchetype::Priest) != EnemyIntent::Support);
  const EnemyActionPlan noSelfRefresh = planner.plan(EnemyIntent::Support, facts, abilities);
  assert(!noSelfRefresh.ability.has_value());
  assert(noSelfRefresh.targetId != facts.selfId);
}

void testYellowCastHasARealInterruptWindow() {
  const EnemyAiConfig config = radiantPriestDefaults();
  const EnemyAbility& cast = config.abilities.front();
  EnemyActionPlan plan;
  plan.intent = cast.category == EnemyAbilityCategory::Support ? EnemyIntent::Support
                                                               : EnemyIntent::Attack;
  plan.state = EnemyAiState::Acting;
  plan.ability = cast;
  plan.targetId = 20;

  ActionExecutor executor;
  assert(executor.start(plan, 1000));
  EnemyExecutionContext context;
  context.targetAlive = true;
  assert(executor.update(1000, 0, context).phase == EnemyActionPhase::Windup);
  assert(!executor.interrupt(1001, cast.interruptThreshold - 1));
  assert(executor.interrupt(1001, cast.interruptThreshold));
  const EnemyExecutionResult interrupted = executor.update(1001, 1, context);
  assert(interrupted.interrupted);
  assert(interrupted.phase == EnemyActionPhase::Recovery);
  assert(!interrupted.hit.has_value());
}

}  // namespace

int main() {
  testDefaultConfigHasTypedYellowInterruptibleCasts();
  testMissingShieldAllyWinsAndSelfRefreshIsForbidden();
  testYellowCastHasARealInterruptWindow();
}
