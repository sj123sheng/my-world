#include "native/engine/world/terrain_heightfield.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kTwoPi = 6.2831853071795864769f;

float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float smoothstep01(float edge, float value) {
  if (edge <= 0.0f) return value >= edge ? 1.0f : 0.0f;
  const float t = std::clamp(value / edge, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

TerrainHeightfield::TerrainHeightfield(TerrainConfig config)
    : config_(config) {
  if (config_.slopeSampleStep <= 0.0f) config_.slopeSampleStep = 0.004f;
}

TerrainHeightfield::TerrainHeightfield(TerrainConfig config,
                                       std::vector<TerrainFeature> features)
    : config_(config), features_(std::move(features)) {
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
  // 主起伏：宽缓双正弦乘积（频率取整数，中心 (0.5, 0.5) 处恰为 0），
  // 幅度刻意压缓，基础层单独叠加永远不会产生可攀爬坡度。
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
  // 边缘山脊环：世界边缘抬升的山体，中心玩法区掩码为 0。山体先于特征层
  // 叠加，湖盆/整平 basin 因此能像手工雕刻一样压过边缘山体（设计镜像同序）。
  const float mountains =
      config_.edgeMountainHeight * edgeMountainMask(config_, x, y);
  float height = primary + detail + ridge + mountains;
  // 地形特征层（原神式手工地貌）：按数据顺序依次叠加，
  // 湖盆/整平区先把基础层拉向目标高度，丘/台地/脊线再叠加骨架。
  for (const TerrainFeature& feature : features_) {
    const float mask = featureMask(feature, x, y);
    if (mask <= 0.0f) continue;
    switch (feature.kind) {
      case TerrainFeatureKind::Hill:
        height += mask * feature.amplitude;
        break;
      case TerrainFeatureKind::Basin:
        height += mask * (feature.targetHeight - height);
        break;
      case TerrainFeatureKind::Terrace:
        height += mask * std::max(0.0f, feature.targetHeight - height);
        break;
      case TerrainFeatureKind::Ridge: {
        const float cosAngle = std::cos(feature.angleRadians);
        const float sinAngle = std::sin(feature.angleRadians);
        const float rotatedU = x * cosAngle + y * sinAngle;
        const float rotatedV = -x * sinAngle + y * cosAngle;
        const float ridgeValue =
            std::sin(kTwoPi * feature.frequency * rotatedU + 2.1f) *
            std::sin(kTwoPi * feature.frequency * 0.7f * rotatedV + 0.4f);
        height += mask * feature.amplitude * ridgeValue;
        break;
      }
    }
  }
  return height;
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

// static
float TerrainHeightfield::featureMask(const TerrainFeature& feature, float x,
                                      float y) {
  if (!std::isfinite(x) || !std::isfinite(y)) return 0.0f;
  if (!std::isfinite(feature.x) || !std::isfinite(feature.y)) return 0.0f;
  if (!(feature.radiusX > 0.0f) || !(feature.radiusY > 0.0f)) return 0.0f;
  const float dx = (x - feature.x) / feature.radiusX;
  const float dy = (y - feature.y) / feature.radiusY;
  const float u = std::sqrt(dx * dx + dy * dy);
  if (u >= 1.0f) return 0.0f;
  const float feather = std::isfinite(feature.feather)
                            ? std::clamp(feature.feather, 0.0f, 1.0f)
                            : 0.0f;
  return smoothstep01(feather, 1.0f - u);
}
