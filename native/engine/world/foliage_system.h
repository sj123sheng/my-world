#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

enum class FoliageKind : int32_t { Grass = 0, Shrub = 1, Tree = 2, Flower = 3, Rock = 4 };

struct FoliageLayer {
  FoliageKind kind = FoliageKind::Grass;
  std::string_view assetId = "foliage_grass_cross";
  float density = 100.0f;
  float minScale = 0.8f;
  float maxScale = 1.2f;
  float minHeight = -0.2f;
  float maxHeight = 0.2f;
  float maxSlope = 0.5f;
  float waterClearance = 0.008f;
  float routeClearance = 0.015f;
  bool castsShadow = false;
};

struct FoliageScatterRegion {
  int32_t blockId = 0;
  glm::vec4 rect{0.0f, 0.0f, 1.0f, 1.0f};
  uint32_t seed = 1u;
};

struct FoliageExclusion {
  glm::vec2 center{0.0f, 0.0f};
  float radius = 0.0f;
};

struct FoliageInstance {
  glm::vec3 position{0.0f};
  float yaw = 0.0f;
  float scale = 1.0f;
  FoliageKind kind = FoliageKind::Grass;
  bool castsShadow = false;

  bool operator==(const FoliageInstance& other) const {
    return position == other.position && yaw == other.yaw &&
           scale == other.scale && kind == other.kind &&
           castsShadow == other.castsShadow;
  }
};

using TerrainSampleFn = std::function<float(float, float)>;
using TerrainMaskFn = std::function<bool(float, float)>;

std::vector<FoliageInstance> ScatterFoliage(
    const FoliageLayer& layer, const FoliageScatterRegion& region,
    const TerrainSampleFn& heightAt, const TerrainSampleFn& slopeAt,
    const TerrainMaskFn& waterAt, const TerrainSampleFn& routeDistance,
    const std::vector<FoliageExclusion>& exclusions);
