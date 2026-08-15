// 区块 GPU 资源组合测试（无限自然世界 Task 9）：
// 回收差量必须同步移除地形网格与植被批次两类 ChunkCoord 键，
// 出生点组合保持核心区自然路线锚点。
#include "native/engine/render/environment.h"

#include "native/engine/render/mesh.h"
#include "native/engine/world/foliage_system.h"

#include <cassert>
#include <map>
#include <vector>

namespace {

Mesh makeStubMesh() {
  Mesh mesh;
  mesh.vertices.resize(3);
  mesh.indices = {0u, 1u, 2u};
  return mesh;
}

void testSpawnStaysInsideCoreChunk() {
  const EnvironmentComposition composition =
      EnvironmentController::defaultComposition();
  assert(composition.spawn.x >= 0.0f && composition.spawn.x <= 1.0f);
  assert(composition.spawn.z >= 0.0f && composition.spawn.z <= 1.0f);
}

void testUnloadDiffRemovesTerrainAndFoliageKeys() {
  // 渲染侧按 ChunkCoord 持有地形网格与植被批次；区块离开两圈缓存后
  // 回收差量必须同步移除两类 GPU 键，不允许任何一类残留。
  const ChunkCoord unloadedCoord{2, -1};
  const ChunkCoord keptCoord{3, 0};
  std::map<ChunkCoord, Mesh> terrainMeshes;
  std::map<ChunkCoord, std::vector<FoliageInstance>> foliageBatches;
  terrainMeshes.emplace(unloadedCoord, makeStubMesh());
  terrainMeshes.emplace(keptCoord, makeStubMesh());
  foliageBatches[unloadedCoord] = {FoliageInstance{}};
  foliageBatches[keptCoord] = {FoliageInstance{}, FoliageInstance{}};

  const std::vector<ChunkCoord> unloaded = {unloadedCoord};
  for (const ChunkCoord coord : unloaded) {
    const auto found = terrainMeshes.find(coord);
    assert(found != terrainMeshes.end());
    found->second.destroy();  // 宿主侧为空操作，锁定释放调用点。
  }
  assert(EraseUnloadedChunkResources(unloaded, terrainMeshes) == 1u);
  assert(EraseUnloadedChunkResources(unloaded, foliageBatches) == 1u);

  assert(terrainMeshes.count(unloadedCoord) == 0u);
  assert(foliageBatches.count(unloadedCoord) == 0u);
  // 未回收区块保持原样。
  assert(terrainMeshes.count(keptCoord) == 1u);
  assert(foliageBatches[keptCoord].size() == 2u);

  // 重复应用同一差量幂等：不再移除任何键。
  assert(EraseUnloadedChunkResources(unloaded, terrainMeshes) == 0u);
  assert(EraseUnloadedChunkResources(unloaded, foliageBatches) == 0u);

  // 差量为空时两张表不受影响。
  assert(EraseUnloadedChunkResources({}, terrainMeshes) == 0u);
  assert(terrainMeshes.size() == 1u);
}

}  // namespace

int main() {
  testSpawnStaysInsideCoreChunk();
  testUnloadDiffRemovesTerrainAndFoliageKeys();
  return 0;
}
