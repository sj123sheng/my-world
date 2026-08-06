#pragma once

#include <cstdint>
#include <vector>

// 每日委托系统（内容优化）：每个游戏日确定性生成四条委托，
// 全部完成后发放一次性奖励。委托组合由当日序号经 LCG 推导，
// 同一 dayIndex 永远得到同一组合（可测试、可回放）。
enum class DailyQuestKind : uint8_t {
  Kill = 0,      // 击败敌人
  Collect = 1,   // 采集
  Anchor = 2,    // 解锁锚点
  Chest = 3,     // 开启宝箱
};

struct DailyQuestDef {
  DailyQuestKind kind = DailyQuestKind::Kill;
  int32_t required = 1;
};

class DailyQuestSystem {
 public:
  static constexpr int32_t kQuestsPerDay = 4;

  // 按游戏日序号推导当日委托组合（四种类型各一条，顺序确定）。
  static std::vector<DailyQuestDef> questsForDay(int32_t dayIndex);
  // 各类型的需求数量（确定性配置）。
  static int32_t requiredFor(DailyQuestKind kind);

  explicit DailyQuestSystem(int32_t dayIndex = 0);

  void notifyEvent(DailyQuestKind kind, int32_t count = 1);

  int32_t progressOf(int32_t slot) const;
  bool isCompleted(int32_t slot) const;
  int32_t completedCount() const;
  bool allCompleted() const;
  int32_t dayIndex() const { return dayIndex_; }
  const std::vector<DailyQuestDef>& quests() const { return quests_; }

 private:
  int32_t dayIndex_ = 0;
  std::vector<DailyQuestDef> quests_;
  std::vector<int32_t> progress_;
  std::vector<bool> completed_;
};
