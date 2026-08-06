#include "native/gameplay/quest/quest_system.h"

#include <cassert>

int main() {
  // 主线初始：第一任务已激活，其余锁定。
  QuestSystem quests = QuestSystem::mainline();
  assert(quests.activeQuestId() == 1);
  assert(quests.statusOf(1) == QuestStatus::Active);
  assert(quests.statusOf(2) == QuestStatus::Locked);
  assert(quests.completedCount() == 0);

  // 快照：当前目标为与引路灵对话。
  QuestProgressSnapshot snap = quests.snapshot();
  assert(snap.questId == 1);
  assert(snap.status == QuestStatus::Active);
  assert(!snap.title.empty());
  assert(!snap.objectiveLabel.empty());
  assert(snap.objectiveProgress == 0);
  assert(snap.objectiveRequired == 1);

  // 无关事件不推进：先杀敌/开箱对 Q1 无效。
  quests.notifyEnemiesKilled(3);
  quests.notifyChestOpened(2);
  assert(quests.activeQuestId() == 1);
  assert(quests.snapshot().objectiveProgress == 0);

  // 对话推进 Q1 → 自动接取 Q2。
  quests.notifyNpcTalked(1);
  assert(quests.isCompleted(1));
  assert(quests.completedCount() == 1);
  assert(quests.activeQuestId() == 2);
  assert(quests.statusOf(2) == QuestStatus::Active);

  // 错误锚点不推进 Q2。
  quests.notifyAnchorReached(3);
  assert(quests.activeQuestId() == 2);
  // 正确锚点推进 Q2 → Q3（击杀 3 敌）。
  quests.notifyAnchorReached(2);
  assert(quests.activeQuestId() == 3);

  // 击杀计数：分两次累计。
  quests.notifyEnemiesKilled(2);
  assert(quests.snapshot().objectiveProgress == 2);
  assert(quests.snapshot().objectiveRequired == 3);
  quests.notifyEnemiesKilled(1);
  assert(quests.activeQuestId() == 4);

  // Q4 顺序目标：先开箱后采集；先采集不推进。
  quests.notifyCollect(3);
  assert(quests.snapshot().objectiveIndex == 0);
  quests.notifyChestOpened(2);
  assert(quests.snapshot().objectiveIndex == 1);
  quests.notifyCollect(3);
  assert(quests.activeQuestId() == 5);

  // Q5 终点：抵达北部高地后全部完成且无激活任务。
  quests.notifyAnchorReached(5);
  assert(quests.completedCount() == 5);
  assert(quests.activeQuestId() == -1);
  assert(quests.statusOf(5) == QuestStatus::Completed);
  const QuestProgressSnapshot done = quests.snapshot();
  assert(done.status == QuestStatus::Completed);
  assert(done.questId == -1);

  // 自定义任务链：Available 才可接取；激活中不可重复接取。
  std::vector<QuestDef> custom;
  custom.push_back({10, "支线甲",
                    {{ObjectiveKind::Collect, 9, 2, "采集两株草"}}, -1});
  QuestSystem side(std::move(custom));
  assert(side.statusOf(10) == QuestStatus::Available);
  assert(side.accept(10));
  assert(!side.accept(10));
  side.notifyCollect(9);
  assert(side.snapshot().objectiveProgress == 1);
  side.notifyCollect(9);
  assert(side.completedCount() == 1);
  assert(side.activeQuestId() == -1);

  // 未知任务接取失败；未知事件无副作用。
  QuestSystem empty{std::vector<QuestDef>{}};
  assert(!empty.accept(99));
  empty.notifyEnemiesKilled(5);
  assert(empty.completedCount() == 0);
  assert(empty.snapshot().status == QuestStatus::Locked);
  return 0;
}
