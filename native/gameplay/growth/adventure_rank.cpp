#include "native/gameplay/growth/adventure_rank.h"

#include <algorithm>

int32_t AdventureRank::expRequired(int32_t rank) {
  if (rank < 1) return 375;
  if (rank >= kMaxRank) return 0;
  if (rank < 20) {
    // 低段：375 起步，每级 +125。
    return 375 + (rank - 1) * 125;
  }
  if (rank < 40) {
    // 中段：3000 起步，每级 +500。
    return 3000 + (rank - 20) * 500;
  }
  // 高段：13000 起步，每级 +1000。
  return 13000 + (rank - 40) * 1000;
}

int32_t AdventureRank::worldLevelFor(int32_t rank) {
  // AR 20/25/30/35/40/45/50/55/58 逐级解锁世界等级（封顶 8）。
  static const int32_t kThresholds[] = {20, 25, 30, 35, 40, 45, 50, 55, 58};
  int32_t level = 0;
  for (const int32_t threshold : kThresholds) {
    if (rank >= threshold) ++level;
  }
  return std::min(level, kMaxWorldLevel);
}

int32_t AdventureRank::dropMultiplierPct(int32_t worldLevel) {
  const int32_t clamped = std::clamp(worldLevel, 0, kMaxWorldLevel);
  return 100 + 25 * clamped;
}

const std::vector<RankReward>& AdventureRank::rankRewards() {
  static const std::vector<RankReward> rewards = {
      {2, 200, 2, 2, 0},
      {5, 400, 3, 4, 1},
      {10, 800, 4, 6, 2},
      {15, 1200, 5, 8, 2},
      {20, 1600, 6, 10, 3},
      {25, 2000, 6, 12, 3},
      {30, 2400, 8, 14, 4},
      {35, 2800, 8, 16, 4},
      {40, 3200, 10, 18, 5},
      {45, 3600, 10, 20, 5},
      {50, 4000, 12, 22, 6},
      {55, 4400, 12, 24, 6},
      {60, 5000, 15, 30, 8},
  };
  return rewards;
}

const RankReward* AdventureRank::rankReward(int32_t rank) {
  for (const RankReward& reward : rankRewards()) {
    if (reward.rank == rank) return &reward;
  }
  return nullptr;
}

int32_t AdventureRank::addExp(int32_t expAmount) {
  if (expAmount <= 0) return 0;
  int32_t levelsGained = 0;
  exp_ += expAmount;
  // 级联升级：封顶 60 后停止积累。
  while (rank_ < kMaxRank) {
    const int32_t required = expRequired(rank_);
    if (exp_ < required) break;
    exp_ -= required;
    rank_ += 1;
    levelsGained += 1;
  }
  if (rank_ >= kMaxRank) {
    exp_ = 0;
  }
  return levelsGained;
}

bool AdventureRank::restore(int32_t rank, int32_t exp) {
  if (rank < 1) return false;
  rank_ = std::clamp(rank, 1, kMaxRank);
  exp_ = rank_ >= kMaxRank ? 0
                            : std::clamp(exp, 0, expRequired(rank_) - 1);
  return true;
}
