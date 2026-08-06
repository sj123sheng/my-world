#include "save.h"
#include <fstream>
bool Save::write(const SaveState& s, const char* path){
  std::ofstream tmp(std::string(path)+".tmp");
  // v7 格式：版本标记 + 核心三字段 + 进度字段 + 养成字段 + 角色三元组
  // + 支线掩码 + 重生倒计时 + 武器三元组（兼容）+ 冒险等级/掉落种子
  // + 新物品计数 + 武器七元组 + 圣遗物六元组 + 已领等阶奖励。
  tmp << "V7 " << s.campLevel << " " << s.relics << " " << s.regionProgress
      << " " << s.completedQuestCount << " " << s.activeQuestId << " "
      << s.unlockedAnchorMask << " " << s.consumedInteractableMask << " "
      << s.fateCount << " " << s.goldCount << " " << s.expCount << " "
      << s.ascensionCount << " " << s.gachaPity5 << " " << s.gachaPity4 << " "
      << s.gachaSeed << " " << s.rosterTriples.size();
  for (int32_t value : s.rosterTriples) {
    tmp << " " << value;
  }
  tmp << " " << s.sideQuestMask;
  tmp << " " << s.collectRespawnMs;
  tmp << " " << s.weaponTriples.size();
  for (int32_t value : s.weaponTriples) {
    tmp << " " << value;
  }
  tmp << " " << s.adventureRank << " " << s.adventureExp << " "
      << s.dropSeed << " " << s.oreLowCount << " " << s.oreMidCount << " "
      << s.oreHighCount << " " << s.expSmallCount << " " << s.expMediumCount
      << " " << s.expLargeCount;
  tmp << " " << s.weaponRecords.size();
  for (int32_t value : s.weaponRecords) {
    tmp << " " << value;
  }
  tmp << " " << s.artifactRecords.size();
  for (int32_t value : s.artifactRecords) {
    tmp << " " << value;
  }
  tmp << " " << s.claimedRanks.size();
  for (int32_t value : s.claimedRanks) {
    tmp << " " << value;
  }
  tmp << "\n";
  tmp.flush();
  std::rename((std::string(path)+".tmp").c_str(), path); // 原子替换
  return true;
}
bool Save::read(SaveState& o, const char* path){
  std::ifstream f(path); if(!f) return false;
  std::string first;
  f >> first;
  if (f.fail()) return false;
  if (first == "V2") {
    f >> o.campLevel >> o.relics >> o.regionProgress >> o.completedQuestCount
        >> o.activeQuestId >> o.unlockedAnchorMask >> o.consumedInteractableMask;
    return !f.fail();
  }
  if (first == "V3") {
    f >> o.campLevel >> o.relics >> o.regionProgress >> o.completedQuestCount
        >> o.activeQuestId >> o.unlockedAnchorMask >> o.consumedInteractableMask
        >> o.fateCount >> o.goldCount >> o.expCount >> o.ascensionCount
        >> o.gachaPity5 >> o.gachaPity4 >> o.gachaSeed;
    if (f.fail()) return false;
    size_t rosterValues = 0;
    f >> rosterValues;
    if (f.fail() || rosterValues > 1024) return false;
    o.rosterTriples.assign(rosterValues, 0);
    for (size_t i = 0; i < rosterValues; ++i) {
      f >> o.rosterTriples[i];
    }
    return !f.fail();
  }
  if (first == "V4") {
    f >> o.campLevel >> o.relics >> o.regionProgress >> o.completedQuestCount
        >> o.activeQuestId >> o.unlockedAnchorMask >> o.consumedInteractableMask
        >> o.fateCount >> o.goldCount >> o.expCount >> o.ascensionCount
        >> o.gachaPity5 >> o.gachaPity4 >> o.gachaSeed;
    if (f.fail()) return false;
    size_t rosterValues = 0;
    f >> rosterValues;
    if (f.fail() || rosterValues > 1024) return false;
    o.rosterTriples.assign(rosterValues, 0);
    for (size_t i = 0; i < rosterValues; ++i) {
      f >> o.rosterTriples[i];
    }
    f >> o.sideQuestMask;
    return !f.fail();
  }
  if (first == "V5") {
    f >> o.campLevel >> o.relics >> o.regionProgress >> o.completedQuestCount
        >> o.activeQuestId >> o.unlockedAnchorMask >> o.consumedInteractableMask
        >> o.fateCount >> o.goldCount >> o.expCount >> o.ascensionCount
        >> o.gachaPity5 >> o.gachaPity4 >> o.gachaSeed;
    if (f.fail()) return false;
    size_t rosterValues = 0;
    f >> rosterValues;
    if (f.fail() || rosterValues > 1024) return false;
    o.rosterTriples.assign(rosterValues, 0);
    for (size_t i = 0; i < rosterValues; ++i) {
      f >> o.rosterTriples[i];
    }
    f >> o.sideQuestMask >> o.collectRespawnMs;
    return !f.fail();
  }
  if (first == "V6") {
    f >> o.campLevel >> o.relics >> o.regionProgress >> o.completedQuestCount
        >> o.activeQuestId >> o.unlockedAnchorMask >> o.consumedInteractableMask
        >> o.fateCount >> o.goldCount >> o.expCount >> o.ascensionCount
        >> o.gachaPity5 >> o.gachaPity4 >> o.gachaSeed;
    if (f.fail()) return false;
    size_t rosterValues = 0;
    f >> rosterValues;
    if (f.fail() || rosterValues > 1024) return false;
    o.rosterTriples.assign(rosterValues, 0);
    for (size_t i = 0; i < rosterValues; ++i) {
      f >> o.rosterTriples[i];
    }
    f >> o.sideQuestMask >> o.collectRespawnMs;
    if (f.fail()) return false;
    size_t weaponValues = 0;
    f >> weaponValues;
    if (f.fail() || weaponValues > 1024) return false;
    o.weaponTriples.assign(weaponValues, 0);
    for (size_t i = 0; i < weaponValues; ++i) {
      f >> o.weaponTriples[i];
    }
    return !f.fail();
  }
  if (first == "V7") {
    f >> o.campLevel >> o.relics >> o.regionProgress >> o.completedQuestCount
        >> o.activeQuestId >> o.unlockedAnchorMask >> o.consumedInteractableMask
        >> o.fateCount >> o.goldCount >> o.expCount >> o.ascensionCount
        >> o.gachaPity5 >> o.gachaPity4 >> o.gachaSeed;
    if (f.fail()) return false;
    size_t rosterValues = 0;
    f >> rosterValues;
    if (f.fail() || rosterValues > 1024) return false;
    o.rosterTriples.assign(rosterValues, 0);
    for (size_t i = 0; i < rosterValues; ++i) {
      f >> o.rosterTriples[i];
    }
    f >> o.sideQuestMask >> o.collectRespawnMs;
    if (f.fail()) return false;
    size_t weaponValues = 0;
    f >> weaponValues;
    if (f.fail() || weaponValues > 1024) return false;
    o.weaponTriples.assign(weaponValues, 0);
    for (size_t i = 0; i < weaponValues; ++i) {
      f >> o.weaponTriples[i];
    }
    uint32_t dropSeed = 0;
    f >> o.adventureRank >> o.adventureExp >> dropSeed >> o.oreLowCount
        >> o.oreMidCount >> o.oreHighCount >> o.expSmallCount
        >> o.expMediumCount >> o.expLargeCount;
    o.dropSeed = dropSeed;
    if (f.fail()) return false;
    size_t weaponRecords = 0;
    f >> weaponRecords;
    if (f.fail() || weaponRecords > 1024 || weaponRecords % 7 != 0) {
      return false;
    }
    o.weaponRecords.assign(weaponRecords, 0);
    for (size_t i = 0; i < weaponRecords; ++i) {
      f >> o.weaponRecords[i];
    }
    size_t artifactRecords = 0;
    f >> artifactRecords;
    if (f.fail() || artifactRecords > 4096 || artifactRecords % 6 != 0) {
      return false;
    }
    o.artifactRecords.assign(artifactRecords, 0);
    for (size_t i = 0; i < artifactRecords; ++i) {
      f >> o.artifactRecords[i];
    }
    size_t claimedCount = 0;
    f >> claimedCount;
    if (f.fail() || claimedCount > 128) return false;
    o.claimedRanks.assign(claimedCount, 0);
    for (size_t i = 0; i < claimedCount; ++i) {
      f >> o.claimedRanks[i];
    }
    return !f.fail();
  }
  // v1 兼容：首 token 即 campLevel，后续两个字段。
  o.campLevel = std::stoi(first);
  f >> o.relics >> o.regionProgress;
  return !f.fail();
}
