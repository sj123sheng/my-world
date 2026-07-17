#include "gameplay/ai/enemy_archetypes.h"
#include "gameplay/combat/damage_resolver.h"

#include <cassert>

namespace {

HitRequest guardHit() {
  HitRequest hit;
  hit.baseDamage = fp(20);
  hit.poiseDamage = fp(25);
  return hit;
}

void testDefaultConfigIsLegalAndHasLongStagger() {
  const EnemyAiConfig config = corrosionGuardDefaults();
  assert(config.validated().has_value());
  assert(config.staggerRecoveryMs >= 1000);
  assert(config.abilities.size() == 1);
  assert(config.abilities.front().id == enemy_ability_ids::kCorrosionGuardBash);
  assert(config.abilities.front().cancelPolicy ==
         EnemyAbilityCancelPolicy::Uninterruptible);
}

void testFrontReducesHpDamageAndBackTakesNormalDamage() {
  DamageResolver resolver(CombatConfig::defaults());
  const DirectionalDefenseProfile defense = corrosionGuardDefense();

  TrainingTarget frontTarget = TrainingTarget::defaults();
  DamageResolutionContext front;
  front.attackerPosition = {1.0f, 0.0f};
  front.defenderPosition = {0.0f, 0.0f};
  front.defenderFacing = {1.0f, 0.0f};
  front.directionalDefense = defense;
  const DamageOutcome frontDamage = resolver.resolve(frontTarget, guardHit(), front);
  assert(frontDamage.hpDamage == fp(10));
  assert(frontDamage.poiseDamage == fp(25));

  TrainingTarget backTarget = TrainingTarget::defaults();
  DamageResolutionContext back = front;
  back.attackerPosition = {-1.0f, 0.0f};
  const DamageOutcome backDamage = resolver.resolve(backTarget, guardHit(), back);
  assert(backDamage.hpDamage == fp(20));
  assert(backDamage.poiseDamage == fp(25));
}

void testPoiseBreakCancelsAndLongStaggerExpiresDeterministically() {
  EnemyAiConfig config = corrosionGuardDefaults();
  EnemyAbility& bash = config.abilities.front();
  bash.cooldownMs = 2000;
  EnemyAgent agent(EnemyArchetype::Guard, config);
  EnemyUpdateInput input;
  input.world = EnemyWorldView::testDefaults();
  input.world.selfId = 10;
  input.world.selfPosition = {0.0f, 0.0f};
  input.world.playerPosition = {0.2f, 0.0f};
  input.execution.targetAlive = true;

  assert(agent.update(input).phase == EnemyActionPhase::Windup);
  input.world.tick = 10;
  input.dtMs = 10;
  input.world.staggered = true;
  const EnemyUpdateResult broken = agent.update(input);
  assert(broken.state == EnemyAiState::Staggered);
  assert(!broken.hit.has_value());

  input.world.staggered = false;
  input.world.tick = config.staggerRecoveryMs;
  input.dtMs = config.staggerRecoveryMs - 10;
  assert(agent.update(input).state == EnemyAiState::Staggered);

  input.world.staggered = true;
  input.world.tick = config.staggerRecoveryMs + 10;
  input.dtMs = 10;
  assert(agent.update(input).state == EnemyAiState::Staggered);

  input.world.tick = config.staggerRecoveryMs + 100;
  input.dtMs = 90;
  assert(agent.update(input).state == EnemyAiState::Staggered);

  input.world.staggered = false;
  input.world.tick = config.staggerRecoveryMs + 101;
  input.dtMs = 1;
  const EnemyUpdateResult recovered = agent.update(input);
  assert(recovered.state != EnemyAiState::Staggered);
  assert(!recovered.plan->ability.has_value());
}

void testResetClearsTheLatchedStaggerDeadline() {
  EnemyAiConfig config = corrosionGuardDefaults();
  EnemyAgent agent(EnemyArchetype::Guard, config);
  EnemyUpdateInput input;
  input.world = EnemyWorldView::testDefaults();
  input.world.selfId = 10;
  input.world.selfPosition = {0.0f, 0.0f};
  input.world.playerPosition = {2.0f, 0.0f};
  input.world.staggered = true;
  assert(agent.update(input).state == EnemyAiState::Staggered);

  agent.reset();
  input.world.tick = 1;
  input.world.staggered = false;
  assert(agent.update(input).state != EnemyAiState::Staggered);
}

void testDeathClearsTheLatchedStaggerDeadline() {
  EnemyAiConfig config = corrosionGuardDefaults();
  EnemyAgent agent(EnemyArchetype::Guard, config);
  EnemyUpdateInput input;
  input.world = EnemyWorldView::testDefaults();
  input.world.selfId = 10;
  input.world.selfPosition = {0.0f, 0.0f};
  input.world.playerPosition = {2.0f, 0.0f};
  input.world.staggered = true;
  assert(agent.update(input).state == EnemyAiState::Staggered);

  input.world.tick = 1;
  input.world.selfAlive = false;
  assert(agent.update(input).state == EnemyAiState::Defeated);

  input.world.tick = 2;
  input.world.selfAlive = true;
  input.world.staggered = false;
  assert(agent.update(input).state != EnemyAiState::Staggered);
}

}  // namespace

int main() {
  testDefaultConfigIsLegalAndHasLongStagger();
  testFrontReducesHpDamageAndBackTakesNormalDamage();
  testPoiseBreakCancelsAndLongStaggerExpiresDeterministically();
  testResetClearsTheLatchedStaggerDeadline();
  testDeathClearsTheLatchedStaggerDeadline();
}
