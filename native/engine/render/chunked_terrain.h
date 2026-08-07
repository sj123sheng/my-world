#pragma once

#include "native/engine/render/mesh.h"
#include "native/engine/render/terrain_mesh.h"

#include <cstdint>
#include <map>
#include <vector>

class TerrainHeightfield;
struct WorldGrid;

// 分块地形 CPU 端网格持有者（开放世界流式渲染 Phase 1）。
// 按 WorldGrid 分块生成并缓存未上传 GPU 的 Mesh 数据，供
// StreamScheduler 的 worker 线程生产、渲染线程消费。所有生成
// 均为确定性纯函数（同输入同输出），可在宿主侧直接断言。
//
// LOD 约定（三档环形，以玩家分块的切比雪夫距离划圈）：
//   近圈（距离 <= nearRingMax）  segments = nearSegments
//   中圈（距离 <= midRingMax）   segments = midSegments
//   远圈（其余）                 segments = farSegments
// 圈半径与档位可被 PerformanceGuard::lodLevel() 收缩（lodLevel
// 越高圈越小、分段越少）。
//
// 侧裙：每块网格四周追加向下延伸固定深度的裙边三角形，遮挡
// 相邻 LOD 档位之间的接缝裂缝；裙边顶点布局固定，可确定性断言。
struct ChunkLodConfig {
  // lodLevel 0（完整）：近圈切比雪夫距离 <=1，中圈 <=2。
  // lodLevel 1（中等）：近圈 <=0（仅玩家分块），中圈 <=1。
  // lodLevel 2（精简）：近圈 <=0，无中圈（全部远档）。
  int32_t nearRingMax = 1;
  int32_t midRingMax = 2;
  uint32_t nearSegments = 16u;
  uint32_t midSegments = 8u;
  uint32_t farSegments = 4u;
  // 侧裙向下延伸的固定深度（世界单位）。
  float skirtDepth = 0.05f;

  // 按性能降级档位收缩圈半径与分段；非法档位钳制到 [0, 2]。
  static ChunkLodConfig forPerfLevel(int32_t lodLevel);
};

// 单个分块的 CPU 端网格数据。
struct TerrainChunkCpuMesh {
  Mesh mesh;
  uint32_t segments = 0;
  // 网格前 (segments+1)^2 个顶点为地形表面，其余为侧裙顶点。
  uint32_t gridVertexCount = 0;
  uint32_t skirtVertexCount = 0;
};

class ChunkedTerrain {
 public:
  ChunkedTerrain(const TerrainHeightfield& terrain, const WorldGrid& grid,
                 ChunkLodConfig config = {});

  // 按玩家分块与 LOD 档位加载一批分块（升序去重后逐块生成，
  // 已存在的分块直接跳过不重建）。纯 CPU 操作，无 GL 调用。
  void requestLoads(const std::vector<int32_t>& chunkIds, int32_t playerChunkId,
                    int32_t perfLodLevel);
  // 直接登记外部已生成的分块数据（供调度器锁外生成、锁内提交）；
  // 已存在的分块跳过，返回是否真正写入。
  bool storeChunk(int32_t chunkId, TerrainChunkCpuMesh entry);
  // 卸载一批分块（升序去重）；未加载的分块忽略。
  void requestUnloads(const std::vector<int32_t>& chunkIds);

  // 生成分块网格（含侧裙）；非法分块 id 或 segments < 1 返回空网格。
  Mesh buildChunkMesh(int32_t chunkId, uint32_t segments) const;
  // 分块的逻辑坐标矩形（世界 [0,1]x[0,1] 内的子区域）。
  TerrainChunkRect chunkRect(int32_t chunkId) const;
  // 给定玩家分块与 LOD 档位，该分块应使用的分段数（确定性）。
  uint32_t segmentsFor(int32_t chunkId, int32_t playerChunkId,
                       int32_t perfLodLevel) const;

  bool hasChunk(int32_t chunkId) const { return chunks_.count(chunkId) > 0; }
  const TerrainChunkCpuMesh* chunkAt(int32_t chunkId) const;
  size_t chunkCount() const { return chunks_.size(); }
  // 当前已加载分块 id（std::map 遍历天然升序）。
  std::vector<int32_t> chunkIds() const;

  const ChunkLodConfig& config() const { return config_; }

 private:
  const TerrainHeightfield* terrain_;
  const WorldGrid* grid_;
  ChunkLodConfig config_;
  std::map<int32_t, TerrainChunkCpuMesh> chunks_;
};
