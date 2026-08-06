#include "native/gameplay/growth/adventure_rank.h"

#include <cassert>

int main() {
  // 经验曲线：分段递增，封顶后为 0。
  assert(AdventureRank::expRequired(1) == 375);
  assert(AdventureRank::expRequired(2) == 500);
  assert(AdventureRank::expRequired(19) == 2625);
  assert(AdventureRank::expRequired(20) == 3000);
  assert(AdventureRank::expRequired(39) == 12500);
  assert(AdventureRank::expRequired(40) == 13000);
  assert(AdventureRank::expRequired(59) == 32000);
  assert(AdventureRank::expRequired(60) == 0);

  // 世界等级映射：AR 20 起逐级解锁，封顶 8。
  assert(AdventureRank::worldLevelFor(1) == 0);
  assert(AdventureRank::worldLevelFor(19) == 0);
  assert(AdventureRank::worldLevelFor(20) == 1);
  assert(AdventureRank::worldLevelFor(25) == 2);
  assert(AdventureRank::worldLevelFor(30) == 3);
  assert(AdventureRank::worldLevelFor(35) == 4);
  assert(AdventureRank::worldLevelFor(40) == 5);
  assert(AdventureRank::worldLevelFor(45) == 6);
  assert(AdventureRank::worldLevelFor(50) == 7);
  assert(AdventureRank::worldLevelFor(55) == 8);
  assert(AdventureRank::worldLevelFor(60) == 8);

  // 掉落倍率：100 + 25 * 世界等级，越界钳制。
  assert(AdventureRank::dropMultiplierPct(0) == 100);
  assert(AdventureRank::dropMultiplierPct(3) == 175);
  assert(AdventureRank::dropMultiplierPct(8) == 300);
  assert(AdventureRank::dropMultiplierPct(99) == 300);

  // 等阶奖励：配置表覆盖关键节点，未知等阶为空。
  const std::vector<RankReward>& rewards = AdventureRank::rankRewards();
  assert(rewards.size() == 13);
  assert(AdventureRank::rankReward(2) != nullptr);
  assert(AdventureRank::rankReward(20) != nullptr);
  assert(AdventureRank::rankReward(60) != nullptr);
  assert(AdventureRank::rankReward(3) == nullptr);
  for (const RankReward& reward : rewards) {
    assert(reward.rank >= 2 && reward.rank <= 60);
    assert(reward.gold > 0);
  }

  // 升级：375 经验恰好升到 2 级。
  AdventureRank rank;
  assert(rank.rank() == 1);
  assert(rank.addExp(375) == 1);
  assert(rank.rank() == 2);
  assert(rank.exp() == 0);

  // 级联升级：一次注入大量经验。
  const int32_t levels = rank.addExp(100000);
  assert(levels >= 10);
  assert(rank.rank() >= 12);

  // 存档恢复：钳制与封顶清零。
  AdventureRank restored;
  assert(restored.restore(25, 999999));
  assert(restored.rank() == 25);
  assert(restored.exp() < AdventureRank::expRequired(25));
  assert(restored.worldLevel() == 2);
  assert(restored.restore(60, 100));
  assert(restored.exp() == 0);
  assert(!restored.restore(0, 0));

  // 封顶 60 后不再升级。
  AdventureRank maxed;
  maxed.addExp(10000000);
  assert(maxed.rank() == 60);
  assert(maxed.exp() == 0);
  assert(maxed.addExp(5000) == 0);

  return 0;
}
