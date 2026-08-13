#pragma once

#include "native/engine/math/vec2.h"
#include "native/engine/world/world_position.h"

#include <cstdint>
#include <vector>

// 无限世界活动网格配置。活动区负责当前可交互分块，缓存区在活动区外
// 额外保留若干圈，避免玩家在边缘移动时立即卸载刚离开的分块。
struct WorldGridConfig {
  int32_t activeRadius = 4;
  int32_t cacheRings = 2;
};

struct WorldGrid {
  explicit WorldGrid(WorldGridConfig config = {});

  // 更新玩家所在无限分块及加载方向。返回 true 表示活动区或缓存区变化。
  // 加载顺序固定为中心、切比雪夫圈、合成前向、坐标。
  bool updateStreaming(ChunkCoord playerChunk, Vec2 cameraForward,
                       Vec2 movement);

  const std::vector<ChunkCoord>& pendingLoads() const {
    return pendingLoads_;
  }
  const std::vector<ChunkCoord>& pendingUnloads() const {
    return pendingUnloads_;
  }
  const std::vector<ChunkCoord>& activeChunks() const { return active_; }
  const std::vector<ChunkCoord>& cachedChunks() const { return cached_; }

  // 质量档位：0=高、1=中、2及以上=低；未知负值按高画质处理。
  static int32_t ActiveRadiusForQuality(int32_t qualityPreset);

  const WorldGridConfig& config() const { return config_; }

 private:
  WorldGridConfig config_;
  std::vector<ChunkCoord> active_;
  std::vector<ChunkCoord> cached_;
  std::vector<ChunkCoord> pendingLoads_;
  std::vector<ChunkCoord> pendingUnloads_;
};
