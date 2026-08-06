#pragma once

#include <cstdint>
#include <vector>

// 抽卡系统（阶段三）：
// 确定性 LCG 随机 + 硬保底。给定初始状态与抽取次数，
// 结果序列完全可重现，便于回归测试验证概率与保底逻辑。
// 单机样板：卡池为内置角色图鉴，不接入服务器与支付。
struct GachaConfig {
  // 五星概率 0.6%，四星概率 5.1%，其余落入四星普通池。
  float p5 = 0.006f;
  float p4 = 0.051f;
  // 硬保底：90 抽必出五星，10 抽必出四星及以上。
  int32_t pity5At = 90;
  int32_t pity4At = 10;
};

struct GachaState {
  uint32_t seed = 0x5EED1234u;
  // 距上次五星/四星以来的抽取数。
  int32_t since5 = 0;
  int32_t since4 = 0;
};

struct GachaPull {
  int32_t characterId = 0;
  int32_t rarity = 4;
  // 是否触发五星保底（供 UI 展示与测试断言）。
  bool pity5 = false;
};

class GachaSystem {
 public:
  explicit GachaSystem(GachaConfig config = {});

  // 抽取 count 次并推进状态（含保底计数）。
  std::vector<GachaPull> draw(GachaState& state, int32_t count) const;

  // 武器卡池（抽卡优化）：五星概率略高于角色池，与角色池共享保底计数
  // （单机样板简化）；返回的 GachaPull::characterId 为武器 id。
  std::vector<GachaPull> drawWeapon(GachaState& state, int32_t count) const;

  const GachaConfig& config() const { return config_; }

 private:
  GachaConfig config_;
};
