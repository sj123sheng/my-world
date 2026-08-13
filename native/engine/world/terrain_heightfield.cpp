#include "native/engine/world/terrain_heightfield.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kCoreTransitionWidth = 0.08;

float smoothstep01(float edge, float value) {
  if (edge <= 0.0f) return value >= edge ? 1.0f : 0.0f;
  const float t = std::clamp(value / edge, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// 先按波长把世界坐标约减到有限区间，再送入三角函数。这样不会把
// 1e18 乃至更大的世界坐标直接乘频率，超远正负坐标仍有限且确定。
double periodicAngle(double coordinate, double frequency,
                     double phase = 0.0) {
  if (!std::isfinite(coordinate) || !std::isfinite(frequency) ||
      !std::isfinite(phase) || frequency == 0.0) {
    return phase;
  }
  const double wavelength = 1.0 / std::abs(frequency);
  if (!std::isfinite(wavelength) || wavelength == 0.0) return phase;
  const double reduced = std::remainder(coordinate, wavelength);
  return kTwoPi * frequency * reduced + phase;
}

float continuousBaseHeight(const TerrainConfig& config, double worldX,
                           double worldY) {
  const double primary =
      static_cast<double>(config.amplitude) *
      std::sin(periodicAngle(worldX, config.frequency)) *
      std::sin(periodicAngle(worldY, config.frequency));
  const double detail =
      static_cast<double>(config.detailAmplitude) *
      std::sin(periodicAngle(worldX, config.detailFrequency, 1.3)) *
      std::cos(periodicAngle(worldY, config.detailFrequency, 0.7));
  const double ridge =
      static_cast<double>(config.ridgeAmplitude) *
      std::sin(periodicAngle(worldX, config.ridgeFrequency, 2.1)) *
      std::sin(periodicAngle(worldY, config.ridgeFrequency * 0.8, 0.4));
  const double height = primary + detail + ridge;
  return std::isfinite(height) ? static_cast<float>(height) : 0.0f;
}

float coreContributionWeight(double worldX, double worldY) {
  if (worldX < 0.0 || worldX > 1.0 || worldY < 0.0 || worldY > 1.0) {
    return 0.0f;
  }
  const double edgeDistance =
      std::min(std::min(worldX, 1.0 - worldX),
               std::min(worldY, 1.0 - worldY));
  const double t =
      std::clamp(edgeDistance / kCoreTransitionWidth, 0.0, 1.0);
  return static_cast<float>(t * t * (3.0 - 2.0 * t));
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

float TerrainHeightfield::heightAt(double worldX, double worldY) const {
  if (!std::isfinite(worldX)) worldX = 0.0;
  if (!std::isfinite(worldY)) worldY = 0.0;
  const float base = continuousBaseHeight(config_, worldX, worldY);
  const float coreWeight = coreContributionWeight(worldX, worldY);
  if (coreWeight <= 0.0f || features_.empty()) return base;

  const float x = static_cast<float>(worldX);
  const float y = static_cast<float>(worldY);
  float sculpted = base;
  // 地形特征层（原神式手工地貌）：按数据顺序依次叠加，
  // 湖盆/整平区先把基础层拉向目标高度，丘/台地/脊线再叠加骨架。
  for (const TerrainFeature& feature : features_) {
    const float mask = featureMask(feature, x, y);
    if (mask <= 0.0f) continue;
    switch (feature.kind) {
      case TerrainFeatureKind::Hill:
        sculpted += mask * feature.amplitude;
        break;
      case TerrainFeatureKind::Basin:
        sculpted += mask * (feature.targetHeight - sculpted);
        break;
      case TerrainFeatureKind::Terrace:
        sculpted += mask * std::max(0.0f, feature.targetHeight - sculpted);
        break;
      case TerrainFeatureKind::Ridge: {
        const float cosAngle = std::cos(feature.angleRadians);
        const float sinAngle = std::sin(feature.angleRadians);
        const float rotatedU = x * cosAngle + y * sinAngle;
        const float rotatedV = -x * sinAngle + y * cosAngle;
        const float ridgeValue =
            std::sin(static_cast<float>(kTwoPi) * feature.frequency * rotatedU +
                     2.1f) *
            std::sin(static_cast<float>(kTwoPi) * feature.frequency * 0.7f *
                         rotatedV +
                     0.4f);
        sculpted += mask * feature.amplitude * ridgeValue;
        break;
      }
    }
  }
  return base + coreWeight * (sculpted - base);
}

float TerrainHeightfield::heightAt(ChunkCoord chunk, float localX,
                                   float localY) const {
  const double worldX = static_cast<double>(chunk.x) + localX;
  const double worldY = static_cast<double>(chunk.y) + localY;
  return heightAt(worldX, worldY);
}

float TerrainHeightfield::slopeAt(double worldX, double worldY) const {
  const double step = config_.slopeSampleStep;
  const float hx = heightAt(worldX + step, worldY) -
                   heightAt(worldX - step, worldY);
  const float hy = heightAt(worldX, worldY + step) -
                   heightAt(worldX, worldY - step);
  const float gx = hx / static_cast<float>(2.0 * step);
  const float gy = hy / static_cast<float>(2.0 * step);
  return std::sqrt(gx * gx + gy * gy);
}

float TerrainHeightfield::slopeAt(ChunkCoord chunk, float localX,
                                  float localY) const {
  const double worldX = static_cast<double>(chunk.x) + localX;
  const double worldY = static_cast<double>(chunk.y) + localY;
  return slopeAt(worldX, worldY);
}

bool TerrainHeightfield::climbableAt(double worldX, double worldY) const {
  return slopeAt(worldX, worldY) >= config_.climbSlopeThreshold;
}

bool TerrainHeightfield::waterAt(double worldX, double worldY) const {
  return heightAt(worldX, worldY) < config_.waterLevel;
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
