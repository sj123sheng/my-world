#include "gameplay/ai/enemy_agent.h"

#include <algorithm>
#include <cassert>
#include <limits>

namespace {

EnemyAbility attackAbility(Tick windupMs = 0) {
  EnemyAbility ability;
  ability.id = 17;
  ability.tag = "agent-test";
  ability.range = fp(1.0);
  ability.windupMs = windupMs;
  ability.activeMs = 1;
  ability.recoveryMs = 0;
  ability.weight = fp(1.0);
  ability.category = EnemyAbilityCategory::Attack;
  ability.targetPolicy = EnemyTargetPolicy::CurrentTarget;
  return ability;
}

EnemyAiConfig configWithRegion(float radius = 5.0f) {
  EnemyAiConfig config;
  config.region = {{0.0f, 0.0f}, radius};
  return config;
}

EnemyUpdateInput inputDefaults() {
  EnemyUpdateInput input;
  input.world = EnemyWorldView::testDefaults();
  input.world.selfId = 10;
  input.world.selfAlive = true;
  input.world.selfPosition = {0.0f, 0.0f};
  input.world.spawnPosition = {0.0f, 0.0f};
  input.world.safeReturnPosition = {-2.0f, 0.0f};
  input.world.playerId = 7;
  input.world.playerPosition = {2.0f, 0.0f};
  input.world.playerVisible = true;
  input.world.playerReachable = true;
  input.execution.targetAlive = true;
  input.execution.baseDamage = fp(10);
  input.execution.poiseDamage = fp(2);
  input.execution.sequence = 99;
  return input;
}

EnemyAgentTuning fastTuning() {
  EnemyAgentTuning tuning;
  tuning.decisionPeriodMs = 100;
  tuning.minimumProgress = 0.1f;
  tuning.noProgressDecisionLimit = 3;
  tuning.chaseTolerance = 0.5f;
  tuning.safePointTolerance = 0.1f;
  tuning.separationDistance = 1.0f;
  return tuning;
}

void testChaseToleranceAndProjection() {
  EnemyAiConfig config = configWithRegion();
  EnemyAgent agent(EnemyArchetype::Guard, config, fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.playerPosition = {5.4f, 0.0f};

  EnemyUpdateResult result = agent.update(input);
  assert(result.intent.has_value());
  assert(*result.intent == EnemyIntent::Chase);
  assert(result.desiredPosition.has_value());
  assert(*result.desiredPosition == (Vec2{5.0f, 0.0f}));
  assert(result.movement.finite());

  input.world.tick = 1;
  input.world.playerPosition = {5.6f, 0.0f};
  result = agent.update(input);
  assert(result.intent.has_value());
  assert(*result.intent == EnemyIntent::ReturnToArea);
  assert(result.desiredPosition == input.world.safeReturnPosition);
}

void testNoProgressEntersEscapeAndSafePointClearsIt() {
  EnemyAgent agent(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  EnemyUpdateInput input = inputDefaults();

  input.world.tick = 0;
  assert(agent.update(input).escapeState == EnemyEscapeState::None);
  input.world.tick = 100;
  assert(agent.update(input).escapeState == EnemyEscapeState::None);
  input.world.tick = 200;
  assert(agent.update(input).escapeState == EnemyEscapeState::None);
  input.world.tick = 300;
  const EnemyUpdateResult stuck = agent.update(input);
  assert(stuck.escapeState == EnemyEscapeState::ReturningToSafePoint);
  assert(stuck.intent == EnemyIntent::BreakFree);
  assert(stuck.desiredPosition == input.world.safeReturnPosition);

  input.world.tick = 400;
  input.world.selfPosition = input.world.safeReturnPosition;
  const EnemyUpdateResult safe = agent.update(input);
  assert(safe.escapeState == EnemyEscapeState::None);
  assert(safe.intent == EnemyIntent::Idle);
  assert(safe.movement.finite());

  input.world.tick = 500;
  input.world.playerPosition = {0.0f, 0.0f};
  const EnemyUpdateResult resumed = agent.update(input);
  assert(resumed.escapeState == EnemyEscapeState::None);
  assert(resumed.intent != EnemyIntent::BreakFree);
}

void testUnreachableStartsRepositioningDeterministically() {
  EnemyAgent agent(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.playerReachable = false;

  const EnemyUpdateResult result = agent.update(input);
  assert(result.escapeState == EnemyEscapeState::Repositioning);
  assert(result.intent == EnemyIntent::BreakFree);
  assert(result.desiredPosition == input.world.safeReturnPosition);
}

void testStableSortedWorldViewProducesStableSeparation() {
  EnemyUpdateInput firstInput = inputDefaults();
  firstInput.world.playerVisible = false;
  firstInput.world.allies = {
      {30, EnemyArchetype::Guard, fp(10), 0, {0.0f, 0.0f}, true, true},
      {20, EnemyArchetype::Guard, fp(10), 0, {0.0f, 0.0f}, true, true},
  };
  EnemyUpdateInput secondInput = firstInput;
  std::reverse(secondInput.world.allies.begin(), secondInput.world.allies.end());

  EnemyAgent first(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  EnemyAgent second(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  const EnemyUpdateResult firstResult = first.update(firstInput);
  const EnemyUpdateResult secondResult = second.update(secondInput);
  assert(firstResult.separation == secondResult.separation);
  assert(firstResult.movement == secondResult.movement);
  assert(firstResult.separation.finite());
}

void testDeathStopsIntentHitAndTargetCandidate() {
  EnemyAiConfig config = configWithRegion();
  config.abilities = {attackAbility(100)};
  EnemyAgent agent(EnemyArchetype::RiftClaw, config, fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.playerPosition = {0.1f, 0.0f};

  input.world.tick = 0;
  const EnemyUpdateResult windingUp = agent.update(input);
  assert(windingUp.intent == EnemyIntent::Attack);
  assert(windingUp.targetCandidate.has_value());
  assert(!windingUp.hit.has_value());

  input.world.tick = 100;
  input.world.selfAlive = false;
  const EnemyUpdateResult dead = agent.update(input);
  assert(!dead.intent.has_value());
  assert(!dead.hit.has_value());
  assert(!dead.targetCandidate.has_value());
  assert(!dead.plan.has_value());
  assert(dead.state == EnemyAiState::Defeated);
  assert(dead.movement.finite());
  assert(dead.separation.finite());

  input.world.tick = 200;
  input.world.selfAlive = true;
  input.world.playerVisible = false;
  const EnemyUpdateResult revived = agent.update(input);
  assert(!revived.hit.has_value());
}

void testStaggerRemainsLatchedUntilExplicitRelease() {
  EnemyAiConfig config = configWithRegion();
  config.abilities = {attackAbility()};
  EnemyAgent agent(EnemyArchetype::RiftClaw, config, fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.playerPosition = {0.1f, 0.0f};
  input.world.staggered = true;

  const EnemyUpdateResult staggered = agent.update(input);
  assert(staggered.intent == EnemyIntent::Idle);
  assert(staggered.state == EnemyAiState::Staggered);
  assert(!staggered.hit.has_value());

  input.world.tick = 100;
  input.world.staggered = false;
  const EnemyUpdateResult stillStaggered = agent.update(input);
  assert(stillStaggered.intent == EnemyIntent::Idle);
  assert(stillStaggered.state == EnemyAiState::Staggered);
  assert(!stillStaggered.hit.has_value());

  agent.releaseStagger();
  input.world.tick = 200;
  const EnemyUpdateResult released = agent.update(input);
  assert(released.intent == EnemyIntent::Attack);
  assert(released.hit.has_value());
}

void testStaggerSuppressesAnExistingEscapeIntent() {
  EnemyAgent agent(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  EnemyUpdateInput input = inputDefaults();
  for (const Tick tick : {0, 100, 200, 300}) {
    input.world.tick = tick;
    agent.update(input);
  }
  assert(agent.escapeState() == EnemyEscapeState::ReturningToSafePoint);

  input.world.tick = 400;
  input.world.staggered = true;
  const EnemyUpdateResult staggered = agent.update(input);
  assert(staggered.intent == EnemyIntent::Idle);
  assert(staggered.state == EnemyAiState::Staggered);
  assert(!staggered.hit.has_value());
}

void testMaximumTickCannotCountTheSameDecisionMoreThanOnce() {
  EnemyAgent agent(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.tick = std::numeric_limits<Tick>::max();

  for (int update = 0; update < 10; ++update) {
    assert(agent.update(input).escapeState == EnemyEscapeState::None);
  }
}

void testInvalidEntityIdDoesNotBecomeTargetCandidate() {
  EnemyAgent agent(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.selfId = 0;

  assert(!agent.update(input).targetCandidate.has_value());
}

void testInvalidGeometryNeverLeaksNan() {
  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  EnemyAgent agent(EnemyArchetype::Guard, configWithRegion(), fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.selfPosition = {nan, infinity};
  input.world.playerPosition = {infinity, nan};
  input.world.selfFacing = {nan, nan};
  input.world.spawnPosition = {infinity, nan};
  input.world.safeReturnPosition = {nan, infinity};
  input.world.allies = {
      {20, EnemyArchetype::Guard, fp(10), 0, {nan, 0.0f}, true, true},
      {30, EnemyArchetype::Guard, fp(10), 0, {0.0f, 0.0f}, true, true},
  };

  const EnemyUpdateResult result = agent.update(input);
  assert(result.movement.finite());
  assert(result.separation.finite());
  if (result.desiredPosition.has_value()) assert(result.desiredPosition->finite());
  if (result.targetCandidate.has_value()) assert(result.targetCandidate->position.finite());
}

void testResetClearsActionAndNoProgressMemory() {
  EnemyAiConfig config = configWithRegion();
  config.abilities = {attackAbility(100)};
  EnemyAgent agent(EnemyArchetype::RiftClaw, config, fastTuning());
  EnemyUpdateInput input = inputDefaults();
  input.world.playerPosition = {0.1f, 0.0f};

  assert(agent.update(input).intent == EnemyIntent::Attack);
  agent.reset();

  input.world.tick = 100;
  input.world.playerVisible = false;
  const EnemyUpdateResult reset = agent.update(input);
  assert(!reset.hit.has_value());
  assert(reset.escapeState == EnemyEscapeState::None);
}

}  // namespace

int main() {
  testChaseToleranceAndProjection();
  testNoProgressEntersEscapeAndSafePointClearsIt();
  testUnreachableStartsRepositioningDeterministically();
  testStableSortedWorldViewProducesStableSeparation();
  testDeathStopsIntentHitAndTargetCandidate();
  testStaggerRemainsLatchedUntilExplicitRelease();
  testStaggerSuppressesAnExistingEscapeIntent();
  testMaximumTickCannotCountTheSameDecisionMoreThanOnce();
  testInvalidEntityIdDoesNotBecomeTargetCandidate();
  testInvalidGeometryNeverLeaksNan();
  testResetClearsActionAndNoProgressMemory();
}
