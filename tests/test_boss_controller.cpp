#include "native/gameplay/entities/boss.h"
#include "native/gameplay/ai/encounter_controller.h"

#include <cassert>

namespace {

void testPhaseThresholdsTriggerOnce() {
  BossController boss;
  assert(boss.start(BossConfig::karounDefaults()));
  assert(boss.snapshot().phase == BossPhase::RadianceLockdown);
  assert(boss.snapshot().hp == fp(1000));
  assert(boss.snapshot().poise == fp(300));

  boss.applyDamage(fp(300), fp(20), 100);
  assert(boss.snapshot().phase == BossPhase::RadianceLockdown);
  assert(boss.snapshot().transitionCount == 0);

  boss.applyDamage(fp(1), fp(0), 116);
  assert(boss.snapshot().phase == BossPhase::CurrentStorm);
  assert(boss.snapshot().mechanic == BossMechanic::CurrentNodes);
  assert(boss.snapshot().transitionCount == 1);

  boss.update({132, 16, false, false, 0});
  assert(boss.snapshot().transitionCount == 1);

  assert(boss.breakCurrentNode(0, 140));
  assert(boss.breakCurrentNode(1, 144));
  boss.applyDamage(fp(350), fp(0), 148);
  assert(boss.snapshot().phase == BossPhase::CorruptionCollapse);
  assert(boss.snapshot().mechanic == BossMechanic::FinalForge);
  assert(boss.snapshot().transitionCount == 2);
}

void testCurrentStormRequiresBothNodesBeforeBossVulnerable() {
  BossController boss;
  assert(boss.start(BossConfig::karounDefaults()));
  boss.applyDamage(fp(301), fp(0), 100);
  assert(boss.snapshot().phase == BossPhase::CurrentStorm);
  assert(!boss.snapshot().vulnerable);

  const FixedPoint hpBefore = boss.snapshot().hp;
  boss.applyDamage(fp(50), fp(0), 116);
  assert(boss.snapshot().hp == hpBefore);

  assert(boss.breakCurrentNode(0, 132));
  assert(!boss.snapshot().vulnerable);
  assert(!boss.breakCurrentNode(0, 148));
  assert(boss.breakCurrentNode(1, 164));
  assert(boss.snapshot().vulnerable);

  boss.applyDamage(fp(50), fp(0), 180);
  assert(boss.snapshot().hp == hpBefore - fp(50));
}

void testFinalForgeFailsWithoutResonanceAndSucceedsWithUltimate() {
  BossController boss;
  assert(boss.start(BossConfig::karounDefaults()));
  boss.applyDamage(fp(301), fp(0), 100);
  assert(boss.breakCurrentNode(0, 116));
  assert(boss.breakCurrentNode(1, 132));
  boss.applyDamage(fp(350), fp(0), 148);
  assert(boss.snapshot().phase == BossPhase::CorruptionCollapse);
  assert(boss.snapshot().castRemainingMs == 5000);

  boss.update({5149, 5001, false, false, 0});
  assert(boss.snapshot().failedMechanic);
  assert(!boss.snapshot().defeated);
  assert(boss.snapshot().hp > 0);

  assert(boss.retry(6000));
  boss.applyDamage(fp(301), fp(0), 6100);
  assert(boss.breakCurrentNode(0, 6116));
  assert(boss.breakCurrentNode(1, 6132));
  boss.applyDamage(fp(350), fp(0), 6148);
  boss.update({6200, 52, true, true, 99});
  assert(!boss.snapshot().failedMechanic);
  assert(boss.snapshot().vulnerable);
  boss.applyDamage(fp(400), fp(100), 6216);
  assert(boss.snapshot().defeated);
  assert(boss.snapshot().hp == 0);
}

void testRetryRestoresInitialState() {
  BossController boss;
  assert(boss.start(BossConfig::karounDefaults()));
  boss.applyDamage(fp(301), fp(10), 100);
  assert(boss.breakCurrentNode(0, 116));
  assert(boss.retry(500));

  const BossSnapshot snapshot = boss.snapshot();
  assert(snapshot.phase == BossPhase::RadianceLockdown);
  assert(snapshot.mechanic == BossMechanic::None);
  assert(snapshot.hp == fp(1000));
  assert(snapshot.poise == fp(300));
  assert(snapshot.transitionCount == 0);
  assert(snapshot.nodesBroken == 0);
  assert(snapshot.castRemainingMs == 0);
  assert(!snapshot.failedMechanic);
  assert(!snapshot.defeated);
  assert(snapshot.vulnerable);
}

void testBossEncounterVictoryAndRetry() {
  CombatConfig config = CombatConfig::defaults();
  config.comboDamage.fill(fp(1000));
  CombatController combat(config);
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Boss));
  assert(encounter.snapshot().candidates.size() == 1);
  assert(encounter.snapshot().candidates.front().id ==
         static_cast<int32_t>(EncounterController::kBossId));

  HitRequest lethal;
  lethal.attacker = EncounterController::kBossId;
  lethal.target = CombatController::kPlayerId;
  lethal.baseDamage = fp(100);
  lethal.tick = 1;
  lethal.sequence = 1;
  lethal.transactionId = 1;
  combat.applyEnemyHit(lethal);
  encounter.update({0, 0, {0.5f, 0.5f}, false,
                    EncounterController::kBossId});
  assert(encounter.snapshot().state == EncounterState::Defeat);
  assert(encounter.retryBoss());
  assert(encounter.snapshot().state == EncounterState::Running);
  assert(encounter.snapshot().levelStage == LevelStage::Boss);
  assert(encounter.snapshot().boss.hp == fp(1000));
  assert(encounter.snapshot().gateState == GateState::Closed);

  combat.enqueue({CombatAction::Attack, 2});
  encounter.update({100, 16, {0.5f, 0.5f}, false,
                    EncounterController::kBossId});
  encounter.update({300, 184, {0.5f, 0.5f}, false,
                    EncounterController::kBossId});
  assert(encounter.snapshot().state == EncounterState::Victory);
  assert(encounter.snapshot().boss.defeated);
  assert(encounter.snapshot().candidates.empty());
}

}  // namespace

int main() {
  testPhaseThresholdsTriggerOnce();
  testCurrentStormRequiresBothNodesBeforeBossVulnerable();
  testFinalForgeFailsWithoutResonanceAndSucceedsWithUltimate();
  testRetryRestoresInitialState();
  testBossEncounterVictoryAndRetry();
}
