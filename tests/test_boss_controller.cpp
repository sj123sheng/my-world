#include "native/gameplay/entities/boss.h"
#include "native/gameplay/ai/encounter_controller.h"

#include <cassert>
#include <cmath>

namespace {

float distanceBetween(Vec2 left, Vec2 right) {
  return (left - right).length();
}

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

void testBossChasesPlayerAndOrbitsAtPreferredRange() {
  BossController boss;
  assert(boss.start(BossConfig::karounDefaults()));
  const BossConfig config = boss.config();

  // 玩家远在期望距离之外：首领直线追击，距离持续缩短且朝向指向玩家。
  BossFrameInput input;
  input.tick = 16;
  input.dtMs = 16;
  input.playerPosition = {0.5f, 0.25f};
  input.playerAlive = true;
  const Vec2 start = boss.snapshot().position;
  const float before = distanceBetween(start, input.playerPosition);
  boss.update(input);
  const BossSnapshot chasing = boss.snapshot();
  assert(chasing.moving);
  assert(distanceBetween(chasing.position, input.playerPosition) < before);
  assert(chasing.facing.finite());
  assert(chasing.facing.y < 0.0f);  // 朝下方玩家
  // 活动范围钳制：任意追击路径不越出竞技场。
  assert(distanceBetween(chasing.position, config.arenaCenter) <=
         config.arenaRadius + 1e-3f);

  // 贴近后环绕走位：位置持续变化但维持交战间距。
  input.playerPosition = chasing.position + Vec2{config.preferredRange, 0.0f};
  bool orbited = false;
  Vec2 previous = chasing.position;
  for (int i = 0; i < 120; ++i) {
    input.tick += 16;
    boss.update(input);
    if (distanceBetween(boss.snapshot().position, previous) > 1e-5f) {
      orbited = true;
    }
    previous = boss.snapshot().position;
  }
  assert(orbited);
}

void testBossBasicAttackCycleAndVariantRotation() {
  BossController boss;
  assert(boss.start(BossConfig::karounDefaults()));
  const BossConfig config = boss.config();

  // 玩家贴身站位：首次延迟结束后冷却归零即起手普攻。
  BossFrameInput input;
  input.dtMs = 100;
  input.playerPosition = config.spawnPosition;
  input.playerAlive = true;
  input.tick = 0;
  while (boss.snapshot().basicAttackCastRemainingMs == 0) {
    input.tick += 100;
    boss.update(input);
    assert(input.tick <= config.basicAttackFirstDelayMs + 200);
  }
  assert(boss.snapshot().basicAttackVariant == 1);
  assert(!boss.snapshot().moving);  // 前摇期间站桩蓄力

  // 前摇推进到归零：命中帧后进入冷却，变体轮换到下一段。
  const Tick windupDeadline = input.tick + config.basicAttackWindupMs + 100;
  while (boss.snapshot().basicAttackCastRemainingMs > 0) {
    input.tick += 100;
    boss.update(input);
    assert(input.tick <= windupDeadline);
  }
  assert(boss.snapshot().basicAttackCooldownRemainingMs > 0);

  // 冷却结束后再次起手，变体继续轮换（2 → 0 回绕）。
  for (Tick elapsed = 0;
       elapsed <= config.basicAttackCooldownMs + 100 &&
       boss.snapshot().basicAttackCastRemainingMs == 0;
       elapsed += 100) {
    input.tick += 100;
    boss.update(input);
  }
  assert(boss.snapshot().basicAttackCastRemainingMs > 0);
  assert(boss.snapshot().basicAttackVariant == 2);
  while (boss.snapshot().basicAttackCastRemainingMs > 0) {
    input.tick += 100;
    boss.update(input);
  }
  for (Tick elapsed = 0;
       elapsed <= config.basicAttackCooldownMs + 100 &&
       boss.snapshot().basicAttackCastRemainingMs == 0;
       elapsed += 100) {
    input.tick += 100;
    boss.update(input);
  }
  assert(boss.snapshot().basicAttackVariant == 0);

  // 玩家阵亡时不再起手普攻：等前摇落地并走完冷却后保持静止。
  while (boss.snapshot().basicAttackCastRemainingMs > 0) {
    input.tick += 100;
    boss.update(input);
  }
  input.playerAlive = false;
  for (Tick elapsed = 0; elapsed <= config.basicAttackCooldownMs * 3;
       elapsed += 100) {
    input.tick += 100;
    boss.update(input);
    assert(boss.snapshot().basicAttackCastRemainingMs == 0);
  }
}

void testBossEncounterAppliesBasicAttackDamage() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Boss));
  const FixedPoint hpBefore = combat.snapshot().playerHp;

  // 玩家站在首领出生点旁：首次普攻延迟后挥击应命中扣血。
  const Vec2 nearBoss{0.5f, 0.74f};
  Tick tick = 0;
  while (combat.snapshot().playerHp == hpBefore && tick < 8000) {
    tick += 100;
    encounter.update({tick, 100, nearBoss, false,
                      EncounterController::kBossId});
  }
  assert(combat.snapshot().playerHp < hpBefore);
  // candidate 跟随首领实时位置。
  assert(encounter.snapshot().candidates.size() == 1);
  assert(encounter.snapshot().candidates.front().position ==
         encounter.snapshot().boss.position);
}

// 交战留白（Plan 2 Task 7）：首领体型越大空挡越大，身躯不遮住主角。
float averageBossGapOverTail(float bodyRadius) {
  BossController boss;
  BossConfig config = BossConfig::karounDefaults();
  config.bodyRadius = bodyRadius;
  config.arenaRadius = 1.0f;  // 放大竞技场，避免钳制干扰间距采样。
  assert(boss.start(config));

  BossFrameInput input;
  input.tick = 0;
  input.dtMs = 16;
  input.playerPosition = config.arenaCenter;  // 主角站在竞技场中心。
  input.playerAlive = true;

  const int totalFrames = 600;
  const int tailFrames = 100;
  double sum = 0.0;
  for (int frame = 0; frame < totalFrames; ++frame) {
    input.tick += 16;
    boss.update(input);
    if (frame >= totalFrames - tailFrames) {
      sum += distanceBetween(boss.snapshot().position, input.playerPosition);
    }
  }
  return static_cast<float>(sum / tailFrames);
}

void testBossBodyRadiusExpandsEngagementGap() {
  const float smallGap = averageBossGapOverTail(0.05f);
  const float largeGap = averageBossGapOverTail(0.20f);
  // 体型放大显著拉开空挡：大体型首领环绕在更远的最小空挡外。
  assert(largeGap > smallGap + 0.1f);
  assert(largeGap >= 0.28f);
}

}  // namespace

int main() {
  testPhaseThresholdsTriggerOnce();
  testCurrentStormRequiresBothNodesBeforeBossVulnerable();
  testFinalForgeFailsWithoutResonanceAndSucceedsWithUltimate();
  testRetryRestoresInitialState();
  testBossEncounterVictoryAndRetry();
  testBossChasesPlayerAndOrbitsAtPreferredRange();
  testBossBasicAttackCycleAndVariantRotation();
  testBossEncounterAppliesBasicAttackDamage();
  testBossBodyRadiusExpandsEngagementGap();
}
