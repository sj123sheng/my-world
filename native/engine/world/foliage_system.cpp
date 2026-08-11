#include "native/engine/world/foliage_system.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace {

uint32_t nextRandom(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

float unitRandom(uint32_t& state) {
  return static_cast<float>(nextRandom(state) >> 8u) / 16777216.0f;
}

bool excluded(glm::vec2 point, const std::vector<FoliageExclusion>& exclusions) {
  for (const FoliageExclusion& exclusion : exclusions) {
    const glm::vec2 delta = point - exclusion.center;
    if (glm::dot(delta, delta) < exclusion.radius * exclusion.radius) return true;
  }
  return false;
}

}  // namespace

std::vector<FoliageInstance> ScatterFoliage(
    const FoliageLayer& layer, const FoliageScatterRegion& region,
    const TerrainSampleFn& heightAt, const TerrainSampleFn& slopeAt,
    const TerrainMaskFn& waterAt, const TerrainSampleFn& routeDistance,
    const std::vector<FoliageExclusion>& exclusions) {
  std::vector<FoliageInstance> result;
  const float width = std::max(0.0f, region.rect.z - region.rect.x);
  const float depth = std::max(0.0f, region.rect.w - region.rect.y);
  const int target = std::max(0, static_cast<int>(std::floor(
                                     width * depth * std::max(0.0f, layer.density))));
  if (target == 0 || !heightAt || !slopeAt || !waterAt || !routeDistance) {
    return result;
  }
  result.reserve(static_cast<size_t>(target));
  uint32_t state = region.seed ^ (static_cast<uint32_t>(region.blockId) * 2654435761u);
  constexpr float kTwoPi = 6.2831853071795864769f;
  const int attempts = target * 16;
  for (int attempt = 0; attempt < attempts && static_cast<int>(result.size()) < target;
       ++attempt) {
    const float x = region.rect.x + unitRandom(state) * width;
    const float z = region.rect.y + unitRandom(state) * depth;
    const float height = heightAt(x, z);
    const float slope = slopeAt(x, z);
    if (!std::isfinite(height) || !std::isfinite(slope)) continue;
    if (height < layer.minHeight || height > layer.maxHeight ||
        slope > layer.maxSlope || waterAt(x, z) ||
        routeDistance(x, z) < layer.routeClearance ||
        excluded({x, z}, exclusions)) {
      continue;
    }
    const float scaleT = unitRandom(state);
    result.push_back({{x, height, z}, unitRandom(state) * kTwoPi,
                      layer.minScale +
                          (layer.maxScale - layer.minScale) * scaleT,
                      layer.kind, layer.castsShadow});
  }
  return result;
}
