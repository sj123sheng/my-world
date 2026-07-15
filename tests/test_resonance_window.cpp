#include "../native/gameplay/combat/action_state_machine.h"
#include "../native/gameplay/combat/source_reaction_system.h"

#include <cassert>
#include <limits>

namespace {

void testNormalAndReactionResonanceAccumulate() {
  CombatResources resources(CombatConfig::defaults());
  resources.addResonance(fp(10));
  TrainingTarget target = TrainingTarget::defaults();
  SourceReactionSystem reactions(CombatConfig::defaults());
  assert(!reactions.apply(target, SourceType::Radiance, FP_ONE, 0, 1).type);
  const ReactionOutcome outcome =
      reactions.apply(target, SourceType::Current, FP_ONE, 1, 1);
  assert(outcome.type == ResonanceType::Refraction);
  resources.addResonance(outcome.resonanceGain);
  assert(resources.resonance() == fp(30));
  resources.addResonance(fp(1000));
  assert(resources.resonance() == fp(100));
}

void testTriSourceBoundaryAndExpiredHistory() {
  CombatResources atBoundary(CombatConfig::defaults());
  atBoundary.addResonance(fp(30));
  atBoundary.recordDistinctSource(SourceType::Radiance, 0);
  atBoundary.recordDistinctSource(SourceType::Current, 3000);
  atBoundary.recordDistinctSource(SourceType::Corruption, 8000);
  assert(atBoundary.resonance() == fp(100));
  assert(atBoundary.canUltimate(8000));

  CombatResources expired(CombatConfig::defaults());
  expired.recordDistinctSource(SourceType::Radiance, 0);
  expired.recordDistinctSource(SourceType::Current, 3000);
  expired.recordDistinctSource(SourceType::Corruption, 8001);
  assert(expired.resonance() == 0);
  assert(!expired.canUltimate(8001));
  expired.recordDistinctSource(SourceType::Radiance, 9000);
  assert(expired.resonance() == fp(100));
}

void testWindowExpirationKeepsFullEnergy() {
  CombatResources resources(CombatConfig::defaults());
  resources.recordDistinctSource(SourceType::Radiance, 0);
  resources.recordDistinctSource(SourceType::Current, 1000);
  resources.recordDistinctSource(SourceType::Corruption, 2000);
  assert(resources.canUltimate(6999));
  assert(resources.ultimateWindowActive(6999));
  resources.advance(7000);
  assert(resources.resonance() == fp(100));
  assert(resources.canUltimate(7000));
  assert(!resources.ultimateWindowActive(7000));
  assert(resources.spendUltimate(7000));
}

void testSaturatingDeadlinesAndOutOfOrderSourceTicks() {
  const Tick maximum = std::numeric_limits<Tick>::max();
  CombatResources deadlines(CombatConfig::defaults());
  deadlines.startSourceCooldown(SourceType::Radiance, maximum - 1000);
  assert(!deadlines.sourceReady(SourceType::Radiance, maximum - 1));
  assert(deadlines.sourceReady(SourceType::Radiance, maximum));

  deadlines.grantInsight(maximum - 1000);
  assert(deadlines.insightAvailableAt(maximum - 1));

  CombatResources nearMaximumWindow(CombatConfig::defaults());
  nearMaximumWindow.recordDistinctSource(SourceType::Radiance, maximum - 8000);
  nearMaximumWindow.recordDistinctSource(SourceType::Current, maximum - 4000);
  nearMaximumWindow.recordDistinctSource(SourceType::Corruption, maximum - 1000);
  assert(nearMaximumWindow.ultimateWindowActive(maximum - 1));
  assert(!nearMaximumWindow.ultimateWindowActive(maximum));
  assert(nearMaximumWindow.resonance() == fp(100));

  CombatResources history(CombatConfig::defaults());
  history.recordDistinctSource(SourceType::Current, 100);
  history.recordDistinctSource(SourceType::Radiance, 99);
  history.recordDistinctSource(SourceType::Corruption, 101);
  assert(history.resonance() == 0);
  history.recordDistinctSource(SourceType::Radiance, 102);
  assert(history.resonance() == fp(100));
}

void testUltimateRejectsAtomicallyAndConsumesOnHit() {
  ActionStateMachine machine(CombatConfig::defaults());
  const ActionContext target{1, 2, true, false, false};
  const auto rejected = machine.request({CombatAction::Ultimate, 1}, target);
  assert(!rejected.accepted && rejected.reason == ActionRejectReason::InsufficientResonance);
  assert(machine.resonance() == 0);

  machine.recordDistinctSource(SourceType::Radiance, 0);
  machine.recordDistinctSource(SourceType::Current, 3000);
  machine.recordDistinctSource(SourceType::Corruption, 7000);
  machine.update(7000, 7000, target);
  assert(machine.request({CombatAction::Ultimate, 2}, target).accepted);
  assert(machine.resonance() == fp(100));
  const auto hit = machine.update(7160, 160, target);
  assert(hit.has_value());
  assert(hit->baseDamage == fp(60) && hit->poiseDamage == fp(40));
  assert(!hit->source.has_value() && hit->tick == 7160);
  assert(machine.resonance() == fp(100));
  TrainingTarget resolvedTarget = TrainingTarget::defaults();
  const DamageOutcome outcome =
      DamageResolver(CombatConfig::defaults()).resolve(resolvedTarget, *hit);
  assert(machine.confirmHit(hit->transactionId,
                            outcome.hpDamage > 0 || outcome.poiseDamage > 0));
  assert(machine.resonance() == 0);
}

void testMissDoesNotSpendUltimate() {
  ActionStateMachine machine(CombatConfig::defaults());
  const ActionContext target{1, 2, true, false, false};
  machine.recordDistinctSource(SourceType::Radiance, 0);
  machine.recordDistinctSource(SourceType::Current, 1000);
  machine.recordDistinctSource(SourceType::Corruption, 2000);
  machine.update(2000, 2000, target);
  assert(machine.request({CombatAction::Ultimate, 1}, target).accepted);
  const auto hit = machine.update(2160, 160, target);
  assert(hit.has_value());

  TrainingTarget deadTarget = TrainingTarget::defaults();
  HitRequest lethal;
  lethal.baseDamage = fp(300);
  lethal.tick = 2100;
  assert(DamageResolver(CombatConfig::defaults()).resolve(deadTarget, lethal).killed);
  const DamageOutcome outcome =
      DamageResolver(CombatConfig::defaults()).resolve(deadTarget, *hit);
  assert(outcome.hpDamage == 0 && outcome.poiseDamage == 0);
  assert(machine.confirmHit(hit->transactionId, false));
  assert(machine.resonance() == fp(100));
  assert(machine.canUltimate(2160));
}

}  // namespace

int main() {
  testNormalAndReactionResonanceAccumulate();
  testTriSourceBoundaryAndExpiredHistory();
  testWindowExpirationKeepsFullEnergy();
  testSaturatingDeadlinesAndOutOfOrderSourceTicks();
  testUltimateRejectsAtomicallyAndConsumesOnHit();
  testMissDoesNotSpendUltimate();
  return 0;
}
