#pragma once
#include "tick_clock.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct GameSnapshot {
  Tick tick = 0;
  FixedPoint hp = fp(100);
  FixedPoint poise = fp(100);
  float playerX = 0.5f;
  float playerY = 0.5f;
  float fps = 0.0f;
  bool moving = false;
  int32_t targetId = 0;
  int32_t bossPhase = 0;
  int32_t encounterMode = 0;
  int32_t encounterState = 0;
  bool rendererReady = false;
  float moveX = 0.0f;
  float moveY = 0.0f;
  float cameraYaw = 0.0f;
  float cameraPitch = 0.0f;
  float targetDist = 0.0f;
  // 锁定目标焦点框：原型（-1 = 无普通敌人锁定）与血量比例。
  int32_t targetArchetype = -1;
  float targetHpRatio = 0.0f;
  uint8_t comboSegment = 0;
  FixedPoint targetHp = fp(300);
  FixedPoint targetPoise = fp(100);
  FixedPoint stamina = fp(100);
  FixedPoint resonance = 0;
  bool hasInsight = false;
  bool invulnerable = false;
  Tick insightMs = 0;
  Tick pulseHitRemainingMs = 0;
  int32_t lastRejectReason = 0;
  uint8_t currentAction = 0;
  Tick comboWindowMs = 0;
  Tick radianceCooldownMs = 0;
  Tick currentCooldownMs = 0;
  Tick corruptionCooldownMs = 0;
  Tick radianceCooldownTotalMs = 0;
  Tick currentCooldownTotalMs = 0;
  Tick corruptionCooldownTotalMs = 0;
  Tick ultimateWindowMs = 0;
  bool targetPoiseBroken = false;
  bool radianceAttached = false;
  bool currentAttached = false;
  bool corruptionAttached = false;
  bool corroded = false;
  int32_t currentReaction = -1;
  uint8_t pulsePhase = 0;
  int32_t levelStage = 0;
  int32_t gateState = 0;
  int32_t supplyState = 0;
  FixedPoint bossHp = fp(1000);
  FixedPoint bossPoise = fp(300);
  int32_t bossMechanic = 0;
  Tick bossCastMs = 0;
  int32_t perfLevel = 0;
  int32_t vfxFlags = 0;
  float cameraShakeX = 0.0f;
  float cameraShakeY = 0.0f;
  float bossHpRatio = 0.0f;
  float bossCastRatio = 0.0f;
  bool debugHud = false;
  bool environmentReady = false;
  uint32_t environmentDrawCalls = 0;
  uint32_t environmentTriangles = 0;
  std::string objectiveLabel;
  std::array<uint8_t, 3> resonanceSlots{0, 0, 0};
  bool showDebugHud = false;
  int32_t inputEventCount = 0;
  float bossCinematicProgress = 0.0f;
  uint8_t bossShardCount = 0;
  uint8_t bossSourceColor = 0;
  bool bossRingBroken = false;

  // ---- 开放世界探索字段（阶段一）----
  // 探索体力（0..max），与战斗体力独立。
  float explorationStamina = 100.0f;
  // 运动状态（MotionState 枚举值）：0地面 1空中 2滑翔 3攀爬 4游泳。
  int32_t motionState = 0;
  // 角色 3D 高度（地形贴合/跳跃/水面）。
  float playerHeight = 0.0f;
  // 当前激活分块数与累计流式加载次数，供调试 HUD 与验收。
  int32_t activeChunkCount = 0;
  int32_t chunkLoadCount = 0;
  // 交互目标（传送锚点等）：id >= 0 表示范围内可交互。
  int32_t interactionAnchorId = -1;
  bool interactionUnlocked = false;
  std::string interactionLabel;
  int32_t unlockedAnchorCount = 0;
  // 相机模式：true = 探索（拉远），false = 战斗。
  bool cameraExploration = false;
  // 小地图数据：锚点位置与解锁状态（与 anchors() 顺序一致）。
  std::vector<float> minimapAnchorX;
  std::vector<float> minimapAnchorY;
  std::vector<uint8_t> minimapAnchorUnlocked;
  // 传送成功脉冲：供 UI 播放传送反馈，按毫秒递减。
  Tick teleportFlashMs = 0;

  // ---- 内容与任务字段（阶段二）----
  int32_t questId = -1;
  int32_t questStatus = 0;
  std::string questTitle;
  std::string questObjectiveLabel;
  int32_t questObjectiveProgress = 0;
  int32_t questObjectiveRequired = 1;
  int32_t completedQuestCount = 0;
  // 对话会话状态。
  bool dialogActive = false;
  std::string dialogSpeaker;
  std::string dialogText;
  int32_t dialogLineIndex = 0;
  int32_t dialogLineCount = 0;
  // 交互提示种类：0=无 1=锚点 2=NPC 3=宝箱 4=采集物。
  int32_t interactionKind = 0;

  // ---- 养成与抽卡字段（阶段三）----
  int32_t fateCount = 0;
  int32_t goldCount = 0;
  int32_t expMaterialCount = 0;
  int32_t ascensionMaterialCount = 0;
  int32_t gachaPity5 = 0;
  // 最近一次抽卡结果（平行数组）。
  std::vector<int32_t> gachaResultIds;
  std::vector<int32_t> gachaResultRarities;
  std::vector<uint8_t> gachaResultIsNew;
  // 已拥有角色（平行数组，按角色 id 升序）。
  std::vector<int32_t> rosterIds;
  std::vector<int32_t> rosterLevels;
  std::vector<int32_t> rosterAscensions;

  // ---- 阶段四打磨字段 ----
  int32_t activeCharacterId = 1;
  // 游戏内小时数（0..24），驱动昼夜光照。
  float dayNightHour = 8.0f;
  int32_t qualityPreset = 0;
  // 天气：0=晴 1=多云 2=雨 3=雪；BGM 区域：0=森林 1=平原 2=高地。
  int32_t weatherId = 0;
  int32_t musicRegionId = 0;
  // 阶段二验收补齐：支线完成数与秘境进度。
  int32_t completedSideQuestCount = 0;
  int32_t dungeonState = 0;
  int32_t dungeonProgress = 0;
  int32_t dungeonRequired = 3;
  // 优化批次：小地图可交互物标记（3宝箱 4采集 5秘境，未消耗才发布）。
  std::vector<float> minimapItemX;
  std::vector<float> minimapItemY;
  std::vector<int32_t> minimapItemKind;
  // 支线任务进度/需求（与 SideQuestSystem::defaults 顺序对应）。
  std::vector<int32_t> sideQuestProgress;
  std::vector<int32_t> sideQuestRequired;
  // 角色派生属性（与 rosterIds 对齐）。
  std::vector<int32_t> rosterHp;
  std::vector<int32_t> rosterAtk;
  // 养成深化：命之座层数与武器清单。
  std::vector<int32_t> rosterConstellations;
  std::vector<int32_t> weaponIds;
  std::vector<int32_t> weaponLevels;
  std::vector<int32_t> weaponEquippedBy;
  // 原神式养成：冒险等级/世界等级。
  int32_t adventureRank = 1;
  int32_t adventureExp = 0;
  int32_t adventureExpRequired = 375;
  int32_t worldLevel = 0;
  // 新增物品计数：矿石三档与角色经验书三档。
  int32_t oreLowCount = 0;
  int32_t oreMidCount = 0;
  int32_t oreHighCount = 0;
  int32_t expSmallCount = 0;
  int32_t expMediumCount = 0;
  int32_t expLargeCount = 0;
  // 武器深化字段（与 weaponIds 对齐）。
  std::vector<int32_t> weaponAscensions;
  std::vector<int32_t> weaponRefines;
  std::vector<int32_t> weaponRefineStocks;
  std::vector<int32_t> weaponExps;
  // 圣遗物清单（平行数组，按实例 id 升序）。
  std::vector<int32_t> artifactInstanceIds;
  std::vector<int32_t> artifactDefIds;
  std::vector<int32_t> artifactRarities;
  std::vector<int32_t> artifactLevels;
  std::vector<int32_t> artifactEquippedBy;
  std::vector<int32_t> artifactSeeds;
  // 探索优化：自动疾跑激活标志（0/1）。
  int32_t sprintActive = 0;
  // 内容优化：每日委托完成数与奖励领取状态。
  int32_t dailyCompletedCount = 0;
  int32_t dailyQuestClaimed = 0;
  // 抽卡优化：当前结果所属卡池（0=角色 1=武器）。
  int32_t gachaPoolKind = 0;
  // NPC 任务发布（Phase 4）：对话结束时接取的支线；-1 表示无。
  // 快照尾部纯追加，UI 侧边沿检测触发接取提示。
  int32_t npcOfferQuestId = -1;
  std::string npcOfferQuestTitle;

  // 垂直切片探索进度与当前导航目标。
  int32_t explorationPoiCount = 0;
  int32_t explorationPuzzleCount = 0;
  int32_t explorationRewardCount = 0;
  int32_t explorationGateCount = 0;
  int32_t explorationTraversalMask = 0;
  int32_t explorationCurrentPoiId = -1;
  std::string explorationCurrentTargetLabel;
  std::string explorationCurrentTargetDistrict;
};
