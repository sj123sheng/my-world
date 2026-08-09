#include "native/gameplay/world/world_terrain.h"

#include "native/generated/world_layout.gen.h"

namespace WL = WorldLayout;

namespace {

// 生成头 kind 整数值 → 引擎枚举（数值契约：0=Hill / 1=Basin /
// 2=Terrace / 3=Ridge，与生成器 terrainFeatureKindValue 一致）。
TerrainFeatureKind featureKindFor(int32_t kind) {
  switch (kind) {
    case 1: return TerrainFeatureKind::Basin;
    case 2: return TerrainFeatureKind::Terrace;
    case 3: return TerrainFeatureKind::Ridge;
    default: return TerrainFeatureKind::Hill;
  }
}

}  // namespace

TerrainHeightfield makeWorldTerrain() {
  std::vector<TerrainFeature> features;
  features.reserve(WL::kTerrainFeatureCount);
  for (const WL::WorldTerrainFeatureDef& def : WL::kTerrainFeatures) {
    TerrainFeature feature;
    feature.kind = featureKindFor(def.kind);
    feature.x = def.x;
    feature.y = def.y;
    feature.radiusX = def.radiusX;
    feature.radiusY = def.radiusY;
    feature.amplitude = def.amplitude;
    feature.targetHeight = def.targetHeight;
    feature.frequency = def.frequency;
    feature.angleRadians = def.angleRadians;
    feature.feather = def.feather;
    features.push_back(feature);
  }
  return TerrainHeightfield(TerrainConfig{}, std::move(features));
}

std::vector<WorldRouteSegment> worldRouteSegments() {
  std::vector<WorldRouteSegment> segments;
  segments.reserve(WL::kRouteCount);
  for (const WL::WorldRouteDef& def : WL::kRoutes) {
    segments.push_back({def.fromX, def.fromY, def.toX, def.toY});
  }
  return segments;
}
