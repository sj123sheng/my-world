#include "../native/engine/resource/save.h"

#include <cassert>
#include <cstdio>
#include <fstream>

namespace {

void assertLegacyWorldDefaults(const SaveState& state) {
  assert(state.worldSeed == 1);
  assert(state.playerChunkX == 0 && state.playerChunkY == 0);
  assert(state.playerLocalX == 0.5f && state.playerLocalY == 0.12f);
}

}  // namespace

int main() {
  Save save;

  // V10 写读往返：全部进度、养成字段与世界位置保持。
  SaveState state;
  state.campLevel = 2;
  state.relics = 3;
  state.regionProgress = 5;
  state.completedQuestCount = 4;
  state.activeQuestId = 5;
  state.unlockedAnchorMask = 0b10101;
  state.consumedInteractableMask = 0b110;
  state.fateCount = 12;
  state.goldCount = 340;
  state.expCount = 8;
  state.ascensionCount = 2;
  state.gachaPity5 = 33;
  state.gachaPity4 = 4;
  state.gachaSeed = 0xABCD1234u;
  state.rosterTriples = {1, 20, 1, 4, 5, 0};
  state.sideQuestMask = 0b101;
  state.collectRespawnMs = 45000;
  state.weaponTriples = {4, 3, 1, 5, 1, 0};
  state.worldSeed = 998877665544332211ULL;
  state.playerChunkX = -4000000000LL;
  state.playerChunkY = 5000000000LL;
  state.playerLocalX = 0.125f;
  state.playerLocalY = 0.875f;
  assert(save.write(state, "/tmp/save_progress.dat"));
  SaveState loaded;
  assert(save.read(loaded, "/tmp/save_progress.dat"));
  assert(loaded.campLevel == 2);
  assert(loaded.relics == 3);
  assert(loaded.regionProgress == 5);
  assert(loaded.completedQuestCount == 4);
  assert(loaded.activeQuestId == 5);
  assert(loaded.unlockedAnchorMask == 0b10101);
  assert(loaded.consumedInteractableMask == 0b110);
  assert(loaded.fateCount == 12);
  assert(loaded.goldCount == 340);
  assert(loaded.expCount == 8);
  assert(loaded.ascensionCount == 2);
  assert(loaded.gachaPity5 == 33);
  assert(loaded.gachaPity4 == 4);
  assert(loaded.gachaSeed == 0xABCD1234u);
  assert(loaded.rosterTriples == state.rosterTriples);
  assert(loaded.sideQuestMask == 0b101);
  assert(loaded.collectRespawnMs == 45000);
  assert(loaded.weaponTriples == state.weaponTriples);
  assert(loaded.worldSeed == 998877665544332211ULL);
  assert(loaded.playerChunkX == -4000000000LL);
  assert(loaded.playerChunkY == 5000000000LL);
  assert(loaded.playerLocalX == 0.125f && loaded.playerLocalY == 0.875f);

  // v6 兼容：旧版最后字段为 weaponTriples，世界位置迁移到出生点。
  {
    std::ofstream v6file("/tmp/save_v6.dat");
    v6file << "V6 1 0 0 3 4 7 6 10 200 5 1 20 3 0 0 5 45000 0\n";
  }
  SaveState v6State;
  assert(save.read(v6State, "/tmp/save_v6.dat"));
  assert(v6State.collectRespawnMs == 45000);
  assertLegacyWorldDefaults(v6State);

  // v5 兼容：无武器三元组的存档仍可读取，武器取默认值。
  {
    std::ofstream v5file("/tmp/save_v5.dat");
    v5file << "V5 1 0 0 3 4 7 6 10 200 5 1 20 3 0 3 1 20 1 5 45000\n";
  }
  SaveState v5State;
  assert(save.read(v5State, "/tmp/save_v5.dat"));
  assert(v5State.completedQuestCount == 3);
  assert(v5State.collectRespawnMs == 45000);
  assert(v5State.weaponTriples.empty());
  assertLegacyWorldDefaults(v5State);

  // v4 兼容：无重生倒计时的存档仍可读取，倒计时取默认值。
  {
    std::ofstream v4file("/tmp/save_v4.dat");
    v4file << "V4 1 0 0 3 4 7 6 10 200 5 1 20 3 0 3 1 20 1 5\n";
  }
  SaveState v4State;
  assert(save.read(v4State, "/tmp/save_v4.dat"));
  assert(v4State.completedQuestCount == 3);
  assert(v4State.sideQuestMask == 5);
  assert(v4State.collectRespawnMs == 0);
  assertLegacyWorldDefaults(v4State);

  // v3 兼容：无支线掩码的旧存档仍可读取，掩码取默认值。
  {
    std::ofstream v3file("/tmp/save_v3.dat");
    v3file << "V3 1 0 0 3 4 7 6 10 200 5 1 20 3 0 3 1 20 1\n";
  }
  SaveState v3State;
  assert(save.read(v3State, "/tmp/save_v3.dat"));
  assert(v3State.completedQuestCount == 3);
  assert(v3State.rosterTriples.size() == 3);
  assert(v3State.sideQuestMask == 0);
  assertLegacyWorldDefaults(v3State);

  // v2 兼容：旧进度格式仍可读取，养成字段取默认值。
  {
    std::ofstream legacy("/tmp/save_v2.dat");
    legacy << "V2 2 3 5 4 5 21 6\n";
  }
  SaveState v2State;
  assert(save.read(v2State, "/tmp/save_v2.dat"));
  assert(v2State.completedQuestCount == 4);
  assert(v2State.activeQuestId == 5);
  assert(v2State.fateCount == 0);
  assert(v2State.rosterTriples.empty());
  assertLegacyWorldDefaults(v2State);

  // v1 兼容：旧三字段格式仍可读取，进度字段取默认值。
  {
    std::ofstream legacy("/tmp/save_legacy.dat");
    legacy << "2 3 5\n";
  }
  SaveState legacyState;
  assert(save.read(legacyState, "/tmp/save_legacy.dat"));
  assert(legacyState.campLevel == 2);
  assert(legacyState.relics == 3);
  assert(legacyState.regionProgress == 5);
  assert(legacyState.completedQuestCount == 0);
  assert(legacyState.activeQuestId == -1);
  assert(legacyState.unlockedAnchorMask == 0);
  assertLegacyWorldDefaults(legacyState);

  // 空文件读取失败。
  { std::ofstream empty("/tmp/save_empty.dat"); }
  SaveState emptyState;
  assert(!save.read(emptyState, "/tmp/save_empty.dat"));

  // 不存在文件读取失败。
  assert(!save.read(emptyState, "/tmp/save_missing_not_exist.dat"));
  return 0;
}
