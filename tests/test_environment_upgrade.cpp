#include "native/engine/render/environment_quality.h"
#include "native/engine/render/terrain_material.h"
#include "native/engine/world/foliage_system.h"
#include "native/engine/world/water_body.h"
#include "native/engine/world/weather_system.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

bool finiteWeights(const TerrainMaterialWeights& weights) {
  return std::isfinite(weights.grass) && std::isfinite(weights.soil) &&
         std::isfinite(weights.rock) && std::isfinite(weights.path);
}

float weightSum(const TerrainMaterialWeights& weights) {
  return weights.grass + weights.soil + weights.rock + weights.path;
}

void testQualityProfileDegradesEnvironmentAsOneUnit() {
  const EnvironmentQualityProfile full = EnvironmentQualityProfileFor(0);
  const EnvironmentQualityProfile medium = EnvironmentQualityProfileFor(2);
  const EnvironmentQualityProfile critical = EnvironmentQualityProfileFor(4);

  assert(full.shadowMapSize == 1024);
  assert(full.shadowDistance > medium.shadowDistance);
  assert(full.foliageDensityScale > medium.foliageDensityScale);
  assert(medium.shadowMapSize == 768);
  assert(!critical.dynamicShadows);
  assert(critical.shadowMapSize == 0);
  assert(!critical.proceduralClouds);
  assert(!critical.terrainDetailNormals);
  assert(critical.foliageDensityScale > 0.0f);
  assert(EnvironmentQualityProfileFor(-10).shadowMapSize == 1024);
  assert(EnvironmentQualityProfileFor(99).shadowMapSize == 0);
  assert(EffectiveEnvironmentQualityLevel(0, false) == 0);
  assert(EffectiveEnvironmentQualityLevel(1, true) == 3);
  assert(EffectiveEnvironmentQualityLevel(4, true) == 4);
  assert((VisualTerrainLodFallbackOrder(0) == std::array<int, 3>{0, 1, 2}));
  assert((VisualTerrainLodFallbackOrder(1) == std::array<int, 3>{1, 2, 0}));
  assert((VisualTerrainLodFallbackOrder(2) == std::array<int, 3>{2, 1, 0}));
}

void testTerrainWeightsRespectPaintedControlAndPhysicalFallback() {
  const TerrainMaterialWeights painted = TerrainMaterialWeightsFor(
      0.02f, 0.08f, 0.03f, 0.0f, {0.9f, 0.05f, 0.03f, 0.02f}, 0.85f);
  assert(finiteWeights(painted));
  assert(std::abs(weightSum(painted) - 1.0f) < 1e-5f);
  assert(painted.grass > 0.7f);

  const TerrainMaterialWeights cliff = TerrainMaterialWeightsFor(
      0.08f, 0.92f, 0.08f, 0.0f, {0.25f, 0.25f, 0.25f, 0.25f}, 0.0f);
  assert(cliff.rock > cliff.grass);
  assert(cliff.rock > cliff.soil);

  const TerrainMaterialWeights route = TerrainMaterialWeightsFor(
      0.01f, 0.05f, 0.02f, 1.0f, {1.0f, 0.0f, 0.0f, 0.0f}, 0.5f);
  assert(route.path > 0.7f);

  const TerrainMaterialWeights guarded = TerrainMaterialWeightsFor(
      std::nanf(""), std::nanf(""), std::nanf(""), std::nanf(""),
      {std::nanf(""), -1.0f, 2.0f, 0.0f}, std::nanf(""));
  assert(finiteWeights(guarded));
  assert(std::abs(weightSum(guarded) - 1.0f) < 1e-5f);
}

void testEnvironmentStateDrivesSkyFogWindAndPrecipitation() {
  const EnvironmentState noon = WeatherSystem::environmentAt(0.0f, 12.0f);
  const EnvironmentState midnight = WeatherSystem::environmentAt(0.0f, 0.0f);
  const EnvironmentState rain = WeatherSystem::environmentAt(120.0f, 12.0f);
  const EnvironmentState snow = WeatherSystem::environmentAt(300.0f, 12.0f);

  assert(noon.weatherId == WeatherSystem::kWeatherClear);
  assert(noon.daylight > midnight.daylight);
  assert(noon.skyTop.x > midnight.skyTop.x);
  assert(rain.weatherId == WeatherSystem::kWeatherRain);
  assert(rain.cloudCoverage > noon.cloudCoverage);
  assert(rain.fogDensity > noon.fogDensity);
  assert(rain.precipitation == PrecipitationKind::Rain);
  assert(rain.precipitationIntensity > 0.9f);
  assert(rain.waterRoughness > noon.waterRoughness);
  assert(snow.precipitation == PrecipitationKind::Snow);
  assert(snow.windStrength > 0.0f);
}

void testLocalWaterBodyRejectsWorldOutsideLake() {
  WaterBody lake;
  lake.center = {0.745f, 0.265f};
  lake.halfExtents = {0.07f, 0.055f};
  lake.level = -0.045f;
  lake.shoreWidth = 0.012f;

  assert(lake.contains({0.745f, 0.265f}));
  assert(lake.contains({0.80f, 0.265f}));
  assert(!lake.contains({0.50f, 0.50f}));
  assert(lake.shoreFactor({0.745f, 0.265f}) == 0.0f);
  assert(lake.shoreFactor({0.812f, 0.265f}) > 0.0f);
  assert(lake.shoreFactor({0.812f, 0.265f}) < 1.0f);
}

void testFoliageScatterIsDeterministicAndHonorsExclusions() {
  FoliageLayer layer;
  layer.kind = FoliageKind::Grass;
  layer.density = 220.0f;
  layer.minScale = 0.8f;
  layer.maxScale = 1.25f;
  layer.maxSlope = 0.45f;
  layer.waterClearance = 0.008f;
  layer.routeClearance = 0.018f;

  FoliageScatterRegion region;
  region.blockId = 4;
  region.rect = {0.50f, 0.00f, 0.625f, 0.125f};
  region.seed = 7331u;

  const auto heightAt = [](float, float) { return 0.02f; };
  const auto slopeAt = [](float x, float) { return x > 0.60f ? 0.8f : 0.1f; };
  const auto waterAt = [](float x, float y) {
    return x > 0.56f && x < 0.58f && y > 0.07f;
  };
  const auto routeDistance = [](float x, float) { return std::abs(x - 0.54f); };
  const std::vector<FoliageExclusion> exclusions{{{0.59f, 0.04f}, 0.018f}};

  const std::vector<FoliageInstance> first = ScatterFoliage(
      layer, region, heightAt, slopeAt, waterAt, routeDistance, exclusions);
  const std::vector<FoliageInstance> second = ScatterFoliage(
      layer, region, heightAt, slopeAt, waterAt, routeDistance, exclusions);

  assert(!first.empty());
  assert(first == second);
  for (const FoliageInstance& instance : first) {
    assert(instance.position.x <= 0.60f);
    assert(!waterAt(instance.position.x, instance.position.z));
    assert(routeDistance(instance.position.x, instance.position.z) >=
           layer.routeClearance);
    const float dx = instance.position.x - exclusions[0].center.x;
    const float dy = instance.position.z - exclusions[0].center.y;
    assert(std::sqrt(dx * dx + dy * dy) >= exclusions[0].radius);
    assert(instance.scale >= layer.minScale && instance.scale <= layer.maxScale);
  }
}

}  // namespace

int main() {
  testQualityProfileDegradesEnvironmentAsOneUnit();
  testTerrainWeightsRespectPaintedControlAndPhysicalFallback();
  testEnvironmentStateDrivesSkyFogWindAndPrecipitation();
  testLocalWaterBodyRejectsWorldOutsideLake();
  testFoliageScatterIsDeterministicAndHonorsExclusions();
  return 0;
}
