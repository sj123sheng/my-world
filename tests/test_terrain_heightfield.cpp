// test_terrain_heightfield.cpp: 地形高度场回归测试。
//
// 覆盖两层：
// 1. 连续基础层（无特征）：无限坐标确定性、有限性、幅度/坡度上限与跨块接缝；
// 2. 核心特征层（makeWorldTerrain）：旧地貌兼容、四边过渡衰减与内容点约束。

#include "native/engine/world/terrain_heightfield.h"
#include "native/gameplay/world/world_terrain.h"
#include "native/generated/world_layout.gen.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

namespace WL = WorldLayout;

namespace {

constexpr float kWalkableSlope = 0.55f;

bool close(float lhs, float rhs, float tolerance = 1e-6f) {
  return std::abs(lhs - rhs) <= tolerance;
}

void checkContentPoint(const TerrainHeightfield& terrain, const char* label,
                       float x, float y) {
  const bool wet = terrain.waterAt(x, y);
  if (wet) {
    std::printf("content point underwater: %s (%.2f, %.2f) h=%.4f\n", label,
                x, y, terrain.heightAt(x, y));
  }
  assert(!wet);
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

  // 连续基础层在任意分块保持有限、总幅度不超过 0.025，坡度低于 0.45。
  for (int chunkX = -12; chunkX <= 12; chunkX += 3) {
    for (int chunkY = -12; chunkY <= 12; chunkY += 3) {
      for (int i = 0; i <= 8; ++i) {
        for (int j = 0; j <= 8; ++j) {
          const float x = static_cast<float>(i) / 8.0f;
          const float y = static_cast<float>(j) / 8.0f;
          const float height = base.heightAt({chunkX, chunkY}, x, y);
          const float slope = base.slopeAt({chunkX, chunkY}, x, y);
          assert(std::isfinite(height));
          assert(std::isfinite(slope));
          assert(std::abs(height) <= 0.025f + 1e-6f);
          assert(slope >= 0.0f && slope < 0.45f);
        }
      }
    }
  }

  // 非法输入和超远正负分块不产生 NaN，且同一坐标确定重放。
  assert(std::isfinite(base.heightAt(std::nanf(""), 0.5f)));
  assert(std::isfinite(base.heightAt(0.5f, std::nanf(""))));
  assert(std::isfinite(base.slopeAt(-1.0f, 2.0f)));
  const ChunkCoord far{1000000000LL, -1000000000LL};
  assert(std::isfinite(base.heightAt(far, 0.5f, 0.5f)));
  assert(base.heightAt(far, 0.5f, 0.5f) ==
         base.heightAt(far, 0.5f, 0.5f));
  assert(std::isfinite(base.heightAt(
      {std::numeric_limits<int64_t>::max(),
       std::numeric_limits<int64_t>::min()},
      0.5f, 0.5f)));
  // 2^53 以后不能先把 chunk+local 合成 double：块内 1/8 单位仍须可见。
  const ChunkCoord beyondDouble{9007199254740992LL, 3};
  assert(base.heightAt(beyondDouble, 0.0f, 0.37f) !=
         base.heightAt(beyondDouble, 0.125f, 0.37f));
  const ChunkCoord positiveExtreme{std::numeric_limits<int64_t>::max(),
                                   std::numeric_limits<int64_t>::min()};
  const ChunkCoord negativeExtreme{std::numeric_limits<int64_t>::min(),
                                   std::numeric_limits<int64_t>::max()};
  for (const ChunkCoord extreme : {positiveExtreme, negativeExtreme}) {
    const float h0 = base.heightAt(extreme, 0.0f, 0.37f);
    const float h1 = base.heightAt(extreme, 0.125f, 0.37f);
    const float slope = base.slopeAt(extreme, 0.5f, 0.5f);
    assert(std::isfinite(h0) && std::isfinite(h1));
    assert(h0 != h1);
    assert(std::isfinite(slope) && slope > 0.0001f && slope < 0.45f);
  }
  assert(base.heightAt(negativeExtreme, 0.375f, 0.625f) ==
         base.heightAt(negativeExtreme, 0.375f, 0.625f));

  // 基础八度压缓不变量：无特征时不产生悬崖或边缘山墙，水面压低后
  // 基础层不再随处积水。
  for (int i = 0; i <= 40; ++i) {
    for (int j = 0; j <= 40; ++j) {
      const float x = static_cast<float>(i) / 40.0f;
      const float y = static_cast<float>(j) / 40.0f;
      assert(base.slopeAt(x, y) < 0.45f);
      assert(std::abs(base.heightAt(x, y)) <= 0.025f + 1e-6f);
      assert(!base.waterAt(x, y));
    }
  }

  // 世界中心 (0.5, 0.5)：主正弦整数周期过零，接近平整。
  assert(std::abs(base.heightAt(0.5f, 0.5f)) <
         base.config().detailAmplitude + 0.001f);

  // 相邻分块使用同一世界坐标相位，四向边界高度完全一致。
  assert(base.heightAt({0, 0}, 1.0f, 0.25f) ==
         base.heightAt({1, 0}, 0.0f, 0.25f));
  assert(base.heightAt({-1, 3}, 1.0f, 0.75f) ==
         base.heightAt({0, 3}, 0.0f, 0.75f));
  assert(base.heightAt({4, -2}, 0.6f, 1.0f) ==
         base.heightAt({4, -1}, 0.6f, 0.0f));
  assert(base.heightAt({4, -1}, 0.6f, 0.0f) ==
         base.heightAt({4, -2}, 0.6f, 1.0f));
  assert(base.heightAt({0, 0}, 0.0f, 0.25f) ==
         base.heightAt({-1, 0}, 1.0f, 0.25f));
  assert(base.heightAt({0, 0}, 0.6f, 0.0f) ==
         base.heightAt({0, -1}, 0.6f, 1.0f));
  // int64 极值无法再表示“外侧相邻块”，但所有可表示相邻边界必须逐位一致。
  const int64_t boundaryChunks[] = {
      std::numeric_limits<int64_t>::min(),
      std::numeric_limits<int64_t>::min() + 1,
      -9007199254740993LL,
      -1,
      0,
      9007199254740992LL,
      std::numeric_limits<int64_t>::max() - 1,
  };
  for (const int64_t chunkValue : boundaryChunks) {
    assert(base.heightAt({chunkValue, -7}, 1.0f, 0.375f) ==
           base.heightAt({chunkValue + 1, -7}, 0.0f, 0.375f));
    assert(base.heightAt({-7, chunkValue}, 0.375f, 1.0f) ==
           base.heightAt({-7, chunkValue + 1}, 0.375f, 0.0f));
  }
  assert(!close(base.heightAt(1.125, 0.37), base.heightAt(1.0, 0.37)));
  assert(base.slopeAt({9, -4}, 0.5f, 0.5f) <
         base.config().climbSlopeThreshold);
  // 回归：三频基础层在该相位曾叠出 0.47 的有限差分坡度。
  assert(base.slopeAt({0, 0}, 0.536f, 0.912f) < 0.45f);

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

  // 核心手工贡献在 0.08 宽过渡带按 smoothstep 衰减：边界为零，
  // 中点恰为一半，进入 0.08 后完整保留。
  TerrainConfig flatConfig;
  flatConfig.amplitude = 0.0f;
  flatConfig.detailAmplitude = 0.0f;
  flatConfig.ridgeAmplitude = 0.0f;
  TerrainFeature broadHill = hill;
  broadHill.radiusX = 100.0f;
  broadHill.radiusY = 100.0f;
  broadHill.amplitude = 0.1f;
  broadHill.feather = 0.0f;
  const TerrainHeightfield transitionTerrain{flatConfig, {broadHill}};
  assert(close(transitionTerrain.heightAt(0.0, 0.5), 0.0f));
  assert(close(transitionTerrain.heightAt(0.04, 0.5), 0.05f));
  assert(close(transitionTerrain.heightAt(0.08, 0.5), 0.1f));

  // ---- 特征层：全量世界地貌 ----
  const TerrainHeightfield terrain = makeWorldTerrain();
  assert(terrain.features().size() == WL::kTerrainFeatureCount);
  assert(WL::kTerrainFeatureCount > 0);

  // 核心手工贡献在四边精确衰减为零，因此与四个外围块无缝相接。
  for (int i = 0; i <= 16; ++i) {
    const float t = static_cast<float>(i) / 16.0f;
    assert(terrain.heightAt({0, 0}, 0.0f, t) ==
           terrain.heightAt({-1, 0}, 1.0f, t));
    assert(terrain.heightAt({0, 0}, 1.0f, t) ==
           terrain.heightAt({1, 0}, 0.0f, t));
    assert(terrain.heightAt({0, 0}, t, 0.0f) ==
           terrain.heightAt({0, -1}, t, 1.0f));
    assert(terrain.heightAt({0, 0}, t, 1.0f) ==
           terrain.heightAt({0, 1}, t, 0.0f));
  }

  // 确定性：两次构造结果逐点一致。
  const TerrainHeightfield terrainAgain = makeWorldTerrain();
  for (int i = 0; i <= 16; ++i) {
    for (int j = 0; j <= 16; ++j) {
      const float x = static_cast<float>(i) / 16.0f;
      const float y = static_cast<float>(j) / 16.0f;
      assert(terrain.heightAt(x, y) == terrainAgain.heightAt(x, y));
    }
  }

  // 辉光湖保留可游泳水域，但自然节点与区域触发都位于干燥湖湾。
  assert(terrain.heightAt(0.745f, 0.265f) < -0.06f);
  // 湖岸内容点不被湖水淹没。
  assert(!terrain.waterAt(0.8f, 0.25f));   // 东部施法者出生点
  assert(!terrain.waterAt(0.7f, 0.2f));    // 湖畔渔夫/湖湾节点

  // 湖心岩台：顶面平整（basin 目标高度），四壁可攀爬。
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

  // 中枢岩脊攀爬悬崖：晶簇旁存在可攀爬坡面。
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

  // 岚冠高地整体高于低地：高原台地地貌成立。
  assert(terrain.heightAt(0.8f, 0.8f) > terrain.heightAt(0.15f, 0.15f) + 0.02f);

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
  for (const WL::WorldNaturalNodeDef& node : WL::kNaturalNodes) {
    checkContentPoint(terrain, "natural-node", node.x, node.y);
  }
  for (const WL::WorldRegionTriggerDef& region : WL::kRegionTriggers) {
    checkContentPoint(terrain, "region-trigger", region.x, region.y);
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
