#include "wild_spawn_system.h"

#include "engagement_spacing.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string_view>
#include <unordered_map>

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

EntityId projectedEntityId(uint64_t stableId) {
  const uint32_t folded = static_cast<uint32_t>(stableId) ^
                          static_cast<uint32_t>(stableId >> 32U);
  return 0x40000000U | (folded & 0x3fffffffU);
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
  Slot(EntityId id, uint64_t stableId, EnemyArchetype archetype, Vec2 position,
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
    this->stableId = stableId;
    patrolCenter = region.center;
    maxHp = target.hp();
    lastObservedHp = maxHp;
  }

  Enemy enemy;
  uint64_t stableId = 0;
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
  bool procedural = false;
  ChunkCoord chunk{};
  Vec2 patrolCenter{};
  std::vector<Vec2> patrolPoints;
  uint64_t aggroGroup = 0;
  Tick respawnMs = 0;
};

struct WildSpawnSystem::Zone {
  WorldLayout::WorldSpawnZoneDef def;
  int32_t index = 0;
  ChunkCoord chunk{};
  bool active = false;
  // zone 内非零巡逻/出生点数量（生成数据用 0 填充未用槽位）。
  int32_t positionCount = 0;
};

EnemyArchetype WildSpawnSystem::fromSpawnArchetype(
    WorldLayout::SpawnArchetype archetype) {
  // SpawnArchetype 数值与 EnemyArchetype 严格一致（world_layout.gen.h 约定）。
  return static_cast<EnemyArchetype>(archetype);
}

ChunkCoord WildSpawnSystem::chunkCoordOf(Vec2 position) {
  return NormalizeWorldPosition({0, 0}, position.x, position.y).chunk;
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
    : WildSpawnSystem(std::vector<WorldLayout::WorldSpawnZoneDef>{}) {}

WildSpawnSystem::~WildSpawnSystem() = default;

WildSpawnSystem::WildSpawnSystem(
    std::vector<WorldLayout::WorldSpawnZoneDef> zones) {
  buildZones(zones);
}

void WildSpawnSystem::buildZones(
    const std::vector<WorldLayout::WorldSpawnZoneDef>& zones) {
  zones_.reserve(zones.size());
  for (std::size_t i = 0; i < zones.size(); ++i) {
    Zone zone;
    zone.def = zones[i];
    zone.index = static_cast<int32_t>(i);
    zone.chunk =
        chunkCoordOf({zones[i].patrolCenterX, zones[i].patrolCenterY});
    for (int32_t p = 0; p < WorldLayout::kMaxSpawnPositions; ++p) {
      if (zones[i].positionX[p] != 0.0f || zones[i].positionY[p] != 0.0f) {
        zone.positionCount += 1;
      }
    }
    zones_.push_back(zone);
  }
}

void WildSpawnSystem::resetZones(
    std::vector<WorldLayout::WorldSpawnZoneDef> zones) {
  // 清空已生成敌人与本步事件，回到新建构造的等价状态。
  slots_.clear();
  snapshot_.clear();
  deaths_.clear();
  playerHits_.clear();
  lastTick_ = 0;
  nextSequence_ = 1;
  proceduralStateChunks_.clear();
  hasLastPlayerChunk_ = false;
  zones_.clear();
  buildZones(zones);
}

void WildSpawnSystem::update(const WildSpawnFrameInput& input) {
  deaths_.clear();
  playerHits_.clear();
  const Tick tick = std::max(lastTick_, input.tick);
  const int64_t dtMs = std::max<int64_t>(0, input.dtMs);
  lastTick_ = tick;

  if (hasLastPlayerChunk_ && input.playerChunk != lastPlayerChunk_) {
    const Vec2 rebase{
        static_cast<float>(lastPlayerChunk_.x - input.playerChunk.x),
        static_cast<float>(lastPlayerChunk_.y - input.playerChunk.y)};
    for (const std::unique_ptr<Slot>& uptr : slots_) {
      Slot& slot = *uptr;
      if (!slot.procedural) continue;
      slot.enemy.position = slot.enemy.position + rebase;
      slot.enemy.spawnPosition = slot.enemy.spawnPosition + rebase;
      slot.enemy.safeReturnPosition = slot.enemy.safeReturnPosition + rebase;
      slot.patrolCenter = slot.patrolCenter + rebase;
      for (Vec2& point : slot.patrolPoints) point = point + rebase;
    }
  }
  lastPlayerChunk_ = input.playerChunk;
  hasLastPlayerChunk_ = true;

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
      if (slot.procedural && input.proceduralChunks != nullptr) {
        const auto runtime = input.proceduralChunks->find(slot.chunk);
        if (runtime != input.proceduralChunks->end()) {
          runtime->second.defeatedEnemyIds.insert(slot.stableId);
        }
      }
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
  syncProceduralChunks(input);

  // 3) 重生：分块仍激活时按 zone.respawnMs 计时。
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    Slot& slot = *uptr;
    if (slot.deathTick == 0) continue;
    if (slot.procedural) continue;
    if (tick - slot.deathTick >= slot.respawnMs) {
      respawnSlot(slot);
    }
  }

  // 4) 活跃配额：超出当前档位上限时冻结离玩家最远的敌人。
  applyActiveQuota(input);

  // 5) AI / 巡逻 / 移动积分。
  std::vector<CombatEffectRequest> effects;
  // 交战留白（Plan 2 Task 7）：按仇恨组收集存活仇恨参与者（ID 排序），
  // 同组敌人共享环形槽位；不同组各自成环，避免跨区拉拽。
  std::unordered_map<uint64_t, std::vector<EntityId>> participantsByGroup;
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    const Slot& s = *uptr;
    if (s.deathTick > 0 || s.frozen || !s.aggroed) continue;
    participantsByGroup[s.aggroGroup].push_back(s.enemy.id);
  }
  for (auto& entry : participantsByGroup) {
    std::sort(entry.second.begin(), entry.second.end());
  }
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    Slot& slot = *uptr;
    if (slot.deathTick > 0 || slot.frozen) continue;
    const CombatRegionConfig regionConfig{slot.patrolCenter,
                                          kZoneRegionRadius};
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
        if (ally.aggroGroup != slot.aggroGroup) continue;
        world.allies.push_back(
            {ally.enemy.id, ally.enemy.archetype, ally.target.hp(),
             ally.enemy.shield, ally.enemy.position, ally.target.alive(),
             region.contains(ally.enemy.position)});
      }
      // 交战留白：原型距离、环形槽位与邻居分离注入世界视图。
      const EngagementRange engagementRange =
          EngagementRangeFor(slot.enemy.archetype, 0.0f, false);
      world.engagementRange = engagementRange;
      std::vector<EntityId> participants;
      const auto participantsIt = participantsByGroup.find(slot.aggroGroup);
      if (participantsIt != participantsByGroup.end()) {
        participants = participantsIt->second;
      } else {
        participants.push_back(slot.enemy.id);
      }
      world.engagementSlot = EngagementSlotPosition(
          slot.enemy.id, input.playerPosition, engagementRange.ideal,
          participants);
      std::vector<EngagementNeighbor> engagementNeighbors;
      for (const std::unique_ptr<Slot>& allyPtr : slots_) {
        const Slot& ally = *allyPtr;
        if (&ally == &slot || ally.deathTick > 0 || ally.frozen) continue;
        if (ally.aggroGroup != slot.aggroGroup) continue;
        engagementNeighbors.push_back({ally.enemy.id, ally.enemy.position});
      }
      world.separationOffset = SeparationOffset(
          slot.enemy.id, slot.enemy.position, engagementNeighbors,
          engagementRange.minimum);

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
    snap.stableId = slot.stableId;
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
                           input.activeChunks->end(), zone.chunk);
    if (shouldBeActive && !zone.active) {
      activateZone(zone);
    } else if (!shouldBeActive && zone.active) {
      deactivateZone(zone);
    }
  }
}

void WildSpawnSystem::syncProceduralChunks(
    const WildSpawnFrameInput& input) {
  proceduralStateChunks_.clear();
  if (input.proceduralChunks == nullptr) {
    slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                                [](const std::unique_ptr<Slot>& slot) {
                                  return slot->procedural;
                                }),
                 slots_.end());
    return;
  }
  for (const auto& item : *input.proceduralChunks) {
    proceduralStateChunks_.insert(item.first);
  }
  slots_.erase(
      std::remove_if(
          slots_.begin(), slots_.end(),
          [&input](const std::unique_ptr<Slot>& slot) {
            if (!slot->procedural) return false;
            const auto runtime = input.proceduralChunks->find(slot->chunk);
            return runtime == input.proceduralChunks->end() ||
                   !runtime->second.active ||
                   runtime->second.defeatedEnemyIds.count(slot->stableId) > 0;
          }),
      slots_.end());

  const auto hasStableId = [this](uint64_t stableId) {
    return std::any_of(slots_.begin(), slots_.end(),
                       [stableId](const std::unique_ptr<Slot>& slot) {
                         return slot->stableId == stableId;
                       });
  };
  const auto idInUse = [this](EntityId id) {
    return std::any_of(slots_.begin(), slots_.end(),
                       [id](const std::unique_ptr<Slot>& slot) {
                         return slot->enemy.id == id;
                       });
  };
  for (auto& item : *input.proceduralChunks) {
    const ChunkCoord coord = item.first;
    ProceduralChunkRuntime& runtime = item.second;
    if (!runtime.active) continue;
    for (const ProceduralEnemySpawn& spawn : runtime.content.enemies) {
      if (runtime.defeatedEnemyIds.count(spawn.stableId) > 0 ||
          hasStableId(spawn.stableId) || registeredCount() >= kMaxRegistered) {
        continue;
      }
      EntityId id = projectedEntityId(spawn.stableId);
      while (idInUse(id)) {
        id = id == std::numeric_limits<EntityId>::max() ? 0x40000000U
                                                        : id + 1U;
      }
      const Vec2 position = RelativeWorldPosition(
          {coord, spawn.position}, {input.playerChunk, {0.0f, 0.0f}});
      const CombatRegionConfig region{position, kZoneRegionRadius};
      auto slot = std::make_unique<Slot>(
          id, spawn.stableId, spawn.archetype, position, region,
          runtime.content.enemies.size());
      slot->procedural = true;
      slot->chunk = coord;
      slot->patrolPoints.push_back(position);
      slot->aggroGroup = StableChunkHash(spawn.stableId, coord, 0x51ULL);
      const auto insertAt = std::upper_bound(
          slots_.begin(), slots_.end(), id,
          [](EntityId lhs, const std::unique_ptr<Slot>& rhs) {
            return lhs < rhs->enemy.id;
          });
      slots_.insert(insertAt, std::move(slot));
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
    auto slot = std::make_unique<Slot>(id, id, archetype, position, region,
                                       static_cast<std::size_t>(count));
    slot->zoneIndex = zone.index;
    slot->slotIndex = slotIndex;
    slot->chunk = zone.chunk;
    slot->respawnMs = static_cast<Tick>(std::max(0, zone.def.respawnMs));
    slot->aggroGroup = zone.def.aggroGroup.empty()
                           ? std::hash<std::string_view>{}(std::string_view{})
                           : std::hash<std::string_view>{}(zone.def.aggroGroup);
    for (int32_t i = 0; i < WorldLayout::kMaxSpawnPositions; ++i) {
      if (zone.def.positionX[i] != 0.0f || zone.def.positionY[i] != 0.0f) {
        slot->patrolPoints.push_back(
            {zone.def.positionX[i], zone.def.positionY[i]});
      }
    }
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
                                return !slot->procedural &&
                                       slot->zoneIndex == zone.index;
                              }),
               slots_.end());
}

void WildSpawnSystem::respawnSlot(Slot& slot) {
  slot.target.reset();
  slot.agent->reset();
  const int32_t posIndex = slot.patrolPoints.empty()
                               ? 0
                               : slot.slotIndex % slot.patrolPoints.size();
  slot.enemy.position = slot.patrolPoints.empty()
                            ? slot.patrolCenter
                            : slot.patrolPoints[posIndex];
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
  for (const std::unique_ptr<Slot>& uptr : slots_) {
    Slot& slot = *uptr;
    if (&slot == &source || slot.deathTick > 0 || slot.aggroed) continue;
    if (slot.aggroGroup != source.aggroGroup) continue;
    const float distance =
        (slot.enemy.position - source.enemy.position).length();
    if (distance <= kAggroAllyRadius) slot.aggroed = true;
  }
}

Vec2 WildSpawnSystem::patrolPointFor(const Slot& slot, Tick tick) const {
  if (slot.patrolPoints.empty()) return slot.patrolCenter;
  // tick 推导的确定性轮换：不同槽位以 slotIndex 错相。
  const int64_t cycle = tick / std::max<Tick>(1, kPatrolSwitchMs);
  int32_t target =
      static_cast<int32_t>((cycle + slot.slotIndex) % slot.patrolPoints.size());
  return slot.patrolPoints[static_cast<size_t>(target)];
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
    // deathTick 在死亡次帧才结算，存活判定兜底保证刚被击杀的
    // 敌人立即退出软锁定候选（与遭遇候选的 alive 过滤对齐）。
    if (slot.deathTick != 0 || slot.frozen || !slot.target.alive()) continue;
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

size_t WildSpawnSystem::proceduralStateChunkCount() const {
  return proceduralStateChunks_.size();
}

std::vector<EntityId> WildSpawnSystem::deactivateProceduralChunk(
    ChunkCoord coord) {
  std::vector<EntityId> removed;
  for (const std::unique_ptr<Slot>& slot : slots_) {
    if (slot->procedural && slot->chunk == coord) {
      removed.push_back(slot->enemy.id);
    }
  }
  slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                              [coord](const std::unique_ptr<Slot>& slot) {
                                return slot->procedural &&
                                       slot->chunk == coord;
                              }),
               slots_.end());
  snapshot_.erase(std::remove_if(snapshot_.begin(), snapshot_.end(),
                                 [&removed](const WildEnemySnapshot& enemy) {
                                   return std::find(removed.begin(),
                                                    removed.end(), enemy.id) !=
                                          removed.end();
                                 }),
                  snapshot_.end());
  proceduralStateChunks_.erase(coord);
  std::sort(removed.begin(), removed.end());
  return removed;
}
