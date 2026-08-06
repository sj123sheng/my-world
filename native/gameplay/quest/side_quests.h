#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 支线任务系统（阶段二验收补齐）：与主线并行的独立计数型任务，
// 无顺序依赖、可同时推进；全部状态确定性可测试，完成掩码可入存档。
enum class SideQuestEvent : uint8_t {
  Kill = 0,          // 击败敌人
  Collect = 1,       // 采集任意采集物
  ReachAnchor = 2,   // 到达/解锁任意锚点
};

struct SideQuestDef {
  int32_t id = 0;
  std::string title;
  SideQuestEvent event = SideQuestEvent::Kill;
  int32_t required = 1;
};

class SideQuestSystem {
 public:
  // 默认三条支线：雾谷肃清（击杀）、脉流采集（采集）、远行之路（锚点）。
  static SideQuestSystem defaults();

  explicit SideQuestSystem(std::vector<SideQuestDef> quests);

  void notifyEvent(SideQuestEvent event, int32_t count = 1);

  bool isCompleted(int32_t questId) const;
  int32_t progressOf(int32_t questId) const;
  int32_t completedCount() const;
  // 完成位掩码：bit(i-1) 对应任务 id i，供存档。
  int32_t completedMask() const;
  void restoreMask(int32_t mask);
  const std::vector<SideQuestDef>& quests() const { return quests_; }

 private:
  std::vector<SideQuestDef> quests_;
  std::vector<int32_t> progress_;
  std::vector<bool> completed_;
};
