#pragma once

#include "native/engine/render/chunked_terrain.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

class TerrainHeightfield;
struct WorldGrid;

// 无限世界 CPU 分块流送调度器。单 worker 按调用方给出的稳定顺序消费
// ChunkCoord；网格生成在锁外执行，所有队列、状态与 CPU cache 在同一把锁下
// 提交或回收。渲染线程经 drainReady 将 Ready 逐帧转为 Active。
struct StreamSchedulerConfig {
  int32_t activeRadius = 4;
  int32_t cacheRings = 2;
  int64_t drainBudgetMs = 2;
};

class StreamScheduler {
public:
  StreamScheduler(const TerrainHeightfield &terrain, const WorldGrid &grid,
                  StreamSchedulerConfig config = {});
  ~StreamScheduler();
  StreamScheduler(const StreamScheduler &) = delete;
  StreamScheduler &operator=(const StreamScheduler &) = delete;

  // 同步模式用于确定性宿主测试；开启时等待既有后台队列消费完毕，之后
  // requestLoads 在调用线程按输入顺序执行。析构则取消未开始任务并等待当前
  // 有界网格生成结束。
  void setSyncMode(bool enabled);
  bool syncMode() const;

  // 真正保留半径为 activeRadius + cacheRings；负值按 0，求和使用无符号
  // 宽类型，避免半径与 int64 坐标距离判断溢出。
  void setKeepRadius(int32_t activeRadius, int32_t cacheRings = 2);

  // 加载去重稳定保留 coords 的首见顺序，不覆盖 WorldGrid 已排好的方向优先级。
  void requestLoads(const std::vector<ChunkCoord> &coords,
                    ChunkCoord playerChunk, int32_t perfLodLevel);
  // 记录卸载意图并返回本次稳定去重后的坐标；真正回收由 applyUnloads 在
  // keep 半径之外执行。
  std::vector<ChunkCoord> requestUnloads(const std::vector<ChunkCoord> &coords);

  std::vector<ChunkCoord> drainReady(int64_t budgetMs);
  std::vector<ChunkCoord> drainReady() {
    return drainReady(config_.drainBudgetMs);
  }
  void beginBurst(int32_t frames, int32_t perFrame);

  // 回收所有超出 keep 半径的 Ready、Active、pending/in-flight 与 CPU cache；
  // 返回值按 ChunkCoord 字典序稳定。
  std::vector<ChunkCoord> applyUnloads();

  // 同步生成固定 3x3 安全圈：中心第一，其余按西北到东南的行序（跳过
  // 中心）排列。正常网格为空时以 farSegments 平坦网格兜底；成功提交的
  // 九块进入 Ready，等待调用方 drain 后成为 Active。
  std::vector<ChunkCoord> loadSafeRingSync(ChunkCoord landingChunk,
                                           int32_t perfLodLevel);

  bool isActive(ChunkCoord coord) const;
  bool isLoaded(ChunkCoord coord) const;
  size_t activeCount() const;
  size_t readyCount() const;
  size_t pendingLoadCount() const;
  std::vector<ChunkCoord> activeChunkIds() const;
  const TerrainChunkCpuMesh *activeChunkMesh(ChunkCoord coord) const;
  const ChunkedTerrain &chunkedTerrain() const { return chunkedTerrain_; }
  const StreamSchedulerConfig &config() const { return config_; }

private:
  enum class TaskKind : int32_t {
    TerrainChunk = 0,
    ModelGlb = 1,
  };

  struct LoadTask {
    TaskKind kind = TaskKind::TerrainChunk;
    ChunkCoord coord{};
    ChunkCoord playerChunk{};
    int32_t perfLodLevel = 0;
    std::shared_ptr<std::atomic_bool> cancelled;
    std::vector<uint8_t> bytes;
  };

  void workerLoop();
  void executeTask(const LoadTask &task);
  void stopWorker(bool cancelOutstanding);
  bool tokenIsCurrentLocked(const LoadTask &task) const;
  void releaseTokenLocked(const LoadTask &task);
  void unloadLocked(ChunkCoord coord);

  ChunkedTerrain chunkedTerrain_;
  StreamSchedulerConfig config_;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::thread worker_;
  bool syncMode_ = false;
  bool stopping_ = false;

  std::vector<LoadTask> pendingLoads_;
  std::vector<ChunkCoord> ready_;
  std::set<ChunkCoord> active_;
  std::set<ChunkCoord> requestedUnloads_;
  // 每个最新请求独占取消令牌。真正卸载时令牌置位并从映射删除；worker
  // 仍持 shared_ptr，可在锁外生成结束后可靠拒绝迟交，而不留下取消墓碑。
  std::map<ChunkCoord, std::shared_ptr<std::atomic_bool>> loadTokens_;
  int32_t burstFrames_ = 0;
  int32_t burstPerFrame_ = 1;
  ChunkCoord lastPlayerChunk_{};
};
