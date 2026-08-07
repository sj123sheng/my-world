#include "native/gameplay/quest/quest_system.h"

#include <algorithm>

namespace {

int32_t findQuestIndex(const std::vector<QuestDef>& quests, int32_t id) {
  for (size_t i = 0; i < quests.size(); ++i) {
    if (quests[i].id == id) return static_cast<int32_t>(i);
  }
  return -1;
}

}  // namespace

QuestSystem QuestSystem::mainline() {
  std::vector<QuestDef> quests;
  quests.push_back({1, "巡脉者的苏醒",
                    {{ObjectiveKind::TalkToNpc, 1, 1, "与引路灵对话"}},
                    2});
  quests.push_back({2, "前往共鸣祭坛",
                    {{ObjectiveKind::ReachAnchor, 2, 1, "抵达并解锁共鸣祭坛"}},
                    3});
  quests.push_back({3, "遗迹守卫",
                    {{ObjectiveKind::KillEnemies, 0, 3, "击败遗迹中的守卫"}},
                    4});
  quests.push_back({4, "遗迹的馈赠",
                    {{ObjectiveKind::OpenChest, 2, 1, "开启遗迹宝箱"},
                     {ObjectiveKind::Collect, 3, 1, "采集脉流花"}},
                    5});
  quests.push_back({5, "远方的回声",
                    {{ObjectiveKind::ReachAnchor, 5, 1, "登上北部高地"}},
                    -1});
  QuestSystem system(std::move(quests));
  // 开局自动接取第一任务。
  system.accept(1);
  return system;
}

QuestSystem QuestSystem::openWorldQuests() {
  // NPC 对话发布的支线（Phase 4）：目标链 TalkToNpc → KillEnemies → TalkToNpc。
  // 发布对话结束时接取并补发一次 TalkToNpc 事件，首个对话目标即告完成；
  // KillEnemies 不限定目标 id，野外击杀（notifyEnemiesKilled）同样计入。
  std::vector<QuestDef> quests;
  quests.push_back({201, "裂隙爪狼之患",
                    {{ObjectiveKind::TalkToNpc, 33, 1, "与低地巡林员交谈"},
                     {ObjectiveKind::KillEnemies, 0, 3, "击败 3 头野外敌人"},
                     {ObjectiveKind::TalkToNpc, 33, 1, "向低地巡林员复命"}},
                    -1});
  quests.push_back({202, "回廊急件",
                    {{ObjectiveKind::TalkToNpc, 35, 1, "与回廊信使交谈"},
                     {ObjectiveKind::KillEnemies, 0, 2, "击败 2 名野外敌人"},
                     {ObjectiveKind::TalkToNpc, 35, 1, "向回廊信使复命"}},
                    -1});
  quests.push_back({203, "圣所试炼",
                    {{ObjectiveKind::TalkToNpc, 37, 1, "与圣所守望者交谈"},
                     {ObjectiveKind::KillEnemies, 0, 4, "击败 4 头野外敌人"},
                     {ObjectiveKind::TalkToNpc, 37, 1, "向圣所守望者复命"}},
                    -1});
  return QuestSystem(std::move(quests));
}

QuestSystem::QuestSystem(std::vector<QuestDef> quests)
    : quests_(std::move(quests)) {
  statuses_.assign(quests_.size(), QuestStatus::Locked);
  // 链条起点：没有前置任务指向的任务标记为 Available。
  for (size_t i = 0; i < quests_.size(); ++i) {
    bool hasPredecessor = false;
    for (const QuestDef& quest : quests_) {
      if (quest.nextQuestId == quests_[i].id) {
        hasPredecessor = true;
        break;
      }
    }
    if (!hasPredecessor) statuses_[i] = QuestStatus::Available;
  }
}

bool QuestSystem::accept(int32_t questId) {
  const int32_t index = findQuestIndex(quests_, questId);
  if (index < 0 || statuses_[index] != QuestStatus::Available ||
      activeIndex_ >= 0) {
    return false;
  }
  statuses_[index] = QuestStatus::Active;
  activeIndex_ = index;
  objectiveIndex_ = 0;
  objectiveProgress_ = 0;
  return true;
}

void QuestSystem::notifyAnchorReached(int32_t anchorId) {
  applyEvent(ObjectiveKind::ReachAnchor, anchorId, 1);
}

void QuestSystem::notifyEnemiesKilled(int32_t count) {
  applyEvent(ObjectiveKind::KillEnemies, 0, count);
}

void QuestSystem::notifyCollect(int32_t collectibleId) {
  applyEvent(ObjectiveKind::Collect, collectibleId, 1);
}

void QuestSystem::notifyChestOpened(int32_t chestId) {
  applyEvent(ObjectiveKind::OpenChest, chestId, 1);
}

void QuestSystem::notifyNpcTalked(int32_t npcId) {
  applyEvent(ObjectiveKind::TalkToNpc, npcId, 1);
}

void QuestSystem::applyEvent(ObjectiveKind kind, int32_t targetId,
                             int32_t count) {
  if (activeIndex_ < 0 || count <= 0) return;
  const QuestDef& quest = quests_[activeIndex_];
  if (objectiveIndex_ >= static_cast<int32_t>(quest.objectives.size())) {
    return;
  }
  const QuestObjectiveDef& objective = quest.objectives[objectiveIndex_];
  if (objective.kind != kind) return;
  // KillEnemies 不限定目标 id；其余目标必须匹配实体 id。
  if (kind != ObjectiveKind::KillEnemies && objective.targetId != targetId) {
    return;
  }
  objectiveProgress_ =
      std::min(objectiveProgress_ + count, objective.requiredCount);
  if (objectiveProgress_ >= objective.requiredCount) {
    completeCurrentObjective();
  }
}

void QuestSystem::completeCurrentObjective() {
  if (activeIndex_ < 0) return;
  const QuestDef& quest = quests_[activeIndex_];
  objectiveIndex_ += 1;
  objectiveProgress_ = 0;
  if (objectiveIndex_ < static_cast<int32_t>(quest.objectives.size())) {
    return;
  }
  // 任务完成：结算并把下一任务解锁后直接接取（主线单链自动推进）。
  statuses_[activeIndex_] = QuestStatus::Completed;
  completedCount_ += 1;
  const int32_t nextId = quest.nextQuestId;
  activeIndex_ = -1;
  objectiveIndex_ = 0;
  if (nextId >= 0) {
    const int32_t nextIndex = findQuestIndex(quests_, nextId);
    if (nextIndex >= 0 && statuses_[nextIndex] == QuestStatus::Locked) {
      statuses_[nextIndex] = QuestStatus::Available;
    }
    (void)accept(nextId);
  }
}

void QuestSystem::restoreLinear(int32_t completedQuests,
                                int32_t activeQuestId) {
  if (completedQuests < 0 ||
      completedQuests > static_cast<int32_t>(quests_.size())) {
    return;
  }
  // 重置当前激活态，存档优先于内存默认状态。
  activeIndex_ = -1;
  objectiveIndex_ = 0;
  objectiveProgress_ = 0;
  // 按声明顺序标记前 N 个任务完成；后续任务重新回到锁定态，
  // 由链条规则把 activeQuestId 置为可接取。
  for (int32_t i = 0; i < completedQuests; ++i) {
    statuses_[i] = QuestStatus::Completed;
  }
  completedCount_ = completedQuests;
  for (size_t i = static_cast<size_t>(completedQuests); i < quests_.size();
       ++i) {
    statuses_[i] = QuestStatus::Locked;
  }
  if (activeQuestId >= 0) {
    const int32_t index = findQuestIndex(quests_, activeQuestId);
    if (index >= 0) {
      statuses_[index] = QuestStatus::Available;
      (void)accept(activeQuestId);
    }
  }
}

void QuestSystem::restoreByMask(int32_t completedMask,
                                int32_t activeQuestId) {
  if (completedMask < 0) return;
  // 重置激活态，存档优先于内存默认状态。
  activeIndex_ = -1;
  objectiveIndex_ = 0;
  objectiveProgress_ = 0;
  completedCount_ = 0;
  for (size_t i = 0; i < quests_.size(); ++i) {
    if (i < 31 && ((completedMask >> i) & 1)) {
      statuses_[i] = QuestStatus::Completed;
      completedCount_ += 1;
    } else {
      // 并行支线无前置依赖：未完成任务一律可接取。
      statuses_[i] = QuestStatus::Available;
    }
  }
  if (activeQuestId >= 0) {
    const int32_t index = findQuestIndex(quests_, activeQuestId);
    if (index >= 0 && statuses_[index] == QuestStatus::Available) {
      (void)accept(activeQuestId);
    }
  }
}

QuestStatus QuestSystem::statusOf(int32_t questId) const {
  const int32_t index = findQuestIndex(quests_, questId);
  return index < 0 ? QuestStatus::Locked : statuses_[index];
}

bool QuestSystem::isCompleted(int32_t questId) const {
  return statusOf(questId) == QuestStatus::Completed;
}

int32_t QuestSystem::activeQuestId() const {
  return activeIndex_ < 0 ? -1 : quests_[activeIndex_].id;
}

int32_t QuestSystem::completedCount() const { return completedCount_; }

QuestProgressSnapshot QuestSystem::snapshot() const {
  QuestProgressSnapshot snap;
  if (activeIndex_ < 0) {
    snap.status = completedCount_ > 0 ? QuestStatus::Completed
                                      : QuestStatus::Locked;
    return snap;
  }
  const QuestDef& quest = quests_[activeIndex_];
  snap.questId = quest.id;
  snap.status = QuestStatus::Active;
  snap.title = quest.title;
  snap.objectiveCount = static_cast<int32_t>(quest.objectives.size());
  snap.objectiveIndex = objectiveIndex_;
  if (objectiveIndex_ < snap.objectiveCount) {
    const QuestObjectiveDef& objective = quest.objectives[objectiveIndex_];
    snap.objectiveLabel = objective.description;
    snap.objectiveProgress = objectiveProgress_;
    snap.objectiveRequired = objective.requiredCount;
  }
  return snap;
}
