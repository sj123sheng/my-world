#pragma once

#include <cstdint>
#include <vector>

#include "../../engine/world/terrain_heightfield.h"

// 世界布局生成数据 → 引擎高度场桥接（原神式手工地貌数据管线）：
// world.json terrainFeatures 经构建期代码生成为 WorldLayout::kTerrainFeatures，
// 这里转换为引擎 TerrainFeature 列表；顺序 = 数据顺序 = 地貌合成顺序。

// 开放世界地形：默认 TerrainConfig + 全部生成特征。
TerrainHeightfield makeWorldTerrain();

// 主干道路径段（世界 [0,1] 坐标，供渲染层在地形上压出路径色）。
struct WorldRouteSegment {
  float fromX = 0.0f;
  float fromY = 0.0f;
  float toX = 0.0f;
  float toY = 0.0f;
};

std::vector<WorldRouteSegment> worldRouteSegments();
