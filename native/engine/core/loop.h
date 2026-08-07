#pragma once
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <mutex>
#include <unordered_map>
#include "fixed_step.h"
#include "../../engine/presentation/vfx_system.h"
#include "../../engine/presentation/performance_guard.h"
#include "../../engine/presentation/damage_numbers.h"
#include "../../platform/harmony/audio_bridge.h"
#include "lifecycle_state.h"
#include "snapshot_store.h"
#include "../render/surface.h"
#include "../render/camera.h"
#include "../input/input_queue.h"
#include "../input/touch_router.h"
#include "../input/virtual_joystick.h"
#include "../input/camera_gesture.h"
#include "../input/player_intent.h"
#include "../../gameplay/player/player_controller.h"
#include "../../gameplay/player/exploration_motion.h"
#include "../../gameplay/targeting/soft_targeting.h"
#include "../../gameplay/combat/combat_controller.h"
#include "../../gameplay/ai/encounter_controller.h"
#include "../../gameplay/ai/wild_spawn_system.h"
#include "../../gameplay/flow/demo_director.h"
#include "../../gameplay/world/teleport_anchor.h"
#include "../../gameplay/world/interactable.h"
#include "../../gameplay/world/npc_agent.h"
#include "../../gameplay/quest/quest_system.h"
#include "../../gameplay/quest/side_quests.h"
#include "../../gameplay/quest/daily_quest.h"
#include "../../gameplay/quest/dialog.h"
#include "../../gameplay/flow/dungeon.h"
#include "../../gameplay/flow/story_director.h"
#include "../../gameplay/inventory/inventory.h"
#include "../../gameplay/growth/character_growth.h"
#include "../../gameplay/growth/gacha_system.h"
#include "../../gameplay/growth/weapon_system.h"
#include "../../gameplay/growth/adventure_rank.h"
#include "../../gameplay/growth/artifact_system.h"
#include "../world/world_grid.h"
#include "../world/terrain_heightfield.h"
#include "../world/stream_scheduler.h"
#include "../world/environment_collision.h"
#include "../world/weather_system.h"

struct Loop {
  Loop() {
    const EnvironmentComposition composition =
        EnvironmentController::defaultComposition();
    surface.player.x = composition.spawn.x;
    surface.player.y = composition.spawn.z;
    motionState = explorationMotion.reset(
        terrain.heightAt(surface.player.x, surface.player.y));
    // 渲染层只读消费同一高度场：地形网格生成与角色/阴影贴地采样。
    surface.terrain = &terrain;
    // 分块地形流式：渲染线程每帧从调度器取就绪分块上传/绘制。
    surface.streamScheduler = &streamScheduler;
    // 建筑碰撞：与渲染批次共用世界适配参数，城墙/塔楼不再穿模。
    buildingCollision = BuildingCollision::fromEnvironmentLayout(
        composition.altarAnchor.x, composition.altarAnchor.z);
    // 开局赠送主角（辉印·莉拉）。
    characters.addCharacter(1);
    (void)encounter.start(EncounterMode::Training);
  }

  Surface surface;
  InputQueue input;
  TouchRouter touchRouter;
  VirtualJoystick joystick{VirtualJoystickConfig{}};
  CameraGesture cameraGesture{CameraGestureConfig{}};
  PlayerIntent intent;
  PlayerController playerController;
  // 开放世界探索基础（阶段一）：分块流式、地形、垂直运动与锚点。
  WorldGrid worldGrid;
  TerrainHeightfield terrain;
  // 分块地形流式调度器：消费 worldGrid 的加/卸载请求，后台生成
  // 分块网格；渲染线程经 surface.streamScheduler 每帧取用。
  StreamScheduler streamScheduler{terrain, worldGrid};
  // 建筑碰撞集（城墙/塔楼）：滑动阻挡 + 墙面攀爬 + 墙头支撑。
  BuildingCollision buildingCollision;
  // 主角碰撞半径（世界单位）：用于建筑 OBB 膨胀与支撑查询。
  static constexpr float playerCollisionRadius = 0.012f;
  ExplorationMotion explorationMotion;
  ExplorationMotionState motionState;
  TeleportAnchorSystem anchors = TeleportAnchorSystem::openWorldLayout();
  AnchorInteraction currentAnchorInteraction;
  // 内容系统（阶段二）：任务、可交互物与对话。
  QuestSystem quests = QuestSystem::mainline();
  // 开放世界支线（Phase 4）：独立于主线，支持对话发布与并行接取。
  QuestSystem openWorldQuests = QuestSystem::openWorldQuests();
  SideQuestSystem sideQuests = SideQuestSystem::defaults();
  // 每日委托（内容优化）：每个游戏日重置，全部完成发放一次性奖励。
  DailyQuestSystem dailyQuests;
  int32_t gameDayCount = 0;
  bool dailyRewarded = false;
  // 当前抽卡结果所属卡池：0=角色池 1=武器池。
  int32_t gachaPoolKind = 0;
  DungeonSession dungeon;
  StoryDirector storyDirector = StoryDirector::opening();
  InteractableRegistry interactables = InteractableRegistry::openWorldLayout();
  InteractableTarget currentInteractable;
  DialogSession dialogSession;
  // NPC 轻量状态机（Phase 4）：巡逻/驻守/对话朝向，只输出位置与朝向。
  NpcAgency npcAgency = NpcAgency::fromWorldLayout();
  // 对话结束时发布的支线（供快照/UI 接取提示）；新对话开始时清除。
  int32_t npcOfferQuestId = -1;
  std::string npcOfferQuestTitle;
  // 养成与抽卡（阶段三）。
  Inventory inventory = Inventory::defaultInventory();
  CharacterGrowth characters;
  WeaponSystem weapons;
  GachaSystem gacha;
  GachaState gachaState;
  std::vector<GachaPull> lastGachaResults;
  std::vector<bool> lastGachaIsNew;
  // 原神式养成：冒险等级/世界等级、圣遗物与掉落种子。
  AdventureRank adventureRank;
  ArtifactSystem artifacts;
  uint32_t dropSeed = 20260805u;
  // 已领取的冒险等阶奖励（一次性发放）。
  std::vector<int32_t> claimedRankRewards;
  // 已发放奖励对应的完成任务数，避免重复发奖。
  int32_t lastRewardedQuestCount = 0;
  int32_t lastRewardedSideCount = 0;
  // 出战角色（阶段四）：循环切换已拥有角色。
  int32_t activeCharacterId = 1;
  // 昼夜时钟（秒），240 秒为一个游戏日。
  float timeOfDaySeconds = 8.0f * 10.0f;
  // 逻辑累计时钟（毫秒）：供演出导演等需稳定时钟的子系统使用。
  Tick loopTimeMs = 0;
  // 画质预设：0=自动（跟随 PerformanceGuard），1=低（强制小流式半径）。
  int32_t qualityPreset = 0;
  // BGM 区域（阶段四）：0=森林 1=平原 2=高地，按玩家位置切换。
  int32_t musicRegionId = 0;
  // 采集物重生倒计时（毫秒）；归零后采集物恢复可交互。
  static constexpr Tick kCollectRespawnMs = 120000;
  Tick collectRespawnRemainingMs = 0;
  bool jumpQueued = false;
  bool glideHeld = false;
  bool interactQueued = false;
  int32_t chunkLoadCount = 0;
  // 野外敌人数量（性能仪表预留）：当前无野外敌人系统恒为 0，
  // 由后续 WildSpawnSystem 写入，PROFILE 打点只读消费。
  int32_t wildEnemyCount = 0;
  Tick teleportFlashMs = 0;
  ThirdPersonCamera camera;
  SoftTargeting softTargeting;
  CombatController combat{CombatConfig::defaults()};
  EncounterController encounter{combat};
  // 野外刷怪系统（Phase 3.2）：分块激活区刷怪/重生/巡逻/仇恨，
  // 战斗走 combat 外部通道，不进 EncounterController 状态机。
  WildSpawnSystem wildSpawn;
  DemoDirector demoDirector;
  std::optional<TargetSelection> currentTarget;
  VfxSystem vfxSystem;
  DamageNumberSystem damageNumbers;
  // 敌人头顶血条滞后条状态：受击后短暂停留再匀速追赶实际血量，
  // 让单次扣血量清晰可读（key = EntityId）。
  struct EnemyHpTrailState {
    float trail = 1.0f;
    Tick holdMs = 0;
    bool chasing = false;
  };
  std::unordered_map<EntityId, EnemyHpTrailState> enemyHpTrails;
  PerformanceGuard performanceGuard;
  AudioBridge audioBridge;
  bool debugHud_ = false;
  FixedStep fixedStep{16, 4};
  SnapshotStore snapshots;
  LifecycleState lifecycle;
  std::mutex inputEnqueueMutex;
  mutable std::mutex combatEventMutex;
  std::atomic<Tick> combatTimeMs_{0};
  CombatEventBatch frameCombatEvents_;
  uint64_t inputSequence = 0;
  int32_t inputEventCount_ = 0;
  std::atomic<bool> running{false};
  std::atomic<bool> shouldStop{false};
  std::atomic<bool> paused{false};
  std::thread runner;
  float fps = 0.0f;
  float particleEmitTimer = 0.0f;
  // 3D 移动尾迹发射计时与连击升阶检测用上一帧值。
  float trailEmitTimer = 0.0f;
  int prevComboSegment = 0;
  // 上一帧源技能冷却：用于上升沿检测释放技能特效。
  Tick prevRadianceCdMs = 0;
  Tick prevCurrentCdMs = 0;
  Tick prevCorruptionCdMs = 0;
  // 上一帧连击段数（特效专用）：变化即视为一次普攻挥击，
  // 用于向目标发射释放过程投射物。
  int prevComboSegmentForVfx = 0;
  // 上一帧动作状态（特效专用）：检测终结技吟唱上升沿释放大招动效。
  uint8_t prevActionForVfx = 0;
  // 命中卡肉（hitstop）剩余毫秒：>0 时冻结固定步逻辑、渲染继续，
  // 制造命中瞬间的顿帧打击感。仅在玩家命中敌人时触发。
  int64_t hitStopRemainingMs = 0;
  // 上一帧遭遇状态：用于结算音效的状态转移检测。
  int lastEncounterStateForAudio = 0;
  // 上一帧首领机制：用于吟唱警示音的上升沿检测。
  int lastBossMechanicForAudio = 0;
  int tickCount = 0;
  std::chrono::steady_clock::time_point lastFpsTime;

  void start();
  void stop();
  // 暂停/恢复帧循环逻辑：暂停期间冻结输入与固定步更新，
  // 画面保持最后一帧，供暂停菜单与结算界面使用。
  void setPaused(bool value);
  bool isPaused() const { return paused.load(); }
  void tickOnce(int64_t elapsedMs);
  void updateFixed(Tick tick, int64_t dtMs);
  void processInput();
  void resetInput();
  void publishRendererStopped();
  bool startEncounter(EncounterMode mode);
  bool advanceLevel();
  bool useSupply();
  bool retryBoss();
  void toggleDebugHud();
  // 对话推进：由 UI 按钮触发，推进当前台词或结束会话。
  void advanceDialog();
  // 进度存档/恢复：路径由 ArkTS 层传入应用沙箱。
  bool saveProgress(const std::string& path);
  bool loadProgress(const std::string& path);
  // 抽卡：消耗共鸣之契，结算并入队新角色；道具不足返回 false。
  bool performGacha(int32_t count);
  // 武器卡池抽卡：消耗契约，结算并入队武器（重复折算金币）。
  bool performWeaponGacha(int32_t count);
  // 角色养成：消耗经验材料升级；消耗突破材料与金币突破。
  bool useExpMaterial(int32_t characterId, int32_t materialCount);
  bool ascendCharacter(int32_t characterId);
  // 武器养成：消耗金币强化；装备到角色（自动换装）。
  bool upgradeWeapon(int32_t weaponId);
  bool equipWeapon(int32_t weaponId, int32_t characterId);
  // 武器深化：矿石强化/突破/精炼。
  bool upgradeWeaponWithOre(int32_t weaponId, int32_t oreItemId,
                            int32_t oreCount);
  bool ascendWeapon(int32_t weaponId);
  bool refineWeapon(int32_t weaponId);
  // 角色经验书（多档）使用。
  bool useExpItem(int32_t characterId, int32_t itemId, int32_t count);
  // 圣遗物：喂食强化/装备。
  bool upgradeArtifact(int32_t targetInstanceId,
                       const std::vector<int32_t>& feedInstanceIds);
  bool equipArtifact(int32_t instanceId, int32_t characterId);
  // 冒险等阶奖励领取（一次性）。
  bool claimRankReward(int32_t rank);
  // 出战角色循环切换；不足两名角色时返回 false。
  bool switchCharacter();
  // 地图快速传送：仅已解锁锚点可用（原神式地图选点传送）。
  bool teleportToAnchor(int32_t anchorId);
  // 画质预设：0=自动，1=低。
  void setQualityPreset(int32_t preset) { qualityPreset = preset == 1 ? 1 : 0; }
  int32_t qualityPresetValue() const { return qualityPreset; }
  // 音效总开关：由设置界面下发，关闭后静音并停环境垫底。
  void setAudioEnabled(bool enabled) { audioBridge.setEnabled(enabled); }
  void skipDemoPhase(DemoPhase phase) { demoDirector.skipTo(phase); }

  template <typename Fn>
  decltype(auto) withLifecycle(Fn&& operation) {
    return lifecycle.synchronized(std::forward<Fn>(operation));
  }

  bool enqueueInput(InputAction action, int32_t pointerId, float x, float y) {
    std::lock_guard<std::mutex> lock(inputEnqueueMutex);
    const bool accepted =
        input.push({action, pointerId, x, y, inputSequence});
    if (accepted) ++inputSequence;
    if (accepted) ++inputEventCount_;
    return accepted;
  }

  GameSnapshot snapshot() const { return snapshots.read(); }
  Tick combatTimeMs() const { return combatTimeMs_.load(); }
  CombatEventBatch combatEvents() const {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    return frameCombatEvents_;
  }
};
