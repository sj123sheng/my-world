// 外围程序化内容的确定性与地形安全约束回归测试。

#include "native/engine/world/terrain_heightfield.h"
#include "native/gameplay/world/procedural_chunk_content.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <type_traits>
#include <vector>

namespace {

constexpr uint64_t kSeed = 0x8d12f4a3bc567890ULL;

using ProductionGenerateProceduralChunk = ProceduralChunkContent (*) (
    uint64_t, ChunkCoord, const TerrainHeightfield&);
static_assert(std::is_same_v<decltype(&GenerateProceduralChunk),
                             ProductionGenerateProceduralChunk>);

bool samePosition(LocalPosition lhs, LocalPosition rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool sameContent(const ProceduralChunkContent& lhs,
                 const ProceduralChunkContent& rhs) {
  if (lhs.foliage.size() != rhs.foliage.size() ||
      lhs.enemies.size() != rhs.enemies.size() ||
      lhs.collectibles.size() != rhs.collectibles.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.foliage.size(); ++i) {
    if (!samePosition(lhs.foliage[i].position, rhs.foliage[i].position) ||
        lhs.foliage[i].kind != rhs.foliage[i].kind ||
        lhs.foliage[i].scale != rhs.foliage[i].scale) {
      return false;
    }
  }
  for (size_t i = 0; i < lhs.enemies.size(); ++i) {
    if (lhs.enemies[i].stableId != rhs.enemies[i].stableId ||
        !samePosition(lhs.enemies[i].position, rhs.enemies[i].position) ||
        lhs.enemies[i].archetype != rhs.enemies[i].archetype) {
      return false;
    }
  }
  for (size_t i = 0; i < lhs.collectibles.size(); ++i) {
    if (lhs.collectibles[i].stableId != rhs.collectibles[i].stableId ||
        !samePosition(lhs.collectibles[i].position, rhs.collectibles[i].position) ||
        lhs.collectibles[i].itemId != rhs.collectibles[i].itemId) {
      return false;
    }
  }
  return true;
}

bool sameEnemiesAndCollectibles(const ProceduralChunkContent& lhs,
                                const ProceduralChunkContent& rhs) {
  ProceduralChunkContent left = lhs;
  ProceduralChunkContent right = rhs;
  left.foliage.clear();
  right.foliage.clear();
  return sameContent(left, right);
}

bool differs(const ProceduralChunkContent& lhs, const ProceduralChunkContent& rhs) {
  return !sameContent(lhs, rhs);
}

void assertTerrainSafe(const TerrainHeightfield& terrain, ChunkCoord coord,
                       LocalPosition position) {
  assert(position.x >= 0.08f && position.x <= 0.92f);
  assert(position.y >= 0.08f && position.y <= 0.92f);
  const float height = terrain.heightAt(coord, position.x, position.y);
  assert(std::isfinite(height));
  assert(height >= terrain.config().waterLevel);
  assert(terrain.slopeAt(coord, position.x, position.y) < 0.55f);
}

void assertSortedAndBounded(const ProceduralChunkContent& content) {
  assert(content.foliage.size() <= 24U);
  assert(content.enemies.size() <= 3U);
  assert(content.collectibles.size() <= 4U);
  assert(std::is_sorted(content.enemies.begin(), content.enemies.end(),
                        [](const ProceduralEnemySpawn& lhs,
                           const ProceduralEnemySpawn& rhs) {
                          return lhs.stableId < rhs.stableId;
                        }));
  assert(std::is_sorted(content.collectibles.begin(), content.collectibles.end(),
                        [](const ProceduralCollectibleSpawn& lhs,
                           const ProceduralCollectibleSpawn& rhs) {
                          return lhs.stableId < rhs.stableId;
                        }));
  std::set<uint64_t> ids;
  for (const ProceduralEnemySpawn& spawn : content.enemies) {
    assert(ids.insert(spawn.stableId).second);
  }
  for (const ProceduralCollectibleSpawn& spawn : content.collectibles) {
    assert(ids.insert(spawn.stableId).second);
  }
}

void assertOrdinaryEnemy(EnemyArchetype archetype) {
  switch (archetype) {
    case EnemyArchetype::RiftClaw:
    case EnemyArchetype::Priest:
    case EnemyArchetype::Guard:
    case EnemyArchetype::Bruiser:
    case EnemyArchetype::Caster:
    case EnemyArchetype::Elite:
      return;
  }
  assert(false);
}

void testReplayCoreAndSaltIsolation() {
  const TerrainHeightfield terrain;
  const ChunkCoord coord{-17, 29};
  const ProceduralChunkContent first = GenerateProceduralChunk(kSeed, coord, terrain);
  const ProceduralChunkContent replay = GenerateProceduralChunk(kSeed, coord, terrain);
  const ProceduralChunkContent other =
      GenerateProceduralChunk(kSeed, {-18, 29}, terrain);
  assert(sameContent(first, replay));
  assert(differs(first, other));

  const ProceduralChunkContent core = GenerateProceduralChunk(kSeed, {0, 0}, terrain);
  assert(core.foliage.empty());
  assert(core.enemies.empty());
  assert(core.collectibles.empty());

  testing::ProceduralGenerationSalts salts;
  salts.foliageSalt = 0x21ULL;
  const ProceduralChunkContent changedFoliage =
      testing::GenerateProceduralChunkForTesting(kSeed, coord, terrain, salts);
  assert(sameEnemiesAndCollectibles(first, changedFoliage));
  assert(first.foliage.size() != changedFoliage.foliage.size() ||
         !std::equal(first.foliage.begin(), first.foliage.end(),
                     changedFoliage.foliage.begin(), changedFoliage.foliage.end(),
                     [](const ProceduralFoliageSpawn& lhs,
                        const ProceduralFoliageSpawn& rhs) {
                       return samePosition(lhs.position, rhs.position) &&
                              lhs.kind == rhs.kind && lhs.scale == rhs.scale;
                     }));
}

void testStableIdsUseFullHashAcrossBoundedLargeSample() {
  const TerrainHeightfield terrain;
  std::set<uint64_t> ids;
  for (int64_t x = -125; x < 125; ++x) {
    for (int64_t y = -100; y < 100; ++y) {
      const ChunkCoord coord{x, y};
      if (coord == ChunkCoord{0, 0}) continue;
      const ProceduralChunkContent first =
          GenerateProceduralChunk(kSeed, coord, terrain);
      const ProceduralChunkContent replay =
          GenerateProceduralChunk(kSeed, coord, terrain);
      assert(sameContent(first, replay));
      for (const ProceduralEnemySpawn& spawn : first.enemies) {
        assert(ids.insert(spawn.stableId).second);
      }
      for (const ProceduralCollectibleSpawn& spawn : first.collectibles) {
        assert(ids.insert(spawn.stableId).second);
      }
    }
  }
  // 默认平缓地形的每个外围块都会产生 3 个敌人与 4 个采集物。
  assert(ids.size() == (250U * 200U - 1U) * 7U);
}

void testTerrainConstraintsAcrossNegativeAndDistantChunks() {
  const TerrainHeightfield terrain;
  const std::vector<ChunkCoord> coords = {
      {-17, 29}, {-1000000000LL, 1000000000LL},
      {std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()},
  };
  for (const ChunkCoord coord : coords) {
    const ProceduralChunkContent content = GenerateProceduralChunk(kSeed, coord, terrain);
    assertSortedAndBounded(content);
    for (const ProceduralFoliageSpawn& spawn : content.foliage) {
      assertTerrainSafe(terrain, coord, spawn.position);
      assert(std::isfinite(spawn.scale));
    }
    for (const ProceduralEnemySpawn& spawn : content.enemies) {
      assertTerrainSafe(terrain, coord, spawn.position);
      assertOrdinaryEnemy(spawn.archetype);
    }
    for (const ProceduralCollectibleSpawn& spawn : content.collectibles) {
      assertTerrainSafe(terrain, coord, spawn.position);
    }
  }
}

}  // namespace

int main() {
  testReplayCoreAndSaltIsolation();
  testTerrainConstraintsAcrossNegativeAndDistantChunks();
  testStableIdsUseFullHashAcrossBoundedLargeSample();
}
