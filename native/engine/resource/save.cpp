#include "save.h"
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <utility>

namespace {

bool validLocalPosition(float value) {
  return std::isfinite(value) && !std::signbit(value) && value >= 0.0f &&
         value < 1.0f;
}

template <typename Integer>
bool parseIntegerToken(const std::string& token, Integer& value) {
  const char* begin = token.data();
  const char* end = begin + token.size();
  const auto result = std::from_chars(begin, end, value, 10);
  return result.ec == std::errc{} && result.ptr == end;
}

bool parseLocalToken(const std::string& token, float& value) {
  float parsed = 0.0f;
  const char* begin = token.data();
  const char* end = begin + token.size();
  const auto result = std::from_chars(begin, end, parsed,
                                      std::chars_format::general);
  if (result.ec != std::errc{} || result.ptr != end ||
      !validLocalPosition(parsed)) {
    return false;
  }
  value = parsed;
  return true;
}

}  // namespace

bool Save::write(const SaveState& s, const char* path){
  if (!validLocalPosition(s.playerLocalX) ||
      !validLocalPosition(s.playerLocalY)) {
    return false;
  }
  std::ofstream tmp(std::string(path)+".tmp");
  tmp.imbue(std::locale::classic());
  // V10 格式：完整 V9 字段后追加世界种子、区块坐标与局部坐标。
  tmp << std::setprecision(std::numeric_limits<float>::max_digits10);
  tmp << "V10 " << s.campLevel << " " << s.relics << " " << s.regionProgress
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
  // V8 字段：开放世界支线进度。
  tmp << " " << s.openWorldQuestMask << " " << s.openWorldQuestActiveId;
  tmp << " " << s.explorationPoiMask << " " << s.explorationPuzzleMask
      << " " << s.explorationRewardMask << " " << s.explorationGateMask
      << " " << s.explorationTraversalMask;
  tmp << " " << (s.worldSeed == 0 ? 1 : s.worldSeed)
      << " " << s.playerChunkX << " " << s.playerChunkY
      << " " << s.playerLocalX << " " << s.playerLocalY;
  tmp << "\n";
  tmp.flush();
  std::rename((std::string(path)+".tmp").c_str(), path); // 原子替换
  return true;
}
bool Save::read(SaveState& out, const char* path){
  std::ifstream f(path); if(!f) return false;
  std::string first;
  f >> first;
  if (f.fail()) return false;
  // 全程解析到临时状态，失败时不部分污染调用方对象。
  SaveState o;
  auto commit = [&]() {
    if (f.fail()) return false;
    f >> std::ws;
    if (f.peek() != std::char_traits<char>::eof() || f.bad()) return false;
    f.clear();
    out = std::move(o);
    return true;
  };
  if (first == "V2") {
    f >> o.campLevel >> o.relics >> o.regionProgress >> o.completedQuestCount
        >> o.activeQuestId >> o.unlockedAnchorMask >> o.consumedInteractableMask;
    return commit();
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
    return commit();
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
    return commit();
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
    return commit();
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
    return commit();
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
    return commit();
  }
  if (first == "V8" || first == "V9" || first == "V10") {
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
    // V8 字段：开放世界支线完成掩码与接取任务 id。
    f >> o.openWorldQuestMask >> o.openWorldQuestActiveId;
    if (f.fail()) return false;
    if (first == "V9" || first == "V10") {
      f >> o.explorationPoiMask >> o.explorationPuzzleMask
          >> o.explorationRewardMask >> o.explorationGateMask
          >> o.explorationTraversalMask;
    }
    if (f.fail()) return false;
    if (first == "V10") {
      std::string seedToken;
      std::string chunkXToken;
      std::string chunkYToken;
      std::string localXToken;
      std::string localYToken;
      f >> seedToken >> chunkXToken >> chunkYToken >> localXToken >> localYToken;
      if (f.fail() || !parseIntegerToken(seedToken, o.worldSeed) ||
          !parseIntegerToken(chunkXToken, o.playerChunkX) ||
          !parseIntegerToken(chunkYToken, o.playerChunkY) ||
          !parseLocalToken(localXToken, o.playerLocalX) ||
          !parseLocalToken(localYToken, o.playerLocalY)) {
        return false;
      }
      if (o.worldSeed == 0) o.worldSeed = 1;
    }
    return commit();
  }
  // v1 兼容：首 token 即 campLevel，后续两个字段。
  if (!parseIntegerToken(first, o.campLevel)) return false;
  f >> o.relics >> o.regionProgress;
  return commit();
}
