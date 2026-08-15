#pragma once
#include <cstdint>
#include <string>
#include <vector>
struct SaveState {
  int campLevel = 0;
  int relics = 0;
  int regionProgress = 0;
  // 阶段二进度字段：任务与探索进度。
  int32_t completedQuestCount = 0;
  int32_t activeQuestId = -1;
  // 锚点解锁位掩码：bit(i-1) 对应锚点 id i（id 1..32）。
  int32_t unlockedAnchorMask = 0;
  // 已消耗交互物位掩码：bit(id-1)。
  int32_t consumedInteractableMask = 0;
  // 阶段三进度字段：背包、抽卡状态与角色图鉴。
  int32_t fateCount = 0;
  int32_t goldCount = 0;
  int32_t expCount = 0;
  int32_t ascensionCount = 0;
  int32_t gachaPity5 = 0;
  int32_t gachaPity4 = 0;
  uint32_t gachaSeed = 0;
  // 角色三元组序列：[id, level, ascension] 循环。
  std::vector<int32_t> rosterTriples;
  // 阶段四补齐：支线任务完成位掩码 bit(id-1)。
  int32_t sideQuestMask = 0;
  // 优化批次：采集物重生倒计时剩余毫秒。
  int64_t collectRespawnMs = 0;
  // 养成深化：武器三元组序列 [weaponId, level, equippedBy] 循环。
  std::vector<int32_t> weaponTriples;
  // 原神式养成（V7）：冒险等级与当前经验。
  int32_t adventureRank = 1;
  int32_t adventureExp = 0;
  // 掉落种子：驱动圣遗物掉落与副属性确定性生成。
  uint32_t dropSeed = 0;
  // 新增物品计数（V7）：矿石三档与经验书三档。
  int32_t oreLowCount = 0;
  int32_t oreMidCount = 0;
  int32_t oreHighCount = 0;
  int32_t expSmallCount = 0;
  int32_t expMediumCount = 0;
  int32_t expLargeCount = 0;
  // 武器七元组序列 [id, level, ascension, refine, refineStock, exp,
  // equippedBy] 循环。
  std::vector<int32_t> weaponRecords;
  // 圣遗物六元组序列 [instanceId, defId, rarity, level, equippedBy,
  // substatSeed] 循环。
  std::vector<int32_t> artifactRecords;
  // 已领取等阶奖励的等阶列表。
  std::vector<int32_t> claimedRanks;
  // 开放世界支线进度（V8）：完成位掩码 bit(i) 对应 openWorldQuests
  // 声明顺序第 i 个任务（并行支线，掩码比计数更精确）。
  int32_t openWorldQuestMask = 0;
  // 开放世界支线当前接取任务 id（-1 = 无）。
  int32_t openWorldQuestActiveId = -1;
  // 垂直切片探索状态（V9）：POI/机关/奖励/路径门与移动能力位掩码。
  int32_t explorationPoiMask = 0;
  int32_t explorationPuzzleMask = 0;
  int32_t explorationRewardMask = 0;
  int32_t explorationGateMask = 0;
  int32_t explorationTraversalMask = 0;
  // 无限世界位置（V10）：区块坐标承载超远距离，局部坐标限制在 [0, 1)。
  uint64_t worldSeed = 1;
  int64_t playerChunkX = 0;
  int64_t playerChunkY = 0;
  float playerLocalX = 0.5f;
  float playerLocalY = 0.12f;
};
struct Save {
  bool write(const SaveState& s, const char* path);
  // 兼容 v1-v10 存档；旧版本世界位置迁移到核心区出生点。
  bool read(SaveState& out, const char* path);
};
