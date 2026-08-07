// test_chunked_terrain.cpp: 分块地形网格生成回归测试。
// 覆盖：8x8 分块矩形全覆盖不重叠、三档环形 LOD 确定性（含
// perf 档位收缩）、侧裙存在性与布局、边界顶点高度与
// terrain.heightAt 严格一致、确定性重放。

#include "native/engine/render/chunked_terrain.h"

#include "native/engine/world/terrain_heightfield.h"
#include "native/engine/world/world_grid.h"

#include <cassert>
#include <cmath>
#include <set>

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
  WorldGrid grid{WorldGridConfig{8, 8, 2}};
  ChunkedTerrain chunked(terrain, grid);

  // ---- 分块矩形覆盖完整性：8x8 全覆盖、不重叠、贴合世界边界。----
  std::set<std::pair<int, int>> covered;
  for (int32_t id = 0; id < grid.chunkCount(); ++id) {
    const TerrainChunkRect rect = chunked.chunkRect(id);
    const int cx = grid.chunkXOf(id);
    const int cy = grid.chunkYOf(id);
    assert(rect.x0 == static_cast<float>(cx) * 0.125f);
    assert(rect.x1 == static_cast<float>(cx + 1) * 0.125f);
    assert(rect.y0 == static_cast<float>(cy) * 0.125f);
    assert(rect.y1 == static_cast<float>(cy + 1) * 0.125f);
    assert(rect.x0 < rect.x1 && rect.y0 < rect.y1);
    covered.insert({cx, cy});
  }
  assert(covered.size() == 64);  // 全覆盖且不重叠。

  // ---- LOD 档位确定性：玩家分块 (4,4)=36，lodLevel 0。----
  const int32_t player = grid.chunkIndexAt({0.5f, 0.5f});
  assert(player == 36);
  assert(chunked.segmentsFor(36, player, 0) == 16u);   // 近圈（距离 0）
  assert(chunked.segmentsFor(27, player, 0) == 16u);   // 近圈（距离 1）
  assert(chunked.segmentsFor(45, player, 0) == 16u);   // 近圈（5,4 距离 1）
  assert(chunked.segmentsFor(18, player, 0) == 8u);    // 中圈（距离 2）
  assert(chunked.segmentsFor(0, player, 0) == 4u);     // 远圈（距离 4）
  assert(chunked.segmentsFor(63, player, 0) == 4u);    // 远圈
  // lodLevel 1：近圈收缩到玩家分块，中圈收缩到距离 1。
  assert(chunked.segmentsFor(36, player, 1) == 16u);
  assert(chunked.segmentsFor(27, player, 1) == 8u);
  assert(chunked.segmentsFor(18, player, 1) == 4u);
  // lodLevel 2：仅玩家分块近档，其余全部远档。
  assert(chunked.segmentsFor(36, player, 2) == 16u);
  assert(chunked.segmentsFor(27, player, 2) == 4u);
  assert(chunked.segmentsFor(36, player, 99) == 16u);  // 非法档位钳制。
  // 同输入重复查询结果一致（确定性）。
  assert(chunked.segmentsFor(27, player, 0) == 16u);

  // ---- 网格生成：表面顶点高度与 heightAt 严格一致。----
  const int32_t chunkId = 9;  // (1,1)
  const uint32_t segments = 8u;
  const Mesh mesh = chunked.buildChunkMesh(chunkId, segments);
  const TerrainChunkRect rect = chunked.chunkRect(chunkId);
  const uint32_t rows = segments + 1u;
  const uint32_t gridVertexCount = rows * rows;
  assert(mesh.vertices.size() == gridVertexCount + 8u * rows);  // 含侧裙。
  assert(mesh.indices.size() ==
         static_cast<size_t>(segments) * segments * 6u +
             static_cast<size_t>(4u) * segments * 6u);
  for (uint32_t j = 0; j < rows; ++j) {
    for (uint32_t i = 0; i < rows; ++i) {
      const Vertex& vertex = mesh.vertices[j * rows + i];
      const float x = rect.x0 + (rect.x1 - rect.x0) *
                                    static_cast<float>(i) /
                                    static_cast<float>(segments);
      const float y = rect.y0 + (rect.y1 - rect.y0) *
                                    static_cast<float>(j) /
                                    static_cast<float>(segments);
      assert(vertex.position.x == x);
      assert(vertex.position.z == y);
      assert(vertex.position.y == terrain.heightAt(x, y));
      assert(vertex.uv.x == x && vertex.uv.y == y);
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
  const Mesh leftMesh = chunked.buildChunkMesh(8, segments);   // (0,1)
  const Mesh rightMesh = chunked.buildChunkMesh(9, segments);  // (1,1)
  // 左块东边界（i = segments）与右块西边界（i = 0）x 坐标相同（0.125 精确可表示）。
  for (uint32_t j = 0; j < rows; ++j) {
    const Vertex& east = leftMesh.vertices[j * rows + segments];
    const Vertex& west = rightMesh.vertices[j * rows];
    assert(east.position.x == west.position.x);
    assert(east.position.y == west.position.y);
  }

  // ---- requestLoads/requestUnloads：去重、非法 id 过滤、卸载。----
  ChunkedTerrain store(terrain, grid);
  assert(store.chunkCount() == 0);
  store.requestLoads({9, 9, 10, -3, 999}, player, 0);
  assert(store.chunkCount() == 2);
  assert(store.hasChunk(9) && store.hasChunk(10));
  const std::vector<int32_t> ids = store.chunkIds();
  assert(ids.size() == 2 && ids[0] == 9 && ids[1] == 10);  // 升序。
  // LOD 记录正确：玩家 36(4,4) 到分块 9(1,1) 切比雪夫距离 3 → 远档。
  assert(store.chunkAt(9)->segments == 4u);
  // 重复加载不重建（网格数据不变）。
  const Mesh before = store.chunkAt(9)->mesh;
  store.requestLoads({9}, player, 0);
  assert(sameVertices(before, store.chunkAt(9)->mesh));
  store.requestUnloads({9, 9, 63});
  assert(store.chunkCount() == 1);
  assert(!store.hasChunk(9) && store.hasChunk(10));

  // ---- 确定性重放：同输入两次构建完全一致。----
  ChunkedTerrain replay(terrain, grid);
  replay.requestLoads({9, 10, 11}, player, 0);
  ChunkedTerrain replay2(terrain, grid);
  replay2.requestLoads({11, 10, 9}, player, 0);  // 乱序输入同样结果。
  assert(replay.chunkIds() == replay2.chunkIds());
  assert(sameVertices(replay.chunkAt(9)->mesh, replay2.chunkAt(9)->mesh));
  assert(sameVertices(replay.chunkAt(11)->mesh, replay2.chunkAt(11)->mesh));

  // 非法 segments 返回空网格。
  assert(chunked.buildChunkMesh(9, 0u).vertices.empty());
  assert(chunked.buildChunkMesh(-1, 4u).vertices.empty());

  return 0;
}
