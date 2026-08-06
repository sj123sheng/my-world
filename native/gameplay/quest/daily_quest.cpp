#include "native/gameplay/quest/daily_quest.h"

#include <algorithm>

std::vector<DailyQuestDef> DailyQuestSystem::questsForDay(int32_t dayIndex) {
  // 四种类型各一条，LCG 费舍尔-耶茨洗牌决定当日顺序（确定性）。
  std::vector<DailyQuestKind> order = {
      DailyQuestKind::Kill, DailyQuestKind::Collect,
      DailyQuestKind::Anchor, DailyQuestKind::Chest,
  };
  uint32_t seed = static_cast<uint32_t>(dayIndex) * 2654435761u + 12345u;
  for (int32_t i = static_cast<int32_t>(order.size()) - 1; i > 0; --i) {
    seed = seed * 1664525u + 1013904223u;
    const int32_t j = static_cast<int32_t>(seed % static_cast<uint32_t>(i + 1));
    std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(j)]);
  }
  std::vector<DailyQuestDef> quests;
  for (DailyQuestKind kind : order) {
    quests.push_back({kind, requiredFor(kind)});
  }
  return quests;
}

int32_t DailyQuestSystem::requiredFor(DailyQuestKind kind) {
  switch (kind) {
    case DailyQuestKind::Kill:
      return 3;
    default:
      return 1;
  }
}

DailyQuestSystem::DailyQuestSystem(int32_t dayIndex)
    : dayIndex_(dayIndex), quests_(questsForDay(dayIndex)) {
  progress_.assign(quests_.size(), 0);
  completed_.assign(quests_.size(), false);
}

void DailyQuestSystem::notifyEvent(DailyQuestKind kind, int32_t count) {
  if (count <= 0) return;
  for (size_t i = 0; i < quests_.size(); ++i) {
    if (completed_[i] || quests_[i].kind != kind) continue;
    progress_[i] += count;
    if (progress_[i] >= quests_[i].required) {
      progress_[i] = quests_[i].required;
      completed_[i] = true;
    }
  }
}

int32_t DailyQuestSystem::progressOf(int32_t slot) const {
  if (slot < 0 || slot >= static_cast<int32_t>(progress_.size())) return 0;
  return progress_[static_cast<size_t>(slot)];
}

bool DailyQuestSystem::isCompleted(int32_t slot) const {
  if (slot < 0 || slot >= static_cast<int32_t>(completed_.size())) return false;
  return completed_[static_cast<size_t>(slot)];
}

int32_t DailyQuestSystem::completedCount() const {
  int32_t count = 0;
  for (bool done : completed_) {
    if (done) count += 1;
  }
  return count;
}

bool DailyQuestSystem::allCompleted() const {
  return completedCount() == static_cast<int32_t>(quests_.size());
}
