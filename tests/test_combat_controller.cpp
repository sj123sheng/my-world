#include "native/gameplay/combat/combat_controller.h"

#include <algorithm>
#include <cassert>

int main() {
  CombatController combat(CombatConfig::defaults());

  combat.enqueue({CombatAction::Attack, 1});
  combat.update({0, 16, false, CombatController::kTrainingTargetId, true});
  assert(combat.snapshot().comboSegment == 1);
  assert(combat.snapshot().targetHp == fp(300));
  combat.update({160, 144, false, CombatController::kTrainingTargetId, true});
  assert(combat.snapshot().targetHp == fp(292));

  CombatController sources(CombatConfig::defaults());
  sources.enqueue({CombatAction::Radiance, 1});
  sources.update({0, 16, false, CombatController::kTrainingTargetId, true});
  sources.update({160, 144, false, CombatController::kTrainingTargetId, true});
  assert(sources.snapshot().resonance == fp(10));
  sources.enqueue({CombatAction::Current, 2});
  sources.update({161, 1, false, CombatController::kTrainingTargetId, true});
  sources.update({321, 159, false, CombatController::kTrainingTargetId, true});
  assert(sources.snapshot().targetHp == fp(252));
  assert(sources.snapshot().resonance == fp(40));
  assert(std::count_if(sources.events().gameplay.begin(),
                       sources.events().gameplay.end(),
                       [](const GameplayEvent& event) {
                         return event.type == GameplayEventType::Resonance;
                       }) == 1);

  CombatController ordered(CombatConfig::defaults());
  ordered.enqueue({CombatAction::Attack, 8});
  ordered.enqueue({CombatAction::Attack, 6});
  ordered.update({0, 16, false, CombatController::kTrainingTargetId, true});
  assert(ordered.snapshot().lastAcceptedSequence == 6);

  CombatController rejectThenAccept(CombatConfig::defaults());
  rejectThenAccept.enqueue({static_cast<CombatAction>(255), 1});
  rejectThenAccept.enqueue({CombatAction::Attack, 2});
  rejectThenAccept.update(
      {0, 16, false, CombatController::kTrainingTargetId, true});
  assert(rejectThenAccept.snapshot().lastAcceptedSequence == 2);
  assert(rejectThenAccept.snapshot().lastRejectReason ==
         ActionRejectReason::InvalidAction);
  rejectThenAccept.reset();
  assert(rejectThenAccept.snapshot().lastRejectReason == ActionRejectReason::None);

  ordered.update({16, 16, true, CombatController::kTrainingTargetId, true});
  assert(ordered.snapshot().comboSegment == 0);

  CombatController invalidTarget(CombatConfig::defaults());
  invalidTarget.enqueue({CombatAction::Attack, 1});
  invalidTarget.update({0, 16, false, 999, true});
  assert(invalidTarget.snapshot().comboSegment == 0);
  assert(invalidTarget.snapshot().targetHp == fp(300));

  CombatController pulse(CombatConfig::defaults());
  pulse.update({0, 1, false, CombatController::kTrainingTargetId, true});
  pulse.enqueue({CombatAction::Dodge, 1});
  pulse.update({700, 1, false, CombatController::kTrainingTargetId, true});
  pulse.update({800, 100, false, CombatController::kTrainingTargetId, true});
  assert(pulse.snapshot().playerHp == fp(100));
  assert(pulse.snapshot().hasInsight);

  CombatController earlyPrecisePulse(CombatConfig::defaults());
  earlyPrecisePulse.update({0, 1, false, CombatController::kTrainingTargetId, true});
  earlyPrecisePulse.enqueue({CombatAction::Dodge, 1});
  earlyPrecisePulse.update({300, 1, false, CombatController::kTrainingTargetId, true});
  assert(earlyPrecisePulse.snapshot().hasInsight);
  assert(earlyPrecisePulse.snapshot().insightMs == 15000);
  earlyPrecisePulse.update({799, 499, false, CombatController::kTrainingTargetId, true});
  assert(!earlyPrecisePulse.snapshot().invulnerable);
  earlyPrecisePulse.update({800, 1, false, CombatController::kTrainingTargetId, true});
  assert(earlyPrecisePulse.snapshot().playerHp == fp(100));
  earlyPrecisePulse.update({3800, 3000, false, CombatController::kTrainingTargetId, true});
  assert(earlyPrecisePulse.snapshot().playerHp == fp(90));

  CombatController historicalPulse(CombatConfig::defaults());
  historicalPulse.update(
      {0, 0, false, CombatController::kTrainingTargetId, true});
  historicalPulse.enqueue({CombatAction::Dodge, 1});
  historicalPulse.update(
      {750, 1, false, CombatController::kTrainingTargetId, true});
  historicalPulse.update(
      {4000, 3249, false, CombatController::kTrainingTargetId, true});
  assert(historicalPulse.snapshot().playerHp == fp(90));
  assert(std::count_if(historicalPulse.events().gameplay.begin(),
                       historicalPulse.events().gameplay.end(),
                       [](const GameplayEvent& event) {
                         return event.type == GameplayEventType::Dodge;
                       }) == 1);

  CombatController reset(CombatConfig::defaults());
  reset.enqueue({CombatAction::Attack, 1});
  reset.update({0, 16, false, CombatController::kTrainingTargetId, true});
  reset.reset();
  assert(reset.snapshot().comboSegment == 0);
  assert(reset.snapshot().targetHp == fp(300));
  assert(reset.snapshot().playerHp == fp(100));
}
