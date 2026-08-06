#include "native/gameplay/quest/daily_quest.h"

#include <cassert>
#include <set>

int main() {
  // 当日组合：四种类型各一条，顺序由 dayIndex 确定性推导。
  const std::vector<DailyQuestDef> day0 = DailyQuestSystem::questsForDay(0);
  assert(day0.size() == DailyQuestSystem::kQuestsPerDay);
  std::set<int> kinds;
  for (const DailyQuestDef& def : day0) {
    kinds.insert(static_cast<int>(def.kind));
    assert(def.required ==
           DailyQuestSystem::requiredFor(def.kind));
  }
  assert(kinds.size() == 4);  // 四种类型各一条。

  // 确定性：同一 dayIndex 组合完全一致；不同 dayIndex 允许不同。
  const std::vector<DailyQuestDef> day0Again =
      DailyQuestSystem::questsForDay(0);
  for (size_t i = 0; i < day0.size(); ++i) {
    assert(day0[i].kind == day0Again[i].kind);
    assert(day0[i].required == day0Again[i].required);
  }

  // 进度推进：事件只推进匹配类型的委托，完成后钳制。
  DailyQuestSystem daily(0);
  assert(daily.completedCount() == 0);
  assert(!daily.allCompleted());
  daily.notifyEvent(DailyQuestKind::Kill, 2);
  daily.notifyEvent(DailyQuestKind::Collect, 1);
  daily.notifyEvent(DailyQuestKind::Anchor, 1);
  daily.notifyEvent(DailyQuestKind::Chest, 1);
  daily.notifyEvent(DailyQuestKind::Kill, 5);  // 击杀委托需 3，过量钳制。
  assert(daily.completedCount() == 4);
  assert(daily.allCompleted());
  for (int32_t slot = 0; slot < 4; ++slot) {
    assert(daily.isCompleted(slot));
    assert(daily.progressOf(slot) ==
           DailyQuestSystem::requiredFor(daily.quests()[static_cast<size_t>(slot)].kind));
  }

  // 非法输入安全。
  daily.notifyEvent(DailyQuestKind::Kill, -1);
  assert(daily.progressOf(-1) == 0);
  assert(daily.progressOf(99) == 0);
  assert(!daily.isCompleted(99));

  // 换日重置：新构造的系统进度归零。
  DailyQuestSystem nextDay(1);
  assert(nextDay.completedCount() == 0);
  assert(nextDay.dayIndex() == 1);

  return 0;
}
