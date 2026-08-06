#pragma once

#include <cstdint>

// 确定性程序化地形高度场（开放世界探索基础）。
// 以固定正弦组合生成世界 [0,1]x[0,1] 上的地面高度，保证：
// 1. 同坐标查询永远返回同一高度（无随机状态），可被确定性测试覆盖；
// 2. 逻辑层据此做地面贴合、攀爬坡度判定与水面区域判定；
// 3. 渲染层采样同一函数生成地形网格，逻辑与视觉严格一致。
// 地貌构成：主起伏 + 次级褶皱 + 山脊 octave 叠加出缓坡、小丘与沟谷；
// 世界边缘再叠加平滑掩码抬升的山脊环，充当天际线远景并遮挡世界边界，
// 掩码在世界中心（出生点与战斗区）附近为 0，不干扰核心玩法区。
// 逻辑坐标 (x, y) 对应 3D 世界 (x, height, z=y)。
struct TerrainConfig {
  // 基础起伏幅度与频率；组合出缓坡与小丘。
  float amplitude = 0.06f;
  float frequency = 3.0f;
  // 次级褶皱：给地形增加细节但不显著抬升坡度。
  float detailAmplitude = 0.008f;
  float detailFrequency = 9.0f;
  // 水面高度：地面高度低于该值的区域视为水域（游泳）。
  float waterLevel = -0.012f;
  // 可行走最大坡度（高度变化/水平距离）：超过即视为可攀爬面。
  float climbSlopeThreshold = 0.55f;
  // 有限差分步长：用于坡度估计。
  float slopeSampleStep = 0.004f;
  // 山脊 octave：相位错开的中频褶皱，进一步丰富地貌层次。
  float ridgeAmplitude = 0.02f;
  float ridgeFrequency = 5.0f;
  // 边缘山脊环：按到世界中心距离的 smoothstep 掩码在世界边缘抬升的山体高度。
  float edgeMountainHeight = 0.13f;
  // 掩码内圈半径：该范围内山体贡献为 0（保护出生点与中心玩法区）。
  float edgeMountainInnerRadius = 0.42f;
  // 掩码外圈半径：达到该距离后山体完全生效。
  float edgeMountainOuterRadius = 0.72f;
};

struct Vec2;

class TerrainHeightfield {
 public:
  explicit TerrainHeightfield(TerrainConfig config = {});

  // 地面高度查询：对任意有限输入返回有限结果；
  // 越界坐标按世界边界钳制采样。
  float heightAt(float x, float y) const;
  // 坡度估计：|∇h| 的近似模长（高度变化率），>=0。
  float slopeAt(float x, float y) const;
  // 是否可攀爬面：坡度超过阈值。
  bool climbableAt(float x, float y) const;
  // 是否水域：地面高度低于水面。
  bool waterAt(float x, float y) const;

  const TerrainConfig& config() const { return config_; }

 private:
  TerrainConfig config_;
};
