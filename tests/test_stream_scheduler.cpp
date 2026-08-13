// test_stream_scheduler.cpp: 无限世界分块流送回归测试。
// 覆盖 ChunkCoord、方向优先顺序、后台取消、同步安全圈、两圈缓存回收、
// 长距离移动的状态上界与重建确定性，以及 int64 极值距离。

#include "native/engine/world/stream_scheduler.h"

#include "native/engine/world/terrain_heightfield.h"
#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace {

void drainAll(StreamScheduler &scheduler) {
  scheduler.beginBurst(1024, 1024);
  while (scheduler.readyCount() > 0) {
    const std::vector<ChunkCoord> drained = scheduler.drainReady(1000);
    assert(!drained.empty());
  }
}

void assertVerticesEqual(const std::vector<Vertex> &lhs,
                         const std::vector<Vertex> &rhs) {
  assert(lhs.size() == rhs.size());
  for (size_t i = 0; i < lhs.size(); ++i) {
    assert(lhs[i].position.x == rhs[i].position.x);
    assert(lhs[i].position.y == rhs[i].position.y);
    assert(lhs[i].position.z == rhs[i].position.z);
    assert(lhs[i].normal.x == rhs[i].normal.x);
    assert(lhs[i].normal.y == rhs[i].normal.y);
    assert(lhs[i].normal.z == rhs[i].normal.z);
    assert(lhs[i].uv.x == rhs[i].uv.x);
    assert(lhs[i].uv.y == rhs[i].uv.y);
  }
}

struct BlockingBuilderState {
  std::mutex mutex;
  std::condition_variable cv;
  bool started = false;
  bool released = false;

  bool waitUntilStarted(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, [this] { return started; });
  }

  void release() {
    std::lock_guard<std::mutex> lock(mutex);
    released = true;
    cv.notify_all();
  }
};

void assertFallbackContract(const TerrainChunkCpuMesh &chunk,
                            ChunkCoord expectedCoord, uint32_t segments) {
  const uint32_t rows = segments + 1u;
  assert(chunk.coord == expectedCoord);
  assert(chunk.segments == segments);
  assert(chunk.gridVertexCount == rows * rows);
  assert(chunk.skirtVertexCount == 8u * rows);
  assert(chunk.mesh.vertices.size() ==
         static_cast<size_t>(chunk.gridVertexCount + chunk.skirtVertexCount));
  assert(chunk.mesh.indices.size() ==
         static_cast<size_t>(segments) * segments * 6u +
             static_cast<size_t>(4u) * segments * 6u);
  for (const Vertex &vertex : chunk.mesh.vertices) {
    assert(std::isfinite(vertex.position.x));
    assert(std::isfinite(vertex.position.y));
    assert(std::isfinite(vertex.position.z));
    assert(std::isfinite(vertex.normal.x));
    assert(std::isfinite(vertex.normal.y));
    assert(std::isfinite(vertex.normal.z));
    assert(std::isfinite(vertex.uv.x));
    assert(std::isfinite(vertex.uv.y));
  }
  for (const uint32_t index : chunk.mesh.indices) {
    assert(index < chunk.mesh.vertices.size());
  }
}

} // namespace

int main() {
  TerrainHeightfield terrain;
  WorldGrid grid;

  // 生产变更破坏点：重新按字典序排序会丢失 WorldGrid 已给出的方向优先级。
  StreamScheduler priority(terrain, grid);
  const std::vector<ChunkCoord> directionOrder = {{0, 0},  {1, 0}, {0, -1},
                                                  {-1, 0}, {0, 1}, {1, 0}};
  priority.requestLoads(directionOrder, {0, 0}, 0);
  priority.setSyncMode(true); // 等待 worker 按入队顺序完成。
  assert(priority.readyCount() == 5);
  priority.beginBurst(1, 8);
  assert(priority.drainReady(1000) ==
         (std::vector<ChunkCoord>{{0, 0}, {1, 0}, {0, -1}, {-1, 0}, {0, 1}}));
  assert(priority.isActive({-1, 0}));
  const TerrainChunkCpuMesh *negative = priority.activeChunkMesh({-1, 0});
  assert(negative != nullptr && !negative->mesh.vertices.empty());
  priority.requestLoads({{-1, 0}, {-1, 0}}, {0, 0}, 0);
  assert(priority.readyCount() == 0);

  // 生产变更破坏点：卸载只清 pending 而不作废锁外生成，会在稍后迟交 Ready。
  auto cancellationState = std::make_shared<BlockingBuilderState>();
  auto cancellationBuilder = std::make_shared<ChunkedTerrain>(terrain);
  StreamScheduler cancellation(
      terrain, grid, {},
      [cancellationState, cancellationBuilder](ChunkCoord coord,
                                               uint32_t segments) {
        {
          std::unique_lock<std::mutex> lock(cancellationState->mutex);
          cancellationState->started = true;
          cancellationState->cv.notify_all();
          const bool released = cancellationState->cv.wait_for(
              lock, std::chrono::seconds(2),
              [cancellationState] { return cancellationState->released; });
          assert(released);
        }
        return cancellationBuilder->buildChunkMesh(coord, segments);
      });
  cancellation.setKeepRadius(0, 0);
  const ChunkCoord cancelledCoord{0, 0};
  cancellation.requestLoads({cancelledCoord}, cancelledCoord, 0);
  assert(cancellationState->waitUntilStarted(std::chrono::seconds(2)));
  assert(cancellation.pendingLoadCount() == 0);
  assert(cancellation.readyCount() == 0);
  cancellation.requestLoads({}, {100, 100}, 0);
  assert(cancellation.requestUnloads({cancelledCoord}) ==
         std::vector<ChunkCoord>{cancelledCoord});
  cancellationState->release();
  cancellation.setSyncMode(true); // 等待正在执行的任务完成并提交/丢弃。
  assert(cancellation.pendingLoadCount() == 0);
  assert(cancellation.readyCount() == 0);
  assert(cancellation.activeCount() == 0);
  assert(cancellation.applyUnloads() ==
         std::vector<ChunkCoord>{cancelledCoord});
  assert(!cancellation.isLoaded(cancelledCoord));
  assert(cancellation.cachedChunkCount() == 0);

  // 受控异步移动：玩家跳远后，阻塞中的旧块也必须计入取消并保持 cache 有界。
  auto movingState = std::make_shared<BlockingBuilderState>();
  auto movingBuilder = std::make_shared<ChunkedTerrain>(terrain);
  StreamScheduler asyncTravel(
      terrain, grid, {},
      [movingState, movingBuilder](ChunkCoord coord, uint32_t segments) {
        {
          std::unique_lock<std::mutex> lock(movingState->mutex);
          movingState->started = true;
          movingState->cv.notify_all();
          const bool released = movingState->cv.wait_for(
              lock, std::chrono::seconds(2),
              [movingState] { return movingState->released; });
          assert(released);
        }
        return movingBuilder->buildChunkMesh(coord, segments);
      });
  asyncTravel.setKeepRadius(4);
  asyncTravel.requestLoads({{0, 0}}, {0, 0}, 0);
  assert(movingState->waitUntilStarted(std::chrono::seconds(2)));
  asyncTravel.requestLoads({}, {50, 0}, 0);
  asyncTravel.requestUnloads({{0, 0}});
  assert(asyncTravel.applyUnloads() == (std::vector<ChunkCoord>{{0, 0}}));
  assert(asyncTravel.activeCount() + asyncTravel.readyCount() +
             asyncTravel.pendingLoadCount() <=
         13u * 13u + 81u);
  assert(asyncTravel.cachedChunkCount() <= 13u * 13u + 81u);
  movingState->release();
  asyncTravel.setSyncMode(true);
  assert(asyncTravel.readyCount() == 0);
  assert(asyncTravel.cachedChunkCount() == 0);

  // 生产变更破坏点：极值落点直接做 +/-1 会溢出，静默跳过则不再是固定 3x3。
  const int64_t min = std::numeric_limits<int64_t>::min();
  const int64_t max = std::numeric_limits<int64_t>::max();
  struct SafeCenterCase {
    ChunkCoord landing;
    ChunkCoord expectedCenter;
  };
  const std::vector<SafeCenterCase> boundaryCases = {
      {{min, min}, {min + 1, min + 1}}, {{min, max}, {min + 1, max - 1}},
      {{max, min}, {max - 1, min + 1}}, {{max, max}, {max - 1, max - 1}},
      {{min, 0}, {min + 1, 0}},         {{max, 0}, {max - 1, 0}},
      {{0, min}, {0, min + 1}},         {{0, max}, {0, max - 1}},
  };
  for (const SafeCenterCase &testCase : boundaryCases) {
    StreamScheduler boundary(terrain, grid);
    boundary.setSyncMode(true);
    const std::vector<ChunkCoord> ring =
        boundary.loadSafeRingSync(testCase.landing, 0);
    assert(ring.size() == 9);
    assert(ring.front() == testCase.expectedCenter);
    assert(std::set<ChunkCoord>(ring.begin(), ring.end()).size() == 9);
    assert(std::find(ring.begin(), ring.end(), testCase.landing) != ring.end());
  }

  // 生产变更破坏点：空网格 fallback 必须走 ChunkedTerrain 标准侧裙契约。
  ChunkedTerrain fallbackBuilder(terrain);
  const uint32_t fallbackSegments = fallbackBuilder.config().farSegments;
  const TerrainChunkCpuMesh directFallback =
      fallbackBuilder.buildFlatFallbackChunk({7, -9}, fallbackSegments);
  assertFallbackContract(directFallback, {7, -9}, fallbackSegments);
  const TerrainChunkCpuMesh normalContract =
      fallbackBuilder.buildChunkMesh({7, -9}, fallbackSegments);
  assert(directFallback.gridVertexCount == normalContract.gridVertexCount);
  assert(directFallback.skirtVertexCount == normalContract.skirtVertexCount);
  assert(directFallback.mesh.vertices.size() ==
         normalContract.mesh.vertices.size());
  assert(directFallback.mesh.indices.size() == normalContract.mesh.indices.size());

  auto fallbackCalls = std::make_shared<int32_t>(0);
  auto safeRingBuilder = std::make_shared<ChunkedTerrain>(terrain);
  StreamScheduler fallbackScheduler(
      terrain, grid, {},
      [fallbackCalls, safeRingBuilder](ChunkCoord coord, uint32_t segments) {
        if ((*fallbackCalls)++ == 0) {
          TerrainChunkCpuMesh empty;
          empty.coord = coord;
          empty.segments = segments;
          return empty;
        }
        return safeRingBuilder->buildChunkMesh(coord, segments);
      });
  fallbackScheduler.setSyncMode(true);
  const ChunkCoord fallbackCoord{13, -21};
  assert(fallbackScheduler.loadSafeRingSync(fallbackCoord, 0).size() == 9);
  drainAll(fallbackScheduler);
  const TerrainChunkCpuMesh *safeFallback =
      fallbackScheduler.activeChunkMesh(fallbackCoord);
  assert(safeFallback != nullptr);
  assertFallbackContract(*safeFallback, fallbackCoord, fallbackSegments);
  assert(*fallbackCalls == 9);

  // 生产变更破坏点：安全圈半径/顺序可变，或同步生成后直接进入 Active。
  StreamScheduler teleport(terrain, grid);
  teleport.setSyncMode(true);
  const ChunkCoord landing{50, -50};
  const std::vector<ChunkCoord> safeRing =
      teleport.loadSafeRingSync(landing, 0);
  assert(safeRing == (std::vector<ChunkCoord>{{50, -50},
                                              {49, -51},
                                              {50, -51},
                                              {51, -51},
                                              {49, -50},
                                              {51, -50},
                                              {49, -49},
                                              {50, -49},
                                              {51, -49}}));
  assert(teleport.readyCount() == 9);
  assert(teleport.activeCount() == 0);
  drainAll(teleport);
  assert(teleport.readyCount() == 0);
  assert(teleport.activeCount() == 9);
  for (const ChunkCoord coord : safeRing) {
    const TerrainChunkCpuMesh *chunk = teleport.activeChunkMesh(coord);
    assert(chunk != nullptr && !chunk->mesh.vertices.empty());
  }

  // 生产变更破坏点：把 activeRadius 当成完整 keep 半径，会提前回收额外两圈。
  StreamScheduler retention(terrain, grid);
  retention.setSyncMode(true);
  retention.setKeepRadius(1); // 默认 cacheRings = 2，keep 半径为 3。
  retention.requestLoads({{0, 0}, {3, 0}, {-3, 0}, {4, 0}, {-4, 0}}, {0, 0}, 0);
  drainAll(retention);
  retention.requestUnloads({{0, 0}, {3, 0}, {-3, 0}, {4, 0}, {-4, 0}});
  const std::vector<ChunkCoord> outsideKeep = retention.applyUnloads();
  assert(outsideKeep == (std::vector<ChunkCoord>{{-4, 0}, {4, 0}}));
  assert(retention.isActive({-3, 0}));
  assert(retention.isActive({3, 0}));
  assert(!retention.isLoaded({-4, 0}));
  assert(!retention.isLoaded({4, 0}));

  // 生产变更破坏点：移动时只清 Active、不清 Ready/pending/CPU cache
  // 会无界增长； 回程若混入进程随机状态，原点表面顶点不会逐字段一致。
  WorldGrid movingGrid{WorldGridConfig{4, 2}};
  StreamScheduler travel(terrain, movingGrid);
  travel.setSyncMode(true);
  travel.setKeepRadius(4); // 默认额外两圈，完整 keep 半径为 6。
  auto streamAt = [&](ChunkCoord player) {
    movingGrid.updateStreaming(player, {1.0f, 0.0f}, {1.0f, 0.0f});
    travel.requestLoads(movingGrid.pendingLoads(), player, 0);
    travel.requestUnloads(movingGrid.pendingUnloads());
    drainAll(travel);
    travel.applyUnloads();
    assert(travel.activeCount() + travel.readyCount() +
               travel.pendingLoadCount() <=
           13u * 13u + 81u);
    assert(travel.cachedChunkCount() <= 13u * 13u + 81u);
  };

  streamAt({0, 0});
  const TerrainChunkCpuMesh *originBefore = travel.activeChunkMesh({0, 0});
  assert(originBefore != nullptr);
  const std::vector<Vertex> originalVertices = originBefore->mesh.vertices;
  const std::vector<uint32_t> originalIndices = originBefore->mesh.indices;
  for (int64_t x = 1; x <= 50; ++x)
    streamAt({x, 0});
  assert(!travel.isLoaded({0, 0}));
  streamAt({0, 0});
  const TerrainChunkCpuMesh *originAfter = travel.activeChunkMesh({0, 0});
  assert(originAfter != nullptr);
  assertVerticesEqual(originalVertices, originAfter->mesh.vertices);
  assert(originalIndices == originAfter->mesh.indices);

  // 生产变更破坏点：有符号减法/abs 在 int64 极值会溢出并误判距离。
  StreamScheduler extremes(terrain, grid);
  extremes.setSyncMode(true);
  extremes.setKeepRadius(std::numeric_limits<int32_t>::max(),
                         std::numeric_limits<int32_t>::max());
  const ChunkCoord low{std::numeric_limits<int64_t>::min(),
                       std::numeric_limits<int64_t>::min()};
  const ChunkCoord high{std::numeric_limits<int64_t>::max(),
                        std::numeric_limits<int64_t>::max()};
  extremes.requestLoads({low, high}, low, 0);
  drainAll(extremes);
  assert(extremes.applyUnloads() == std::vector<ChunkCoord>{high});
  assert(extremes.isActive(low));
  assert(!extremes.isLoaded(high));

  return 0;
}
