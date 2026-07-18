#include "native/gameplay/ai/encounter_controller.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace {

const EncounterEnemySnapshot& enemyByArchetype(const EncounterSnapshot& snapshot,
                                               EnemyArchetype archetype) {
  const auto found = std::find_if(
      snapshot.enemies.begin(), snapshot.enemies.end(),
      [archetype](const EncounterEnemySnapshot& enemy) {
        return enemy.archetype == archetype;
      });
  assert(found != snapshot.enemies.end());
  return *found;
}

void update(EncounterController& encounter, Tick tick, int64_t dtMs,
            EntityId targetId = 0) {
  encounter.update({tick, dtMs, {0.5f, 0.5f}, false, targetId});
}

void testAllPlayerSourcesAffectCurrentEnemy() {
  const std::array<CombatAction, 3> actions{
      CombatAction::Radiance, CombatAction::Current, CombatAction::Corruption};

  for (std::size_t index = 0; index < actions.size(); ++index) {
    CombatController combat(CombatConfig::defaults());
    EncounterController encounter(combat);
    assert(encounter.start(EncounterMode::Beast));
    const EntityId enemyId = encounter.snapshot().enemies.front().id;
    combat.enqueue({actions[index], index + 1});
    update(encounter, 0, 16, enemyId);
    update(encounter, 160, 144, enemyId);

    const EncounterEnemySnapshot& enemy = encounter.snapshot().enemies.front();
    assert(enemy.hp < fp(300));
    assert(index != 0 || enemy.radianceAttached);
    assert(index != 1 || enemy.currentAttached);
    assert(index != 2 || enemy.corruptionAttached);
  }
}

void testEnemyHitDamagesPlayer() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Beast));
  update(encounter, 0, 0);
  update(encounter, 180, 180);

  assert(combat.snapshot().playerHp == fp(90));
  assert(std::any_of(
      encounter.events().combat.gameplay.begin(),
      encounter.events().combat.gameplay.end(),
      [](const GameplayEvent& event) {
        return event.source != CombatController::kPlayerId &&
               event.target == CombatController::kPlayerId &&
               event.type == GameplayEventType::Damage;
      }));
}

void testEnemyEncounterDoesNotGrantTrainingPulseInsight() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Beast));
  combat.enqueue({CombatAction::Dodge, 1});
  update(encounter, 300, 1);

  assert(!combat.snapshot().hasInsight);
  assert(combat.snapshot().pulseHitRemainingMs == 0);
}

void testShieldRequestAppliesAsShieldAtItsBoundary() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Mixed));
  const EntityId riftId =
      enemyByArchetype(encounter.snapshot(), EnemyArchetype::RiftClaw).id;
  update(encounter, 0, 0);
  update(encounter, 700, 700);

  const EncounterEnemySnapshot& shielded =
      enemyByArchetype(encounter.snapshot(), EnemyArchetype::RiftClaw);
  assert(shielded.hp == fp(300));
  assert(shielded.shield == fp(40));
  assert(encounter.events().effects.size() == 1);
  assert(encounter.events().effects.front().type == CombatEffectType::Shield);
  assert(encounter.events().effects.front().target == riftId);
  assert(encounter.events().effects.front().tick == 700);
  assert(std::none_of(
      encounter.events().combat.gameplay.begin(),
      encounter.events().combat.gameplay.end(),
      [riftId](const GameplayEvent& event) {
        return event.target == riftId &&
               event.type == GameplayEventType::Damage;
      }));

  combat.enqueue({CombatAction::Attack, 1});
  update(encounter, 800, 16, riftId);
  update(encounter, 960, 144, riftId);
  const EncounterEnemySnapshot& afterHit =
      enemyByArchetype(encounter.snapshot(), EnemyArchetype::RiftClaw);
  assert(afterHit.hp == fp(300));
  assert(afterHit.shield == fp(32));
}

}  // namespace

int main() {
  testAllPlayerSourcesAffectCurrentEnemy();
  testEnemyHitDamagesPlayer();
  testEnemyEncounterDoesNotGrantTrainingPulseInsight();
  testShieldRequestAppliesAsShieldAtItsBoundary();
}
