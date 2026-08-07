#pragma once

#include "native/engine/render/chunked_terrain.h"

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

class TerrainHeightfield;
struct WorldGrid;

// 流式加载调度器（开放世界流式渲染 Phase 1）。
// 单 worker 线程消费加载请求，在后台做 CPU 重活（当前为分块地形
// 网格生成；任务类型上预留 GLB 解析扩展位）。产出的就绪区块进入
// Ready 队列，渲染线程每帧经 drainReady 最多取 1 个提交 GPU
// （默认 2ms 预算，超时推下帧）。
//
// 卸载滞后带：WorldGrid 半径边缘的分块不会立刻卸载，仅当分块
// 离开玩家分块 keepRadius（流式半径 + 1）时才真正卸载，防止
// 边界抖动导致反复加载/卸载。
//
// 同步模式（setSyncMode）：请求即时执行、无 worker 线程，供确定性
// 测试与传送同步准备使用；所有队列仍按分块 id 升序消费，同输入
// 同输出。
struct StreamSchedulerConfig {
  // 卸载滞后带半径（切比雪夫距离）：通常为流式半径 + 1。
  int32_t keepRadius = 3;
  // 渲染线程每帧取用 Ready 队列的时间预算（毫秒）。
  int64_t drainBudgetMs = 2;
};

class StreamScheduler {
 public:
  StreamScheduler(const TerrainHeightfield& terrain, const WorldGrid& grid,
                  StreamSchedulerConfig config = {});
  ~StreamScheduler();
  StreamScheduler(const StreamScheduler&) = delete;
  StreamScheduler& operator=(const StreamScheduler&) = delete;

  // 切换确定性同步模式：开启时停止 worker 并即时消费积压任务，
  // 关闭时重启 worker。
  void setSyncMode(bool enabled);
  bool syncMode() const;

  // 动态调整卸载滞后带半径（通常为当前流式半径 + 1，随性能降级收缩）。
  void setKeepRadius(int32_t radius);

  // 逻辑线程：转发 WorldGrid 本帧的加载/卸载请求。loads 升序入队；
  // unloads 进入滞后带评估，不立即卸载。perfLodLevel 取
  // PerformanceGuard::lodLevel()。
  void requestLoads(const std::vector<int32_t>& chunkIds, int32_t playerChunkId,
                    int32_t perfLodLevel);
  void requestUnloads(const std::vector<int32_t>& chunkIds);

  // 渲染线程：消费 Ready 队列，返回本次上传后置为活跃的分块 id
  // （升序）。正常配额每帧 1 块；burst 窗口内（beginBurst，如传送
  // 黑屏转场）放宽到 perFrame 块。每次取块前实测剩余时间，预算
  // 耗尽立即停止推下帧；无就绪时返回空。
  std::vector<int32_t> drainReady(int64_t budgetMs);
  std::vector<int32_t> drainReady() { return drainReady(config_.drainBudgetMs); }

  // 传送后临时放宽 drainReady 每帧配额 frames 帧（确定性：配额
  // 与帧数均为显式参数，不依赖时钟）。
  void beginBurst(int32_t frames, int32_t perFrame);

  // 卸载滞后带评估：返回本次真正卸载的分块 id（升序）。请求卸载的
  // 分块与任何仍超出 keepRadius 的活跃分块都会被移除；渲染线程凭
  // 返回值释放对应 GPU 资源。
  std::vector<int32_t> applyUnloads();

  // 传送同步路径：立即（当前线程）生成以 playerChunkId 为心、
  // radius 为切比雪夫半径的一圈分块，全部进入 Ready 队列由渲染
  // 线程 drainReady 逐帧上传后置活跃；返回本次新产出的分块 id
  // （升序）。锁外生成网格，不阻塞渲染线程。
  std::vector<int32_t> loadRingSync(int32_t playerChunkId, int32_t radius,
                                    int32_t perfLodLevel);
  // 同步卸载指定分块（升序去重），返回实际卸载的 id。
  std::vector<int32_t> unloadSync(const std::vector<int32_t>& chunkIds);

  bool isActive(int32_t chunkId) const;
  bool isLoaded(int32_t chunkId) const;
  size_t activeCount() const;
  size_t readyCount() const;
  size_t pendingLoadCount() const;
  std::vector<int32_t> activeChunkIds() const;
  // CPU 端网格访问（仅活跃分块有效，渲染线程上传用）。
  const TerrainChunkCpuMesh* activeChunkMesh(int32_t chunkId) const;
  const ChunkedTerrain& chunkedTerrain() const { return chunkedTerrain_; }
  const StreamSchedulerConfig& config() const { return config_; }

 private:
  enum class TaskKind : int32_t {
    TerrainChunk = 0,
    // 预留：GLB 模型解析任务（Phase 2 接入）。
    ModelGlb = 1,
  };

  struct LoadTask {
    TaskKind kind = TaskKind::TerrainChunk;
    int32_t chunkId = -1;
    int32_t playerChunkId = -1;
    int32_t perfLodLevel = 0;
    // ModelGlb 任务预留负载。
    std::vector<uint8_t> bytes;
  };

  void workerLoop();
  // 执行单个任务；TerrainChunk 生成分块网格，ModelGlb 暂未实现直接丢弃。
  // 三段式：锁内查重/登记 → 锁外生成网格 → 锁内提交进 Ready。
  void executeTask(const LoadTask& task);
  // 立即处理一个加载任务并进入 Ready 队列（同步模式/传送路径共用）。
  void processTaskInline(const LoadTask& task);
  void stopWorker();

  // 以下两个辅助均要求调用方已持 mutex_。
  // 从 generating_ 移除（锁外生成结束/作废时登记清理）。
  void removeGeneratingLocked(int32_t chunkId);
  // 取走并清除 cancelled_ 中的作废标记（生成期间被请求卸载）。
  bool takeCancelledLocked(int32_t chunkId);

  const WorldGrid* grid_;
  ChunkedTerrain chunkedTerrain_;
  StreamSchedulerConfig config_;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::thread worker_;
  bool syncMode_ = false;
  bool stopping_ = false;

  // 待执行任务队列（锁保护，worker 消费）。
  std::vector<LoadTask> pendingLoads_;
  // 已生成待上传区块（锁保护，渲染线程 drain）。
  std::vector<int32_t> ready_;
  // 已上传/活跃区块（drain 移入；卸载滞后带评估对象）。
  std::map<int32_t, bool> active_;
  // 请求卸载但尚在滞后带内的区块。
  std::vector<int32_t> requestedUnloads_;
  // 正在锁外生成网格的分块（避免重复入队；卸载时转作废标记）。
  std::vector<int32_t> generating_;
  // 锁外生成期间被请求卸载的分块：提交时丢弃结果。
  std::vector<int32_t> cancelled_;
  // burst 窗口（传送后放宽上传配额）。
  int32_t burstFrames_ = 0;
  int32_t burstPerFrame_ = 1;
  int32_t lastPlayerChunkId_ = 0;
};
