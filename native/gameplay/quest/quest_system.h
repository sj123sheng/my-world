#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 数据驱动任务系统（阶段二）：
// 任务由目标序列构成，目标按顺序推进；当前目标完成后才接收后续事件。
// 事件源包括：到达锚点、击杀敌人、采集、开启宝箱、NPC 对话。
// 全部状态确定性可测试，供存档与 HUD 消费。
enum class ObjectiveKind : uint8_t {
  ReachAnchor = 0,   // 到达/解锁指定锚点
  KillEnemies = 1,   // 累计击败敌人数量
  Collect = 2,       // 采集指定采集物
  OpenChest = 3,     // 开启指定宝箱
  TalkToNpc = 4,     // 与指定 NPC 对话
};

enum class QuestStatus : uint8_t {
  Locked = 0,
  Available = 1,
  Active = 2,
  Completed = 3,
};

struct QuestObjectiveDef {
  ObjectiveKind kind = ObjectiveKind::ReachAnchor;
  // 目标实体 id；KillEnemies 忽略 id 只计数量。
  int32_t targetId = 0;
  int32_t requiredCount = 1;
  std::string description;
};

struct QuestDef {
  int32_t id = 0;
  std::string title;
  std::vector<QuestObjectiveDef> objectives;
  // 完成后自动解锁的下一任务 id；-1 表示链条终点。
  int32_t nextQuestId = -1;
};

struct QuestProgressSnapshot {
  int32_t questId = -1;
  QuestStatus status = QuestStatus::Locked;
  std::string title;
  // 当前目标描述与进度。
  std::string objectiveLabel;
  int32_t objectiveProgress = 0;
  int32_t objectiveRequired = 1;
  int32_t objectiveIndex = 0;
  int32_t objectiveCount = 0;
};

class QuestSystem {
 public:
  // 首条主线（教学→探索→战斗→收集→远行），与默认世界布局对应。
  static QuestSystem mainline();

  explicit QuestSystem(std::vector<QuestDef> quests);

  // 接受 Available 状态的任务；返回是否成功。
  bool accept(int32_t questId);

  // ---- 事件入口（仅推进当前激活任务的当前目标）----
  void notifyAnchorReached(int32_t anchorId);
  void notifyEnemiesKilled(int32_t count);
  void notifyCollect(int32_t collectibleId);
  void notifyChestOpened(int32_t chestId);
  void notifyNpcTalked(int32_t npcId);

  QuestStatus statusOf(int32_t questId) const;
  bool isCompleted(int32_t questId) const;
  int32_t activeQuestId() const;
  int32_t completedCount() const;
  QuestProgressSnapshot snapshot() const;
  const std::vector<QuestDef>& quests() const { return quests_; }

  // 存档恢复（主线线性链）：把前 completedCount 个任务标记完成，
  // 并接取 activeQuestId 指定的任务；非法输入保持现状。
  void restoreLinear(int32_t completedCount, int32_t activeQuestId);

 private:
  void applyEvent(ObjectiveKind kind, int32_t targetId, int32_t count);
  void completeCurrentObjective();

  std::vector<QuestDef> quests_;
  std::vector<QuestStatus> statuses_;
  // 激活任务的目标索引与各目标累计进度。
  int32_t activeIndex_ = -1;
  int32_t objectiveIndex_ = 0;
  int32_t objectiveProgress_ = 0;
  int32_t completedCount_ = 0;
};
