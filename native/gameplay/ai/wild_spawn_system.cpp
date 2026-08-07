#include "wild_spawn_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// 数值镜像 EncounterController 的原型表（该表为文件局部函数，无法直接
// 复用）：野外敌人手动组装 TrainingTarget，forMode 工厂的 archetypeMaxHp
// 映射不适用，此处显式对齐同一套血量/韧性/伤害手感。
FixedPoint wildMaxHp(EnemyArchetype archetype) {
  switch (archetype) {
    case EnemyArchetype::RiftClaw: return fp(180);  // 脆皮快速
    case EnemyArchetype::Priest: return fp(240);    // 中等
    case EnemyArchetype::Guard: return fp(420);     // 重甲坦克
    case EnemyArchetype::Bruiser: return fp(480);   // 重甲近战
    case EnemyArchetype::Caster: return fp(200);    // 远程脆皮
    case EnemyArchetype::Elite: return fp(620);     // 精英高血量
  }
  return fp(300);
}

FixedPoint wildMaxPoise(EnemyArchetype archetype) {
  switch (archetype) {
    case EnemyArchetype::RiftClaw: return fp(80);
    case EnemyArchetype::Priest: return fp(90);
    case EnemyArchetype::Guard: return fp(160);
    case EnemyArchetype::Bruiser: return fp(200);
    case EnemyArchetype::Caster: return fp(70);
    case EnemyArchetype::Elite: return fp(240);
  }
  return fp(100);
}

FixedPoint wildBaseDamage(EnemyArchetype archetype) {
  switch (archetype) {
    case EnemyArchetype::RiftClaw: return fp(8);
    case EnemyArchetype::Priest: return fp(12);
    case EnemyArchetype::Guard: return fp(18);
    case EnemyArchetype::Bruiser: return fp(22);
    case EnemyArchetype::Caster: return fp(10);
    case EnemyArchetype::Elite: return fp(20);
  }
  return fp(10);
}

FixedPoint wildPoiseDamage(EnemyArchetype archetype) {
  switch (archetype) {
    case EnemyArchetype::RiftClaw: return fp(4);
    case EnemyArchetype::Priest: return fp(6);
    case EnemyArchetype::Guard: return fp(12);
    case EnemyArchetype::Bruiser: return fp(15);
    case EnemyArchetype::Caster: return fp(5);
    case EnemyArchetype::Elite: return fp(12);
  }
  return fp(5);
}

uint64_t nextStableSequence(uint64_t& next) {
  const uint64_t value = next;
  if (next != std::numeric_limits<uint64_t>::max()) ++next;
  return value;
}

// 与 EncounterController 同构的移动积分：速度钳制 + 区域投影。
Vec2 advancePosition(Vec2 position, Vec2 movement, int64_t dtMs,
                     const CombatRegion& region, float speedPerMs) {
  if (!position.finite() || !movement.finite() || dtMs <= 0) return position;
  const float distance = movement.length();
  if (!std::isfinite(distance) || distance <= 0.0f) return position;
  const float step = std::min(distance, speedPerMs * static_cast<float>(dtMs));
  return region.projectInside(position + movement * (step / distance));
}

bool effectLess(const CombatEffectRequest& left,
                const CombatEffectRequest& right) {
  if (left.tick != right.tick) return left.tick < right.tick;
  if (left.target != right.target) return left.target < right.target;
  if (left.sequence != right.sequence) return left.sequence < right.sequence;
  return left.transactionId < right.transactionId;
}

bool hitLess(const HitRequest& left, const HitRequest& right) {
  if (left.tick != right.tick) return left.tick < right.tick;
  if (left.attacker != right.attacker) return left.attacker < right.attacker;
  if (left.target != right.target) return left.target < right.target;
  if (left.sequence != right.sequence) return left.sequence < right.sequence;
  return left.transactionId < right.transactionId;
}

}  // namespace

struct WildSpawnSystem::Slot {
  Slot(EntityId id, EnemyArchetype archetype, Vec2 position,
       const CombatRegionConfig& region, std::size_t zoneCapacity)
      : target(targetConfig(archetype)),
        agent(std::make_unique<EnemyAgent>(
            archetype, aiConfig(archetype, region, zoneCapacity))) {
    enemy.id = id;
    enemy.hp = target.hp();
    enemy.poise = target.poise();
    enemy.archetype = archetype;
    enemy.position = position;
    enemy.spawnPosition = position;
    enemy.safeReturnPosition = position;
    maxHp = target.hp();
    lastObservedHp = maxHp;
  }

  Enemy enemy;
  FixedPoint maxHp = 0;
  TrainingTarget target;
  std::unique_ptr<EnemyAgent> agent;
  Vec2 facing = {0.0f, -1.0f};
  EnemyActionPhase phase = EnemyActionPhase::None;
  bool moving = false;
  bool attacking = false;
  bool hit = false;
  // 仇恨态：true 时 playerVisible=true 驱动 DecisionPolicy 追击/攻击。
  bool aggroed = false;
  // 活跃配额冻结：跳过 AI 与渲染，保留状态等待解冻。
  bool frozen = false;
  FixedPoint lastObservedHp = 0;
  // 死亡时刻（0 = 存活）：驱动 zone.respawnMs 重生计时。
  Tick deathTick = 0;
  int32_t zoneIndex = 0;
  int32_t slotIndex = 0;
};

struct WildSpawnSystem::Zone {
  WorldLayout::WorldSpawnZoneDef def;
  int32_t index = 0;
  int32_t chunkId = 0;
  bool active = false;
  // zone 内非零巡逻/出生点数量（生成数据用 0 填充未用槽位）。
  int32_t positionCount = 0;
};

EnemyArchetype WildSpawnSystem::fromSpawnArchetype(
    WorldLayout::SpawnArchetype archetype) {
  // SpawnArchetype 数值与 EnemyArchetype 严格一致（world_layout.gen.h 约定）。
  return static_cast<EnemyArchetype>(archetype);
}

int32_t WildSpawnSystem::chunkIdOf(Vec2 position) {
  // 与 WorldGrid::chunkIndexAt 同公式（id = y * countX + x）。
  const int32_t cx = std::clamp(
      static_cast<int32_t>(position.x * WorldLayout::kGridCountX), 0,
      WorldLayout::kGridCountX - 1);
  const int32_t cy = std::clamp(
      static_cast<int32_t>(position.y * WorldLayout::kGridCountY), 0,
      WorldLayout::kGridCountY - 1);
  return cy * WorldLayout::kGridCountX + cx;
}

CombatConfig WildSpawnSystem::targetConfig(EnemyArchetype archetype) {
  CombatConfig config = CombatConfig::defaults();
  config.trainingTargetHp = wildMaxHp(archetype);
  config.trainingTargetPoise = wildMaxPoise(archetype);
  // 禁用 TrainingTarget 自动复活（默认 2000ms，设 0 也会立即重置）：
  // 野外重生由本系统按 zone.respawnMs 显式调用 reset()。
  config.trainingDeathResetMs = kNoAutoRespawnMs;
  return config;
}

EnemyAiConfig WildSpawnSystem::aiConfig(EnemyArchetype archetype,
                                        const CombatRegionConfig& region,
                                        std::size_t zoneCapacity) {
  EnemyAiConfig config;
  switch (archetype) {
    case EnemyArchetype::RiftClaw:
      config = riftClawDefaults();
      break;
    case EnemyArchetype::Priest:
      config = radiantPriestDefaults();
      break;
    case EnemyArchetype::Guard:
      config = corrosionGuardDefaults();
      break;
    case EnemyArchetype::Bruiser:
      config = bruiserDefaults();
      break;
    case EnemyArchetype::Caster:
      config = casterDefaults();
      break;
    case EnemyArchetype::Elite:
      config = eliteDefaults();
      break;
  }
  config.region = region;
  // maxEnemies 按区容量显式设置（默认 3 仅服务竞技场）。
  config.maxEnemies = std::max<std::size_t>(
      1, std::min(zoneCapacity,
                  static_cast<std::size_t>(EnemyAiConfig::kMaxEnemies)));
  return config;
}

WildSpawnSystem::WildSpawnSystem()
    : WildSpawnSystem(std::vector<WorldLayout::WorldSpawnZoneDef>(
          WorldLayout::kSpawnZones.begin(), WorldLayout::kSpawnZones.end())) {}

WildSpawnSystem::~WildSpawnSystem() = default;

WildSpawnSystem::WildSpawnSystem(
    std::vector<WorldLayout::WorldSpawnZoneDef> zones) {
  zones_.reserve(zones.size());
  for (std::size_t i = 0; i < zones.size(); ++i) {
    Zone zone;
    zone.def = zones[i];
    zone.index = static_cast<int32_t>(i);
    zone.chunkId =
        chunkIdOf({zones[i].patrolCenterX, zones[i].patrolCenterY});
    for (int32_t p = 0; p < WorldLayout::kMaxSpawnPositions; ++p) {
      if (zones[i].positionX[p] != 0.0f || zones[i].positionY[p] != 0.0f) {
        zone.positionCount += 1;
      }
    }
    zones_.push_back(zone);
  }
}

void WildSpawnSystem::update(const WildSpawnFrameInput& input) {
  deaths_.clear();
  playerHits_.clear();
  const Tick tick = std::max(lastTick_, input.tick);
  const int64_t dtMs = std::max<int64_t>(0, input.dtMs);
  lastTick_ = tick;

  // 1) 同步上一步战斗结算：hp 差分检测受击/死亡（确定性，无事件依赖）。
  //    受击即仇恨并联动同组；死亡进入重生计时。
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    Slot& slot = *uptr;
    slot.hit = false;
    if (slot.deathTick > 0) continue;
    if (!slot.target.alive()) {
      slot.deathTick = tick;
      slot.phase = EnemyActionPhase::None;
      slot.attacking = false;
      slot.moving = false;
      slot.lastObservedHp = 0;
      deaths_.push_back({slot.enemy.id, slot.enemy.archetype, tick});
      continue;
    }
    if (slot.target.hp() < slot.lastObservedHp) {
      slot.hit = true;
      slot.aggroed = true;
      linkAggroGroup(slot);
    }
    slot.lastObservedHp = slot.target.hp();
    // kNoAutoRespawnMs 禁用自动复活，advance 只推进破韧/状态恢复。
    slot.target.advance(tick);
    slot.enemy.hp = slot.target.hp();
    slot.enemy.poise = slot.target.poise();
  }

  // 2) 分块生命周期：激活集进出 → 生成/回收。
  syncZones(input);

  // 3) 重生：分块仍激活时按 zone.respawnMs 计时。
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    Slot& slot = *uptr;
    if (slot.deathTick == 0) continue;
    const Zone& zone = zones_[slot.zoneIndex];
    if (!zone.active) continue;
    if (tick - slot.deathTick >= static_cast<Tick>(zone.def.respawnMs)) {
      respawnSlot(slot, zone);
    }
  }

  // 4) 活跃配额：超出当前档位上限时冻结离玩家最远的敌人。
  applyActiveQuota(input);

  // 5) AI / 巡逻 / 移动积分。
  std::vector<CombatEffectRequest> effects;
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    Slot& slot = *uptr;
    if (slot.deathTick > 0 || slot.frozen) continue;
    const Zone& zone = zones_[slot.zoneIndex];
    const CombatRegionConfig regionConfig{
        {zone.def.patrolCenterX, zone.def.patrolCenterY}, kZoneRegionRadius};
    const CombatRegion region(regionConfig);
    const Vec2 toPlayer = input.playerPosition - slot.enemy.position;
    const float playerDistance = toPlayer.length();
    const bool dormant = playerDistance >= kFarDistance;

    // 脱战牵引：远离出生点 / 玩家脱离远距 / 玩家阵亡时解除仇恨，
    // DecisionPolicy 随即走 ReturnToArea 回归巡逻。
    if (slot.aggroed) {
      const float fromSpawn =
          (slot.enemy.position - slot.enemy.spawnPosition).length();
      if (fromSpawn > kLeashRadius || dormant || !input.playerAlive) {
        slot.aggroed = false;
      }
    }
    // 感知半径内发现玩家 → 追击，并联动同仇恨组同伴。
    // 感知半径随性能档位收缩（Phase 5）。
    if (!slot.aggroed && input.playerAlive &&
        playerDistance <= perceptionRadiusForPerf()) {
      slot.aggroed = true;
      linkAggroGroup(slot);
    }

    // 巡逻点周期轮换：同时作为 ReturnToArea 的 safeReturnPosition，
    // 让脱战回程目标随巡逻推进（确定性轮换，无随机源）。
    const Vec2 patrolTarget = patrolPointFor(slot, tick);
    slot.enemy.safeReturnPosition = patrolTarget;

    if (dormant) {
      // 远距休眠：重生/巡逻计时均由 tick 推导、隐式推进，
      // 此处跳过 AI 决策与移动，把算力留给近距战斗。
      slot.moving = false;
      slot.attacking = false;
      continue;
    }

    Vec2 movement{0.0f, 0.0f};
    float speedPerMs = kMovementPerMillisecond;
    if (slot.aggroed) {
      // AI 距离 LOD：近距（<kNearDistance）决策周期 100ms；
      // 中距拉长到 400ms 降频——EnemyAgent 结构不支持跳过
      // tactical_planner，故以拉长 decisionPeriodMs 近似（任务约定）。
      slot.agent->setDecisionPeriodMs(
          playerDistance < kNearDistance ? kNearDecisionMs : kMidDecisionMs);

      EnemyWorldView world;
      world.tick = tick;
      world.selfId = slot.enemy.id;
      world.selfAlive = true;
      world.selfPosition = slot.enemy.position;
      world.selfFacing = slot.facing;
      world.spawnPosition = slot.enemy.spawnPosition;
      world.safeReturnPosition = slot.enemy.safeReturnPosition;
      world.region = regionConfig;
      world.playerId = CombatController::kPlayerId;
      world.playerPosition = input.playerPosition;
      world.playerVisible = slot.aggroed && input.playerAlive;
      world.playerReachable = true;
      world.recentlyHit = slot.hit;
      world.poise = slot.target.poise();
      world.staggered = slot.target.poiseBroken(tick);
      world.actionPhase = slot.phase;
      // allies 限定同 aggroGroup：避免 O(n²) 全局盟友查询。
      for (const std::unique_ptr<Slot>& allyPtr : slots_) {
        const Slot& ally = *allyPtr;
        if (&ally == &slot || ally.deathTick > 0 || ally.frozen) continue;
        const Zone& allyZone = zones_[ally.zoneIndex];
        if (allyZone.def.aggroGroup != zone.def.aggroGroup) continue;
        world.allies.push_back(
            {ally.enemy.id, ally.enemy.archetype, ally.target.hp(),
             ally.enemy.shield, ally.enemy.position, ally.target.alive(),
             region.contains(ally.enemy.position)});
      }

      EnemyExecutionContext execution;
      execution.attacker = slot.enemy.id;
      execution.targetAlive = input.playerAlive;
      execution.baseDamage = wildBaseDamage(slot.enemy.archetype);
      execution.poiseDamage = wildPoiseDamage(slot.enemy.archetype);
      execution.sequence = nextStableSequence(nextSequence_);
      const EnemyUpdateResult result =
          slot.agent->update({world, dtMs, execution, std::nullopt});
      slot.phase = result.phase;
      slot.attacking = result.phase == EnemyActionPhase::Windup ||
                       result.phase == EnemyActionPhase::Active;
      slot.hit = slot.hit || result.interrupted;
      const float movementLength = result.movement.length();
      slot.moving = result.movement.finite() &&
                    std::isfinite(movementLength) && movementLength > 0.0f;
      if (result.hit.has_value()) playerHits_.push_back(*result.hit);
      if (result.effect.has_value()) effects.push_back(*result.effect);
      movement = result.movement;
      // 追击朝向：面向玩家。
      if (toPlayer.finite() && playerDistance > 0.0f) {
        slot.facing = toPlayer * (1.0f / playerDistance);
      }
    } else {
      // 未仇恨：系统层确定性巡逻——朝轮换巡逻点慢速积分移动。
      slot.phase = EnemyActionPhase::None;
      slot.attacking = false;
      const Vec2 delta = patrolTarget - slot.enemy.position;
      const float distance = delta.length();
      if (delta.finite() && distance > 0.004f) {
        speedPerMs = kPatrolSpeedPerMs;
        movement = delta;
        slot.moving = true;
        slot.facing = delta * (1.0f / distance);
      } else {
        slot.moving = false;
      }
    }

    const Vec2 previous = slot.enemy.position;
    slot.enemy.position =
        advancePosition(previous, movement, dtMs, region, speedPerMs);
    // 宿主层碰撞解算：与遭遇敌人共用同一建筑碰撞集。
    if (input.positionResolver) {
      input.positionResolver(slot.enemy.position, kEnemyCollisionRadius);
    }
  }

  // 祭司/支援护盾效果：排序后叠加到目标敌人（与 encounter 同规则）。
  std::sort(effects.begin(), effects.end(), effectLess);
  for (const CombatEffectRequest& effect : effects) {
    if (effect.type != CombatEffectType::Shield || effect.amount <= 0) continue;
    Slot* target = findSlot(effect.target);
    if (target == nullptr || target->deathTick > 0) continue;
    const FixedPoint maximum = std::numeric_limits<FixedPoint>::max();
    target->enemy.shield = target->enemy.shield > maximum - effect.amount
                               ? maximum
                               : target->enemy.shield + effect.amount;
  }

  // 敌方命中排序后交宿主层转发 CombatController（确定性结算顺序）。
  std::sort(playerHits_.begin(), playerHits_.end(), hitLess);

  // 6) 快照：冻结槽位不参与渲染/锁定；死亡槽位保留（尸体淡出）。
  snapshot_.clear();
  snapshot_.reserve(slots_.size());
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    const Slot& slot = *uptr;
    if (slot.frozen) continue;
    WildEnemySnapshot snap;
    snap.id = slot.enemy.id;
    snap.archetype = static_cast<int32_t>(slot.enemy.archetype);
    snap.position = slot.enemy.position;
    snap.hp = slot.target.hp();
    snap.maxHp = slot.maxHp;
    snap.alive = slot.deathTick == 0;
    snap.facing = slot.facing;
    snap.moving = slot.moving;
    snap.attacking = slot.attacking;
    snap.windingUp = slot.phase == EnemyActionPhase::Windup;
    snap.hit = slot.hit;
    snapshot_.push_back(snap);
  }
}

void WildSpawnSystem::syncZones(const WildSpawnFrameInput& input) {
  for (Zone& zone : zones_) {
    const bool shouldBeActive =
        input.activeChunks == nullptr ||
        std::binary_search(input.activeChunks->begin(),
                           input.activeChunks->end(), zone.chunkId);
    if (shouldBeActive && !zone.active) {
      activateZone(zone);
    } else if (!shouldBeActive && zone.active) {
      deactivateZone(zone);
    }
  }
}

void WildSpawnSystem::activateZone(Zone& zone) {
  zone.active = true;
  const EnemyArchetype archetype = fromSpawnArchetype(zone.def.archetype);
  const CombatRegionConfig region{
      {zone.def.patrolCenterX, zone.def.patrolCenterY}, kZoneRegionRadius};
  const int32_t count = std::max(0, zone.def.count);
  for (int32_t slotIndex = 0; slotIndex < count; ++slotIndex) {
    if (registeredCount() >= kMaxRegistered) break;  // 全场注册配额
    const EntityId id = kIdBase + zone.index * 10 + slotIndex;
    const int32_t posIndex = slotIndex % WorldLayout::kMaxSpawnPositions;
    const Vec2 position{zone.def.positionX[posIndex],
                        zone.def.positionY[posIndex]};
    auto slot = std::make_unique<Slot>(id, archetype, position, region,
                                       static_cast<std::size_t>(count));
    slot->zoneIndex = zone.index;
    slot->slotIndex = slotIndex;
    // slots_ 维持 id 升序，保证遍历/结算顺序确定性。
    const auto insertAt = std::upper_bound(
        slots_.begin(), slots_.end(), id,
        [](EntityId lhs, const std::unique_ptr<Slot>& rhs) {
          return lhs < rhs->enemy.id;
        });
    slots_.insert(insertAt, std::move(slot));
  }
}

void WildSpawnSystem::deactivateZone(Zone& zone) {
  zone.active = false;
  // 分块卸载即回收全部槽位（含死亡计时），重新激活时全新生成。
  slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                              [&zone](const std::unique_ptr<Slot>& slot) {
                                return slot->zoneIndex == zone.index;
                              }),
               slots_.end());
}

void WildSpawnSystem::respawnSlot(Slot& slot, const Zone& zone) {
  slot.target.reset();
  slot.agent->reset();
  const int32_t posIndex = slot.slotIndex % WorldLayout::kMaxSpawnPositions;
  slot.enemy.position = {zone.def.positionX[posIndex],
                         zone.def.positionY[posIndex]};
  slot.enemy.spawnPosition = slot.enemy.position;
  slot.enemy.safeReturnPosition = slot.enemy.position;
  slot.enemy.hp = slot.maxHp;
  slot.enemy.poise = slot.target.poise();
  slot.enemy.shield = 0;
  slot.facing = {0.0f, -1.0f};
  slot.phase = EnemyActionPhase::None;
  slot.moving = false;
  slot.attacking = false;
  slot.hit = false;
  slot.aggroed = false;
  slot.deathTick = 0;
  slot.lastObservedHp = slot.maxHp;
}

void WildSpawnSystem::applyActiveQuota(const WildSpawnFrameInput& input) {
  std::vector<Slot*> alive;
  alive.reserve(slots_.size());
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    uptr->frozen = false;
    if (uptr->deathTick == 0) alive.push_back(uptr.get());
  }
  const int32_t maxActive = maxActiveForPerf();
  if (static_cast<int32_t>(alive.size()) <= maxActive) return;
  // 确定性排序：距离升序、id 升序；超出配额的最远者冻结。
  std::sort(alive.begin(), alive.end(),
            [&input](const Slot* left, const Slot* right) {
              const float dl =
                  (left->enemy.position - input.playerPosition).length();
              const float dr =
                  (right->enemy.position - input.playerPosition).length();
              if (dl != dr) return dl < dr;
              return left->enemy.id < right->enemy.id;
            });
  for (std::size_t i = static_cast<std::size_t>(maxActive); i < alive.size();
       ++i) {
    alive[i]->frozen = true;
  }
}

void WildSpawnSystem::linkAggroGroup(const Slot& source) {
  const Zone& sourceZone = zones_[source.zoneIndex];
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    Slot& slot = *uptr;
    if (&slot == &source || slot.deathTick > 0 || slot.aggroed) continue;
    const Zone& zone = zones_[slot.zoneIndex];
    if (zone.def.aggroGroup != sourceZone.def.aggroGroup) continue;
    const float distance =
        (slot.enemy.position - source.enemy.position).length();
    if (distance <= kAggroAllyRadius) slot.aggroed = true;
  }
}

Vec2 WildSpawnSystem::patrolPointFor(const Slot& slot, Tick tick) const {
  const Zone& zone = zones_[slot.zoneIndex];
  if (zone.positionCount <= 0) {
    return {zone.def.patrolCenterX, zone.def.patrolCenterY};
  }
  // tick 推导的确定性轮换：不同槽位以 slotIndex 错相。
  const int64_t cycle = tick / std::max<Tick>(1, kPatrolSwitchMs);
  int32_t target =
      static_cast<int32_t>((cycle + slot.slotIndex) % zone.positionCount);
  for (int32_t i = 0; i < WorldLayout::kMaxSpawnPositions; ++i) {
    if (zone.def.positionX[i] == 0.0f && zone.def.positionY[i] == 0.0f) {
      continue;
    }
    if (target == 0) {
      return {zone.def.positionX[i], zone.def.positionY[i]};
    }
    --target;
  }
  return {zone.def.patrolCenterX, zone.def.patrolCenterY};
}

WildSpawnSystem::Slot* WildSpawnSystem::findSlot(EntityId id) {
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    if (uptr->enemy.id == id) return uptr.get();
  }
  return nullptr;
}

const WildSpawnSystem::Slot* WildSpawnSystem::findSlot(EntityId id) const {
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    if (uptr->enemy.id == id) return uptr.get();
  }
  return nullptr;
}

std::vector<TargetCandidate> WildSpawnSystem::candidates() const {
  std::vector<TargetCandidate> result;
  result.reserve(slots_.size());
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    const Slot& slot = *uptr;
    if (slot.deathTick != 0 || slot.frozen) continue;
    result.push_back({static_cast<int32_t>(slot.enemy.id),
                      slot.enemy.position});
  }
  return result;
}

CombatTargetBinding WildSpawnSystem::combatBinding(EntityId id,
                                                   Vec2 playerPosition) {
  CombatTargetBinding binding;
  Slot* slot = findSlot(id);
  if (slot == nullptr || slot->deathTick > 0 || !slot->target.alive()) {
    return binding;
  }
  binding.id = slot->enemy.id;
  binding.target = &slot->target;
  binding.shield = &slot->enemy.shield;
  binding.damageContext.attackerPosition = playerPosition;
  binding.damageContext.defenderPosition = slot->enemy.position;
  binding.damageContext.defenderFacing = slot->facing;
  if (slot->enemy.archetype == EnemyArchetype::Guard) {
    binding.damageContext.directionalDefense = corrosionGuardDefense();
  }
  return binding;
}

int32_t WildSpawnSystem::activeCount() const {
  int32_t count = 0;
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    if (uptr->deathTick == 0 && !uptr->frozen) ++count;
  }
  return count;
}

bool WildSpawnSystem::positionOf(EntityId id, Vec2& outPosition) const {
  const Slot* slot = findSlot(id);
  if (slot == nullptr) return false;
  outPosition = slot->enemy.position;
  return true;
}

bool WildSpawnSystem::isAggroed(EntityId id) const {
  const Slot* slot = findSlot(id);
  return slot != nullptr && slot->aggroed;
}

bool WildSpawnSystem::isAlive(EntityId id) const {
  const Slot* slot = findSlot(id);
  return slot != nullptr && slot->deathTick == 0;
}

bool WildSpawnSystem::isFrozen(EntityId id) const {
  const Slot* slot = findSlot(id);
  return slot != nullptr && slot->frozen;
}

Tick WildSpawnSystem::decisionPeriodOf(EntityId id) const {
  const Slot* slot = findSlot(id);
  return slot != nullptr && slot->agent != nullptr
             ? slot->agent->decisionPeriodMs()
             : 0;
}

void WildSpawnSystem::setPerformanceLevel(int32_t lodLevel) {
  performanceLevel_ = lodLevel < 0 ? 0 : (lodLevel > 2 ? 2 : lodLevel);
}

int32_t WildSpawnSystem::maxActiveForPerf() const {
  return kMaxActiveByPerfLevel[performanceLevel_];
}

float WildSpawnSystem::perceptionRadiusForPerf() const {
  return kPerceptionRadius * kPerceptionScaleByPerfLevel[performanceLevel_];
}
