#include "native/gameplay/ai/encounter_controller.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace {

const EncounterEnemySnapshot* findEnemy(const EncounterSnapshot& snapshot,
                                        EntityId id) {
  const auto found = std::find_if(
      snapshot.enemies.begin(), snapshot.enemies.end(),
      [id](const EncounterEnemySnapshot& enemy) { return enemy.id == id; });
  return found == snapshot.enemies.end() ? nullptr : &*found;
}

void update(EncounterController& encounter, Tick tick, int64_t dtMs,
            EntityId targetId = 0) {
  encounter.update({tick, dtMs, {0.5f, 0.5f}, false, targetId});
}

void testStartsAllModesWithStableEntities() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  const std::array<EncounterMode, 4> modes{
      EncounterMode::Training, EncounterMode::Beast,
      EncounterMode::Mixed, EncounterMode::Guard};

  for (const EncounterMode mode : modes) {
    assert(encounter.start(mode));
    const EncounterSnapshot first = encounter.snapshot();
    assert(first.state == EncounterState::Running);
    assert(first.mode == mode);
    assert(first.candidates.size() <= EnemyAiConfig::kMaxEnemies);
    update(encounter, 0, 0);
    assert(encounter.snapshot().candidates == first.candidates);
  }

  assert(encounter.start(EncounterMode::Mixed));
  const EncounterSnapshot mixed = encounter.snapshot();
  assert(mixed.enemies.size() == EnemyAiConfig::kMaxEnemies);
  assert(std::is_sorted(
      mixed.enemies.begin(), mixed.enemies.end(),
      [](const EncounterEnemySnapshot& left,
         const EncounterEnemySnapshot& right) { return left.id < right.id; }));
}

void testRejectsInvalidConfigurationAtomically() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Beast));
  const EncounterSnapshot before = encounter.snapshot();

  EncounterConfig invalid = EncounterConfig::forMode(EncounterMode::Mixed);
  invalid.enemies[1].id = invalid.enemies[0].id;
  assert(!encounter.start(invalid));
  assert(encounter.snapshot() == before);

  invalid = EncounterConfig::forMode(EncounterMode::Mixed);
  invalid.maxEnemies = EnemyAiConfig::kMaxEnemies + 1;
  assert(!encounter.start(invalid));
  assert(encounter.snapshot() == before);
}

void testDeathRemovesCandidateAndKeepsFinalSnapshot() {
  CombatConfig combatConfig = CombatConfig::defaults();
  CombatController combat(combatConfig);
  EncounterController encounter(combat);
  EncounterConfig config = EncounterConfig::forMode(EncounterMode::Beast);
  config.enemies.front().hp = combatConfig.comboDamage.front();
  assert(encounter.start(config));
  const EntityId enemyId = encounter.snapshot().enemies.front().id;

  combat.enqueue({CombatAction::Attack, 1});
  update(encounter, 0, 16, enemyId);
  update(encounter, 160, 144, enemyId);

  const EncounterSnapshot final = encounter.snapshot();
  const EncounterEnemySnapshot* enemy = findEnemy(final, enemyId);
  assert(enemy != nullptr && !enemy->alive && enemy->hp == 0);
  assert(final.candidates.empty());
  assert(final.state == EncounterState::Victory);
  assert(final.victory);
  assert(std::count_if(
             encounter.events().combat.gameplay.begin(),
             encounter.events().combat.gameplay.end(),
             [](const GameplayEvent& event) {
               return event.type == GameplayEventType::Death;
             }) == 1);

  update(encounter, 5000, 4840, enemyId);
  assert(encounter.snapshot() == final);
  assert(encounter.events().combat.gameplay.empty());
  assert(encounter.events().effects.empty());
}

void testStopHasNoEventsAndResetClearsState() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Beast));
  const EncounterSnapshot active = encounter.snapshot();

  encounter.stop();
  assert(encounter.snapshot().state == EncounterState::Stopped);
  assert(encounter.snapshot().enemies == active.enemies);
  assert(encounter.snapshot().candidates.empty());
  update(encounter, 1000, 1000, active.enemies.front().id);
  assert(encounter.events().combat.gameplay.empty());
  assert(encounter.events().combat.presentation.empty());
  assert(encounter.events().effects.empty());

  encounter.reset();
  assert(encounter.snapshot().state == EncounterState::Stopped);
  assert(encounter.snapshot().enemies.empty());
  assert(encounter.snapshot().candidates.empty());
  assert(!encounter.snapshot().victory);
  assert(combat.snapshot().playerHp == fp(100));
  assert(combat.snapshot().targetHp == fp(300));
}

void testTrainingModeKeepsStageThreePulseSemantics() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Training));
  assert(encounter.snapshot().candidates.size() == 1);
  assert(encounter.snapshot().candidates.front().id ==
         static_cast<int32_t>(CombatController::kTrainingTargetId));

  update(encounter, 0, 0, CombatController::kTrainingTargetId);
  combat.enqueue({CombatAction::Dodge, 1});
  update(encounter, 300, 1, CombatController::kTrainingTargetId);
  update(encounter, 800, 500, CombatController::kTrainingTargetId);
  assert(combat.snapshot().playerHp == fp(100));
  assert(combat.snapshot().hasInsight);
  assert(combat.snapshot().pulseHitRemainingMs == 3000);
}

void testStandaloneModeClearsLevelFlowState() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::LevelFlow));
  assert(encounter.snapshot().mode == EncounterMode::LevelFlow);

  assert(encounter.start(EncounterMode::Beast));
  assert(encounter.snapshot().mode == EncounterMode::Beast);
  assert(encounter.snapshot().levelStage == LevelStage::Training);
  assert(encounter.snapshot().gateState == GateState::Closed);
  assert(encounter.snapshot().supplyState == SupplyState::Unavailable);
}

void testEnemyAnimationFactsArePublished() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Beast));
  const EntityId enemyId = encounter.snapshot().enemies.front().id;

  bool observedMoving = false;
  bool observedAttacking = false;
  for (Tick tick = 100; tick <= 2000; tick += 100) {
    update(encounter, tick, 100, enemyId);
    const EncounterEnemySnapshot* enemy = findEnemy(encounter.snapshot(), enemyId);
    assert(enemy != nullptr);
    observedMoving = observedMoving || enemy->moving;
    observedAttacking = observedAttacking || enemy->attacking;
  }
  assert(observedMoving);
  assert(observedAttacking);

  combat.enqueue({CombatAction::Attack, 99});
  update(encounter, 2100, 100, enemyId);
  update(encounter, 2260, 160, enemyId);
  const EncounterEnemySnapshot* hitEnemy = findEnemy(encounter.snapshot(), enemyId);
  assert(hitEnemy != nullptr && hitEnemy->hit);
}

void testEnemySnapshotEqualityIncludesAnimationFacts() {
  EncounterEnemySnapshot left;
  EncounterEnemySnapshot right;
  left.moving = true;
  assert(!(left == right));
  right.moving = true;
  left.attacking = true;
  assert(!(left == right));
  right.attacking = true;
  left.hit = true;
  assert(!(left == right));
  right.hit = true;
  assert(left == right);
}

}  // namespace

int main() {
  testStartsAllModesWithStableEntities();
  testRejectsInvalidConfigurationAtomically();
  testDeathRemovesCandidateAndKeepsFinalSnapshot();
  testStopHasNoEventsAndResetClearsState();
  testTrainingModeKeepsStageThreePulseSemantics();
  testStandaloneModeClearsLevelFlowState();
  testEnemyAnimationFactsArePublished();
  testEnemySnapshotEqualityIncludesAnimationFacts();
}
