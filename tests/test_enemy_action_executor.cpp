#include "gameplay/ai/action_executor.h"

#include <cassert>
#include <limits>

namespace {

EnemyAbility abilityWithTimings(
    Tick windupMs, Tick activeMs, Tick recoveryMs,
    EnemyAbilityCancelPolicy cancelPolicy = EnemyAbilityCancelPolicy::WindupOnly,
    FixedPoint interruptThreshold = fp(10)) {
  EnemyAbility ability;
  ability.id = 17;
  ability.tag = "timeline-test";
  ability.range = fp(2);
  ability.windupMs = windupMs;
  ability.activeMs = activeMs;
  ability.recoveryMs = recoveryMs;
  ability.weight = fp(1);
  ability.cancelPolicy = cancelPolicy;
  ability.interruptThreshold = interruptThreshold;
  return ability;
}

EnemyAbility shieldAbility() {
  EnemyAbility ability = abilityWithTimings(200, 16, 300);
  ability.id = 18;
  ability.category = EnemyAbilityCategory::Support;
  ability.targetPolicy = EnemyTargetPolicy::LowestShieldAlly;
  ability.effect = EnemyAbilityEffect::Shield;
  ability.effectAmount = fp(40);
  return ability;
}

EnemyActionPlan planFor(const EnemyAbility& ability) {
  EnemyActionPlan plan;
  plan.createdAt = 900;
  plan.intent = EnemyIntent::Attack;
  plan.state = EnemyAiState::Acting;
  plan.ability = ability;
  plan.targetId = 7;
  return plan;
}

EnemyExecutionContext context() {
  EnemyExecutionContext value;
  value.attacker = 3;
  value.targetAlive = true;
  value.baseDamage = fp(12);
  value.poiseDamage = fp(4);
  value.sequence = 99;
  return value;
}

void testFixedTimelineAndSingleHit() {
  ActionExecutor executor;
  const EnemyActionPlan plan = planFor(abilityWithTimings(200, 16, 300));
  const EnemyExecutionContext updateContext = context();

  assert(executor.start(plan, 1000));
  const EnemyExecutionResult beforeHit = executor.update(1199, 199, updateContext);
  assert(beforeHit.phase == EnemyActionPhase::Windup);
  assert(!beforeHit.hit.has_value());

  const EnemyExecutionResult onHit = executor.update(1200, 1, updateContext);
  assert(onHit.phase == EnemyActionPhase::Active);
  assert(onHit.hit.has_value());
  assert(!onHit.effect.has_value());
  assert(onHit.hit->attacker == 3);
  assert(onHit.hit->target == 7);
  assert(onHit.hit->ability == 17);
  assert(onHit.hit->baseDamage == fp(12));
  assert(onHit.hit->poiseDamage == fp(4));
  assert(onHit.hit->tick == 1200);
  assert(onHit.hit->sequence == 99);
  assert(onHit.hit->transactionId != 0);

  const EnemyExecutionResult repeated = executor.update(1200, 0, updateContext);
  assert(!repeated.hit.has_value());
  assert(!repeated.effect.has_value());
}

void testShieldProducesOneEffectAndNeverAHit() {
  EnemyAbility emptyShield = shieldAbility();
  emptyShield.effectAmount = 0;
  ActionExecutor invalidExecutor;
  assert(!invalidExecutor.start(planFor(emptyShield), 1000));

  ActionExecutor executor;
  const EnemyActionPlan plan = planFor(shieldAbility());
  const EnemyExecutionContext updateContext = context();

  assert(executor.start(plan, 1000));
  const EnemyExecutionResult beforeEffect = executor.update(1199, 199, updateContext);
  assert(!beforeEffect.hit.has_value());
  assert(!beforeEffect.effect.has_value());

  const EnemyExecutionResult onEffect = executor.update(1200, 1, updateContext);
  assert(!onEffect.hit.has_value());
  assert(onEffect.effect.has_value());
  assert(onEffect.effect->type == CombatEffectType::Shield);
  assert(onEffect.effect->target == 7);
  assert(onEffect.effect->amount == fp(40));
  assert(onEffect.effect->tick == 1200);
  assert(onEffect.effect->sequence == 99);
  assert(onEffect.effect->transactionId != 0);

  const EnemyExecutionResult repeated = executor.update(1200, 0, updateContext);
  assert(!repeated.hit.has_value());
  assert(!repeated.effect.has_value());
}

void testLargeTickKeepsOriginalHitTickAndCrossesAllPhases() {
  ActionExecutor executor;
  const EnemyActionPlan plan = planFor(abilityWithTimings(200, 16, 300));
  assert(executor.start(plan, 1000));

  const EnemyExecutionResult result = executor.update(5000, 4000, context());
  assert(result.hit.has_value());
  assert(result.hit->tick == 1200);
  assert(result.phase == EnemyActionPhase::None);
  assert(result.state == EnemyAiState::Idle);
  assert(result.actionCompleted);
  assert(executor.start(plan, 5000));
}

void testRecoveryRejectsNewAction() {
  ActionExecutor executor;
  const EnemyActionPlan plan = planFor(abilityWithTimings(200, 16, 300));
  assert(executor.start(plan, 1000));
  assert(executor.update(1200, 200, context()).hit.has_value());
  assert(!executor.start(plan, 1200));

  const EnemyExecutionResult recovery = executor.update(1216, 16, context());
  assert(recovery.phase == EnemyActionPhase::Recovery);
  assert(recovery.state == EnemyAiState::Recovering);
  assert(!executor.start(plan, 1216));
  assert(!executor.start(plan, 1515));

  assert(executor.update(1516, 300, context()).phase == EnemyActionPhase::None);
  assert(executor.start(plan, 1516));
}

void testOrdinaryInterruptNeedsPolicyPhaseAndThreshold() {
  const EnemyActionPlan plan = planFor(abilityWithTimings(
      200, 16, 300, EnemyAbilityCancelPolicy::WindupOnly, fp(10)));
  ActionExecutor belowThreshold;
  assert(belowThreshold.start(plan, 1000));
  assert(!belowThreshold.interrupt(1100, fp(9)));
  assert(belowThreshold.update(1100, 100, context()).phase == EnemyActionPhase::Windup);

  assert(belowThreshold.interrupt(1100, fp(10)));
  const EnemyExecutionResult interrupted = belowThreshold.update(1100, 0, context());
  assert(interrupted.phase == EnemyActionPhase::Recovery);
  assert(interrupted.state == EnemyAiState::Recovering);
  assert(!interrupted.hit.has_value());
  assert(!belowThreshold.start(plan, 1399));
  assert(belowThreshold.update(1400, 300, context()).phase == EnemyActionPhase::None);

  ActionExecutor wrongPhase;
  assert(wrongPhase.start(plan, 1000));
  assert(wrongPhase.update(1200, 200, context()).hit.has_value());
  assert(!wrongPhase.interrupt(1200, fp(10)));
}

void testPoiseBreakAlwaysCancelsIntoStagger() {
  ActionExecutor executor;
  const EnemyActionPlan plan = planFor(abilityWithTimings(
      200, 16, 300, EnemyAbilityCancelPolicy::Uninterruptible, fp(1000)));
  assert(executor.start(plan, 1000));
  assert(!executor.interrupt(1050, fp(1000)));
  assert(executor.interrupt(1050, 0, EnemyInterruptCause::PoiseBreak));

  const EnemyExecutionResult staggered = executor.update(1050, 50, context());
  assert(staggered.phase == EnemyActionPhase::None);
  assert(staggered.state == EnemyAiState::Staggered);
  assert(staggered.staggered);
  assert(!staggered.hit.has_value());
  assert(!executor.start(plan, 1350));
  assert(executor.update(5000, 3950, context()).state == EnemyAiState::Staggered);
  executor.reset();
  assert(executor.start(plan, 5000));
}

void testPlanAndAbilitySnapshotAreImmutableDuringExecution() {
  ActionExecutor executor;
  EnemyActionPlan plan = planFor(abilityWithTimings(200, 16, 300));
  assert(executor.start(plan, 1000));

  plan.ability->id = 999;
  plan.ability->windupMs = 900;
  plan.targetId = 88;

  const EnemyExecutionResult hit = executor.update(1200, 200, context());
  assert(hit.hit.has_value());
  assert(hit.hit->ability == 17);
  assert(hit.hit->target == 7);
  assert(hit.hit->tick == 1200);
}

void testNoAbilityAndDeadTargetCannotProduceTransaction() {
  ActionExecutor executor;
  EnemyActionPlan movement;
  movement.intent = EnemyIntent::Chase;
  movement.state = EnemyAiState::Moving;
  movement.targetId = 7;
  assert(!executor.start(movement, 1000));

  const EnemyActionPlan plan = planFor(abilityWithTimings(200, 16, 300));
  assert(executor.start(plan, 1000));
  EnemyExecutionContext deadTarget = context();
  deadTarget.targetAlive = false;
  const EnemyExecutionResult cancelled = executor.update(1200, 200, deadTarget);
  assert(!cancelled.hit.has_value());
  assert(!cancelled.effect.has_value());
  assert(cancelled.phase == EnemyActionPhase::Recovery);
}

void testTargetDeathAfterHitDoesNotExtendRecovery() {
  ActionExecutor executor;
  const EnemyActionPlan plan = planFor(abilityWithTimings(200, 16, 300));
  assert(executor.start(plan, 1000));
  assert(executor.update(1200, 200, context()).hit.has_value());

  EnemyExecutionContext deadTarget = context();
  deadTarget.targetAlive = false;
  assert(executor.update(1400, 200, deadTarget).phase == EnemyActionPhase::Recovery);
  assert(executor.update(1516, 116, deadTarget).phase == EnemyActionPhase::None);
}

void testSaturatedDeadlinesDoNotOverflowUnderUbsan() {
  ActionExecutor executor;
  const Tick maximum = std::numeric_limits<Tick>::max();
  const EnemyActionPlan plan = planFor(abilityWithTimings(200, 16, 300));
  assert(executor.start(plan, maximum - 100));
  const EnemyExecutionResult result = executor.update(maximum, 100, context());
  assert(result.hit.has_value());
  assert(result.hit->tick == maximum);
  assert(result.phase == EnemyActionPhase::None);
}

}  // namespace

int main() {
  testFixedTimelineAndSingleHit();
  testShieldProducesOneEffectAndNeverAHit();
  testLargeTickKeepsOriginalHitTickAndCrossesAllPhases();
  testRecoveryRejectsNewAction();
  testOrdinaryInterruptNeedsPolicyPhaseAndThreshold();
  testPoiseBreakAlwaysCancelsIntoStagger();
  testPlanAndAbilitySnapshotAreImmutableDuringExecution();
  testNoAbilityAndDeadTargetCannotProduceTransaction();
  testTargetDeathAfterHitDoesNotExtendRecovery();
  testSaturatedDeadlinesDoNotOverflowUnderUbsan();
}
