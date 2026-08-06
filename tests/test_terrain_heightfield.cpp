#include "native/engine/world/terrain_heightfield.h"

#include <cassert>
#include <cmath>

int main() {
  TerrainHeightfield terrain;

  // 确定性：同坐标多次查询结果一致。
  assert(terrain.heightAt(0.3f, 0.7f) == terrain.heightAt(0.3f, 0.7f));
  assert(terrain.slopeAt(0.2f, 0.4f) == terrain.slopeAt(0.2f, 0.4f));

  // 有限性：全域采样结果有限。
  for (int i = 0; i <= 20; ++i) {
    for (int j = 0; j <= 20; ++j) {
      const float x = static_cast<float>(i) / 20.0f;
      const float y = static_cast<float>(j) / 20.0f;
      assert(std::isfinite(terrain.heightAt(x, y)));
      assert(std::isfinite(terrain.slopeAt(x, y)));
      assert(terrain.slopeAt(x, y) >= 0.0f);
    }
  }

  // 非法输入不产生 NaN。
  assert(std::isfinite(terrain.heightAt(std::nanf(""), 0.5f)));
  assert(std::isfinite(terrain.heightAt(0.5f, std::nanf(""))));
  assert(std::isfinite(terrain.slopeAt(-1.0f, 2.0f)));

  // 出生点 (0.5, 0.5)：主正弦在整数倍周期处为 0，接近平整。
  assert(std::abs(terrain.heightAt(0.5f, 0.5f)) < terrain.config().detailAmplitude + 0.001f);

  // 存在水域：全域最低点低于水面。
  bool hasWater = false;
  bool hasClimbable = false;
  bool hasDryLand = false;
  for (int i = 0; i <= 40; ++i) {
    for (int j = 0; j <= 40; ++j) {
      const float x = static_cast<float>(i) / 40.0f;
      const float y = static_cast<float>(j) / 40.0f;
      if (terrain.waterAt(x, y)) hasWater = true;
      if (terrain.climbableAt(x, y)) hasClimbable = true;
      if (!terrain.waterAt(x, y) && terrain.heightAt(x, y) > 0.01f) {
        hasDryLand = true;
      }
    }
  }
  assert(hasWater);
  assert(hasClimbable);
  assert(hasDryLand);

  // 水域判定与高度一致：水面以下即水域。
  const float probeX = 0.25f;
  const float probeY = 0.75f;
  const float probeHeight = terrain.heightAt(probeX, probeY);
  assert(terrain.waterAt(probeX, probeY) ==
         (probeHeight < terrain.config().waterLevel));

  // 坡度连续性：相邻采样点坡度差不应爆炸（有限差分步长内平滑）。
  const float slopeA = terrain.slopeAt(0.3f, 0.3f);
  const float slopeB = terrain.slopeAt(0.304f, 0.3f);
  assert(std::abs(slopeA - slopeB) < 5.0f);

  // 边缘山脊环：世界角落被掩码抬升出山体，高度显著高于中心玩法区。
  const float cornerHeight = terrain.heightAt(0.0f, 0.0f);
  assert(cornerHeight > terrain.config().edgeMountainHeight * 0.5f);
  // 掩码内圈（中心玩法区）不受山体影响：仅含正弦项。
  TerrainHeightfield noMountains{[] {
    TerrainConfig config;
    config.edgeMountainHeight = 0.0f;
    return config;
  }()};
  assert(std::abs(terrain.heightAt(0.5f, 0.3f) -
                  noMountains.heightAt(0.5f, 0.3f)) < 1e-6f);

  // 非法步长配置被规范化。
  TerrainHeightfield guarded{TerrainConfig{0.035f, 3.0f, 0.008f, 9.0f,
                                            -0.012f, 0.55f, -1.0f}};
  assert(guarded.config().slopeSampleStep > 0.0f);
  return 0;
}
