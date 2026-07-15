#include "native/gameplay/combat/combat_controller.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

int main() {
  CombatController interrupted(CombatConfig::defaults());
  interrupted.enqueue({CombatAction::Attack, 1});
  interrupted.update({0, 0, false, CombatController::kTrainingTargetId, true});
  interrupted.update({160, 160, false, CombatController::kTrainingTargetId, true});
  interrupted.update({800, 640, false, CombatController::kTrainingTargetId, true});
  interrupted.enqueue({CombatAction::Attack, 2});
  interrupted.update({801, 1, false, CombatController::kTrainingTargetId, true});
  interrupted.update({961, 160, false, CombatController::kTrainingTargetId, true});
  assert(interrupted.snapshot().lastAbility == 1);
  assert(interrupted.events().gameplay.front().value == fp(8));

  CombatController lethal(CombatConfig::defaults());
  lethal.update({27800, 27800, false, CombatController::kTrainingTargetId, true});
  assert(lethal.snapshot().playerHp == fp(100));
  assert(lethal.snapshot().playerPoise == fp(100));
  assert(lethal.snapshot().targetHp == fp(300));
  assert(lethal.events().gameplay.size() == 11);
  assert(lethal.events().gameplay[9].type == GameplayEventType::Damage);
  assert(lethal.events().gameplay[10].type == GameplayEventType::EncounterReset);
  lethal.update({27801, 1, false, CombatController::kTrainingTargetId, true});
  assert(lethal.snapshot().playerHp == fp(100));
  assert(lethal.events().gameplay.empty());
  lethal.update({28599, 798, false, CombatController::kTrainingTargetId, true});
  assert(lethal.snapshot().playerHp == fp(100));
  lethal.update({28600, 1, false, CombatController::kTrainingTargetId, true});
  assert(lethal.snapshot().playerHp == fp(90));
  assert(lethal.events().gameplay.size() == 1);
  assert(lethal.events().gameplay[0].tick == 28600);

  CombatController fields(CombatConfig::defaults());
  fields.enqueue({CombatAction::Radiance, 7});
  fields.update({0, 0, false, CombatController::kTrainingTargetId, true});
  assert(fields.snapshot().currentAction != static_cast<uint8_t>(ActionState::Idle));
  fields.update({160, 160, false, CombatController::kTrainingTargetId, true});
  assert(fields.snapshot().radianceCooldownMs > 0);
  assert(fields.snapshot().radianceAttached);
  fields.update({800, 640, false, CombatController::kTrainingTargetId, true});
  assert(fields.snapshot().pulsePhase == static_cast<uint8_t>(PulseEventKind::Hit));

  CombatController corrosion(CombatConfig::defaults());
  corrosion.enqueue({CombatAction::Corruption, 8});
  corrosion.update({0, 0, false, CombatController::kTrainingTargetId, true});
  corrosion.update({160, 160, false, CombatController::kTrainingTargetId, true});
  assert(corrosion.snapshot().corroded);
  assert(std::any_of(corrosion.events().gameplay.begin(), corrosion.events().gameplay.end(),
                     [](const GameplayEvent& event) {
                       return event.type == GameplayEventType::AuraApplied;
                     }));

  CombatConfig overflow = CombatConfig::defaults();
  overflow.staminaRecoveryPerSecond = std::numeric_limits<FixedPoint>::max();
  CombatResources safe(overflow);
  assert(safe.spendStamina(fp(30), 0));
  safe.advance(std::numeric_limits<Tick>::max());
  assert(safe.stamina() == safe.maxStamina());

  GameplayEvent wide{};
  wide.sequence = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;
  assert(wide.sequence > std::numeric_limits<uint32_t>::max());

  CombatController chronological(CombatConfig::defaults());
  chronological.enqueue({CombatAction::Attack,
      static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 9});
  chronological.update({0, 0, false, CombatController::kTrainingTargetId, true});
  chronological.update({800, 800, false, CombatController::kTrainingTargetId, true});
  assert(chronological.events().gameplay.size() == 3);
  assert(chronological.events().gameplay[0].tick == 160);
  assert(chronological.events().gameplay[0].sequence >
         std::numeric_limits<uint32_t>::max());
  assert(chronological.events().gameplay[1].tick == 160);
  assert(chronological.events().gameplay[2].tick == 800);

  CombatController reactionOrder(CombatConfig::defaults());
  reactionOrder.enqueue({CombatAction::Radiance, 1});
  reactionOrder.update({0, 0, false, CombatController::kTrainingTargetId, true});
  reactionOrder.update({160, 160, false, CombatController::kTrainingTargetId, true});
  reactionOrder.enqueue({CombatAction::Current, 2});
  reactionOrder.update({161, 1, false, CombatController::kTrainingTargetId, true});
  reactionOrder.update({321, 160, false, CombatController::kTrainingTargetId, true});
  assert(reactionOrder.events().gameplay[0].type == GameplayEventType::Hit);
  assert(reactionOrder.events().gameplay[1].type == GameplayEventType::Damage);
  assert(reactionOrder.events().gameplay[2].type == GameplayEventType::AuraApplied);
  assert(reactionOrder.events().gameplay[3].type == GameplayEventType::Resonance);
  return 0;
}
