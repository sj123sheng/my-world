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
  const EnemyAbility& shield = config.abilities[0];
  const EnemyAbility& bolt = config.abilities[1];
  assert(shield.id == enemy_ability_ids::kRadiantPriestShield);
  assert(shield.effect == EnemyAbilityEffect::Shield);
  assert(shield.effectAmount == fp(40));
  assert(shield.range == fp(4.0));
  assert(shield.cooldownMs == 3000);
  assert(shield.windupMs == 700);
  assert(shield.activeMs == 100);
  assert(shield.recoveryMs == 500);
  assert(shield.weight == fp(2.0));
  assert(shield.category == EnemyAbilityCategory::Support);
  assert(shield.targetPolicy == EnemyTargetPolicy::LowestShieldAlly);
  assert(bolt.id == enemy_ability_ids::kRadiantPriestBolt);
  assert(bolt.effect == EnemyAbilityEffect::Damage);
  assert(bolt.effectAmount == 0);
  assert(bolt.range == fp(4.0));
  assert(bolt.cooldownMs == 1500);
  assert(bolt.windupMs == 600);
  assert(bolt.activeMs == 80);
  assert(bolt.recoveryMs == 400);
  assert(bolt.weight == fp(1.0));
  assert(bolt.category == EnemyAbilityCategory::Attack);
  assert(bolt.targetPolicy == EnemyTargetPolicy::CurrentTarget);
  for (const EnemyAbility& ability : config.abilities) {
    assert(ability.telegraph == EnemyAbilityTelegraph::WarningYellow);
    assert(ability.cancelPolicy == EnemyAbilityCancelPolicy::WindupOnly);
    assert(ability.interruptThreshold > 0);
    assert(ability.windupMs > 0);
  }
}

void testAgentShieldTimelineEmitsOneEffectTransaction() {
  EnemyAgent agent(EnemyArchetype::Priest, radiantPriestDefaults());
  EnemyUpdateInput input;
  input.world = EnemyWorldView::testDefaults();
  input.world.selfId = 10;
  input.world.selfPosition = {0.0f, 0.0f};
  input.world.playerPosition = {2.0f, 0.0f};
  input.world.allies = {
      {30, EnemyArchetype::Guard, fp(100), 0, {1.5f, 0.0f}, true, true},
  };
  input.execution.targetAlive = true;
  input.execution.sequence = 77;

  const EnemyUpdateResult started = agent.update(input);
  assert(started.phase == EnemyActionPhase::Windup);
  assert(!started.hit.has_value());
  assert(!started.effect.has_value());

  input.world.tick = 699;
  input.dtMs = 699;
  const EnemyUpdateResult beforeEffect = agent.update(input);
  assert(!beforeEffect.hit.has_value());
  assert(!beforeEffect.effect.has_value());

  input.world.tick = 700;
  input.dtMs = 1;
  const EnemyUpdateResult applied = agent.update(input);
  assert(!applied.hit.has_value());
  assert(applied.effect.has_value());
  assert(applied.effect->type == CombatEffectType::Shield);
  assert(applied.effect->target == 30);
  assert(applied.effect->amount == fp(40));
  assert(applied.effect->tick == 700);
  assert(applied.effect->sequence == 77);
  assert(applied.effect->transactionId != 0);

  input.dtMs = 0;
  const EnemyUpdateResult repeated = agent.update(input);
  assert(!repeated.hit.has_value());
  assert(!repeated.effect.has_value());

  input.world.tick = 800;
  input.dtMs = 100;
  const EnemyUpdateResult recovering = agent.update(input);
  assert(recovering.phase == EnemyActionPhase::Recovery);
  assert(!recovering.hit.has_value());
  assert(!recovering.effect.has_value());

  input.world.tick = 1300;
  input.dtMs = 500;
  const EnemyUpdateResult completed = agent.update(input);
  assert(completed.phase == EnemyActionPhase::None);
  assert(!completed.hit.has_value());
  assert(!completed.effect.has_value());
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

EnemyUpdateInput priestAttackInput() {
  EnemyUpdateInput input;
  input.world = EnemyWorldView::testDefaults();
  input.world.selfId = 10;
  input.world.selfPosition = {0.0f, 0.0f};
  input.world.playerPosition = {2.0f, 0.0f};
  input.execution.targetAlive = true;
  return input;
}

void testAgentOrdinaryInterruptUsesThresholdAndKeepsCooldown() {
  const EnemyAiConfig config = radiantPriestDefaults();
  const EnemyAbility& bolt = config.abilities[1];
  EnemyAgent agent(EnemyArchetype::Priest, config);
  EnemyUpdateInput input = priestAttackInput();

  assert(agent.update(input).phase == EnemyActionPhase::Windup);

  input.world.tick = 100;
  input.dtMs = 100;
  input.interrupt = EnemyAgentInterrupt{bolt.interruptThreshold - 1};
  const EnemyUpdateResult belowThreshold = agent.update(input);
  assert(belowThreshold.phase == EnemyActionPhase::Windup);
  assert(!belowThreshold.interrupted);

  input.world.tick = 200;
  input.dtMs = 100;
  input.interrupt = EnemyAgentInterrupt{bolt.interruptThreshold};
  const EnemyUpdateResult interrupted = agent.update(input);
  assert(interrupted.phase == EnemyActionPhase::Recovery);
  assert(interrupted.state == EnemyAiState::Recovering);
  assert(interrupted.interrupted);
  assert(!interrupted.hit.has_value());
  assert(!interrupted.effect.has_value());

  input.world.tick = 600;
  input.dtMs = 400;
  input.interrupt.reset();
  assert(agent.update(input).phase == EnemyActionPhase::None);

  input.world.tick = 601;
  input.dtMs = 1;
  const EnemyUpdateResult cooldownHeld = agent.update(input);
  assert(!cooldownHeld.plan->ability.has_value());
}

void testAgentWindupOnlyCastRejectsActivePhaseInterrupt() {
  const EnemyAiConfig config = radiantPriestDefaults();
  const EnemyAbility& bolt = config.abilities[1];
  EnemyAgent agent(EnemyArchetype::Priest, config);
  EnemyUpdateInput input = priestAttackInput();

  assert(agent.update(input).phase == EnemyActionPhase::Windup);
  input.world.tick = bolt.windupMs;
  input.dtMs = bolt.windupMs;
  assert(agent.update(input).phase == EnemyActionPhase::Active);

  input.world.tick = bolt.windupMs + 1;
  input.dtMs = 1;
  input.interrupt = EnemyAgentInterrupt{bolt.interruptThreshold};
  const EnemyUpdateResult active = agent.update(input);
  assert(active.phase == EnemyActionPhase::Active);
  assert(!active.interrupted);
}

}  // namespace

int main() {
  testDefaultConfigHasTypedYellowInterruptibleCasts();
  testAgentShieldTimelineEmitsOneEffectTransaction();
  testMissingShieldAllyWinsAndSelfRefreshIsForbidden();
  testYellowCastHasARealInterruptWindow();
  testAgentOrdinaryInterruptUsesThresholdAndKeepsCooldown();
  testAgentWindupOnlyCastRejectsActivePhaseInterrupt();
}
