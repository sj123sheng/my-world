#include "native/gameplay/quest/side_quests.h"

SideQuestSystem SideQuestSystem::defaults() {
  return SideQuestSystem({
      {1, "雾谷肃清", SideQuestEvent::Kill, 3},
      {2, "脉流采集", SideQuestEvent::Collect, 2},
      {3, "远行之路", SideQuestEvent::ReachAnchor, 2},
  });
}

SideQuestSystem::SideQuestSystem(std::vector<SideQuestDef> quests)
    : quests_(std::move(quests)) {
  progress_.assign(quests_.size(), 0);
  completed_.assign(quests_.size(), false);
}

void SideQuestSystem::notifyEvent(SideQuestEvent event, int32_t count) {
  if (count <= 0) return;
  for (size_t i = 0; i < quests_.size(); ++i) {
    if (completed_[i] || quests_[i].event != event) continue;
    progress_[i] += count;
    if (progress_[i] >= quests_[i].required) {
      progress_[i] = quests_[i].required;
      completed_[i] = true;
    }
  }
}

bool SideQuestSystem::isCompleted(int32_t questId) const {
  for (size_t i = 0; i < quests_.size(); ++i) {
    if (quests_[i].id == questId) return completed_[i];
  }
  return false;
}

int32_t SideQuestSystem::progressOf(int32_t questId) const {
  for (size_t i = 0; i < quests_.size(); ++i) {
    if (quests_[i].id == questId) return progress_[i];
  }
  return 0;
}

int32_t SideQuestSystem::completedCount() const {
  int32_t count = 0;
  for (bool done : completed_) {
    if (done) count += 1;
  }
  return count;
}

int32_t SideQuestSystem::completedMask() const {
  int32_t mask = 0;
  for (size_t i = 0; i < quests_.size(); ++i) {
    if (completed_[i]) mask |= (1 << (quests_[i].id - 1));
  }
  return mask;
}

void SideQuestSystem::restoreMask(int32_t mask) {
  for (size_t i = 0; i < quests_.size(); ++i) {
    const bool done = (mask & (1 << (quests_[i].id - 1))) != 0;
    completed_[i] = done;
    progress_[i] = done ? quests_[i].required : 0;
  }
}
