// test_stream_scheduler.cpp: 流式调度器回归测试（同步模式）。
// 覆盖：同步模式下加载/卸载序列确定性、每帧配额截断（多个就绪
// 每帧只取 1）、预算为 0 推下帧、卸载滞后带行为、传送圈全链路
// （loadRingSync 产出全部进 Ready、逐帧 drain 上传后全部 active）、
// burst 配额放宽、乱序输入归一、双实例确定性对照。

#include "native/engine/world/stream_scheduler.h"

#include "native/engine/world/terrain_heightfield.h"
#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <cassert>

int main() {
  TerrainHeightfield terrain;
  WorldGrid grid{WorldGridConfig{8, 8, 2}};

  // ---- 同步模式加载序列确定性：Ready 按分块 id 升序消费。----
  StreamScheduler scheduler(terrain, grid, StreamSchedulerConfig{3, 2});
  scheduler.setSyncMode(true);
  assert(scheduler.syncMode());
  assert(scheduler.activeCount() == 0);
  assert(scheduler.readyCount() == 0);

  // 乱序请求被归一为升序就绪序列。
  scheduler.requestLoads({9, 4, 12}, 0, 0);
  assert(scheduler.readyCount() == 3);
  assert(scheduler.pendingLoadCount() == 0);
  // 每帧配额截断：多个就绪每帧只取 1 个。
  std::vector<int32_t> drained = scheduler.drainReady();
  assert(drained == std::vector<int32_t>{4});
  assert(scheduler.activeCount() == 1);
  assert(scheduler.readyCount() == 2);
  assert(scheduler.drainReady() == std::vector<int32_t>{9});
  assert(scheduler.drainReady() == std::vector<int32_t>{12});
  assert(scheduler.drainReady().empty());  // 队列耗尽。
  assert(scheduler.activeCount() == 3);
  // 活跃分块的 CPU 网格可读且非空。
  const TerrainChunkCpuMesh* chunk = scheduler.activeChunkMesh(4);
  assert(chunk != nullptr && !chunk->mesh.vertices.empty());
  assert(scheduler.activeChunkMesh(63) == nullptr);  // 非活跃分块。
  assert(scheduler.isLoaded(4) && scheduler.isActive(4));

  // ---- 预算为 0：即便有就绪也推下帧（实测预算生效）。----
  scheduler.requestLoads({5}, 0, 0);
  assert(scheduler.readyCount() == 1);
  assert(scheduler.drainReady(0).empty());
  assert(scheduler.readyCount() == 1);  // 仍在就绪队列，等待下帧。
  assert(scheduler.drainReady() == std::vector<int32_t>{5});

  // ---- 重复请求幂等：已就绪/已活跃的不会重复产出。----
  scheduler.requestLoads({4, 5, 5}, 0, 0);
  assert(scheduler.readyCount() == 0);

  // ---- Major 回归：传送圈分块全部进 Ready 队列，由 drainReady
  //      逐帧上传后置 active（而非直接进 active 永不上传）。----
  StreamScheduler teleport(terrain, grid, StreamSchedulerConfig{3, 2});
  teleport.setSyncMode(true);
  const std::vector<int32_t> teleRing = teleport.loadRingSync(36, 2, 0);
  assert(teleRing.size() == 25);  // 中心 (4,4) 半径 2 的 5x5 圈。
  assert(teleport.readyCount() == 25);
  assert(teleport.activeCount() == 0);  // 未上传前不活跃。
  // 逐帧 drainReady 取完：每帧 1 块共 25 帧，上传顺序升序。
  std::vector<int32_t> uploadOrder;
  for (int32_t frame = 0; frame < 25; ++frame) {
    const std::vector<int32_t> batch = teleport.drainReady();
    assert(batch.size() == 1);
    uploadOrder.insert(uploadOrder.end(), batch.begin(), batch.end());
  }
  assert(teleport.drainReady().empty());
  assert(uploadOrder == teleRing);  // 升序一致。
  assert(teleport.activeCount() == 25);
  assert(teleport.readyCount() == 0);
  // 活跃后 CPU 网格可读。
  assert(teleport.activeChunkMesh(36) != nullptr);
  // 再次 loadRingSync 同圈：全部已活跃，无新增、无就绪积压。
  assert(teleport.loadRingSync(36, 2, 0).empty());
  assert(teleport.readyCount() == 0);

  // ---- burst 配额：传送后临时放宽每帧上限，窗口结束回落。----
  StreamScheduler burst(terrain, grid, StreamSchedulerConfig{3, 2});
  burst.setSyncMode(true);
  burst.loadRingSync(36, 2, 0);
  assert(burst.readyCount() == 25);
  burst.beginBurst(2, 4);
  assert(burst.drainReady().size() == 4);   // 第 1 帧 burst。
  assert(burst.drainReady().size() == 4);   // 第 2 帧 burst。
  assert(burst.drainReady().size() == 1);   // 窗口结束，回落每帧 1。
  assert(burst.activeCount() == 9);

  // ---- 卸载滞后带：请求卸载但仍在 keepRadius 内不卸载。----
  // 玩家位于分块 36(4,4)，keepRadius = 3。
  StreamScheduler hysteresis(terrain, grid, StreamSchedulerConfig{3, 2});
  hysteresis.setSyncMode(true);
  hysteresis.requestLoads({36}, 36, 0);
  assert(hysteresis.drainReady() == std::vector<int32_t>{36});
  // 同步加载半径 2 的一圈（传送路径）：36 已活跃，其余 24 个新产出。
  const std::vector<int32_t> ring = hysteresis.loadRingSync(36, 2, 0);
  assert(ring.size() == 24);
  assert(hysteresis.readyCount() == 24);
  assert(hysteresis.activeCount() == 1);
  // 再次同步加载同一圈：无新增。
  assert(hysteresis.loadRingSync(36, 2, 0).empty());
  // 模拟渲染线程逐帧上传，直到传送圈全部活跃。
  while (!hysteresis.drainReady().empty()) {
  }
  assert(hysteresis.activeCount() == 25);
  // 圈升序且边界钳制：角落中心半径 2 只覆盖 3x3。
  StreamScheduler corner(terrain, grid, StreamSchedulerConfig{3, 2});
  corner.setSyncMode(true);
  const std::vector<int32_t> cornerRing = corner.loadRingSync(0, 2, 0);
  assert(cornerRing.size() == 9);
  assert(cornerRing.front() == 0 && cornerRing.back() == 18);
  while (!corner.drainReady().empty()) {
  }
  assert(corner.activeCount() == 9);

  // 请求卸载距离 2 的分块（仍在滞后带内）：不卸载。
  hysteresis.requestUnloads({18});  // (2,2) 距 (4,4) = 2 <= 3
  assert(hysteresis.applyUnloads().empty());
  assert(hysteresis.isActive(18));
  assert(hysteresis.isLoaded(18));

  // 玩家移动到角落分块 0(0,0)：超出 keepRadius=3 的分块被卸载，
  // 滞后带内的保留；卸载序列升序。
  hysteresis.requestLoads({}, 0, 0);  // 仅更新玩家分块。
  const std::vector<int32_t> unloaded = hysteresis.applyUnloads();
  assert(!unloaded.empty());
  for (size_t i = 1; i < unloaded.size(); ++i) {
    assert(unloaded[i] > unloaded[i - 1]);
  }
  // 分块 36(4,4) 距角落 4 > 3：被卸载；CPU 网格一并释放。
  assert(std::find(unloaded.begin(), unloaded.end(), 36) != unloaded.end());
  assert(!hysteresis.isActive(36));
  assert(!hysteresis.isLoaded(36));
  // 分块 27(3,3) 距角落 3 <= 3：滞后带内保留。
  assert(std::find(unloaded.begin(), unloaded.end(), 27) == unloaded.end());
  assert(hysteresis.isActive(27));
  // 滞后带内再次请求卸载：仍不卸载（抖动防护）。
  hysteresis.requestUnloads({27});
  assert(hysteresis.applyUnloads().empty());
  assert(hysteresis.isActive(27));
  // 角落滞后带内仅剩原圈与 3x3 窗口的交集：18,19,26,27。
  assert(hysteresis.activeCount() == 4);

  // ---- 动态收缩 keepRadius 后残留分块被兜底清理。----
  hysteresis.setKeepRadius(1);
  const std::vector<int32_t> pruned = hysteresis.applyUnloads();
  assert(std::find(pruned.begin(), pruned.end(), 27) != pruned.end());
  assert(pruned.size() == 4);
  assert(hysteresis.activeCount() == 0);

  // ---- unloadSync：同步卸载路径（含就绪队列中的未上传分块）。----
  hysteresis.requestLoads({0, 1, 9}, 0, 0);
  assert(hysteresis.drainReady() == std::vector<int32_t>{0});
  assert(hysteresis.drainReady() == std::vector<int32_t>{1});
  // 9 仍在就绪队列；unloadSync 一并释放活跃 0 与就绪 9，忽略非法 63。
  const std::vector<int32_t> removed = hysteresis.unloadSync({0, 0, 9, 63});
  assert(removed.size() == 2 && removed[0] == 0 && removed[1] == 9);
  assert(!hysteresis.isActive(0) && !hysteresis.isLoaded(9));
  assert(hysteresis.readyCount() == 0);

  // ---- 确定性重放：同序列两个实例结果一致。----
  StreamScheduler replayA(terrain, grid, StreamSchedulerConfig{3, 2});
  replayA.setSyncMode(true);
  replayA.requestLoads({36, 27}, 36, 0);
  replayA.drainReady();
  replayA.requestUnloads({27});
  StreamScheduler replayB(terrain, grid, StreamSchedulerConfig{3, 2});
  replayB.setSyncMode(true);
  replayB.requestLoads({27, 36}, 36, 0);  // 乱序输入。
  replayB.drainReady();
  replayB.requestUnloads({27});
  assert(replayA.activeChunkIds() == replayB.activeChunkIds());
  assert(replayA.readyCount() == replayB.readyCount());
  // 传送圈全链路双实例对照：产出、上传顺序与网格规模一致。
  assert(replayA.loadRingSync(36, 2, 0) == replayB.loadRingSync(36, 2, 0));
  std::vector<int32_t> orderA;
  std::vector<int32_t> orderB;
  for (;;) {
    const std::vector<int32_t> batchA = replayA.drainReady();
    const std::vector<int32_t> batchB = replayB.drainReady();
    assert(batchA == batchB);
    if (batchA.empty()) break;
    orderA.insert(orderA.end(), batchA.begin(), batchA.end());
    orderB.insert(orderB.end(), batchB.begin(), batchB.end());
  }
  assert(orderA == orderB);
  assert(replayA.activeChunkIds() == replayB.activeChunkIds());
  assert(replayA.chunkedTerrain().chunkIds() ==
         replayB.chunkedTerrain().chunkIds());
  const TerrainChunkCpuMesh* meshA = replayA.chunkedTerrain().chunkAt(36);
  const TerrainChunkCpuMesh* meshB = replayB.chunkedTerrain().chunkAt(36);
  assert(meshA != nullptr && meshB != nullptr);
  assert(meshA->mesh.vertices.size() == meshB->mesh.vertices.size());
  assert(meshA->mesh.indices.size() == meshB->mesh.indices.size());

  return 0;
}
