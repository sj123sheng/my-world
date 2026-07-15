#include "../native/gameplay/combat/action_state_machine.h"
#include "../native/gameplay/combat/damage_resolver.h"

#include <cassert>

namespace {

ActionContext targetContext() { return {1, 2, true, false, false}; }

HitRequest cast(ActionStateMachine& machine,
                CombatAction action,
                Tick requestTick,
                Tick hitTick,
                uint64_t sequence) {
  machine.update(requestTick, 0, targetContext());
  assert(machine.request({action, sequence}, targetContext()).accepted);
  const auto hit = machine.update(hitTick, hitTick - requestTick, targetContext());
  assert(hit.has_value());
  return *hit;
}

bool landed(const DamageOutcome& outcome) {
  return outcome.hpDamage > 0 || outcome.poiseDamage > 0;
}

void testResourcesCommitOnlyAfterResolvedHit() {
  ActionStateMachine machine(CombatConfig::defaults());
  machine.grantInsight(0);
  const HitRequest hit = cast(machine, CombatAction::Current, 0, 160, 41);
  assert(hit.baseDamage == fp(24) && hit.sourceAmount == fp(1.5));
  assert(machine.hasInsight());
  assert(machine.resonance() == 0);
  assert(machine.request({CombatAction::Radiance, 99}, targetContext()).reason ==
         ActionRejectReason::ActionLocked);

  TrainingTarget target = TrainingTarget::defaults();
  const DamageOutcome outcome = DamageResolver(CombatConfig::defaults()).resolve(target, hit);
  assert(landed(outcome));
  assert(hit.transactionId != 0);
  assert(machine.confirmHit(hit.transactionId, landed(outcome)));
  assert(!machine.hasInsight());
  assert(machine.resonance() == fp(10));
  assert(!machine.confirmHit(hit.transactionId, true));
  assert(machine.resonance() == fp(10));
  assert(machine.request({CombatAction::Current, 42}, targetContext()).reason ==
         ActionRejectReason::Cooldown);
}

void testDeadTargetResolutionDoesNotCommitResourcesOrHistory() {
  ActionStateMachine machine(CombatConfig::defaults());
  machine.grantInsight(0);
  const HitRequest hit = cast(machine, CombatAction::Radiance, 0, 160, 51);

  TrainingTarget target = TrainingTarget::defaults();
  HitRequest lethal;
  lethal.baseDamage = fp(300);
  lethal.tick = 100;
  assert(DamageResolver(CombatConfig::defaults()).resolve(target, lethal).killed);
  const DamageOutcome outcome = DamageResolver(CombatConfig::defaults()).resolve(target, hit);
  assert(!landed(outcome));
  assert(machine.confirmHit(hit.transactionId, landed(outcome)));
  assert(machine.hasInsight());
  assert(machine.resonance() == 0);

  machine.recordDistinctSource(SourceType::Current, 1000);
  machine.recordDistinctSource(SourceType::Corruption, 2000);
  assert(machine.resonance() == 0);
  assert(machine.request({CombatAction::Radiance, 52}, targetContext()).accepted);
}

void testMismatchedConfirmationDoesNotCommitOrDuplicate() {
  ActionStateMachine machine(CombatConfig::defaults());
  const HitRequest hit = cast(machine, CombatAction::Corruption, 0, 160, 61);
  assert(!machine.confirmHit(hit.transactionId + 1, true));
  assert(machine.resonance() == 0);
  assert(machine.confirmHit(hit.transactionId, false));
  assert(machine.resonance() == 0);
  assert(!machine.confirmHit(hit.transactionId, true));
}

void testExternalSequenceReuseCannotConfirmAnotherTransaction() {
  ActionStateMachine machine(CombatConfig::defaults());
  const HitRequest first = cast(machine, CombatAction::Radiance, 0, 160, 77);
  assert(machine.confirmHit(first.transactionId, false));

  const HitRequest second = cast(machine, CombatAction::Current, 160, 320, 77);
  assert(first.sequence == second.sequence);
  assert(first.transactionId != second.transactionId);
  assert(!machine.confirmHit(first.transactionId, true));
  assert(machine.resonance() == 0);
  assert(machine.confirmHit(second.transactionId, true));
  assert(machine.resonance() == fp(10));
}

void testResetDoesNotReuseTransactionIdentity() {
  ActionStateMachine machine(CombatConfig::defaults());
  const HitRequest beforeReset = cast(machine, CombatAction::Radiance, 0, 160, 88);
  machine.reset();
  const HitRequest afterReset = cast(machine, CombatAction::Current, 0, 160, 88);
  assert(beforeReset.transactionId != afterReset.transactionId);
  assert(!machine.confirmHit(beforeReset.transactionId, true));
  assert(machine.resonance() == 0);
  assert(machine.confirmHit(afterReset.transactionId, true));
  assert(machine.resonance() == fp(10));
}

void testThreeSourceHitsAndCooldownBoundaries() {
  ActionStateMachine radiance(CombatConfig::defaults());
  const HitRequest light = cast(radiance, CombatAction::Radiance, 0, 160, 1);
  assert(light.source == SourceType::Radiance);
  assert(light.baseDamage == fp(20) && light.poiseDamage == fp(6));
  assert(light.tick == 160 && light.sequence == 1);
  assert(radiance.resonance() == 0);
  assert(radiance.confirmHit(light.transactionId, true));
  assert(radiance.resonance() == fp(10));

  auto cooling = radiance.request({CombatAction::Radiance, 2}, targetContext());
  assert(!cooling.accepted && cooling.reason == ActionRejectReason::Cooldown);
  radiance.update(3159, 2999, targetContext());
  assert(radiance.request({CombatAction::Radiance, 3}, targetContext()).reason ==
         ActionRejectReason::Cooldown);
  radiance.update(3160, 1, targetContext());
  assert(radiance.request({CombatAction::Radiance, 4}, targetContext()).accepted);

  ActionStateMachine current(CombatConfig::defaults());
  const HitRequest currentHit = cast(current, CombatAction::Current, 0, 160, 1);
  assert(currentHit.baseDamage == fp(16));
  assert(current.confirmHit(currentHit.transactionId, true));
  current.update(4159, 3999, targetContext());
  assert(current.request({CombatAction::Current, 2}, targetContext()).reason ==
         ActionRejectReason::Cooldown);
  current.update(4160, 1, targetContext());
  assert(current.request({CombatAction::Current, 3}, targetContext()).accepted);

  ActionStateMachine corruption(CombatConfig::defaults());
  const HitRequest dark = cast(corruption, CombatAction::Corruption, 0, 160, 1);
  assert(dark.baseDamage == fp(12) && dark.poiseDamage == fp(18));
  assert(corruption.confirmHit(dark.transactionId, true));
  corruption.update(5159, 4999, targetContext());
  assert(corruption.request({CombatAction::Corruption, 2}, targetContext()).reason ==
         ActionRejectReason::Cooldown);
  corruption.update(5160, 1, targetContext());
  assert(corruption.request({CombatAction::Corruption, 3}, targetContext()).accepted);
}

void testInsightConsumesOnlyOnSuccessfulSourceHit() {
  ActionStateMachine machine(CombatConfig::defaults());
  machine.grantInsight(0);
  auto invalid = targetContext();
  invalid.targetAlive = false;
  assert(machine.request({CombatAction::Current, 1}, invalid).reason ==
         ActionRejectReason::TargetDead);
  assert(machine.hasInsight());

  machine.update(0, 0, targetContext());
  assert(machine.request({CombatAction::Current, 2}, targetContext()).accepted);
  assert(machine.hasInsight());
  assert(!machine.update(159, 159, targetContext()).has_value());
  assert(machine.hasInsight());

  auto diedBeforeHit = targetContext();
  diedBeforeHit.targetAlive = false;
  assert(!machine.update(160, 1, diedBeforeHit).has_value());
  assert(machine.hasInsight());

  const HitRequest empowered = cast(machine, CombatAction::Current, 160, 320, 3);
  assert(empowered.baseDamage == fp(24));
  assert(empowered.sourceAmount == fp(1.5));
  assert(machine.hasInsight());
  assert(machine.confirmHit(empowered.transactionId, true));
  assert(!machine.hasInsight());
}

void testLargeDtUsesExactSourceHitTick() {
  ActionStateMachine machine(CombatConfig::defaults());
  machine.grantInsight(0);
  assert(machine.request({CombatAction::Corruption, 7}, targetContext()).accepted);
  const auto hit = machine.update(1000, 1000, targetContext());
  assert(hit.has_value());
  assert(hit->tick == 160);
  assert(hit->baseDamage == fp(18) && hit->poiseDamage == fp(18));
  assert(hit->sourceAmount == fp(1.5));
  assert(machine.hasInsight());
  assert(machine.confirmHit(hit->transactionId, true));
  assert(!machine.hasInsight());
}

}  // namespace

int main() {
  testResourcesCommitOnlyAfterResolvedHit();
  testDeadTargetResolutionDoesNotCommitResourcesOrHistory();
  testMismatchedConfirmationDoesNotCommitOrDuplicate();
  testExternalSequenceReuseCannotConfirmAnotherTransaction();
  testResetDoesNotReuseTransactionIdentity();
  testThreeSourceHitsAndCooldownBoundaries();
  testInsightConsumesOnlyOnSuccessfulSourceHit();
  testLargeDtUsesExactSourceHitTick();
  return 0;
}
