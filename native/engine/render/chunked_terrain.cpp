// chunked_terrain.cpp: 分块地形 CPU 网格生成与缓存。
//
// 每个 WorldGrid 分块独立生成 (segments+1)^2 表面顶点 + 四周侧裙，
// 高度/法线/UV 约定与整世界地形网格完全一致（共享
// createTerrainChunkMesh）。侧裙向下延伸固定深度，遮挡相邻 LOD
// 档位高度采样不同造成的接缝裂缝。所有输出为输入的纯函数，
// 同输入同输出，可被确定性测试直接断言。

#include "native/engine/render/chunked_terrain.h"

#include "native/engine/world/terrain_heightfield.h"
#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <cmath>

namespace {

int32_t clampInt(int32_t value, int32_t lo, int32_t hi) {
  return value < lo ? lo : (value > hi ? hi : value);
}

// 侧裙四边约定（按序：北/东/南/西），每边 rows 个顶点。
struct SkirtEdge {
  glm::vec3 outwardNormal;
};

}  // namespace

ChunkLodConfig ChunkLodConfig::forPerfLevel(int32_t lodLevel) {
  ChunkLodConfig config{};
  const int32_t level = clampInt(lodLevel, 0, 2);
  if (level >= 2) {
    // 精简：仅玩家分块近档，其余全部远档。
    config.nearRingMax = 0;
    config.midRingMax = 0;
  } else if (level == 1) {
    // 中等：近圈收缩到玩家分块，中圈收缩到距离 1。
    config.nearRingMax = 0;
    config.midRingMax = 1;
  }
  return config;
}

ChunkedTerrain::ChunkedTerrain(const TerrainHeightfield& terrain,
                               const WorldGrid& grid, ChunkLodConfig config)
    : terrain_(&terrain), grid_(&grid), config_(config) {
  if (config_.skirtDepth < 0.0f) config_.skirtDepth = 0.0f;
  if (config_.nearRingMax < 0) config_.nearRingMax = 0;
  if (config_.midRingMax < config_.nearRingMax) {
    config_.midRingMax = config_.nearRingMax;
  }
}

TerrainChunkRect ChunkedTerrain::chunkRect(int32_t chunkId) const {
  const int32_t count = grid_->chunkCount();
  const int32_t id = clampInt(chunkId, 0, count - 1);
  const int32_t cx = grid_->chunkXOf(id);
  const int32_t cy = grid_->chunkYOf(id);
  TerrainChunkRect rect;
  rect.x0 = static_cast<float>(cx) * grid_->chunkSizeX();
  rect.x1 = static_cast<float>(cx + 1) * grid_->chunkSizeX();
  rect.y0 = static_cast<float>(cy) * grid_->chunkSizeY();
  rect.y1 = static_cast<float>(cy + 1) * grid_->chunkSizeY();
  return rect;
}

uint32_t ChunkedTerrain::segmentsFor(int32_t chunkId, int32_t playerChunkId,
                                     int32_t perfLodLevel) const {
  const ChunkLodConfig effective = ChunkLodConfig::forPerfLevel(perfLodLevel);
  const int32_t count = grid_->chunkCount();
  const int32_t id = clampInt(chunkId, 0, count - 1);
  const int32_t player = clampInt(playerChunkId, 0, count - 1);
  const int32_t chebyshev =
      std::max(std::abs(grid_->chunkXOf(id) - grid_->chunkXOf(player)),
               std::abs(grid_->chunkYOf(id) - grid_->chunkYOf(player)));
  if (chebyshev <= effective.nearRingMax) return config_.nearSegments;
  if (chebyshev <= effective.midRingMax) return config_.midSegments;
  return config_.farSegments;
}

Mesh ChunkedTerrain::buildChunkMesh(int32_t chunkId, uint32_t segments) const {
  Mesh mesh;
  if (segments < 1u || terrain_ == nullptr || grid_ == nullptr) return mesh;
  if (chunkId < 0 || chunkId >= grid_->chunkCount()) return mesh;

  // 表面网格：与整世界地形同一采样约定。
  mesh = createTerrainChunkMesh(*terrain_, chunkRect(chunkId), segments);
  if (mesh.vertices.empty()) return mesh;

  const uint32_t rows = segments + 1u;
  const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

  // 四边顶环顶点序（网格内索引）：北(+Z)沿 x 递增、东(+X)沿 z 递增、
  // 南(-Z)沿 x 递减、西(-X)沿 z 递减，保证裙边三角形卷绕朝外。
  std::vector<uint32_t> edgeIndices;
  edgeIndices.reserve(static_cast<size_t>(4u) * rows);
  for (uint32_t i = 0; i < rows; ++i) {
    edgeIndices.push_back(segments * rows + i);  // 北：j = segments
  }
  for (uint32_t j = 0; j < rows; ++j) {
    edgeIndices.push_back(j * rows + segments);  // 东：i = segments
  }
  for (uint32_t k = rows; k-- > 0;) {
    edgeIndices.push_back(k);  // 南：j = 0，x 递减
  }
  for (uint32_t k = rows; k-- > 0;) {
    edgeIndices.push_back(k * rows);  // 西：i = 0，z 递减
  }

  const SkirtEdge edges[4] = {
      {{0.0f, 0.0f, 1.0f}},
      {{1.0f, 0.0f, 0.0f}},
      {{0.0f, 0.0f, -1.0f}},
      {{-1.0f, 0.0f, 0.0f}},
  };

  // 先追加 4*rows 个顶环顶点，再追加 4*rows 个下沉顶点；
  // 下沉顶点法线保持水平朝外，避免光照在裙边产生亮边。
  mesh.vertices.reserve(base + static_cast<size_t>(8u) * rows);
  for (size_t index = 0; index < edgeIndices.size(); ++index) {
    const Vertex& top = mesh.vertices[edgeIndices[index]];
    const glm::vec3 outward = edges[index / rows].outwardNormal;
    mesh.vertices.push_back({top.position, outward, top.uv});
  }
  for (size_t index = 0; index < edgeIndices.size(); ++index) {
    const Vertex& top = mesh.vertices[edgeIndices[index]];
    const glm::vec3 outward = edges[index / rows].outwardNormal;
    glm::vec3 bottom = top.position;
    bottom.y -= config_.skirtDepth;
    mesh.vertices.push_back({bottom, outward, top.uv});
  }

  // 裙边三角形：每边 (rows-1) 个四边形、2 个三角形。
  const uint32_t topBase = base;
  const uint32_t bottomBase = base + 4u * rows;
  mesh.indices.reserve(mesh.indices.size() +
                       static_cast<size_t>(4u) * (rows - 1u) * 6u);
  for (uint32_t e = 0; e < 4u; ++e) {
    for (uint32_t k = 0; k + 1u < rows; ++k) {
      const uint32_t ta = topBase + e * rows + k;
      const uint32_t tb = ta + 1u;
      const uint32_t ba = bottomBase + e * rows + k;
      const uint32_t bb = ba + 1u;
      mesh.indices.insert(mesh.indices.end(), {ta, ba, tb, tb, ba, bb});
    }
  }

  return mesh;
}

void ChunkedTerrain::requestLoads(const std::vector<int32_t>& chunkIds,
                                  int32_t playerChunkId,
                                  int32_t perfLodLevel) {
  std::vector<int32_t> sorted(chunkIds);
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  for (const int32_t chunkId : sorted) {
    if (chunkId < 0 || chunkId >= grid_->chunkCount()) continue;
    if (chunks_.count(chunkId) > 0) continue;
    const uint32_t segments = segmentsFor(chunkId, playerChunkId, perfLodLevel);
    TerrainChunkCpuMesh entry;
    entry.segments = segments;
    entry.mesh = buildChunkMesh(chunkId, segments);
    const uint32_t rows = segments + 1u;
    entry.gridVertexCount = rows * rows;
    entry.skirtVertexCount = 8u * rows;
    chunks_.emplace(chunkId, std::move(entry));
  }
}

bool ChunkedTerrain::storeChunk(int32_t chunkId, TerrainChunkCpuMesh entry) {
  if (chunkId < 0 || chunkId >= grid_->chunkCount()) return false;
  if (chunks_.count(chunkId) > 0) return false;
  chunks_.emplace(chunkId, std::move(entry));
  return true;
}

void ChunkedTerrain::requestUnloads(const std::vector<int32_t>& chunkIds) {
  std::vector<int32_t> sorted(chunkIds);
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  for (const int32_t chunkId : sorted) {
    chunks_.erase(chunkId);
  }
}

const TerrainChunkCpuMesh* ChunkedTerrain::chunkAt(int32_t chunkId) const {
  const auto found = chunks_.find(chunkId);
  return found == chunks_.end() ? nullptr : &found->second;
}

std::vector<int32_t> ChunkedTerrain::chunkIds() const {
  std::vector<int32_t> ids;
  ids.reserve(chunks_.size());
  for (const auto& entry : chunks_) ids.push_back(entry.first);
  return ids;
}
