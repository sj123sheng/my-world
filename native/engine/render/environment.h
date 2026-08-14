#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <vector>

#include "native/engine/world/world_position.h"

struct EnvironmentComposition {
  glm::vec3 spawn;
  glm::vec3 foregroundOccluder;
  glm::vec3 combatAnchor;
  glm::vec3 altarAnchor;
  glm::vec3 cameraFocus;
};

class EnvironmentController {
 public:
  static EnvironmentComposition defaultComposition();
};

// 相对原点渲染（无限自然世界 Task 9）：渲染空间以 origin 分块角点为
// 原点，玩家锚定在 originLocal；target 分块的平移为整数分块差。
// 分块差先按 long double 计算再转 float，避免 10^12 级坐标先转 float
// 造成的精度塌缩；差值超出 float 安全整数范围时钳制，输出恒有限。
glm::vec3 ChunkRenderTranslation(ChunkCoord target, ChunkCoord origin,
                                 LocalPosition originLocal);

// 目标块是否允许提交 GPU 网格：切比雪夫距离不超过活动半径。
// 超出活动半径的块（画质收缩/传送后的残留 Active）不进入渲染映射。
bool ChunkRenderCommittable(ChunkCoord target, ChunkCoord origin,
                            int32_t activeRadius);

// 区块回收差量：同步移除按 ChunkCoord 键控的渲染资源表条目
//（地形网格与程序植被批次共用同一差量，保证卸载块无残留 GPU 键）。
// 返回实际移除条目数；重复应用同一差量幂等。
template <typename ChunkResourceMap>
size_t EraseUnloadedChunkResources(
    const std::vector<ChunkCoord>& unloaded, ChunkResourceMap& resources) {
  size_t removed = 0;
  for (const ChunkCoord coord : unloaded) {
    removed += resources.erase(coord);
  }
  return removed;
}
