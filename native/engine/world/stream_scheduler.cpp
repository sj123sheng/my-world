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

bool tryOffset(int64_t value, int64_t offset, int64_t *result) noexcept {
  if ((offset > 0 && value > std::numeric_limits<int64_t>::max() - offset) ||
      (offset < 0 && value < std::numeric_limits<int64_t>::min() - offset)) {
    return false;
  }
  *result = value + offset;
  return true;
}

std::vector<ChunkCoord> safeRingCoords(ChunkCoord center) {
  std::vector<ChunkCoord> coords;
  coords.reserve(9);
  coords.push_back(center);
  for (int64_t dy = -1; dy <= 1; ++dy) {
    int64_t y = 0;
    if (!tryOffset(center.y, dy, &y))
      continue;
    for (int64_t dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0)
        continue;
      int64_t x = 0;
      if (tryOffset(center.x, dx, &x))
        coords.push_back({x, y});
    }
  }
  return coords;
}

TerrainChunkCpuMesh buildFlatFallback(ChunkCoord coord, uint32_t segments) {
  TerrainChunkCpuMesh entry;
  entry.coord = coord;
  entry.segments = std::max(segments, 1u);
  const uint32_t rows = entry.segments + 1u;
  entry.gridVertexCount = rows * rows;
  entry.mesh.vertices.reserve(entry.gridVertexCount);
  entry.mesh.indices.reserve(static_cast<size_t>(entry.segments) *
                             entry.segments * 6u);

  for (uint32_t y = 0; y < rows; ++y) {
    const float fy = static_cast<float>(y) / entry.segments;
    for (uint32_t x = 0; x < rows; ++x) {
      const float fx = static_cast<float>(x) / entry.segments;
      entry.mesh.vertices.push_back(
          {{fx, 0.0f, fy}, {0.0f, 1.0f, 0.0f}, {fx, fy}});
    }
  }
  for (uint32_t y = 0; y < entry.segments; ++y) {
    for (uint32_t x = 0; x < entry.segments; ++x) {
      const uint32_t a = y * rows + x;
      const uint32_t b = a + 1u;
      const uint32_t c = a + rows;
      const uint32_t d = c + 1u;
      entry.mesh.indices.insert(entry.mesh.indices.end(), {a, c, b, b, c, d});
    }
  }
  return entry;
}

} // namespace

StreamScheduler::StreamScheduler(const TerrainHeightfield &terrain,
                                 const WorldGrid &grid,
                                 StreamSchedulerConfig config)
    : chunkedTerrain_(terrain), config_(config) {
  (void)grid;
  setKeepRadius(config_.activeRadius, config_.cacheRings);
  if (config_.drainBudgetMs < 0)
    config_.drainBudgetMs = 0;
  worker_ = std::thread([this] { workerLoop(); });
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
    executeTask(task);
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
    built = chunkedTerrain_.buildChunkMesh(task.coord, segments);
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
  for (const LoadTask &task : tasks) {
    const uint32_t segments =
        chunkedTerrain_.segmentsFor(task.coord, landingChunk, perfLodLevel);
    TerrainChunkCpuMesh entry =
        chunkedTerrain_.buildChunkMesh(task.coord, segments);
    if (entry.mesh.vertices.empty()) {
      entry =
          buildFlatFallback(task.coord, chunkedTerrain_.config().farSegments);
    }
    built.emplace_back(task, std::move(entry));
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
    ready_.push_back(task.coord);
    committed.push_back(task.coord);
    releaseTokenLocked(task);
  }
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
