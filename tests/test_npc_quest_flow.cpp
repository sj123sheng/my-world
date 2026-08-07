#include "native/gameplay/quest/dialog.h"
#include "native/gameplay/quest/quest_system.h"

#include <cassert>

namespace {

// 重放“接取 → TalkToNpc → KillEnemies → TalkToNpc”全链，
// 供双实例确定性对照。
void replayQuestFlow(QuestSystem& quests) {
  assert(quests.accept(201));
  // 发布对话本身计入首个 TalkToNpc 目标（Loop 侧 accept 后补发）。
  quests.notifyNpcTalked(33);
  quests.notifyEnemiesKilled(2);
  quests.notifyEnemiesKilled(1);
  quests.notifyNpcTalked(33);
}

}  // namespace

int main() {
  // 工厂定义：3 条支线，id 段 201-203 避开主线（1-5）；
  // 目标链均为 TalkToNpc → KillEnemies → TalkToNpc。
  QuestSystem quests = QuestSystem::openWorldQuests();
  assert(quests.quests().size() == 3);
  for (const QuestDef& def : quests.quests()) {
    assert(def.id >= 201 && def.id <= 203);
    assert(!def.title.empty());
    assert(def.objectives.size() == 3);
    assert(def.objectives[0].kind == ObjectiveKind::TalkToNpc);
    assert(def.objectives[1].kind == ObjectiveKind::KillEnemies);
    assert(def.objectives[2].kind == ObjectiveKind::TalkToNpc);
    assert(def.nextQuestId == -1);
  }
  // 独立发布、无链条前置：全部 Available，无激活任务。
  assert(quests.statusOf(201) == QuestStatus::Available);
  assert(quests.statusOf(202) == QuestStatus::Available);
  assert(quests.statusOf(203) == QuestStatus::Available);
  assert(quests.activeQuestId() == -1);

  // 对话 101 发布支线 201：advance 前捕获 offeredQuestId
  //（会话结束后 def_ 置空，访问器回落到 -1）。
  const DialogLibrary& library = DialogLibrary::defaults();
  const DialogDef* def101 = library.find(101);
  assert(def101 != nullptr);
  assert(def101->offeredQuestId == 201);
  DialogSession session;
  session.start(def101);
  assert(session.active());
  assert(session.offeredQuestId() == 201);
  const int32_t offered = session.offeredQuestId();
  while (session.advance()) {
  }
  assert(!session.active());
  assert(session.offeredQuestId() == -1);

  // 接取 + 补发对话事件：首个 TalkToNpc 目标即刻完成，进入击杀目标。
  assert(quests.accept(offered));
  // 激活中不可再接取另一条支线（单激活约束）。
  assert(!quests.accept(202));
  quests.notifyNpcTalked(33);
  QuestProgressSnapshot snap = quests.snapshot();
  assert(snap.questId == 201);
  assert(snap.status == QuestStatus::Active);
  assert(snap.objectiveIndex == 1);
  assert(snap.objectiveProgress == 0);
  assert(snap.objectiveRequired == 3);

  // KillEnemies 计数分次累计，与野外击杀通道（notifyEnemiesKilled）兼容。
  quests.notifyEnemiesKilled(2);
  assert(quests.snapshot().objectiveProgress == 2);
  quests.notifyEnemiesKilled(1);
  snap = quests.snapshot();
  assert(snap.objectiveIndex == 2);
  // 复命只认发布 NPC：其他 NPC 对话不推进。
  quests.notifyNpcTalked(35);
  assert(quests.activeQuestId() == 201);
  quests.notifyNpcTalked(33);
  assert(quests.isCompleted(201));
  assert(quests.completedCount() == 1);
  assert(quests.activeQuestId() == -1);

  // 确定性：双实例重放同一事件序列，终态完全一致。
  QuestSystem lhs = QuestSystem::openWorldQuests();
  QuestSystem rhs = QuestSystem::openWorldQuests();
  replayQuestFlow(lhs);
  replayQuestFlow(rhs);
  assert(lhs.completedCount() == rhs.completedCount());
  assert(lhs.isCompleted(201) && rhs.isCompleted(201));
  const QuestProgressSnapshot lhsDone = lhs.snapshot();
  const QuestProgressSnapshot rhsDone = rhs.snapshot();
  assert(lhsDone.questId == rhsDone.questId);
  assert(lhsDone.status == rhsDone.status);
  assert(lhsDone.status == QuestStatus::Completed);

  // 完成后可继续接取其他支线并走完全链。
  assert(lhs.accept(202));
  lhs.notifyNpcTalked(35);
  lhs.notifyEnemiesKilled(2);
  lhs.notifyNpcTalked(35);
  assert(lhs.isCompleted(202));
  assert(lhs.completedCount() == 2);
  return 0;
}
