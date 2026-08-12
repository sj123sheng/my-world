// WildSpawnSystem（Phase 3.2/3.3 野外刷怪）确定性单元测试。
// 覆盖：archetype 映射 6 原型、分块进出生成/回收序列确定性、
// 重生计时、注册/活跃配额钳制（24/8）、仇恨组联动、距离 LOD 降频。
#include "gameplay/ai/wild_spawn_system.h"

#include <cassert>
#include <cstdio>
#include <vector>

namespace {

constexpr int64_t kStepMs = 16;

WorldLayout::WorldSpawnZoneDef makeZone(const char* zoneId,
                                        WorldLayout::SpawnArchetype archetype,
                                        int32_t count, float centerX,
                                        float centerY, const char* aggroGroup,
                                        int32_t respawnMs,
                                        std::vector<Vec2> positions = {}) {
  WorldLayout::WorldSpawnZoneDef def{};
  def.zoneId = zoneId;
  def.districtId = "test";
  def.aggroGroup = aggroGroup;
  def.archetype = archetype;
  def.count = count;
  def.respawnMs = respawnMs;
  def.patrolCenterX = centerX;
  def.patrolCenterY = centerY;
  for (std::size_t i = 0; i < WorldLayout::kMaxSpawnPositions; ++i) {
    if (i < positions.size()) {
      def.positionX[i] = positions[i].x;
      def.positionY[i] = positions[i].y;
    } else if (positions.empty()) {
      // 无显式巡逻点时以中心做出生点（count 个槽位复用同一点）。
      def.positionX[i] = centerX;
      def.positionY[i] = centerY;
    } else {
      def.positionX[i] = 0.0f;
      def.positionY[i] = 0.0f;
    }
  }
  return def;
}

WildSpawnFrameInput makeInput(Tick tick, Vec2 player,
                              const std::vector<int32_t>* chunks = nullptr) {
  WildSpawnFrameInput input;
  input.tick = tick;
  input.dtMs = kStepMs;
  input.playerPosition = player;
  input.playerAlive = true;
  input.activeChunks = chunks;
  return input;
}

// 远离全部测试敌人的安全玩家位置（不触发感知/配额近距偏好干扰）。
const Vec2 kFarPlayer{0.98f, 0.98f};

void testArchetypeMapping() {
  using SA = WorldLayout::SpawnArchetype;
  std::vector<WorldLayout::WorldSpawnZoneDef> zones;
  const SA archetypes[]{SA::RiftClaw, SA::Priest, SA::Guard,
                        SA::Bruiser,  SA::Caster, SA::Elite};
  for (int i = 0; i < 6; ++i) {
    zones.push_back(makeZone("arch", archetypes[i], 1, 0.5f, 0.5f,
                             "solo", 10000));
  }
  WildSpawnSystem wild(zones);
  wild.update(makeInput(kStepMs, kFarPlayer));
  assert(wild.snapshot().size() == 6);
  for (int i = 0; i < 6; ++i) {
    const WildEnemySnapshot& snap = wild.snapshot()[i];
    // id = kIdBase + zoneIndex * 10 + slotIndex；archetype 数值 0-5 全覆盖。
    assert(snap.id == WildSpawnSystem::kIdBase + i * 10);
    assert(snap.archetype == i);
    assert(snap.alive);
    assert(snap.hp == snap.maxHp && snap.maxHp > 0);
  }
  std::printf("testArchetypeMapping ok\n");
}

void testChunkLifecycleDeterminism() {
  // zone 中心 (0.05, 0.05) → 分块 (0,0) → chunkId 0。
  std::vector<WorldLayout::WorldSpawnZoneDef> zones{
      makeZone("north", WorldLayout::SpawnArchetype::RiftClaw, 2, 0.05f,
               0.05f, "north", 8000)};
  const std::vector<int32_t> inChunk{0};
  const std::vector<int32_t> outChunk{1};

  // 两个系统跑完全相同的输入序列，逐步断言快照一致（确定性）。
  WildSpawnSystem a(zones);
  WildSpawnSystem b(zones);
  Tick tick = 0;
  const auto stepBoth = [&](const std::vector<int32_t>* chunks) {
    tick += kStepMs;
    a.update(makeInput(tick, kFarPlayer, chunks));
    b.update(makeInput(tick, kFarPlayer, chunks));
    assert(a.snapshot().size() == b.snapshot().size());
    for (std::size_t i = 0; i < a.snapshot().size(); ++i) {
      assert(a.snapshot()[i].id == b.snapshot()[i].id);
      assert(a.snapshot()[i].position == b.snapshot()[i].position);
      assert(a.snapshot()[i].archetype == b.snapshot()[i].archetype);
    }
  };

  stepBoth(&inChunk);  // 进入分块 → 生成 2 敌。
  assert(a.snapshot().size() == 2);
  assert(a.snapshot()[0].id == WildSpawnSystem::kIdBase);
  assert(a.snapshot()[1].id == WildSpawnSystem::kIdBase + 1);
  stepBoth(&inChunk);
  stepBoth(&outChunk);  // 离开分块 → 全部回收。
  assert(a.snapshot().empty());
  assert(a.registeredCount() == 0);
  stepBoth(&inChunk);  // 重新进入 → 相同 id 序列重新生成。
  assert(a.snapshot().size() == 2);
  assert(a.snapshot()[0].id == WildSpawnSystem::kIdBase);
  assert(a.snapshot()[1].id == WildSpawnSystem::kIdBase + 1);
  std::printf("testChunkLifecycleDeterminism ok\n");
}

void testRespawnTiming() {
  constexpr Tick kRespawnMs = 2000;
  std::vector<WorldLayout::WorldSpawnZoneDef> zones{
      makeZone("respawn", WorldLayout::SpawnArchetype::Guard, 1, 0.5f, 0.5f,
               "r", kRespawnMs)};
  WildSpawnSystem wild(zones);
  Tick tick = 0;
  const auto step = [&](Vec2 player) {
    tick += kStepMs;
    wild.update(makeInput(tick, player));
  };
  step(kFarPlayer);
  const EntityId id = WildSpawnSystem::kIdBase;
  assert(wild.isAlive(id));
  const FixedPoint maxHp = wild.snapshot().front().maxHp;

  // 模拟玩家击杀：经战斗绑定直接打空血量（CombatController 外部通道路径）。
  CombatTargetBinding binding = wild.combatBinding(id, kFarPlayer);
  assert(binding.id == id && binding.target != nullptr);
  binding.target->applyHpDamage(maxHp + fp(1), tick);

  step(kFarPlayer);  // hp 差分检测 → 死亡事件。
  assert(!wild.isAlive(id));
  assert(wild.deaths().size() == 1);
  assert(wild.deaths().front().id == id);
  assert(wild.deaths().front().archetype == EnemyArchetype::Guard);
  assert(wild.deaths().front().tick == tick);
  const Tick deathTick = tick;

  // 重生前一步仍未到点。
  while (tick - deathTick < kRespawnMs - kStepMs) step(kFarPlayer);
  assert(!wild.isAlive(id));
  // 到点重生：满血、回出生点、deaths 清空。
  step(kFarPlayer);
  assert(wild.isAlive(id));
  assert(wild.snapshot().front().hp == maxHp);
  assert(wild.snapshot().front().position == (Vec2{0.5f, 0.5f}));
  assert(wild.deaths().empty());
  // 死亡槽位不再重复产生事件。
  step(kFarPlayer);
  assert(wild.deaths().empty());
  std::printf("testRespawnTiming ok\n");
}

void testQuotaClamp() {
  // 注册配额：13 区 × 2 = 26 > kMaxRegistered(24) → 只注册 24。
  std::vector<WorldLayout::WorldSpawnZoneDef> zones;
  for (int i = 0; i < 13; ++i) {
    const float coord = 0.1f + 0.05f * static_cast<float>(i);
    zones.push_back(makeZone("reg", WorldLayout::SpawnArchetype::RiftClaw, 2,
                             coord, 0.5f, "reg", 10000));
  }
  WildSpawnSystem regWild(zones);
  regWild.update(makeInput(kStepMs, kFarPlayer));
  assert(regWild.registeredCount() == WildSpawnSystem::kMaxRegistered);

  // 活跃配额：16 敌全部激活，8 近 8 远 → activeCount 钳到 8，
  // 离玩家近的保留，最远者冻结（不进快照/候选）。
  std::vector<WorldLayout::WorldSpawnZoneDef> activeZones;
  for (int i = 0; i < 4; ++i) {  // 近距 4 区 × 2 = 8，围绕 (0.5, 0.5)。
    const float offset = 0.02f + 0.01f * static_cast<float>(i);
    activeZones.push_back(makeZone(
        "near", WorldLayout::SpawnArchetype::Priest, 2,
        0.5f + (i % 2 == 0 ? offset : -offset),
        0.5f + (i < 2 ? offset : -offset), "near", 10000));
  }
  for (int i = 0; i < 4; ++i) {  // 远距 4 区 × 2 = 8，四角。
    const float x = i % 2 == 0 ? 0.15f : 0.85f;
    const float y = i < 2 ? 0.15f : 0.85f;
    activeZones.push_back(makeZone("far", WorldLayout::SpawnArchetype::Caster,
                                   2, x, y, "far", 10000));
  }
  WildSpawnSystem wild(activeZones);
  const Vec2 player{0.5f, 0.5f};
  wild.update(makeInput(kStepMs, player));
  assert(wild.registeredCount() == 16);
  assert(wild.activeCount() == WildSpawnSystem::kMaxActive);
  // 近区（zone 0-3）全部保留为活跃。
  for (int zone = 0; zone < 4; ++zone) {
    for (int slot = 0; slot < 2; ++slot) {
      assert(!wild.isFrozen(WildSpawnSystem::kIdBase + zone * 10 + slot));
    }
  }
  int frozenCount = 0;
  for (const WildEnemySnapshot& snap : wild.snapshot()) {
    if (wild.isFrozen(snap.id)) ++frozenCount;
    // 快照排除冻结者：出现的都应是近区敌人（zone 0-3 → id < 5040）。
    assert(snap.id < WildSpawnSystem::kIdBase + 40);
  }
  for (int zone = 4; zone < 8; ++zone) {
    for (int slot = 0; slot < 2; ++slot) {
      const EntityId id = WildSpawnSystem::kIdBase + zone * 10 + slot;
      assert(wild.isFrozen(id));
      ++frozenCount;
    }
  }
  assert(frozenCount == 8);
  assert(wild.snapshot().size() == 8);
  assert(wild.candidates().size() == 8);
  std::printf("testQuotaClamp ok\n");
}

void testAggroGroupLink() {
  // A(0.5,0.5) 与 B(0.63,0.5) 同组：玩家贴近 A（0.05 < 感知半径 0.12），
  // B 距玩家 0.139 超感知半径但距 A 0.13 ≤ 联动半径 0.15 → 被唤醒。
  // C 不同组、同距离 → 保持中立。
  std::vector<WorldLayout::WorldSpawnZoneDef> zones{
      makeZone("a", WorldLayout::SpawnArchetype::Bruiser, 1, 0.5f, 0.5f,
               "pack", 10000),
      makeZone("b", WorldLayout::SpawnArchetype::Bruiser, 1, 0.63f, 0.5f,
               "pack", 10000),
      makeZone("c", WorldLayout::SpawnArchetype::Bruiser, 1, 0.63f, 0.63f,
               "loner", 10000)};
  WildSpawnSystem wild(zones);
  const Vec2 player{0.5f, 0.45f};
  wild.update(makeInput(kStepMs, player));
  const EntityId a = WildSpawnSystem::kIdBase;
  const EntityId b = WildSpawnSystem::kIdBase + 10;
  const EntityId c = WildSpawnSystem::kIdBase + 20;
  assert(wild.isAggroed(a));
  assert(wild.isAggroed(b));  // 仇恨组联动。
  assert(!wild.isAggroed(c)); // 异组不联动。

  // 受击联动：玩家在中距（非休眠且超感知半径）时打击 A，
  // hp 差分触发仇恨并唤醒同组 B。
  const Vec2 midPlayer{0.5f, 0.65f};  // 距 A 0.15 > 感知 0.12，< 休眠 0.35
  WildSpawnSystem hitWild(zones);
  hitWild.update(makeInput(kStepMs, midPlayer));
  assert(!hitWild.isAggroed(a) && !hitWild.isAggroed(b));
  CombatTargetBinding binding = hitWild.combatBinding(a, midPlayer);
  assert(binding.id == a);
  binding.target->applyHpDamage(fp(10), kStepMs);
  hitWild.update(makeInput(2 * kStepMs, midPlayer));
  assert(hitWild.isAggroed(a));
  assert(hitWild.isAggroed(b));
  assert(!hitWild.isAggroed(c));
  std::printf("testAggroGroupLink ok\n");
}

void testDistanceLod() {
  std::vector<WorldLayout::WorldSpawnZoneDef> zones{
      makeZone("lod", WorldLayout::SpawnArchetype::RiftClaw, 1, 0.5f, 0.5f,
               "lod", 10000)};
  WildSpawnSystem wild(zones);
  const EntityId id = WildSpawnSystem::kIdBase;
  Tick tick = 0;

  // 近距（0.05 < 0.08）：感知仇恨后决策周期 = kNearDecisionMs。
  tick += kStepMs;
  wild.update(makeInput(tick, Vec2{0.5f, 0.45f}));
  assert(wild.isAggroed(id));
  assert(wild.decisionPeriodOf(id) == WildSpawnSystem::kNearDecisionMs);

  // 中距（0.15）：仍仇恨（未脱缰）但决策周期拉长到 kMidDecisionMs。
  tick += kStepMs;
  wild.update(makeInput(tick, Vec2{0.5f, 0.65f}));
  assert(wild.isAggroed(id));
  assert(wild.decisionPeriodOf(id) == WildSpawnSystem::kMidDecisionMs);

  // 远距休眠（>0.35）：解除仇恨且位置冻结（跳过 AI 与巡逻积分）。
  Vec2 positionBefore{};
  assert(wild.positionOf(id, positionBefore));
  for (int i = 0; i < 30; ++i) {
    tick += kStepMs;
    wild.update(makeInput(tick, Vec2{0.95f, 0.95f}));
  }
  assert(!wild.isAggroed(id));
  Vec2 positionAfter{};
  assert(wild.positionOf(id, positionAfter));
  assert(positionBefore == positionAfter);
  std::printf("testDistanceLod ok\n");
}

void testPatrolAndCandidates() {
  // 未仇恨时确定性巡逻：朝巡逻点慢速移动，candidates 可供软锁定。
  std::vector<WorldLayout::WorldSpawnZoneDef> zones{
      makeZone("patrol", WorldLayout::SpawnArchetype::Elite, 1, 0.5f, 0.5f,
               "p", 10000,
               {{0.46f, 0.5f}, {0.54f, 0.5f}, {0.5f, 0.54f}})};
  WildSpawnSystem wild(zones);
  // 玩家保持中距（> 感知半径 0.12，< 休眠线 0.35）：敌人不仇恨也不休眠，
  // 走系统层确定性巡逻。
  const Vec2 patrolPlayer{0.6f, 0.68f};
  Tick tick = 0;
  Vec2 previous{};
  tick += kStepMs;
  wild.update(makeInput(tick, patrolPlayer));
  assert(wild.positionOf(WildSpawnSystem::kIdBase, previous));
  bool moved = false;
  // 跑过一个巡逻轮换周期（kPatrolSwitchMs=6000）：出生点即第一巡逻点，
  // 轮换到下一点后才产生位移。
  for (int i = 0; i < 450; ++i) {
    tick += kStepMs;
    wild.update(makeInput(tick, patrolPlayer));
    Vec2 current{};
    assert(wild.positionOf(WildSpawnSystem::kIdBase, current));
    if (!(current == previous)) moved = true;
    previous = current;
  }
  assert(moved);
  const std::vector<TargetCandidate> candidates = wild.candidates();
  assert(candidates.size() == 1);
  assert(candidates.front().id == static_cast<int32_t>(WildSpawnSystem::kIdBase));
  // 默认构造：消费 WorldLayout::kSpawnZones（7 区），全激活下注册 ≤ 24。
  WildSpawnSystem worldDefault;
  worldDefault.update(makeInput(kStepMs, kFarPlayer));
  assert(worldDefault.registeredCount() > 0);
  assert(worldDefault.registeredCount() <= WildSpawnSystem::kMaxRegistered);
  std::printf("testPatrolAndCandidates ok\n");
}

void testGeneratedLayoutKeepsSpawnPlateauSafe() {
  std::vector<WorldLayout::WorldSpawnZoneDef> zones(
      WorldLayout::kSpawnZones.begin(), WorldLayout::kSpawnZones.end());
  WildSpawnSystem wild(zones);
  const std::vector<int32_t> spawnChunk{4};
  const Vec2 player{0.50f, 0.12f};
  wild.update(makeInput(kStepMs, player, &spawnChunk));

  for (const WildEnemySnapshot& enemy : wild.snapshot()) {
    assert((enemy.position - player).length() >= 0.15f);
  }
  assert(wild.snapshot().empty());
  std::printf("testGeneratedLayoutKeepsSpawnPlateauSafe ok\n");
}

}  // namespace

int main() {
  testArchetypeMapping();
  testChunkLifecycleDeterminism();
  testRespawnTiming();
  testQuotaClamp();
  testAggroGroupLink();
  testDistanceLod();
  testPatrolAndCandidates();
  testGeneratedLayoutKeepsSpawnPlateauSafe();
  std::printf("test_wild_spawn_system all passed\n");
  return 0;
}
