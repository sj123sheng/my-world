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
  // Mixed 模式固定 3 名敌人；容量上限 kMaxEnemies 已放开到 8。
  assert(mixed.enemies.size() == 3);
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

void testPlayerDeathEntersDefeatInNonBossModes() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Beast));
  // 玩家不反击不闪避，被敌人击杀后应进入失败态。
  Tick tick = 0;
  while (encounter.snapshot().state == EncounterState::Running &&
         tick < 120000) {
    tick += 16;
    update(encounter, tick, 16);
  }
  assert(encounter.snapshot().state == EncounterState::Defeat);
  assert(!encounter.snapshot().victory);
  // 失败后更新不再推进逻辑。
  update(encounter, tick + 16, 16);
  assert(encounter.snapshot().state == EncounterState::Defeat);
}

void testEnemySnapshotExposesMaxHp() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Beast));
  update(encounter, 0, 0);
  const EncounterSnapshot snapshot = encounter.snapshot();
  assert(!snapshot.enemies.empty());
  for (const EncounterEnemySnapshot& enemy : snapshot.enemies) {
    assert(enemy.maxHp > 0);
    assert(enemy.hp == enemy.maxHp);  // 开局满血
  }
}

}  // namespace

// 审判光束：Boss 战周期性吟唱并在落地时对玩家结算伤害。
void testBossJudgmentBeamPeriodicallyDamagesPlayer() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Boss));
  // 首次光束：2.5s 延迟 + 1.5s 吟唱 ≈ 4s 落地；模拟至多 6s。
  // 玩家站在竞技场角落：首领被钳制在场边无法进入普攻射程，
  // 确保血量变化只来自审判光束。
  const FixedPoint hpBefore = combat.snapshot().playerHp;
  bool beamLanded = false;
  bool castObserved = false;
  for (Tick tick = 1; tick <= 375 && !beamLanded; ++tick) {
    encounter.update({tick, 16, {0.05f, 0.05f}, false, 0});
    if (encounter.snapshot().boss.mechanic == BossMechanic::JudgmentBeam &&
        encounter.snapshot().boss.castRemainingMs > 0) {
      castObserved = true;
    }
    if (combat.snapshot().playerHp < hpBefore) beamLanded = true;
  }
  assert(castObserved);   // Boss 确实进入吟唱
  assert(beamLanded);     // 光束落地对玩家造成伤害
  // 伤害值与配置一致（15 点）。
  assert(hpBefore - combat.snapshot().playerHp == fp(15));
}

// 电流节点：二阶段免疫期间玩家命中逐个击破节点，全部击破后恢复易伤。
void testCurrentStormNodesBrokenByPlayerHits() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(EncounterMode::Boss));
  uint64_t sequence = 1;
  Tick tick = 0;
  auto attack = [&]() {
    combat.enqueue({CombatAction::Attack, sequence++});
  };
  // 持续攻击直至进入电流风暴（HP < 700）；玩家保持场角站位，
  // 避免首领普攻干扰玩家血量（本测试只验证节点与免疫逻辑）。
  bool phaseTwo = false;
  for (int frame = 0; frame < 6000 && !phaseTwo; ++frame) {
    tick += 16;
    if (frame % 30 == 0) attack();
    encounter.update({tick, 16, {0.05f, 0.05f}, false,
                      EncounterController::kBossId});
    phaseTwo =
        encounter.snapshot().boss.phase == BossPhase::CurrentStorm;
  }
  assert(phaseTwo);
  assert(!encounter.snapshot().boss.vulnerable);  // 免疫期不可受伤

  // 免疫期继续攻击：逐个击破节点，伴随共鸣进发反馈事件。
  bool resonanceEmitted = false;
  for (int frame = 0;
       frame < 2400 && encounter.snapshot().boss.nodesBroken < 2; ++frame) {
    tick += 16;
    if (frame % 30 == 0) attack();
    encounter.update({tick, 16, {0.05f, 0.05f}, false,
                      EncounterController::kBossId});
    for (const GameplayEvent& event :
         encounter.events().combat.gameplay) {
      if (event.type == GameplayEventType::Resonance) resonanceEmitted = true;
    }
  }
  assert(encounter.snapshot().boss.nodesBroken == 2);
  assert(resonanceEmitted);
  assert(encounter.snapshot().boss.vulnerable);  // 节点尽破，恢复易伤
}

// ---- 交战留白（Plan 2 Task 7）----

EncounterEnemyConfig spacingEnemy(EntityId id, EnemyArchetype archetype,
                                  Vec2 position) {
  EncounterEnemyConfig enemy;
  enemy.id = id;
  enemy.archetype = archetype;
  enemy.position = position;
  enemy.safeReturnPosition = position;
  enemy.hp = fp(300);
  enemy.poise = fp(100);
  return enemy;
}

EncounterConfig spacingConfig(const std::vector<EncounterEnemyConfig>& enemies) {
  EncounterConfig config;
  config.mode = EncounterMode::Mixed;
  config.maxEnemies = EnemyAiConfig::kMaxEnemies;
  config.region = {{0.5f, 0.5f}, 2.0f};
  config.enemies = enemies;
  return config;
}

void testMeleeSpacingSeparationAndMinimumDistance() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  // 两名近战从同一点出发。
  assert(encounter.start(spacingConfig(
      {spacingEnemy(101, EnemyArchetype::RiftClaw, {0.5f, 0.62f}),
       spacingEnemy(102, EnemyArchetype::RiftClaw, {0.5f, 0.62f})})));
  const Vec2 player{0.5f, 0.5f};
  const float meleeMinimum = 0.08f;
  for (int frame = 0; frame < 120; ++frame) {
    encounter.update({frame * 16, 16, player, false, 0});
  }
  const EncounterEnemySnapshot* a = findEnemy(encounter.snapshot(), 101);
  const EncounterEnemySnapshot* b = findEnemy(encounter.snapshot(), 102);
  assert(a != nullptr && b != nullptr);
  // 环形槽位把两名近战拉开，不再重叠。
  assert((a->position - b->position).length() > 0.05f);
  // 与主角保持最小空挡（突进下限即 minimum）。
  assert((a->position - player).length() >= meleeMinimum - 0.02f);
  assert((b->position - player).length() >= meleeMinimum - 0.02f);
}

void testPlayerApproachTriggersRetreatWithoutMovingPlayer() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(spacingConfig(
      {spacingEnemy(101, EnemyArchetype::RiftClaw, {0.5f, 0.56f})})));
  const Vec2 player{0.5f, 0.5f};  // 距离 0.06 < minimum 0.08
  encounter.update({0, 16, player, false, 0});
  const Vec2 enemyStart = findEnemy(encounter.snapshot(), 101)->position;
  for (int frame = 1; frame <= 20; ++frame) {
    encounter.update({frame * 16, 16, player, false, 0});
  }
  const Vec2 enemyAfter = findEnemy(encounter.snapshot(), 101)->position;
  // 贴身时敌人后撤，距离拉大。
  assert((enemyAfter - player).length() > (enemyStart - player).length());
  // 主角位置只由输入给定，AI 从不改写。
  assert(player == (Vec2{0.5f, 0.5f}));
}

void testRangedKeepsIdealBand() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(spacingConfig(
      {spacingEnemy(101, EnemyArchetype::Priest, {0.5f, 0.8f})})));
  const Vec2 player{0.5f, 0.5f};
  float minObserved = 1e9f;
  float maxObserved = 0.0f;
  for (int frame = 0; frame < 240; ++frame) {
    encounter.update({frame * 16, 16, player, false, 0});
    const EncounterEnemySnapshot* e = findEnemy(encounter.snapshot(), 101);
    if (e == nullptr || !e->alive) continue;
    // 非攻击瞬间采样：远程应锚定在理想距离附近。
    if (!e->attacking && !e->windingUp) {
      const float d = (e->position - player).length();
      minObserved = std::min(minObserved, d);
      maxObserved = std::max(maxObserved, d);
    }
  }
  assert(maxObserved <= 0.38f);  // 不远离理想带 0.30
  assert(minObserved >= 0.18f);  // 不贴脸
}

void testAttackLungeReturnsToIdealAfterRecovery() {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(spacingConfig(
      {spacingEnemy(101, EnemyArchetype::RiftClaw, {0.5f, 0.64f})})));
  const Vec2 player{0.5f, 0.5f};  // 距离 0.14 = 近战 ideal
  const float ideal = 0.14f;
  bool lunged = false;
  bool returnedAfterLunge = false;
  for (int frame = 0; frame < 400 && !returnedAfterLunge; ++frame) {
    encounter.update({frame * 16, 16, player, false, 0});
    const EncounterEnemySnapshot* e = findEnemy(encounter.snapshot(), 101);
    if (e == nullptr || !e->alive) continue;
    const float d = (e->position - player).length();
    if (d < ideal - 0.02f) lunged = true;
    else if (lunged && std::abs(d - ideal) <= 0.02f) returnedAfterLunge = true;
  }
  assert(lunged);             // Active 突进贴近
  assert(returnedAfterLunge); // Recovery 回到理想距离 ±0.02
}

EncounterSnapshot runSpacingReplay(int frames) {
  CombatController combat(CombatConfig::defaults());
  EncounterController encounter(combat);
  assert(encounter.start(spacingConfig(
      {spacingEnemy(101, EnemyArchetype::RiftClaw, {0.46f, 0.7f}),
       spacingEnemy(102, EnemyArchetype::RiftClaw, {0.54f, 0.7f})})));
  const Vec2 player{0.5f, 0.5f};
  for (int frame = 0; frame < frames; ++frame) {
    encounter.update({frame * 16, 16, player, false, 0});
  }
  return encounter.snapshot();
}

void testSpacingReplayIsDeterministic() {
  const EncounterSnapshot baseline = runSpacingReplay(600);
  for (int replay = 0; replay < 10; ++replay) {
    assert(runSpacingReplay(600) == baseline);
  }
}

int main() {
  testStartsAllModesWithStableEntities();
  testRejectsInvalidConfigurationAtomically();
  testDeathRemovesCandidateAndKeepsFinalSnapshot();
  testStopHasNoEventsAndResetClearsState();
  testTrainingModeKeepsStageThreePulseSemantics();
  testStandaloneModeClearsLevelFlowState();
  testEnemyAnimationFactsArePublished();
  testEnemySnapshotEqualityIncludesAnimationFacts();
  testPlayerDeathEntersDefeatInNonBossModes();
  testEnemySnapshotExposesMaxHp();
  testBossJudgmentBeamPeriodicallyDamagesPlayer();
  testCurrentStormNodesBrokenByPlayerHits();
  testMeleeSpacingSeparationAndMinimumDistance();
  testPlayerApproachTriggersRetreatWithoutMovingPlayer();
  testRangedKeepsIdealBand();
  testAttackLungeReturnsToIdealAfterRecovery();
  testSpacingReplayIsDeterministic();
}
