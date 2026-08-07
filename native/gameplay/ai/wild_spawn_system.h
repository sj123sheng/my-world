#pragma once

#include "enemy_agent.h"
#include "enemy_archetypes.h"
#include "gameplay/combat/combat_controller.h"
#include "gameplay/entities/enemy.h"
#include "gameplay/targeting/soft_targeting.h"
#include "generated/world_layout.gen.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// 野外敌人逐帧快照：只保留渲染与任务需要的字段（仿 EncounterEnemySnapshot
// 裁剪），渲染层经 Surface::wildEnemies3d 只读消费。
struct WildEnemySnapshot {
  EntityId id = 0;
  // 与 EnemyArchetype 数值一致的 int，避免渲染层反向依赖 gameplay 枚举。
  int32_t archetype = 0;
  Vec2 position;
  FixedPoint hp = 0;
  FixedPoint maxHp = 0;
  bool alive = false;
  Vec2 facing = {0.0f, -1.0f};
  bool moving = false;
  bool attacking = false;
  bool windingUp = false;
  bool hit = false;
};

// 野外敌人死亡事件（仅本步新增）：供宿主层重生调度与击杀统计消费。
// 注意：玩家击杀产生的权威 Death 事件来自 CombatController 伤害结算
// （target = 野外敌人 id），自动进入现有掉落/任务管线。
struct WildDeathEvent {
  EntityId id = 0;
  EnemyArchetype archetype = EnemyArchetype::RiftClaw;
  Tick tick = 0;
};

struct WildSpawnFrameInput {
  Tick tick = 0;
  int64_t dtMs = 0;
  Vec2 playerPosition;
  bool playerAlive = true;
  // 当前激活分块 id 集合（升序）；nullptr 时全部刷怪区激活（测试用）。
  const std::vector<int32_t>* activeChunks = nullptr;
  // 宿主层碰撞解算（建筑推出/沿墙滑动），可为空。
  std::function<void(Vec2&, float)> positionResolver;
};

// 野外刷怪系统（Phase 3.2/3.3）：独立于 EncounterController 的野外敌人
// 生命周期管理——分块激活时按 WorldLayout::kSpawnZones 生成敌人，分块
// 卸载时回收；死亡按 zone.respawnMs 重生；zone 内周期巡逻；同 aggroGroup
// 仇恨联动；全场注册/同屏活跃配额；按玩家距离做 AI 决策 LOD。
// 战斗解算走 CombatController 外部通道（setExternalTargetBinding /
// enqueueExternalEnemyHit），不触碰 EncounterController 状态机，
// Victory/Defeat 信号天然隔离。
// 确定性：固定步长驱动，全部计时用 Tick 累加，无随机源；
// 槽位按 id 升序遍历，同输入同输出。
class WildSpawnSystem {
 public:
  // ---- 调参常量（集中管理）----
  // 敌人 EntityId 基址：避开玩家 1 / 训练假人 1001 / 遭遇敌人 2001+ /
  // Boss 3001；id = kIdBase + zoneIndex * 10 + slotIndex。
  static constexpr EntityId kIdBase = 5000;
  // 全场注册上限（已生成槽位总数）。
  static constexpr int32_t kMaxRegistered = 24;
  // 同屏活跃上限：超出时优先保留离玩家近的，其余冻结（跳过 AI 与渲染）。
  static constexpr int32_t kMaxActive = 8;
  // 性能降级联动（Phase 5，与 PerformanceGuard::lodLevel() 档位对应）：
  // lodLevel 0=完整/1=中等/2=精简，活跃上限 8→6→4，感知半径
  // 同步收缩，降低远处敌人的 AI 与战斗压力。
  static constexpr int32_t kMaxActiveByPerfLevel[3] = {8, 6, 4};
  static constexpr float kPerceptionScaleByPerfLevel[3] = {1.0f, 0.85f,
                                                           0.7f};
  // 距离 LOD 分档（世界单位）：近距满频决策，中距降频，远距休眠。
  static constexpr float kNearDistance = 0.08f;
  static constexpr float kMidDistance = 0.20f;
  static constexpr float kFarDistance = 0.35f;
  // LOD 决策周期（ms）：近距 100ms；中距 400ms（EnemyAgent 无法跳过
  // tactical_planner，以拉长决策周期近似降频，见 .cpp 注释）。
  static constexpr Tick kNearDecisionMs = 100;
  static constexpr Tick kMidDecisionMs = 400;
  // 感知半径：玩家进入后敌人转入追击（仇恨）。
  static constexpr float kPerceptionRadius = 0.12f;
  // 仇恨组联动半径：同 aggroGroup 任一敌人受击/发现玩家时唤醒组内同伴。
  static constexpr float kAggroAllyRadius = 0.15f;
  // 脱战牵引半径：离开出生点过远或玩家脱离远距时解除仇恨。
  static constexpr float kLeashRadius = 0.16f;
  // AI 活动区域半径（以 zone 巡逻中心为圆心）。
  static constexpr float kZoneRegionRadius = 0.16f;
  // 巡逻：周期轮换 zone 内巡逻点，慢速积分移动（确定性，无随机）。
  static constexpr Tick kPatrolSwitchMs = 6000;
  static constexpr float kPatrolSpeedPerMs = 0.0003f;
  // 战斗移动速度上限（与 EncounterController 一致）。
  static constexpr float kMovementPerMillisecond = 0.001f;
  // 碰撞半径（与 EncounterController::kEnemyCollisionRadius 一致）。
  static constexpr float kEnemyCollisionRadius = 0.012f;
  // 禁用 TrainingTarget 自动复活的超长重置周期（约 31700 年）：
  // 野外敌人重生由本系统按 zone.respawnMs 显式控制。
  static constexpr Tick kNoAutoRespawnMs = 1000000000000;

  // 默认使用 WorldLayout::kSpawnZones。
  WildSpawnSystem();
  // 注入自定义刷怪区（测试合成场景用）。
  explicit WildSpawnSystem(std::vector<WorldLayout::WorldSpawnZoneDef> zones);
  // Slot/Zone 为前向声明的不完整类型，析构需在 .cpp 实例化。
  ~WildSpawnSystem();

  // 替换刷怪区集合并清空已生成槽位/事件，回到新建构造的等价状态；
  // 空列表关闭全部野外刷怪（测试场景隔离用：出生点侦察敌会抢
  // 软锁定并提前打玩家，干扰纯时序断言）。
  void resetZones(std::vector<WorldLayout::WorldSpawnZoneDef> zones);

  void update(const WildSpawnFrameInput& input);

  // 性能降级档位转发（Phase 5）：取 PerformanceGuard::lodLevel()，
  // 非法值钳制到 [0, 2]；Loop 每步或档位变化时调用。
  void setPerformanceLevel(int32_t lodLevel);
  int32_t performanceLevel() const { return performanceLevel_; }
  // 当前档位下的活跃上限与感知半径（确定性，供单测断言）。
  int32_t maxActiveForPerf() const;
  float perceptionRadiusForPerf() const;

  const std::vector<WildEnemySnapshot>& snapshot() const { return snapshot_; }
  // 本步死亡事件（供宿主层消费后由下一次 update 清空）。
  const std::vector<WildDeathEvent>& deaths() const { return deaths_; }
  // 软锁定候选：宿主层并入 encounter 候选后交给 SoftTargeting。
  std::vector<TargetCandidate> candidates() const;
  // 玩家锁定目标的外部战斗绑定：id 非本系统存活敌人时返回空绑定。
  CombatTargetBinding combatBinding(EntityId id, Vec2 playerPosition);
  // 本步敌方对玩家的命中请求（宿主层转发到
  // CombatController::enqueueExternalEnemyHit）。
  const std::vector<HitRequest>& playerHits() const { return playerHits_; }
  // 活跃敌人数（存活且未被配额冻结）：供 Loop::wildEnemyCount 与 PROFILE。
  int32_t activeCount() const;
  int32_t registeredCount() const {
    return static_cast<int32_t>(slots_.size());
  }
  bool positionOf(EntityId id, Vec2& outPosition) const;
  // 测试辅助。
  bool isAggroed(EntityId id) const;
  bool isAlive(EntityId id) const;
  bool isFrozen(EntityId id) const;
  // 指定敌人当前 AI 决策周期（ms）：验证距离 LOD 降频；不存在时返回 0。
  Tick decisionPeriodOf(EntityId id) const;

 private:
  struct Slot;
  struct Zone;

  // zone 定义 → 内部 Zone 表（构造函数与 resetZones 共用）。
  void buildZones(const std::vector<WorldLayout::WorldSpawnZoneDef>& zones);

  static EnemyArchetype fromSpawnArchetype(WorldLayout::SpawnArchetype a);
  static int32_t chunkIdOf(Vec2 position);
  static CombatConfig targetConfig(EnemyArchetype archetype);
  static EnemyAiConfig aiConfig(EnemyArchetype archetype,
                                const CombatRegionConfig& region,
                                std::size_t zoneCapacity);

  void syncZones(const WildSpawnFrameInput& input);
  void activateZone(Zone& zone);
  void deactivateZone(Zone& zone);
  void respawnSlot(Slot& slot, const Zone& zone);
  void applyActiveQuota(const WildSpawnFrameInput& input);
  void linkAggroGroup(const Slot& source);
  Vec2 patrolPointFor(const Slot& slot, Tick tick) const;
  Slot* findSlot(EntityId id);
  const Slot* findSlot(EntityId id) const;

  std::vector<Zone> zones_;
  // 全部槽位，按 id 升序（zone 序 + slot 序天然升序）。
  std::vector<std::unique_ptr<Slot>> slots_;
  std::vector<WildEnemySnapshot> snapshot_;
  std::vector<WildDeathEvent> deaths_;
  std::vector<HitRequest> playerHits_;
  Tick lastTick_ = 0;
  uint64_t nextSequence_ = 1;
  // 性能降级档位（0/1/2）：驱动活跃配额与感知半径收缩。
  int32_t performanceLevel_ = 0;
};
