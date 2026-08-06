#include "native/engine/world/terrain_heightfield.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kTwoPi = 6.2831853071795864769f;

float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

}  // namespace

TerrainHeightfield::TerrainHeightfield(TerrainConfig config)
    : config_(config) {
  if (config_.slopeSampleStep <= 0.0f) config_.slopeSampleStep = 0.004f;
}

namespace {

// 边缘山脊环掩码：到世界中心距离 <= inner 时为 0，>= outer 时为 1，
// 中间用 smoothstep 平滑过渡，保证山体坡度连续、无突变接缝。
float edgeMountainMask(const TerrainConfig& config, float x, float y) {
  const float dx = x - 0.5f;
  const float dy = y - 0.5f;
  const float distance = std::sqrt(dx * dx + dy * dy);
  const float span = config.edgeMountainOuterRadius - config.edgeMountainInnerRadius;
  if (span <= 0.0f) {
    return distance >= config.edgeMountainOuterRadius ? 1.0f : 0.0f;
  }
  const float t = std::clamp(
      (distance - config.edgeMountainInnerRadius) / span, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

float TerrainHeightfield::heightAt(float x, float y) const {
  if (!std::isfinite(x)) x = 0.0f;
  if (!std::isfinite(y)) y = 0.0f;
  x = clamp01(x);
  y = clamp01(y);
  // 主起伏：双正弦乘积形成规则的小丘与沟谷，
  // 中心区域（0.5, 0.5）附近高度为 0，保证出生点地面平整。
  const float primary =
      config_.amplitude * std::sin(kTwoPi * config_.frequency * x) *
      std::sin(kTwoPi * config_.frequency * y);
  // 次级褶皱：相位错开避免与主起伏同向叠加出过陡坡度。
  const float detail =
      config_.detailAmplitude *
      std::sin(kTwoPi * config_.detailFrequency * x + 1.3f) *
      std::cos(kTwoPi * config_.detailFrequency * y + 0.7f);
  // 山脊 octave：不同方向频率混合，打破双正弦的网格感。
  const float ridge =
      config_.ridgeAmplitude *
      std::sin(kTwoPi * config_.ridgeFrequency * x + 2.1f) *
      std::sin(kTwoPi * config_.ridgeFrequency * y * 0.8f + 0.4f);
  // 边缘山脊环：世界边缘抬升的山体，中心玩法区掩码为 0。
  const float mountains =
      config_.edgeMountainHeight * edgeMountainMask(config_, x, y);
  return primary + detail + ridge + mountains;
}

float TerrainHeightfield::slopeAt(float x, float y) const {
  const float step = config_.slopeSampleStep;
  const float hx =
      heightAt(std::min(x + step, 1.0f), y) - heightAt(std::max(x - step, 0.0f), y);
  const float hy =
      heightAt(x, std::min(y + step, 1.0f)) - heightAt(x, std::max(y - step, 0.0f));
  const float dx = std::min(x + step, 1.0f) - std::max(x - step, 0.0f);
  const float dy = std::min(y + step, 1.0f) - std::max(y - step, 0.0f);
  const float gx = dx > 0.0f ? hx / dx : 0.0f;
  const float gy = dy > 0.0f ? hy / dy : 0.0f;
  return std::sqrt(gx * gx + gy * gy);
}

bool TerrainHeightfield::climbableAt(float x, float y) const {
  return slopeAt(x, y) >= config_.climbSlopeThreshold;
}

bool TerrainHeightfield::waterAt(float x, float y) const {
  return heightAt(x, y) < config_.waterLevel;
}
