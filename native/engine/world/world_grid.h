#pragma once

#include "native/engine/math/vec2.h"

#include <cstdint>
#include <vector>

// 世界分块流式加载系统（开放世界探索基础）。
// 世界为 [0,1]x[0,1] 的逻辑平面，被均匀切分为 countX*countY 个分块。
// 按玩家所在分块与流式半径维护激活分块集合：进入半径的分块请求
// 加载，离开半径的分块请求卸载。所有序列按分块 id 升序确定性排序，
// 同输入下加/卸载顺序可重现，便于回归测试与资产提交校验。
struct WorldGridConfig {
  int32_t countX = 8;
  int32_t countY = 8;
  // 流式半径（分块数）：激活集合为曼哈顿邻域近似——
  // 玩家所在分块向四周扩展 radius 的矩形窗口。
  int32_t streamingRadius = 2;
};

struct WorldGrid {
  explicit WorldGrid(WorldGridConfig config = {});

  // 按玩家位置更新激活分块集合。返回 true 表示集合发生变化。
  bool updateStreaming(Vec2 playerPosition);

  // 动态调整流式半径（性能降级时缩小激活窗口）；非法值被钳制。
  // 返回 true 表示半径发生变化。
  bool setStreamingRadius(int32_t radius);

  // 本帧需要新加载的分块 id（升序）。仅在 updateStreaming 返回 true
  // 后的当帧有效。
  const std::vector<int32_t>& pendingLoads() const { return pendingLoads_; }
  // 本帧需要卸载的分块 id（升序）。
  const std::vector<int32_t>& pendingUnloads() const {
    return pendingUnloads_;
  }
  // 当前激活的分块 id（升序）。
  const std::vector<int32_t>& activeChunks() const { return active_; }

  int32_t chunkIndexAt(Vec2 position) const;
  int32_t chunkCount() const { return config_.countX * config_.countY; }
  float chunkSizeX() const { return 1.0f / static_cast<float>(config_.countX); }
  float chunkSizeY() const { return 1.0f / static_cast<float>(config_.countY); }
  const WorldGridConfig& config() const { return config_; }

 private:
  WorldGridConfig config_;
  std::vector<int32_t> active_;
  std::vector<int32_t> pendingLoads_;
  std::vector<int32_t> pendingUnloads_;
};
