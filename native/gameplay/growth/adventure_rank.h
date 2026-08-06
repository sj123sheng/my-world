#pragma once

#include <cstdint>
#include <vector>

// 冒险等级与世界等级（原神式体系）：
// 打怪/任务/秘境获得冒险经验，等级提升解锁世界等级；
// 世界等级放大打怪掉落。全部规则确定性、无随机，可独立测试。
struct RankReward {
  int32_t rank = 0;
  int32_t gold = 0;
  int32_t fate = 0;
  int32_t expMaterial = 0;
  int32_t ascensionMaterial = 0;
};

class AdventureRank {
 public:
  static constexpr int32_t kMaxRank = 60;
  static constexpr int32_t kMaxWorldLevel = 8;

  // 升到下一等阶所需冒险经验（分段递增曲线）。
  static int32_t expRequired(int32_t rank);
  // 冒险等阶 -> 世界等级解锁映射（AR 20/25/.../58 逐级解锁 0..8）。
  static int32_t worldLevelFor(int32_t rank);
  // 世界等级掉落倍率（百分比）：100 + 25 * 世界等级。
  static int32_t dropMultiplierPct(int32_t worldLevel);
  // 等阶奖励配置表：到达对应等阶一次性发放。
  static const std::vector<RankReward>& rankRewards();
  static const RankReward* rankReward(int32_t rank);

  // 注入冒险经验并级联升级；封顶 60 后不再积累。返回升了几级。
  int32_t addExp(int32_t expAmount);

  int32_t rank() const { return rank_; }
  int32_t exp() const { return exp_; }
  int32_t worldLevel() const { return worldLevelFor(rank_); }

  // 存档恢复：非法值钳制。
  bool restore(int32_t rank, int32_t exp);

 private:
  int32_t rank_ = 1;
  int32_t exp_ = 0;
};
