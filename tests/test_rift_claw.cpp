#include "gameplay/ai/enemy_archetypes.h"

#include <cassert>
#include <limits>

namespace {

EnemyUpdateInput baseInput() {
  EnemyUpdateInput input;
  input.world = EnemyWorldView::testDefaults();
  input.world.selfId = 10;
  input.world.selfPosition = {0.0f, 0.0f};
  input.world.spawnPosition = {0.0f, 0.0f};
  input.world.safeReturnPosition = {-2.0f, 0.0f};
  input.world.playerId = 7;
  input.world.playerPosition = {2.0f, 0.0f};
  input.execution.targetAlive = true;
  input.execution.baseDamage = fp(10);
  input.execution.poiseDamage = fp(4);
  return input;
}

void testDefaultConfigIsLegalAndUsesTypedAttackFields() {
  const EnemyAiConfig config = riftClawDefaults();
  assert(config.validated().has_value());
  assert(config.abilities.size() == 1);
  const EnemyAbility& claw = config.abilities.front();
  assert(claw.id == enemy_ability_ids::kRiftClawSlash);
  assert(claw.category == EnemyAbilityCategory::Attack);
  assert(claw.targetPolicy == EnemyTargetPolicy::CurrentTarget);
  assert(claw.effect == EnemyAbilityEffect::Damage);
  assert(claw.cancelPolicy == EnemyAbilityCancelPolicy::WindupOnly);
  assert(claw.windupMs > 0);
}

void testAlertChaseWindupSeparationAndReturn() {
  EnemyAgent agent(EnemyArchetype::RiftClaw, riftClawDefaults());
  EnemyUpdateInput input = baseInput();
  input.world.playerVisible = false;
  assert(agent.update(input).intent == EnemyIntent::Idle);

  input.world.tick = 1;
  input.world.playerVisible = true;
  const EnemyUpdateResult chasing = agent.update(input);
  assert(chasing.intent == EnemyIntent::Chase);

  input.world.tick = 2;
  input.world.playerPosition = {0.2f, 0.0f};
  input.world.allies = {
      {20, EnemyArchetype::RiftClaw, fp(10), 0, {0.0f, 0.0f}, true, true},
  };
  const EnemyUpdateResult windingUp = agent.update(input);
  assert(windingUp.intent == EnemyIntent::Attack);
  assert(windingUp.phase == EnemyActionPhase::Windup);
  assert(windingUp.separation.finite());
  assert(windingUp.separation.length() > 0.0f);

  input.world.tick = 3;
  input.world.playerPosition = {20.0f, 0.0f};
  input.world.allies.clear();
  const EnemyUpdateResult returning = agent.update(input);
  assert(returning.intent == EnemyIntent::ReturnToArea);
  assert(returning.desiredPosition == input.world.safeReturnPosition);
}

void testCooldownStartsOnlyOnSuccessfulStartAndUsesFixedDt() {
  EnemyAiConfig config = riftClawDefaults();
  EnemyAbility& claw = config.abilities.front();
  claw.cooldownMs = 50;
  claw.windupMs = 0;
  claw.activeMs = 100;
  claw.recoveryMs = 100;
  EnemyAgent agent(EnemyArchetype::RiftClaw, config);
  EnemyUpdateInput input = baseInput();
  input.world.playerPosition = {0.2f, 0.0f};

  input.world.tick = 0;
  assert(agent.update(input).plan->ability.has_value());

  input.world.tick = 50;
  input.dtMs = 50;
  assert(agent.update(input).phase == EnemyActionPhase::Active);

  input.world.tick = 100;
  input.dtMs = 50;
  assert(agent.update(input).phase == EnemyActionPhase::Recovery);

  input.world.tick = 200;
  input.dtMs = 100;
  assert(agent.update(input).phase == EnemyActionPhase::None);

  input.world.tick = 201;
  input.dtMs = 1;
  const EnemyUpdateResult restarted = agent.update(input);
  assert(restarted.plan->ability.has_value());
  assert(restarted.phase == EnemyActionPhase::Active);
}

void testCancelPoiseBreakResetAndDeathHaveDefinedCooldownSemantics() {
  EnemyAiConfig config = riftClawDefaults();
  EnemyAbility& claw = config.abilities.front();
  claw.cooldownMs = 1000;
  claw.windupMs = 100;
  claw.activeMs = 10;
  claw.recoveryMs = 10;
  EnemyAgent agent(EnemyArchetype::RiftClaw, config);
  EnemyUpdateInput input = baseInput();
  input.world.playerPosition = {0.2f, 0.0f};

  assert(agent.update(input).phase == EnemyActionPhase::Windup);
  input.world.tick = 10;
  input.dtMs = 10;
  input.world.playerPosition = {20.0f, 0.0f};
  assert(agent.update(input).intent == EnemyIntent::ReturnToArea);

  input.world.tick = 20;
  input.dtMs = 10;
  input.world.playerPosition = {0.2f, 0.0f};
  assert(!agent.update(input).plan->ability.has_value());

  input.world.tick = 30;
  input.dtMs = 10;
  input.world.staggered = true;
  assert(agent.update(input).state == EnemyAiState::Staggered);
  agent.releaseStagger();
  input.world.tick = 40;
  input.dtMs = 10;
  input.world.staggered = false;
  assert(!agent.update(input).plan->ability.has_value());

  agent.reset();
  input.world.tick = 41;
  input.dtMs = 0;
  assert(agent.update(input).plan->ability.has_value());

  input.world.tick = 42;
  input.world.selfAlive = false;
  assert(agent.update(input).state == EnemyAiState::Defeated);
  input.world.tick = 43;
  input.world.selfAlive = true;
  input.world.staggered = false;
  assert(agent.update(input).plan->ability.has_value());
}

void testCooldownDtSaturatesAtZero() {
  EnemyAiConfig config = riftClawDefaults();
  EnemyAbility& claw = config.abilities.front();
  claw.cooldownMs = 100;
  claw.windupMs = 0;
  claw.activeMs = 0;
  claw.recoveryMs = 0;
  EnemyAgent agent(EnemyArchetype::RiftClaw, config);
  EnemyUpdateInput input = baseInput();
  input.world.playerPosition = {0.2f, 0.0f};
  assert(agent.update(input).plan->ability.has_value());

  input.world.tick = std::numeric_limits<Tick>::max();
  input.dtMs = std::numeric_limits<int64_t>::max();
  assert(agent.update(input).plan->ability.has_value());
}

}  // namespace

int main() {
  testDefaultConfigIsLegalAndUsesTypedAttackFields();
  testAlertChaseWindupSeparationAndReturn();
  testCooldownStartsOnlyOnSuccessfulStartAndUsesFixedDt();
  testCancelPoiseBreakResetAndDeathHaveDefinedCooldownSemantics();
  testCooldownDtSaturatesAtZero();
}
