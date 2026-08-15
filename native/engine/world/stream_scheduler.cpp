// stream_scheduler.cpp: 无限世界 ChunkCoord CPU 分块流送调度器。

#include "native/engine/world/stream_scheduler.h"

#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

namespace {

std::vector<ChunkCoord> stableUnique(const std::vector<ChunkCoord> &coords) {
  std::set<ChunkCoord> seen;
  std::vector<ChunkCoord> unique;
  unique.reserve(coords.size());
  for (const ChunkCoord coord : coords) {
    if (seen.insert(coord).second)
      unique.push_back(coord);
  }
  return unique;
}

uint64_t unsignedDistance(int64_t lhs, int64_t rhs) noexcept {
  return lhs >= rhs ? static_cast<uint64_t>(lhs) - static_cast<uint64_t>(rhs)
                    : static_cast<uint64_t>(rhs) - static_cast<uint64_t>(lhs);
}

std::vector<ChunkCoord> safeRingCoords(ChunkCoord center) {
  const int64_t minSafe = std::numeric_limits<int64_t>::min() + 1;
  const int64_t maxSafe = std::numeric_limits<int64_t>::max() - 1;
  center.x = std::clamp(center.x, minSafe, maxSafe);
  center.y = std::clamp(center.y, minSafe, maxSafe);
  std::vector<ChunkCoord> coords;
  coords.reserve(9);
  coords.push_back(center);
  for (int64_t dy = -1; dy <= 1; ++dy) {
    for (int64_t dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0)
        continue;
      coords.push_back({center.x + dx, center.y + dy});
    }
  }
  return coords;
}

} // namespace

StreamScheduler::StreamScheduler(const TerrainHeightfield &terrain,
                                 const WorldGrid &grid,
                                 StreamSchedulerConfig config,
                                 ChunkBuilder chunkBuilder)
    : chunkedTerrain_(terrain), chunkBuilder_(std::move(chunkBuilder)),
      config_(config) {
  (void)grid;
  setKeepRadius(config_.activeRadius, config_.cacheRings);
  if (config_.drainBudgetMs < 0)
    config_.drainBudgetMs = 0;
  worker_ = std::thread([this] { workerLoop(); });
}

TerrainChunkCpuMesh StreamScheduler::buildChunk(ChunkCoord coord,
                                                uint32_t segments) const {
  return chunkBuilder_ ? chunkBuilder_(coord, segments)
                       : chunkedTerrain_.buildChunkMesh(coord, segments);
}

StreamScheduler::~StreamScheduler() { stopWorker(true); }

void StreamScheduler::stopWorker(bool cancelOutstanding) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    if (cancelOutstanding) {
      for (auto &item : loadTokens_)
        item.second->store(true);
      pendingLoads_.clear();
      loadTokens_.clear();
    }
  }
  cv_.notify_all();
  if (worker_.joinable())
    worker_.join();
}

void StreamScheduler::setSyncMode(bool enabled) {
  if (enabled) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (syncMode_)
        return;
      syncMode_ = true;
      stopping_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable())
      worker_.join();
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!syncMode_)
    return;
  syncMode_ = false;
  stopping_ = false;
  worker_ = std::thread([this] { workerLoop(); });
}

bool StreamScheduler::syncMode() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return syncMode_;
}

void StreamScheduler::setKeepRadius(int32_t activeRadius, int32_t cacheRings) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.activeRadius = std::max(activeRadius, 0);
  config_.cacheRings = std::max(cacheRings, 0);
}

void StreamScheduler::workerLoop() {
  for (;;) {
    LoadTask task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stopping_ || !pendingLoads_.empty(); });
      if (pendingLoads_.empty()) {
        if (stopping_)
          return;
        continue;
      }
      task = std::move(pendingLoads_.front());
      pendingLoads_.erase(pendingLoads_.begin());
    }
    try {
      executeTask(task);
    } catch (...) {
      // 自定义 builder 属于调度器边界外代码；异常不得逃出 worker 触发
      // std::terminate。网格尚未提交，只需释放本次 token 允许重试。
      std::lock_guard<std::mutex> lock(mutex_);
      releaseTokenLocked(task);
    }
  }
}

bool StreamScheduler::tokenIsCurrentLocked(const LoadTask &task) const {
  const auto found = loadTokens_.find(task.coord);
  return found != loadTokens_.end() && found->second == task.cancelled &&
         !task.cancelled->load();
}

void StreamScheduler::releaseTokenLocked(const LoadTask &task) {
  const auto found = loadTokens_.find(task.coord);
  if (found != loadTokens_.end() && found->second == task.cancelled) {
    loadTokens_.erase(found);
  }
}

void StreamScheduler::executeTask(const LoadTask &task) {
  if (task.kind == TaskKind::ModelGlb) {
    std::lock_guard<std::mutex> lock(mutex_);
    releaseTokenLocked(task);
    return;
  }

  bool needsGeneration = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tokenIsCurrentLocked(task) || active_.count(task.coord) > 0 ||
        std::find(ready_.begin(), ready_.end(), task.coord) != ready_.end()) {
      releaseTokenLocked(task);
      return;
    }
    needsGeneration = !chunkedTerrain_.hasChunk(task.coord);
  }

  TerrainChunkCpuMesh built;
  if (needsGeneration) {
    const uint32_t segments = chunkedTerrain_.segmentsFor(
        task.coord, task.playerChunk, task.perfLodLevel);
    built = buildChunk(task.coord, segments);
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!tokenIsCurrentLocked(task) || active_.count(task.coord) > 0 ||
      std::find(ready_.begin(), ready_.end(), task.coord) != ready_.end()) {
    releaseTokenLocked(task);
    return;
  }
  if (needsGeneration) {
    chunkedTerrain_.storeChunk(task.coord, std::move(built));
  }
  ready_.push_back(task.coord);
  releaseTokenLocked(task);
}

void StreamScheduler::requestLoads(const std::vector<ChunkCoord> &coords,
                                   ChunkCoord playerChunk,
                                   int32_t perfLodLevel) {
  const std::vector<ChunkCoord> unique = stableUnique(coords);
  std::vector<LoadTask> inlineTasks;
  bool sync = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    lastPlayerChunk_ = playerChunk;
    sync = syncMode_;
    for (const ChunkCoord coord : unique) {
      requestedUnloads_.erase(coord);
      if (active_.count(coord) > 0 ||
          std::find(ready_.begin(), ready_.end(), coord) != ready_.end()) {
        continue;
      }
      const auto existing = loadTokens_.find(coord);
      if (existing != loadTokens_.end() && !existing->second->load())
        continue;

      LoadTask task;
      task.coord = coord;
      task.playerChunk = playerChunk;
      task.perfLodLevel = perfLodLevel;
      task.cancelled = std::make_shared<std::atomic_bool>(false);
      loadTokens_[coord] = task.cancelled;
      if (sync) {
        inlineTasks.push_back(std::move(task));
      } else {
        pendingLoads_.push_back(std::move(task));
      }
    }
  }

  if (sync) {
    for (const LoadTask &task : inlineTasks)
      executeTask(task);
  } else if (!unique.empty()) {
    cv_.notify_one();
  }
}

std::vector<ChunkCoord>
StreamScheduler::requestUnloads(const std::vector<ChunkCoord> &coords) {
  const std::vector<ChunkCoord> unique = stableUnique(coords);
  std::lock_guard<std::mutex> lock(mutex_);
  for (const ChunkCoord coord : unique) {
    requestedUnloads_.insert(coord);
    const auto token = loadTokens_.find(coord);
    if (token != loadTokens_.end())
      token->second->store(true);
  }
  return unique;
}

std::vector<ChunkCoord> StreamScheduler::drainReady(int64_t budgetMs) {
  if (budgetMs <= 0)
    return {};
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
  std::vector<ChunkCoord> drained;
  std::lock_guard<std::mutex> lock(mutex_);
  int32_t quota = 1;
  if (burstFrames_ > 0) {
    quota = burstPerFrame_;
    --burstFrames_;
  }
  while (quota > 0 && !ready_.empty() &&
         std::chrono::steady_clock::now() < deadline) {
    const ChunkCoord coord = ready_.front();
    ready_.erase(ready_.begin());
    active_.insert(coord);
    drained.push_back(coord);
    --quota;
  }
  return drained;
}

void StreamScheduler::beginBurst(int32_t frames, int32_t perFrame) {
  std::lock_guard<std::mutex> lock(mutex_);
  burstFrames_ = std::max(frames, 0);
  burstPerFrame_ = std::max(perFrame, 1);
}

void StreamScheduler::unloadLocked(ChunkCoord coord) {
  active_.erase(coord);
  ready_.erase(std::remove(ready_.begin(), ready_.end(), coord), ready_.end());
  pendingLoads_.erase(std::remove_if(pendingLoads_.begin(), pendingLoads_.end(),
                                     [coord](const LoadTask &task) {
                                       if (task.coord != coord)
                                         return false;
                                       task.cancelled->store(true);
                                       return true;
                                     }),
                      pendingLoads_.end());
  const auto token = loadTokens_.find(coord);
  if (token != loadTokens_.end()) {
    token->second->store(true);
    loadTokens_.erase(token);
  }
  requestedUnloads_.erase(coord);
  chunkedTerrain_.requestUnloads({coord});
}

std::vector<ChunkCoord> StreamScheduler::applyUnloads() {
  std::lock_guard<std::mutex> lock(mutex_);
  const uint64_t keepRadius = static_cast<uint64_t>(config_.activeRadius) +
                              static_cast<uint64_t>(config_.cacheRings);
  auto beyondKeepRadius = [&](ChunkCoord coord) {
    return std::max(unsignedDistance(coord.x, lastPlayerChunk_.x),
                    unsignedDistance(coord.y, lastPlayerChunk_.y)) > keepRadius;
  };

  std::set<ChunkCoord> candidates = requestedUnloads_;
  candidates.insert(active_.begin(), active_.end());
  candidates.insert(ready_.begin(), ready_.end());
  for (const LoadTask &task : pendingLoads_)
    candidates.insert(task.coord);
  for (const auto &item : loadTokens_)
    candidates.insert(item.first);
  const std::vector<ChunkCoord> cached = chunkedTerrain_.chunkCoords();
  candidates.insert(cached.begin(), cached.end());

  std::vector<ChunkCoord> unloaded;
  for (const ChunkCoord coord : candidates) {
    if (beyondKeepRadius(coord))
      unloaded.push_back(coord);
  }
  for (const ChunkCoord coord : unloaded)
    unloadLocked(coord);
  return unloaded;
}

std::vector<ChunkCoord>
StreamScheduler::loadSafeRingSync(ChunkCoord landingChunk,
                                  int32_t perfLodLevel) {
  const std::vector<ChunkCoord> coords = safeRingCoords(landingChunk);
  std::vector<LoadTask> tasks;
  tasks.reserve(coords.size());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    lastPlayerChunk_ = landingChunk;
    for (const ChunkCoord coord : coords) {
      unloadLocked(coord);
      LoadTask task;
      task.coord = coord;
      task.playerChunk = landingChunk;
      task.perfLodLevel = perfLodLevel;
      task.cancelled = std::make_shared<std::atomic_bool>(false);
      loadTokens_[coord] = task.cancelled;
      tasks.push_back(std::move(task));
    }
  }

  std::vector<std::pair<LoadTask, TerrainChunkCpuMesh>> built;
  built.reserve(tasks.size());
  try {
    for (const LoadTask &task : tasks) {
      const uint32_t segments =
          chunkedTerrain_.segmentsFor(task.coord, landingChunk, perfLodLevel);
      TerrainChunkCpuMesh entry = buildChunk(task.coord, segments);
      if (entry.mesh.vertices.empty()) {
        entry = chunkedTerrain_.buildFlatFallbackChunk(
            task.coord, chunkedTerrain_.config().farSegments);
      }
      built.emplace_back(task, std::move(entry));
    }
  } catch (...) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const LoadTask& task : tasks) releaseTokenLocked(task);
    throw;
  }

  std::vector<ChunkCoord> committed;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto &item : built) {
    const LoadTask &task = item.first;
    if (!tokenIsCurrentLocked(task)) {
      releaseTokenLocked(task);
      continue;
    }
    chunkedTerrain_.storeChunk(task.coord, std::move(item.second));
    committed.push_back(task.coord);
    releaseTokenLocked(task);
  }
  // 传送安全圈必须先于旧位置遗留的普通 Ready 队列激活；否则即使 burst
  // 配额为 9，也可能持续消费旧块而让遮罩提前超时或长期不恢复。
  ready_.insert(ready_.begin(), committed.begin(), committed.end());
  return committed;
}

bool StreamScheduler::isActive(ChunkCoord coord) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_.count(coord) > 0;
}

bool StreamScheduler::isLoaded(ChunkCoord coord) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return chunkedTerrain_.hasChunk(coord);
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

size_t StreamScheduler::cachedChunkCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return chunkedTerrain_.chunkCount();
}

std::vector<ChunkCoord> StreamScheduler::activeChunkIds() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {active_.begin(), active_.end()};
}

const TerrainChunkCpuMesh *
StreamScheduler::activeChunkMesh(ChunkCoord coord) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_.count(coord) == 0)
    return nullptr;
  return chunkedTerrain_.chunkAt(coord);
}
