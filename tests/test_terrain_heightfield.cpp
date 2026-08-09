// test_terrain_heightfield.cpp: 地形高度场回归测试。
//
// 覆盖两层：
// 1. 基础层（无特征）：确定性、有限性、非法输入、中心平整、边缘山脊环；
// 2. 特征层（makeWorldTerrain 全量世界地貌）：湖盆水域、mesa 攀爬崖、
//    全部内容点位干地且可行走、出生点缓坡、设计镜像关键高度对照。

#include "native/engine/world/terrain_heightfield.h"
#include "native/gameplay/world/world_terrain.h"
#include "native/generated/world_layout.gen.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace WL = WorldLayout;

namespace {

constexpr float kWalkableSlope = 0.55f;

// 内容点位坡度/干地断言的豁免名单：这两个点位按设计必须在水中
// （湖畔浮桥机关与湖心浮桥，Swimming 玩法）。
bool isIntendedWaterPoint(float x, float y) {
  for (const WL::WorldPuzzleNodeDef& puzzle : WL::kPuzzleNodes) {
    if (puzzle.id == 71 && std::abs(puzzle.x - x) < 1e-6f &&
        std::abs(puzzle.y - y) < 1e-6f) {
      return true;
    }
  }
  for (const WL::WorldTraversalGateDef& gate : WL::kTraversalGates) {
    if (gate.id == 81 && std::abs(gate.x - x) < 1e-6f &&
        std::abs(gate.y - y) < 1e-6f) {
      return true;
    }
  }
  return false;
}

void checkContentPoint(const TerrainHeightfield& terrain, const char* label,
                       float x, float y) {
  const bool intendedWater = isIntendedWaterPoint(x, y);
  const bool wet = terrain.waterAt(x, y);
  if (!intendedWater) {
    if (wet) {
      std::printf("content point underwater: %s (%.2f, %.2f) h=%.4f\n", label,
                  x, y, terrain.heightAt(x, y));
    }
    assert(!wet);
  }
  // 水中点位（浮桥/游泳机关）不做坡度可行走断言：玩家在该点游泳。
  if (intendedWater) return;
  const float slope = terrain.slopeAt(x, y);
  if (slope >= kWalkableSlope) {
    std::printf("content point not walkable: %s (%.2f, %.2f) slope=%.3f\n",
                label, x, y, slope);
  }
  assert(slope < kWalkableSlope);
}

}  // namespace

int main() {
  // ---- 基础层（无特征）----
  TerrainHeightfield base;

  // 确定性：同坐标多次查询结果一致。
  assert(base.heightAt(0.3f, 0.7f) == base.heightAt(0.3f, 0.7f));
  assert(base.slopeAt(0.2f, 0.4f) == base.slopeAt(0.2f, 0.4f));

  // 有限性：全域采样结果有限。
  for (int i = 0; i <= 20; ++i) {
    for (int j = 0; j <= 20; ++j) {
      const float x = static_cast<float>(i) / 20.0f;
      const float y = static_cast<float>(j) / 20.0f;
      assert(std::isfinite(base.heightAt(x, y)));
      assert(std::isfinite(base.slopeAt(x, y)));
      assert(base.slopeAt(x, y) >= 0.0f);
    }
  }

  // 非法输入不产生 NaN。
  assert(std::isfinite(base.heightAt(std::nanf(""), 0.5f)));
  assert(std::isfinite(base.heightAt(0.5f, std::nanf(""))));
  assert(std::isfinite(base.slopeAt(-1.0f, 2.0f)));

  // 基础八度压缓不变量：山体掩码内圈（中心玩法区）无特征时坡度
  // 全部低于攀爬阈值，可攀爬面只能来自有意布置的特征层；水面压低
  // 后基础层全域不再随处积水。
  for (int i = 0; i <= 40; ++i) {
    for (int j = 0; j <= 40; ++j) {
      const float x = static_cast<float>(i) / 40.0f;
      const float y = static_cast<float>(j) / 40.0f;
      const float dx = x - 0.5f;
      const float dy = y - 0.5f;
      if (std::sqrt(dx * dx + dy * dy) <
          base.config().edgeMountainInnerRadius) {
        assert(base.slopeAt(x, y) < kWalkableSlope);
      }
      assert(!base.waterAt(x, y));
    }
  }

  // 世界中心 (0.5, 0.5)：主正弦整数周期过零，接近平整。
  assert(std::abs(base.heightAt(0.5f, 0.5f)) <
         base.config().detailAmplitude + 0.001f);

  // 边缘山脊环：角落抬升；内圈玩法区不受山体影响。
  assert(base.heightAt(0.0f, 0.0f) > base.config().edgeMountainHeight * 0.5f);
  TerrainHeightfield noMountains{[] {
    TerrainConfig config;
    config.edgeMountainHeight = 0.0f;
    return config;
  }()};
  assert(std::abs(base.heightAt(0.5f, 0.3f) -
                  noMountains.heightAt(0.5f, 0.3f)) < 1e-6f);

  // 非法步长配置被规范化。
  TerrainHeightfield guarded{TerrainConfig{0.035f, 3.0f, 0.008f, 9.0f,
                                            -0.012f, 0.55f, -1.0f}};
  assert(guarded.config().slopeSampleStep > 0.0f);

  // ---- 特征掩码纯函数 ----
  TerrainFeature hill;
  hill.kind = TerrainFeatureKind::Hill;
  hill.x = 0.5f;
  hill.y = 0.5f;
  hill.radiusX = 0.1f;
  hill.radiusY = 0.1f;
  hill.feather = 0.5f;
  assert(TerrainHeightfield::featureMask(hill, 0.5f, 0.5f) == 1.0f);
  assert(TerrainHeightfield::featureMask(hill, 0.61f, 0.5f) == 0.0f);
  const float midMask = TerrainHeightfield::featureMask(hill, 0.575f, 0.5f);
  assert(midMask > 0.0f && midMask < 1.0f);
  // 退化特征不产生掩码。
  TerrainFeature degenerate = hill;
  degenerate.radiusX = 0.0f;
  assert(TerrainHeightfield::featureMask(degenerate, 0.5f, 0.5f) == 0.0f);
  assert(TerrainHeightfield::featureMask(hill, std::nanf(""), 0.5f) == 0.0f);

  // ---- 特征层：全量世界地貌 ----
  const TerrainHeightfield terrain = makeWorldTerrain();
  assert(terrain.features().size() == WL::kTerrainFeatureCount);
  assert(WL::kTerrainFeatureCount > 0);

  // 确定性：两次构造结果逐点一致。
  const TerrainHeightfield terrainAgain = makeWorldTerrain();
  for (int i = 0; i <= 16; ++i) {
    for (int j = 0; j <= 16; ++j) {
      const float x = static_cast<float>(i) / 16.0f;
      const float y = static_cast<float>(j) / 16.0f;
      assert(terrain.heightAt(x, y) == terrainAgain.heightAt(x, y));
    }
  }

  // 辉光湖：机关与浮桥点位必须在水下，湖心深度留足游泳余量。
  assert(terrain.waterAt(0.74f, 0.25f));
  assert(terrain.waterAt(0.78f, 0.28f));
  assert(terrain.heightAt(0.745f, 0.265f) < -0.06f);
  // 湖岸内容点不被湖水淹没。
  assert(!terrain.waterAt(0.8f, 0.25f));   // 东部施法者出生点
  assert(!terrain.waterAt(0.7f, 0.2f));    // 湖畔渔夫/渡口

  // 湖心残塔 mesa：顶面平整（basin 目标高度），四壁可攀爬。
  assert(std::abs(terrain.heightAt(0.86f, 0.12f) - 0.055f) < 0.004f);
  bool mesaClimbable = false;
  for (int i = 0; i <= 20; ++i) {
    for (int j = 0; j <= 20; ++j) {
      const float x = 0.86f - 0.08f + 0.16f * static_cast<float>(i) / 20.0f;
      const float y = 0.12f - 0.08f + 0.16f * static_cast<float>(j) / 20.0f;
      if (terrain.climbableAt(x, y)) mesaClimbable = true;
    }
  }
  assert(mesaClimbable);

  // 回廊攀爬悬崖：升降机关旁存在可攀爬坡面。
  bool corridorClimbable = false;
  for (int i = 0; i <= 20; ++i) {
    for (int j = 0; j <= 20; ++j) {
      const float x = 0.51f + 0.1f * static_cast<float>(i) / 20.0f;
      const float y = 0.39f + 0.1f * static_cast<float>(j) / 20.0f;
      if (terrain.climbableAt(x, y)) corridorClimbable = true;
    }
  }
  assert(corridorClimbable);

  // 出生点与中心战斗区：平整缓坡。
  assert(terrain.slopeAt(0.5f, 0.12f) < 0.3f);
  assert(std::abs(terrain.heightAt(0.5f, 0.5f)) < 0.009f);
  assert(terrain.slopeAt(0.5f, 0.48f) < kWalkableSlope);

  // 圣所高地整体高于低地：高原台地地貌成立。
  assert(terrain.heightAt(0.8f, 0.8f) > terrain.heightAt(0.15f, 0.15f) + 0.02f);

  // 边缘山脊环仍高于中心（天际线遮挡世界边界）。
  assert(terrain.heightAt(0.0f, 0.0f) >
         terrain.config().edgeMountainHeight * 0.5f);

  // 全部内容点位：干地（豁免名单除外）且坡度可行走。
  for (const WL::WorldAnchorDef& anchor : WL::kAnchors) {
    checkContentPoint(terrain, "anchor", anchor.x, anchor.y);
  }
  for (const WL::WorldNpcDef& npc : WL::kNpcs) {
    checkContentPoint(terrain, "npc", npc.x, npc.y);
    for (int32_t p = 0; p < npc.patrolCount; ++p) {
      checkContentPoint(terrain, "npc-patrol", npc.patrolX[p], npc.patrolY[p]);
    }
  }
  for (const WL::WorldSpawnZoneDef& zone : WL::kSpawnZones) {
    for (int32_t p = 0; p < zone.count; ++p) {
      checkContentPoint(terrain, "spawn", zone.positionX[p], zone.positionY[p]);
    }
    checkContentPoint(terrain, "spawn-patrol", zone.patrolCenterX,
                      zone.patrolCenterY);
  }
  for (const WL::WorldChestDef& chest : WL::kChests) {
    checkContentPoint(terrain, "chest", chest.x, chest.y);
  }
  for (const WL::WorldCollectibleDef& collectible : WL::kCollectibles) {
    checkContentPoint(terrain, "collectible", collectible.x, collectible.y);
  }
  for (const WL::WorldPointOfInterestDef& poi : WL::kPointsOfInterest) {
    checkContentPoint(terrain, "poi", poi.x, poi.y);
  }
  for (const WL::WorldPuzzleNodeDef& puzzle : WL::kPuzzleNodes) {
    checkContentPoint(terrain, "puzzle", puzzle.x, puzzle.y);
  }
  for (const WL::WorldTraversalGateDef& gate : WL::kTraversalGates) {
    checkContentPoint(terrain, "gate", gate.x, gate.y);
  }

  // 主干道：至少一条路线段存在且端点可行走。
  const std::vector<WorldRouteSegment> routes = worldRouteSegments();
  assert(routes.size() == WL::kRouteCount);
  assert(!routes.empty());
  for (const WorldRouteSegment& route : routes) {
    checkContentPoint(terrain, "route-from", route.fromX, route.fromY);
    checkContentPoint(terrain, "route-to", route.toX, route.toY);
  }

  return 0;
}
