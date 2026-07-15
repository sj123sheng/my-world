#include "../native/gameplay/combat/action_state_machine.h"

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

void testThreeSourceHitsAndCooldownBoundaries() {
  ActionStateMachine radiance(CombatConfig::defaults());
  const HitRequest light = cast(radiance, CombatAction::Radiance, 0, 160, 1);
  assert(light.source == SourceType::Radiance);
  assert(light.baseDamage == fp(20) && light.poiseDamage == fp(6));
  assert(light.tick == 160 && light.sequence == 1);
  assert(radiance.resonance() == fp(10));

  auto cooling = radiance.request({CombatAction::Radiance, 2}, targetContext());
  assert(!cooling.accepted && cooling.reason == ActionRejectReason::Cooldown);
  radiance.update(3159, 2999, targetContext());
  assert(radiance.request({CombatAction::Radiance, 3}, targetContext()).reason ==
         ActionRejectReason::Cooldown);
  radiance.update(3160, 1, targetContext());
  assert(radiance.request({CombatAction::Radiance, 4}, targetContext()).accepted);

  ActionStateMachine current(CombatConfig::defaults());
  assert(cast(current, CombatAction::Current, 0, 160, 1).baseDamage == fp(16));
  current.update(4159, 3999, targetContext());
  assert(current.request({CombatAction::Current, 2}, targetContext()).reason ==
         ActionRejectReason::Cooldown);
  current.update(4160, 1, targetContext());
  assert(current.request({CombatAction::Current, 3}, targetContext()).accepted);

  ActionStateMachine corruption(CombatConfig::defaults());
  const HitRequest dark = cast(corruption, CombatAction::Corruption, 0, 160, 1);
  assert(dark.baseDamage == fp(12) && dark.poiseDamage == fp(18));
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
  assert(!machine.hasInsight());
}

}  // namespace

int main() {
  testThreeSourceHitsAndCooldownBoundaries();
  testInsightConsumesOnlyOnSuccessfulSourceHit();
  testLargeDtUsesExactSourceHitTick();
  return 0;
}
