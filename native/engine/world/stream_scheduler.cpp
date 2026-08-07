// stream_scheduler.cpp: 分块内容流式调度器。
//
// 单 worker 线程按入队顺序（分块 id 升序）消费加载任务，产出进入
// Ready 队列；渲染线程每帧 drainReady 按配额取块上传 GPU（正常
// 每帧 1 块，burst 窗口内放宽）。所有网格生成均在锁外执行，
// 避免阻塞渲染线程。卸载走滞后带：仅当分块离开玩家分块
// keepRadius（流式半径 + 1）才真正释放，防止半径边界抖动。
// 同步模式关闭 worker，所有请求即时执行，供确定性测试与传送
// 同步准备使用。

#include "native/engine/world/stream_scheduler.h"

#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {

std::vector<int32_t> sortedUnique(std::vector<int32_t> ids) {
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}

}  // namespace

StreamScheduler::StreamScheduler(const TerrainHeightfield& terrain,
                                 const WorldGrid& grid,
                                 StreamSchedulerConfig config)
    : grid_(&grid), chunkedTerrain_(terrain, grid), config_(config) {
  if (config_.keepRadius < 0) config_.keepRadius = 0;
  if (config_.drainBudgetMs < 0) config_.drainBudgetMs = 0;
  worker_ = std::thread([this] { workerLoop(); });
}

StreamScheduler::~StreamScheduler() { stopWorker(); }

void StreamScheduler::stopWorker() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void StreamScheduler::setSyncMode(bool enabled) {
  if (enabled) {
    stopWorker();
    std::vector<LoadTask> backlog;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      syncMode_ = true;
      // 同步模式下不保留未消费的后台任务：立即全部执行，保证
      // 测试序列可断言。
      backlog.swap(pendingLoads_);
    }
    // executeTask 内部自行加锁，不能在持锁期间调用。
    for (const LoadTask& task : backlog) {
      executeTask(task);
    }
  } else {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      syncMode_ = false;
      stopping_ = false;
    }
    worker_ = std::thread([this] { workerLoop(); });
  }
}

bool StreamScheduler::syncMode() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return syncMode_;
}

void StreamScheduler::setKeepRadius(int32_t radius) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.keepRadius = std::max(radius, 0);
}

void StreamScheduler::workerLoop() {
  for (;;) {
    LoadTask task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stopping_ || !pendingLoads_.empty(); });
      if (pendingLoads_.empty()) {
        if (stopping_) return;
        continue;
      }
      // 入队时已按分块 id 升序维护，取队首保证消费顺序确定性。
      task = std::move(pendingLoads_.front());
      pendingLoads_.erase(pendingLoads_.begin());
    }
    executeTask(task);
  }
}

void StreamScheduler::executeTask(const LoadTask& task) {
  if (task.kind == TaskKind::ModelGlb) {
    // 预留任务类型：GLB 解析接入前直接丢弃，不影响地形管线。
    return;
  }

  // 阶段一（锁内）：查重与登记，不做重量级工作。
  bool needsGeneration = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool alreadyReady =
        std::find(ready_.begin(), ready_.end(), task.chunkId) != ready_.end();
    if (active_.count(task.chunkId) > 0 || alreadyReady) {
      removeGeneratingLocked(task.chunkId);
      return;
    }
    needsGeneration = !chunkedTerrain_.hasChunk(task.chunkId);
  }

  // 阶段二（锁外）：重量级网格生成，不阻塞渲染线程。
  // segmentsFor/buildChunkMesh 均为只读纯函数，可安全锁外调用。
  TerrainChunkCpuMesh entry;
  if (needsGeneration) {
    const uint32_t segments = chunkedTerrain_.segmentsFor(
        task.chunkId, task.playerChunkId, task.perfLodLevel);
    entry.segments = segments;
    entry.mesh = chunkedTerrain_.buildChunkMesh(task.chunkId, segments);
    const uint32_t rows = segments + 1u;
    entry.gridVertexCount = rows * rows;
    entry.skirtVertexCount = 8u * rows;
  }

  // 阶段三（锁内）：提交结果；生成期间被请求卸载则丢弃。
  {
    std::lock_guard<std::mutex> lock(mutex_);
    removeGeneratingLocked(task.chunkId);
    const bool alreadyReady =
        std::find(ready_.begin(), ready_.end(), task.chunkId) != ready_.end();
    if (active_.count(task.chunkId) > 0 || alreadyReady) return;
    if (takeCancelledLocked(task.chunkId)) {
      // 生成期间被请求卸载：丢弃结果并清理可能的 CPU 缓存。
      chunkedTerrain_.requestUnloads({task.chunkId});
      return;
    }
    if (needsGeneration) {
      chunkedTerrain_.storeChunk(task.chunkId, std::move(entry));
    }
    ready_.push_back(task.chunkId);
    std::sort(ready_.begin(), ready_.end());
  }
}

void StreamScheduler::processTaskInline(const LoadTask& task) {
  executeTask(task);
}

void StreamScheduler::requestLoads(const std::vector<int32_t>& chunkIds,
                                   int32_t playerChunkId,
                                   int32_t perfLodLevel) {
  const std::vector<int32_t> ids = sortedUnique(chunkIds);
  std::vector<LoadTask> fresh;
  bool sync = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    lastPlayerChunkId_ = playerChunkId;
    sync = syncMode_;
    for (const int32_t chunkId : ids) {
      if (chunkId < 0 || chunkId >= grid_->chunkCount()) continue;
      if (active_.count(chunkId) > 0) {
        // 重新请求加载即取消此前的卸载申请。
        requestedUnloads_.erase(
            std::remove(requestedUnloads_.begin(), requestedUnloads_.end(),
                        chunkId),
            requestedUnloads_.end());
        continue;
      }
      requestedUnloads_.erase(
          std::remove(requestedUnloads_.begin(), requestedUnloads_.end(),
                      chunkId),
          requestedUnloads_.end());
      const bool alreadyReady =
          std::find(ready_.begin(), ready_.end(), chunkId) != ready_.end();
      const bool alreadyPending =
          std::any_of(pendingLoads_.begin(), pendingLoads_.end(),
                      [chunkId](const LoadTask& task) {
                        return task.chunkId == chunkId;
                      });
      const bool alreadyGenerating =
          std::find(generating_.begin(), generating_.end(), chunkId) !=
          generating_.end();
      if (alreadyReady || alreadyPending || alreadyGenerating) continue;
      LoadTask task;
      task.kind = TaskKind::TerrainChunk;
      task.chunkId = chunkId;
      task.playerChunkId = playerChunkId;
      task.perfLodLevel = perfLodLevel;
      if (chunkedTerrain_.hasChunk(chunkId)) {
        // 网格仍在 CPU 缓存（滞后带内未被真正卸载）：直接就绪。
        ready_.push_back(chunkId);
      } else {
        // 登记生成中：worker/同步路径锁外生成，提交时移出。
        generating_.push_back(chunkId);
        fresh.push_back(std::move(task));
      }
    }
    std::sort(ready_.begin(), ready_.end());
    if (!sync) {
      pendingLoads_.insert(pendingLoads_.end(), fresh.begin(), fresh.end());
      std::sort(pendingLoads_.begin(), pendingLoads_.end(),
                [](const LoadTask& a, const LoadTask& b) {
                  return a.chunkId < b.chunkId;
                });
    }
  }
  if (sync) {
    // 同步模式：释放锁后即时执行（executeTask 内部自行加锁）。
    for (const LoadTask& task : fresh) {
      processTaskInline(task);
    }
    return;
  }
  if (!fresh.empty()) cv_.notify_one();
}

void StreamScheduler::requestUnloads(const std::vector<int32_t>& chunkIds) {
  const std::vector<int32_t> ids = sortedUnique(chunkIds);
  std::lock_guard<std::mutex> lock(mutex_);
  for (const int32_t chunkId : ids) {
    if (chunkId < 0 || chunkId >= grid_->chunkCount()) continue;
    if (std::find(requestedUnloads_.begin(), requestedUnloads_.end(),
                  chunkId) == requestedUnloads_.end()) {
      requestedUnloads_.push_back(chunkId);
    }
  }
  std::sort(requestedUnloads_.begin(), requestedUnloads_.end());
}

std::vector<int32_t> StreamScheduler::drainReady(int64_t budgetMs) {
  const std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
  std::vector<int32_t> drained;
  std::lock_guard<std::mutex> lock(mutex_);
  // 配额限流：正常每帧最多 1 块，burst 窗口内放宽。
  int32_t quota = 1;
  if (burstFrames_ > 0) {
    quota = burstPerFrame_;
    --burstFrames_;
  }
  while (quota > 0 && !ready_.empty()) {
    // 实测预算：每次取块前检查时间，预算耗尽推下帧
    // （budgetMs == 0 时首次比较即命中，直接返回空）。
    if (std::chrono::steady_clock::now() >= deadline) break;
    const int32_t chunkId = ready_.front();
    ready_.erase(ready_.begin());
    active_[chunkId] = true;
    drained.push_back(chunkId);
    --quota;
  }
  return drained;
}

void StreamScheduler::beginBurst(int32_t frames, int32_t perFrame) {
  std::lock_guard<std::mutex> lock(mutex_);
  burstFrames_ = std::max(frames, 0);
  burstPerFrame_ = std::max(perFrame, 1);
}

std::vector<int32_t> StreamScheduler::applyUnloads() {
  std::vector<int32_t> unloaded;
  std::lock_guard<std::mutex> lock(mutex_);
  const int32_t playerX = grid_->chunkXOf(lastPlayerChunkId_);
  const int32_t playerY = grid_->chunkYOf(lastPlayerChunkId_);
  auto beyondKeepRadius = [&](int32_t chunkId) {
    const int32_t distance =
        std::max(std::abs(grid_->chunkXOf(chunkId) - playerX),
                 std::abs(grid_->chunkYOf(chunkId) - playerY));
    return distance > config_.keepRadius;
  };

  // 请求卸载的分块：仅当确实离开滞后带才执行。
  std::vector<int32_t> retained;
  for (const int32_t chunkId : requestedUnloads_) {
    if (beyondKeepRadius(chunkId)) {
      unloaded.push_back(chunkId);
    } else {
      retained.push_back(chunkId);
    }
  }
  requestedUnloads_ = std::move(retained);

  // 活跃集合兜底扫描：超出滞后带的分块无论是否被请求都卸载，
  // 保证半径收缩（性能降级）后残留分块最终被清理。
  for (const auto& entry : active_) {
    if (beyondKeepRadius(entry.first) &&
        std::find(unloaded.begin(), unloaded.end(), entry.first) ==
            unloaded.end()) {
      unloaded.push_back(entry.first);
    }
  }

  unloaded = sortedUnique(std::move(unloaded));
  for (const int32_t chunkId : unloaded) {
    active_.erase(chunkId);
    ready_.erase(std::remove(ready_.begin(), ready_.end(), chunkId),
                 ready_.end());
    const bool wasPending =
        std::any_of(pendingLoads_.begin(), pendingLoads_.end(),
                    [chunkId](const LoadTask& task) {
                      return task.chunkId == chunkId;
                    });
    pendingLoads_.erase(
        std::remove_if(pendingLoads_.begin(), pendingLoads_.end(),
                       [chunkId](const LoadTask& task) {
                         return task.chunkId == chunkId;
                       }),
        pendingLoads_.end());
    // 锁外生成中的分块：保留 generating_ 登记，提交时凭作废标记丢弃；
    // 仅任务被取消（尚未开始生成）时才直接清除登记。
    const bool inGenerating =
        std::find(generating_.begin(), generating_.end(), chunkId) !=
        generating_.end();
    if (inGenerating) {
      if (wasPending) {
        removeGeneratingLocked(chunkId);
      } else if (std::find(cancelled_.begin(), cancelled_.end(), chunkId) ==
                 cancelled_.end()) {
        cancelled_.push_back(chunkId);
      }
    }
    chunkedTerrain_.requestUnloads({chunkId});
  }
  return unloaded;
}

std::vector<int32_t> StreamScheduler::loadRingSync(int32_t playerChunkId,
                                                   int32_t radius,
                                                   int32_t perfLodLevel) {
  std::vector<int32_t> generated;
  std::vector<int32_t> toGenerate;

  // 阶段一（锁内）：收集传送圈、取消旧任务与卸载申请、登记待生成。
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const int32_t count = grid_->chunkCount();
    if (playerChunkId < 0 || playerChunkId >= count) return generated;
    lastPlayerChunkId_ = playerChunkId;
    const int32_t clampedRadius = std::max(radius, 0);
    const int32_t centerX = grid_->chunkXOf(playerChunkId);
    const int32_t centerY = grid_->chunkYOf(playerChunkId);
    std::vector<int32_t> ring;
    for (int32_t y = centerY - clampedRadius; y <= centerY + clampedRadius;
         ++y) {
      for (int32_t x = centerX - clampedRadius; x <= centerX + clampedRadius;
           ++x) {
        if (x < 0 || x >= grid_->config().countX) continue;
        if (y < 0 || y >= grid_->config().countY) continue;
        ring.push_back(y * grid_->config().countX + x);
      }
    }
    ring = sortedUnique(std::move(ring));
    for (const int32_t chunkId : ring) {
      // 传送圈重新需要该分块：取消此前的卸载申请。
      requestedUnloads_.erase(
          std::remove(requestedUnloads_.begin(), requestedUnloads_.end(),
                      chunkId),
          requestedUnloads_.end());
      if (active_.count(chunkId) > 0) continue;
      const bool alreadyReady =
          std::find(ready_.begin(), ready_.end(), chunkId) != ready_.end();
      const bool alreadyGenerating =
          std::find(generating_.begin(), generating_.end(), chunkId) !=
          generating_.end();
      // 已就绪/生成中的保留在原队列，等待 drainReady 逐帧上传。
      if (alreadyReady || alreadyGenerating) continue;
      // 取消旧的待执行任务：传送路径按最新 LOD 重新产出。
      pendingLoads_.erase(
          std::remove_if(pendingLoads_.begin(), pendingLoads_.end(),
                         [chunkId](const LoadTask& task) {
                           return task.chunkId == chunkId;
                         }),
          pendingLoads_.end());
      generated.push_back(chunkId);
      if (chunkedTerrain_.hasChunk(chunkId)) {
        // CPU 缓存仍在：直接就绪。
        ready_.push_back(chunkId);
      } else {
        toGenerate.push_back(chunkId);
        generating_.push_back(chunkId);
      }
    }
    std::sort(ready_.begin(), ready_.end());
  }

  // 阶段二（锁外）：逐块生成网格，不阻塞渲染线程。
  std::vector<std::pair<int32_t, TerrainChunkCpuMesh>> built;
  built.reserve(toGenerate.size());
  for (const int32_t chunkId : toGenerate) {
    const uint32_t segments =
        chunkedTerrain_.segmentsFor(chunkId, playerChunkId, perfLodLevel);
    TerrainChunkCpuMesh entry;
    entry.segments = segments;
    entry.mesh = chunkedTerrain_.buildChunkMesh(chunkId, segments);
    const uint32_t rows = segments + 1u;
    entry.gridVertexCount = rows * rows;
    entry.skirtVertexCount = 8u * rows;
    built.emplace_back(chunkId, std::move(entry));
  }

  // 阶段三（锁内）：产出全部进入 Ready 队列（而非 active_），
  // 由渲染线程 drainReady 逐帧上传 GPU 后置活跃。
  if (!built.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& item : built) {
      const int32_t chunkId = item.first;
      removeGeneratingLocked(chunkId);
      if (takeCancelledLocked(chunkId)) continue;
      const bool alreadyReady =
          std::find(ready_.begin(), ready_.end(), chunkId) != ready_.end();
      if (active_.count(chunkId) > 0 || alreadyReady) continue;
      chunkedTerrain_.storeChunk(chunkId, std::move(item.second));
      ready_.push_back(chunkId);
    }
    std::sort(ready_.begin(), ready_.end());
  }
  return sortedUnique(std::move(generated));
}

std::vector<int32_t> StreamScheduler::unloadSync(
    const std::vector<int32_t>& chunkIds) {
  const std::vector<int32_t> ids = sortedUnique(chunkIds);
  std::vector<int32_t> unloaded;
  std::lock_guard<std::mutex> lock(mutex_);
  for (const int32_t chunkId : ids) {
    if (chunkId < 0 || chunkId >= grid_->chunkCount()) continue;
    const bool inGenerating =
        std::find(generating_.begin(), generating_.end(), chunkId) !=
        generating_.end();
    const bool tracked = active_.count(chunkId) > 0 ||
                         std::find(ready_.begin(), ready_.end(), chunkId) !=
                             ready_.end() ||
                         inGenerating || chunkedTerrain_.hasChunk(chunkId);
    if (!tracked) continue;
    active_.erase(chunkId);
    ready_.erase(std::remove(ready_.begin(), ready_.end(), chunkId),
                 ready_.end());
    const bool wasPending =
        std::any_of(pendingLoads_.begin(), pendingLoads_.end(),
                    [chunkId](const LoadTask& task) {
                      return task.chunkId == chunkId;
                    });
    pendingLoads_.erase(
        std::remove_if(pendingLoads_.begin(), pendingLoads_.end(),
                       [chunkId](const LoadTask& task) {
                         return task.chunkId == chunkId;
                       }),
        pendingLoads_.end());
    requestedUnloads_.erase(
        std::remove(requestedUnloads_.begin(), requestedUnloads_.end(),
                    chunkId),
        requestedUnloads_.end());
    // 锁外生成中的分块：任务尚未开始则直接清除登记；已开始生成
    // 则登记作废，提交时丢弃结果。
    if (inGenerating) {
      if (wasPending) {
        removeGeneratingLocked(chunkId);
      } else if (std::find(cancelled_.begin(), cancelled_.end(), chunkId) ==
                 cancelled_.end()) {
        cancelled_.push_back(chunkId);
      }
    }
    chunkedTerrain_.requestUnloads({chunkId});
    unloaded.push_back(chunkId);
  }
  return unloaded;
}

bool StreamScheduler::isActive(int32_t chunkId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_.count(chunkId) > 0;
}

bool StreamScheduler::isLoaded(int32_t chunkId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return chunkedTerrain_.hasChunk(chunkId);
}

size_t StreamScheduler::activeCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_.size();
}

size_t StreamScheduler::readyCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return ready_.size();
}

size_t StreamScheduler::pendingLoadCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pendingLoads_.size();
}

std::vector<int32_t> StreamScheduler::activeChunkIds() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<int32_t> ids;
  ids.reserve(active_.size());
  for (const auto& entry : active_) ids.push_back(entry.first);
  return ids;
}

const TerrainChunkCpuMesh* StreamScheduler::activeChunkMesh(
    int32_t chunkId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_.count(chunkId) == 0) return nullptr;
  return chunkedTerrain_.chunkAt(chunkId);
}

void StreamScheduler::removeGeneratingLocked(int32_t chunkId) {
  generating_.erase(std::remove(generating_.begin(), generating_.end(),
                                chunkId),
                    generating_.end());
}

bool StreamScheduler::takeCancelledLocked(int32_t chunkId) {
  const auto found =
      std::find(cancelled_.begin(), cancelled_.end(), chunkId);
  if (found == cancelled_.end()) return false;
  cancelled_.erase(found);
  return true;
}
