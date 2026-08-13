// 存档 V10：世界位置往返、旧版本迁移、坏位置拒绝与任务恢复语义。
#include "../native/engine/resource/save.h"
#include "../native/gameplay/quest/quest_system.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>

namespace {

void writeV10Fixture(const char* path, const std::string& worldFields) {
  std::ofstream file(path);
  file << "V10 "
       << "7 8 9 0 -1 0 0 0 0 0 0 0 0 0 "
       << "0 "       // rosterTriples 数量
       << "0 0 "     // sideQuestMask / collectRespawnMs
       << "0 "       // weaponTriples 数量
       << "1 0 0 0 0 0 0 0 0 "
       << "0 0 0 "   // weaponRecords / artifactRecords / claimedRanks 数量
       << "0 -1 "    // 开放世界任务掩码 / 当前任务
       << "1 2 3 4 5 "  // V9 探索字段
       << worldFields << "\n";
}

SaveState sentinelState() {
  SaveState state;
  state.campLevel = 91;
  state.rosterTriples = {9, 8, 7};
  state.explorationPoiMask = 77;
  state.worldSeed = 123456789;
  state.playerChunkX = -81;
  state.playerChunkY = 82;
  state.playerLocalX = 0.33f;
  state.playerLocalY = 0.66f;
  return state;
}

void assertSentinelUnchanged(const SaveState& state) {
  assert(state.campLevel == 91);
  assert((state.rosterTriples == std::vector<int32_t>{9, 8, 7}));
  assert(state.explorationPoiMask == 77);
  assert(state.worldSeed == 123456789);
  assert(state.playerChunkX == -81);
  assert(state.playerChunkY == 82);
  assert(state.playerLocalX == 0.33f);
  assert(state.playerLocalY == 0.66f);
}

void assertLegacyWorldDefaults(const SaveState& state) {
  assert(state.worldSeed == 1);
  assert(state.playerChunkX == 0 && state.playerChunkY == 0);
  assert(state.playerLocalX == 0.5f && state.playerLocalY == 0.12f);
}

}  // namespace

int main() {
  // ---- V10 超远世界位置往返 ----
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
  in.explorationPoiMask = 0b00101;
  in.explorationPuzzleMask = 0b0110;
  in.explorationRewardMask = 0b0100;
  in.explorationGateMask = 0b0110;
  in.explorationTraversalMask = 0b11111;
  in.worldSeed = std::numeric_limits<uint64_t>::max();
  in.playerChunkX = -1234567890123LL;
  in.playerChunkY = 987654321012LL;
  in.playerLocalX = 0.25f;
  in.playerLocalY = 0.75f;
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
  assert(out.explorationPoiMask == 0b00101);
  assert(out.explorationPuzzleMask == 0b0110);
  assert(out.explorationRewardMask == 0b0100);
  assert(out.explorationGateMask == 0b0110);
  assert(out.explorationTraversalMask == 0b11111);
  assert(out.worldSeed == std::numeric_limits<uint64_t>::max());
  assert(out.playerChunkX == -1234567890123LL);
  assert(out.playerChunkY == 987654321012LL);
  assert(out.playerLocalX == 0.25f && out.playerLocalY == 0.75f);

  {
    std::ifstream written("/tmp/save_v8.dat");
    std::string version;
    written >> version;
    assert(version == "V10");
  }

  // seed 0 写入后规范为 1。
  SaveState zeroSeed;
  zeroSeed.worldSeed = 0;
  zeroSeed.playerLocalX = 0.0f;
  zeroSeed.playerLocalY = 0.999999f;
  assert(save.write(zeroSeed, "/tmp/save_v10_seed0.dat"));
  SaveState normalizedSeed;
  assert(save.read(normalizedSeed, "/tmp/save_v10_seed0.dat"));
  assert(normalizedSeed.worldSeed == 1);
  assert(normalizedSeed.playerLocalX == 0.0f);
  assert(normalizedSeed.playerLocalY == 0.999999f);

  writeV10Fixture("/tmp/save_v10_seed0_fixture.dat", "0 0 0 0.5 0.12");
  SaveState loadedZeroSeed;
  assert(save.read(loadedZeroSeed, "/tmp/save_v10_seed0_fixture.dat"));
  assert(loadedZeroSeed.worldSeed == 1);

  SaveState invalidWrite;
  invalidWrite.playerLocalX = std::numeric_limits<float>::quiet_NaN();
  assert(!save.write(invalidWrite, "/tmp/save_v10_invalid_write.dat"));
  invalidWrite.playerLocalX = 0.5f;
  invalidWrite.playerLocalY = 1.0f;
  assert(!save.write(invalidWrite, "/tmp/save_v10_invalid_write.dat"));

  // ---- V9 旧存档迁移：探索语义保留，世界位置进入核心区出生点 ----
  {
    std::ofstream v9("/tmp/save_v9.dat");
    v9 << "V9 "
       << "7 8 9 0 -1 0 0 0 0 0 0 0 0 0 "
       << "0 0 0 0 1 0 0 0 0 0 0 0 0 "
       << "0 0 0 0 -1 1 2 3 4 5\n";
  }
  SaveState v9State = sentinelState();
  assert(save.read(v9State, "/tmp/save_v9.dat"));
  assert(v9State.explorationPoiMask == 1);
  assert(v9State.explorationPuzzleMask == 2);
  assert(v9State.explorationRewardMask == 3);
  assert(v9State.explorationGateMask == 4);
  assert(v9State.explorationTraversalMask == 5);
  assertLegacyWorldDefaults(v9State);

  // V8 仍可读，V9 探索字段及 V10 世界字段使用默认值。
  {
    std::ofstream v8("/tmp/save_v8_legacy.dat");
    v8 << "V8 "
       << "7 8 9 0 -1 0 0 0 0 0 0 0 0 0 "
       << "0 0 0 0 1 0 0 0 0 0 0 0 0 "
       << "0 0 0 0 -1\n";
  }
  SaveState v8State = sentinelState();
  assert(save.read(v8State, "/tmp/save_v8_legacy.dat"));
  assert(v8State.explorationPoiMask == 0);
  assert(v8State.explorationTraversalMask == 0);
  assertLegacyWorldDefaults(v8State);

  // 非法 local、坏整数、截断及多余字段均失败，且不污染调用方状态。
  const std::string invalidWorldFields[] = {
      "1 0 0 nan 0.5",          "1 0 0 inf 0.5",
      "1 0 0 -inf 0.5",         "1 0 0 -0 0.5",
      "1 0 0 -0.01 0.5",        "1 0 0 1.0 0.5",
      "1 0 0 0.5 1.0",          "-1 0 0 0.5 0.5",
      "18446744073709551616 0 0 0.5 0.5",
      "1 -9223372036854775809 0 0.5 0.5",
      "1 9223372036854775808 0 0.5 0.5",
      "1 0 0 0.5",              "1 0 nope 0.5 0.5",
      "1 0 0 0.5 0.5 trailing",
  };
  for (size_t i = 0; i < std::size(invalidWorldFields); ++i) {
    const std::string path = "/tmp/save_v10_invalid_" + std::to_string(i) + ".dat";
    writeV10Fixture(path.c_str(), invalidWorldFields[i]);
    SaveState unchanged = sentinelState();
    assert(!save.read(unchanged, path.c_str()));
    assertSentinelUnchanged(unchanged);
    std::remove(path.c_str());
  }

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
  legacy.explorationPoiMask = 0x7;
  legacy.explorationPuzzleMask = 0x7;
  legacy.explorationRewardMask = 0x7;
  legacy.explorationGateMask = 0x7;
  legacy.explorationTraversalMask = 0x1F;
  assert(save.read(legacy, "/tmp/save_v7.dat"));
  assert(legacy.adventureRank == 5 && legacy.adventureExp == 800);
  assert(legacy.openWorldQuestMask == 0);
  assert(legacy.openWorldQuestActiveId == -1);
  assert(legacy.explorationPoiMask == 0);
  assert(legacy.explorationPuzzleMask == 0);
  assert(legacy.explorationRewardMask == 0);
  assert(legacy.explorationGateMask == 0);
  assert(legacy.explorationTraversalMask == 0);
  assertLegacyWorldDefaults(legacy);

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
  std::remove("/tmp/save_v10_seed0.dat");
  std::remove("/tmp/save_v10_seed0_fixture.dat");
  std::remove("/tmp/save_v10_invalid_write.dat");
  std::remove("/tmp/save_v9.dat");
  std::remove("/tmp/save_v8_legacy.dat");
  std::remove("/tmp/save_v7.dat");
  return 0;
}
