// test_chunked_terrain.cpp: 分块地形网格生成回归测试。
// 覆盖：无限 ChunkCoord 缓存、三档环形 LOD、局部 [0,1] 网格、
// 侧裙布局、边界连续、高度场同源与确定性重放。

#include "native/engine/render/chunked_terrain.h"

#include "native/engine/world/terrain_heightfield.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

bool sameVertices(const Mesh& a, const Mesh& b) {
  if (a.vertices.size() != b.vertices.size()) return false;
  for (size_t i = 0; i < a.vertices.size(); ++i) {
    if (!(a.vertices[i].position == b.vertices[i].position)) return false;
    if (!(a.vertices[i].normal == b.vertices[i].normal)) return false;
    if (!(a.vertices[i].uv == b.vertices[i].uv)) return false;
  }
  return a.indices == b.indices;
}

}  // namespace

int main() {
  TerrainHeightfield terrain;
  ChunkedTerrain chunked(terrain);

  // ---- LOD 档位确定性：使用 ChunkCoord 的切比雪夫距离。----
  const ChunkCoord player{4, 4};
  assert(chunked.segmentsFor({4, 4}, player, 0) == 16u);  // 距离 0
  assert(chunked.segmentsFor({3, 3}, player, 0) == 16u);  // 距离 1
  assert(chunked.segmentsFor({6, 2}, player, 0) == 8u);   // 距离 2
  assert(chunked.segmentsFor({0, 0}, player, 0) == 4u);   // 距离 4
  // lodLevel 1：近圈收缩到玩家分块，中圈收缩到距离 1。
  assert(chunked.segmentsFor({4, 4}, player, 1) == 16u);
  assert(chunked.segmentsFor({3, 3}, player, 1) == 8u);
  assert(chunked.segmentsFor({6, 2}, player, 1) == 4u);
  // lodLevel 2：仅玩家分块近档，其余全部远档。
  assert(chunked.segmentsFor({4, 4}, player, 2) == 16u);
  assert(chunked.segmentsFor({3, 3}, player, 2) == 4u);
  assert(chunked.segmentsFor({4, 4}, player, 99) == 16u);
  // 极端 int64 坐标不能通过有符号减法触发溢出。
  assert(chunked.segmentsFor(
             {std::numeric_limits<int64_t>::min(), 0},
             {std::numeric_limits<int64_t>::max(), 0}, 0) == 4u);

  // ---- 网格生成：顶点局部 [0,1]，高度与目标分块 heightAt 同源。----
  const ChunkCoord coord{-4, 7};
  const uint32_t segments = 8u;
  const TerrainChunkCpuMesh built = chunked.buildChunkMesh(coord, segments);
  const Mesh& mesh = built.mesh;
  const uint32_t rows = segments + 1u;
  const uint32_t gridVertexCount = rows * rows;
  assert(built.coord == coord);
  assert(built.segments == segments);
  assert(built.gridVertexCount == gridVertexCount);
  assert(built.skirtVertexCount == 8u * rows);
  assert(mesh.vertices.size() == gridVertexCount + 8u * rows);  // 含侧裙。
  assert(mesh.indices.size() ==
         static_cast<size_t>(segments) * segments * 6u +
             static_cast<size_t>(4u) * segments * 6u);
  for (uint32_t j = 0; j < rows; ++j) {
    for (uint32_t i = 0; i < rows; ++i) {
      const Vertex& vertex = mesh.vertices[j * rows + i];
      const float x = static_cast<float>(i) / static_cast<float>(segments);
      const float y = static_cast<float>(j) / static_cast<float>(segments);
      assert(vertex.position.x == x);
      assert(vertex.position.z == y);
      assert(vertex.position.x >= 0.0f && vertex.position.x <= 1.0f);
      assert(vertex.position.z >= 0.0f && vertex.position.z <= 1.0f);
      assert(vertex.position.y == terrain.heightAt(coord, x, y));
      assert(std::isfinite(vertex.uv.x) && std::isfinite(vertex.uv.y));
      assert(vertex.normal.y > 0.0f);
    }
  }

  // ---- 侧裙布局：顶环 + 下沉环，下沉固定深度，法线水平朝外。----
  const float skirtDepth = chunked.config().skirtDepth;
  for (uint32_t k = 0; k < 4u * rows; ++k) {
    const Vertex& top = mesh.vertices[gridVertexCount + k];
    const Vertex& bottom = mesh.vertices[gridVertexCount + 4u * rows + k];
    assert(bottom.position.x == top.position.x);
    assert(bottom.position.z == top.position.z);
    assert(std::abs((top.position.y - bottom.position.y) - skirtDepth) < 1e-6f);
    assert(top.normal.y == 0.0f && bottom.normal.y == 0.0f);
    const float length = std::sqrt(top.normal.x * top.normal.x +
                                   top.normal.z * top.normal.z);
    assert(std::abs(length - 1.0f) < 1e-4f);
  }

  // ---- 相邻分块边界高度一致（共享边界采样同一高度场）。----
  const Mesh leftMesh = chunked.buildChunkMesh({0, 0}, segments).mesh;
  const Mesh rightMesh = chunked.buildChunkMesh({1, 0}, segments).mesh;
  for (uint32_t j = 0; j < rows; ++j) {
    const Vertex& east = leftMesh.vertices[j * rows + segments];
    const Vertex& west = rightMesh.vertices[j * rows];
    assert(east.position.x == 1.0f);
    assert(west.position.x == 0.0f);
    assert(east.position.y == west.position.y);
  }

  // ---- requestLoads/requestUnloads：无限坐标、去重、缓存键与卸载。----
  ChunkedTerrain store(terrain);
  assert(store.chunkCount() == 0);
  store.requestLoads({{-4, 7}, {-4, 7}, {10, -2}}, player, 0);
  assert(store.chunkCount() == 2);
  assert(store.hasChunk({-4, 7}) && store.hasChunk({10, -2}));
  const std::vector<ChunkCoord> coords = store.chunkCoords();
  const ChunkCoord negativeCoord{-4, 7};
  const ChunkCoord positiveCoord{10, -2};
  assert(coords.size() == 2 && coords[0] == negativeCoord &&
         coords[1] == positiveCoord);
  assert(store.chunkAt({-4, 7})->coord == negativeCoord);
  assert(store.chunkAt({-4, 7})->segments == 4u);
  // 重复加载不重建（网格数据不变）。
  const Mesh before = store.chunkAt({-4, 7})->mesh;
  store.requestLoads({{-4, 7}}, player, 0);
  assert(sameVertices(before, store.chunkAt({-4, 7})->mesh));
  store.requestUnloads({{-4, 7}, {-4, 7}, {63, 63}});
  assert(store.chunkCount() == 1);
  assert(!store.hasChunk({-4, 7}) && store.hasChunk({10, -2}));

  // ---- 确定性重放：同输入两次构建完全一致。----
  ChunkedTerrain replay(terrain);
  replay.requestLoads({{-4, 7}, {10, -2}, {0, 0}}, player, 0);
  ChunkedTerrain replay2(terrain);
  replay2.requestLoads({{0, 0}, {10, -2}, {-4, 7}}, player, 0);
  assert(replay.chunkCoords() == replay2.chunkCoords());
  assert(sameVertices(replay.chunkAt({-4, 7})->mesh,
                      replay2.chunkAt({-4, 7})->mesh));
  assert(sameVertices(replay.chunkAt({10, -2})->mesh,
                      replay2.chunkAt({10, -2})->mesh));

  // 非法 segments 返回空网格。
  assert(chunked.buildChunkMesh({9, 0}, 0u).mesh.vertices.empty());

  return 0;
}
