// 存档 V8 单测（Phase 5）：V8 往返 + V7 旧存档兼容默认值 +
// QuestSystem::restoreByMask 恢复语义。
#include "../native/engine/resource/save.h"
#include "../native/gameplay/quest/quest_system.h"

#include <cassert>
#include <cstdio>
#include <fstream>

int main() {
  // ---- V8 往返 ----
  Save save;
  SaveState in;
  in.campLevel = 2;
  in.relics = 3;
  in.regionProgress = 5;
  in.completedQuestCount = 4;
  in.activeQuestId = 5;
  in.unlockedAnchorMask = 0x1F;
  in.consumedInteractableMask = 0x3;
  in.adventureRank = 7;
  in.adventureExp = 1200;
  in.dropSeed = 42;
  in.claimedRanks = {1, 2};
  in.rosterTriples = {1, 20, 1};
  in.weaponRecords = {10, 1, 5, 0, 1, 0, 900};      // 7 的倍数
  in.artifactRecords = {7, 3, 4, 2, 1, 99};        // 6 的倍数
  in.openWorldQuestMask = 0b010;  // 支线 202 完成
  in.openWorldQuestActiveId = 203;
  assert(save.write(in, "/tmp/save_v8.dat"));
  SaveState out;
  assert(save.read(out, "/tmp/save_v8.dat"));
  assert(out.campLevel == 2 && out.relics == 3 && out.regionProgress == 5);
  assert(out.completedQuestCount == 4 && out.activeQuestId == 5);
  assert(out.unlockedAnchorMask == 0x1F);
  assert(out.adventureRank == 7 && out.adventureExp == 1200);
  assert(out.dropSeed == 42);
  assert(out.claimedRanks.size() == 2 && out.claimedRanks[1] == 2);
  assert(out.weaponRecords.size() == 7 && out.weaponRecords[6] == 900);
  assert(out.artifactRecords.size() == 6 && out.artifactRecords[5] == 99);
  assert(out.openWorldQuestMask == 0b010);
  assert(out.openWorldQuestActiveId == 203);

  // ---- V7 旧存档兼容：V8 新字段走默认值 ----
  {
    std::ofstream v7("/tmp/save_v7.dat");
    v7 << "V7 " << 1 << " " << 2 << " " << 3 << " " << 2 << " " << 3
       << " " << 0 << " " << 0 << " " << 0 << " " << 0 << " " << 0 << " "
       << 0 << " " << 0 << " " << 0 << " " << 0 << " "
       << 0                          // roster 值数量
       << " " << 0                   // sideQuestMask
       << " " << 0                   // collectRespawnMs
       << " " << 0                   // weaponTriples 值数量
       << " " << 5 << " " << 800     // adventureRank / exp
       << " " << 0u                  // dropSeed
       << " " << 0 << " " << 0 << " " << 0 << " " << 0 << " " << 0 << " " << 0
       << " " << 0                   // weaponRecords 值数量
       << " " << 0                   // artifactRecords 值数量
       << " " << 0                   // claimedRanks 数量
       << "\n";
  }
  SaveState legacy;
  assert(save.read(legacy, "/tmp/save_v7.dat"));
  assert(legacy.adventureRank == 5 && legacy.adventureExp == 800);
  assert(legacy.openWorldQuestMask == 0);
  assert(legacy.openWorldQuestActiveId == -1);

  // ---- restoreByMask：并行支线恢复语义 ----
  QuestSystem quests = QuestSystem::openWorldQuests();
  quests.restoreByMask(0b010, 201);
  assert(quests.statusOf(201) == QuestStatus::Active);
  assert(quests.statusOf(202) == QuestStatus::Completed);
  assert(quests.statusOf(203) == QuestStatus::Available);
  assert(quests.completedCount() == 1);
  assert(quests.activeQuestId() == 201);

  // 空掩码恢复等价初始全部可接取态（V1-V7 旧存档默认路径）。
  QuestSystem fresh = QuestSystem::openWorldQuests();
  fresh.restoreByMask(0, -1);
  assert(fresh.statusOf(201) == QuestStatus::Available);
  assert(fresh.statusOf(202) == QuestStatus::Available);
  assert(fresh.statusOf(203) == QuestStatus::Available);
  assert(fresh.completedCount() == 0 && fresh.activeQuestId() == -1);

  // 已完成任务不可再被接取。
  QuestSystem done = QuestSystem::openWorldQuests();
  done.restoreByMask(0b111, -1);
  assert(done.completedCount() == 3);
  assert(!done.accept(202));

  std::remove("/tmp/save_v8.dat");
  std::remove("/tmp/save_v7.dat");
  return 0;
}
