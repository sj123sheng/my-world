#include "../native/gameplay/combat/action_state_machine.h"

#include <cassert>

namespace {

ActionContext targetContext() {
  return {1, 2, true, false, false};
}

HitRequest advanceUntilHit(ActionStateMachine& machine, Tick& now) {
  const auto beforeHit = machine.update(now += 159, 159, targetContext());
  assert(!beforeHit.has_value());
  const auto hit = machine.update(now += 1, 1, targetContext());
  assert(hit.has_value());
  assert(!machine.update(now, 0, targetContext()).has_value());
  return *hit;
}

HitRequest chainAndHit(ActionStateMachine& machine, Tick& now, uint64_t sequence) {
  const auto decision = machine.request({CombatAction::Attack, sequence}, targetContext());
  assert(decision.accepted);
  return advanceUntilHit(machine, now);
}

void testFourAttackChainAndSingleHitPerSegment() {
  ActionStateMachine machine(CombatConfig::defaults());
  Tick now = 0;

  assert(machine.request({CombatAction::Attack, 1}, targetContext()).accepted);
  const auto locked = machine.request({CombatAction::Attack, 99}, targetContext());
  assert(!locked.accepted);
  assert(locked.reason == ActionRejectReason::ActionLocked);

  const auto first = advanceUntilHit(machine, now);
  assert(first.attacker == 1 && first.target == 2);
  assert(first.ability == 1 && !first.source.has_value());
  assert(first.baseDamage == fp(8) && first.poiseDamage == fp(4));
  assert(first.tick == 160 && first.sequence == 1);

  const auto second = chainAndHit(machine, now, 2);
  assert(second.ability == 2 && second.baseDamage == fp(10));
  const auto third = chainAndHit(machine, now, 3);
  assert(third.ability == 3 && third.baseDamage == fp(12));
  const auto fourth = chainAndHit(machine, now, 4);
  assert(fourth.ability == 4 && fourth.baseDamage == fp(18));
}

void testComboResetsOnMove() {
  ActionStateMachine machine(CombatConfig::defaults());
  Tick now = 0;
  assert(machine.request({CombatAction::Attack, 1}, targetContext()).accepted);
  (void)advanceUntilHit(machine, now);

  auto moving = targetContext();
  moving.moving = true;
  assert(!machine.update(++now, 1, moving).has_value());
  assert(machine.request({CombatAction::Attack, 2}, targetContext()).accepted);
  assert(advanceUntilHit(machine, now).ability == 1);
}

void testComboResetsOnDamageTaken() {
  ActionStateMachine machine(CombatConfig::defaults());
  Tick now = 0;
  assert(machine.request({CombatAction::Attack, 1}, targetContext()).accepted);
  (void)advanceUntilHit(machine, now);

  auto damaged = targetContext();
  damaged.damageTaken = true;
  assert(!machine.update(++now, 1, damaged).has_value());
  assert(machine.request({CombatAction::Attack, 2}, targetContext()).accepted);
  assert(advanceUntilHit(machine, now).ability == 1);
}

void testComboResetsAfterWindowExpires() {
  ActionStateMachine machine(CombatConfig::defaults());
  Tick now = 0;
  assert(machine.request({CombatAction::Attack, 1}, targetContext()).accepted);
  (void)advanceUntilHit(machine, now);
  assert(!machine.update(now += 481, 481, targetContext()).has_value());
  assert(machine.request({CombatAction::Attack, 2}, targetContext()).accepted);
  assert(advanceUntilHit(machine, now).ability == 1);
}

void testInvalidTargetDoesNotChangeState() {
  ActionStateMachine machine(CombatConfig::defaults());
  auto noTarget = targetContext();
  noTarget.target = 0;
  assert(machine.request({CombatAction::Attack, 1}, noTarget).reason ==
         ActionRejectReason::NoTarget);

  auto deadTarget = targetContext();
  deadTarget.targetAlive = false;
  assert(machine.request({CombatAction::Attack, 2}, deadTarget).reason ==
         ActionRejectReason::TargetDead);

  Tick now = 0;
  assert(machine.request({CombatAction::Attack, 3}, targetContext()).accepted);
  const auto hit = advanceUntilHit(machine, now);
  assert(hit.ability == 1 && hit.sequence == 3);
}

void testLargeTickUsesFixedHitTimeAndCarriesRemainder() {
  ActionStateMachine machine(CombatConfig::defaults());
  assert(machine.request({CombatAction::Attack, 1}, targetContext()).accepted);

  const auto hit = machine.update(200, 200, targetContext());
  assert(hit.has_value());
  assert(hit->tick == 160);

  assert(!machine.update(640, 440, targetContext()).has_value());
  assert(machine.request({CombatAction::Attack, 2}, targetContext()).accepted);
  const auto second = machine.update(800, 160, targetContext());
  assert(second.has_value() && second->ability == 2 && second->tick == 800);
}

void testSingleTickCanHitAndExpireChainWindow() {
  ActionStateMachine machine(CombatConfig::defaults());
  assert(machine.request({CombatAction::Attack, 1}, targetContext()).accepted);

  const auto hit = machine.update(641, 641, targetContext());
  assert(hit.has_value() && hit->tick == 160);
  assert(machine.request({CombatAction::Attack, 2}, targetContext()).accepted);
  const auto next = machine.update(801, 160, targetContext());
  assert(next.has_value() && next->ability == 1);
}

void testChainWindowIncludesExactly480ButExcludes481() {
  ActionStateMachine atBoundary(CombatConfig::defaults());
  assert(atBoundary.request({CombatAction::Attack, 1}, targetContext()).accepted);
  assert(atBoundary.update(640, 640, targetContext()).has_value());
  assert(atBoundary.request({CombatAction::Attack, 2}, targetContext()).accepted);
  const auto chained = atBoundary.update(800, 160, targetContext());
  assert(chained.has_value() && chained->ability == 2);

  ActionStateMachine pastBoundary(CombatConfig::defaults());
  assert(pastBoundary.request({CombatAction::Attack, 1}, targetContext()).accepted);
  assert(pastBoundary.update(641, 641, targetContext()).has_value());
  assert(pastBoundary.request({CombatAction::Attack, 2}, targetContext()).accepted);
  const auto reset = pastBoundary.update(801, 160, targetContext());
  assert(reset.has_value() && reset->ability == 1);
}

}  // namespace

int main() {
  testFourAttackChainAndSingleHitPerSegment();
  testComboResetsOnMove();
  testComboResetsOnDamageTaken();
  testComboResetsAfterWindowExpires();
  testInvalidTargetDoesNotChangeState();
  testLargeTickUsesFixedHitTimeAndCarriesRemainder();
  testSingleTickCanHitAndExpireChainWindow();
  testChainWindowIncludesExactly480ButExcludes481();
  return 0;
}
