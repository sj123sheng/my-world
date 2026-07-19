#include "native/gameplay/ai/encounter_controller.h"

#include <cassert>

namespace {

void defeatCurrentWave(CombatController& combat,
                       EncounterController& encounter) {
  Tick tick = 0;
  uint64_t sequence = 1;
  while (encounter.snapshot().state == EncounterState::Running &&
         tick < 30000) {
    assert(!encounter.snapshot().candidates.empty());
    const EntityId target = static_cast<EntityId>(
        encounter.snapshot().candidates.front().id);
    combat.enqueue({CombatAction::Attack, sequence++});
    encounter.update({tick, 16, {0.5f, 0.5f}, false, target});
    tick += 200;
    encounter.update({tick, 184, {0.5f, 0.5f}, false, target});
    tick += 800;
  }
  assert(encounter.snapshot().state == EncounterState::Victory);
}

void testLevelDoorsOpenOnlyAfterVictory() {
  CombatConfig config = CombatConfig::defaults();
  config.trainingTargetHp = config.comboDamage.front();
  CombatController combat(config);
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::LevelFlow));
  assert(encounter.snapshot().mode == EncounterMode::LevelFlow);
  assert(encounter.snapshot().levelStage == LevelStage::Training);
  assert(encounter.snapshot().gateState == GateState::Closed);
  assert(!encounter.advanceLevel());

  defeatCurrentWave(combat, encounter);
  assert(encounter.snapshot().gateState == GateState::Open);
  assert(encounter.advanceLevel());
  assert(encounter.snapshot().levelStage == LevelStage::RiftClawFight);
  assert(encounter.snapshot().gateState == GateState::Closed);
  assert(encounter.snapshot().state == EncounterState::Running);
  assert(encounter.snapshot().enemies.size() == 1);
}

void testSupplyCanOnlyBeConsumedOnce() {
  CombatConfig config = CombatConfig::defaults();
  config.trainingTargetHp = fp(1);
  config.comboDamage.fill(fp(1000));
  CombatController combat(config);
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::LevelFlow));
  assert(!encounter.useSupply());
  assert(encounter.snapshot().supplyState == SupplyState::Unavailable);

  for (int completed = 0; completed < 4; ++completed) {
    defeatCurrentWave(combat, encounter);
    assert(encounter.snapshot().gateState == GateState::Open);
    assert(encounter.advanceLevel());
  }
  assert(encounter.snapshot().levelStage == LevelStage::Supply);
  assert(encounter.snapshot().supplyState == SupplyState::Available);
  assert(encounter.useSupply());
  assert(encounter.snapshot().supplyState == SupplyState::Consumed);
  assert(!encounter.useSupply());
  assert(encounter.snapshot().mode == EncounterMode::LevelFlow);
  assert(encounter.snapshot().gateState == GateState::Open);
  assert(encounter.advanceLevel());
  assert(encounter.snapshot().levelStage == LevelStage::Boss);
}

}  // namespace

int main() {
  testLevelDoorsOpenOnlyAfterVictory();
  testSupplyCanOnlyBeConsumedOnce();
}
