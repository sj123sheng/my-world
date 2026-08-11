#include "native/engine/render/terrain_material.h"

#include <algorithm>
#include <cmath>

namespace {

float finiteClamp01(float value) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

TerrainMaterialWeights normalized(TerrainMaterialWeights value) {
  value.grass = finiteClamp01(value.grass);
  value.soil = finiteClamp01(value.soil);
  value.rock = finiteClamp01(value.rock);
  value.path = finiteClamp01(value.path);
  const float sum = value.grass + value.soil + value.rock + value.path;
  if (!(sum > 1e-6f)) return {0.45f, 0.35f, 0.20f, 0.0f};
  value.grass /= sum;
  value.soil /= sum;
  value.rock /= sum;
  value.path /= sum;
  return value;
}

}  // namespace

TerrainMaterialWeights TerrainMaterialWeightsFor(
    float height, float slope, float shoreDistance, float pathMask,
    const glm::vec4& paintedControl, float paintedStrength) {
  height = std::isfinite(height) ? height : 0.0f;
  slope = std::isfinite(slope) ? std::max(0.0f, slope) : 0.0f;
  shoreDistance = std::isfinite(shoreDistance) ? std::max(0.0f, shoreDistance)
                                                : 0.0f;
  pathMask = finiteClamp01(pathMask);
  paintedStrength = finiteClamp01(paintedStrength);

  const float rock = finiteClamp01((slope - 0.30f) / 0.42f +
                                    std::max(0.0f, height - 0.06f) * 4.0f);
  const float dampSoil = 1.0f - finiteClamp01(shoreDistance / 0.025f);
  TerrainMaterialWeights fallback{
      (1.0f - rock) * (0.72f - dampSoil * 0.28f),
      (1.0f - rock) * (0.28f + dampSoil * 0.72f), rock, 0.0f};
  fallback = normalized(fallback);

  const TerrainMaterialWeights painted = normalized(
      {paintedControl.x, paintedControl.y, paintedControl.z, paintedControl.w});
  TerrainMaterialWeights mixed{
      fallback.grass * (1.0f - paintedStrength) + painted.grass * paintedStrength,
      fallback.soil * (1.0f - paintedStrength) + painted.soil * paintedStrength,
      fallback.rock * (1.0f - paintedStrength) + painted.rock * paintedStrength,
      fallback.path * (1.0f - paintedStrength) + painted.path * paintedStrength};

  // 道路属于玩法引导信息，路径掩码在手绘控制图之上拥有明确优先级。
  const float pathOverride = pathMask * 0.86f;
  mixed.grass *= 1.0f - pathOverride;
  mixed.soil *= 1.0f - pathOverride;
  mixed.rock *= 1.0f - pathOverride;
  mixed.path = std::max(mixed.path, pathOverride);
  return normalized(mixed);
}
