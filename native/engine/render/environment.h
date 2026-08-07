#pragma once

#include "native/engine/render/static_model.h"
#include "native/engine/world/environment_collision.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <vector>

enum class EnvironmentBatchKind : uint8_t {
  OuterRing = 0,
  CenterRift = 1,
  Backdrop = 2,
  Decoration = 3,
};

enum class EnvironmentBatchStatus : uint8_t {
  Empty = 0,
  Pending = 1,
  Ready = 2,
  Failed = 3,
};

// 流式分组（Phase 2）：环境布局按 blockId 分组——-1 为全局组（始终
// 启用），≥0 的区块批次仅在对应 8×8 分块激活时绘制。
constexpr int32_t kEnvironmentGlobalBlockId = -1;
constexpr int32_t kEnvironmentBlockColumns = 8;
constexpr int32_t kEnvironmentBlockRows = 8;
constexpr int32_t kEnvironmentBlockCount =
    kEnvironmentBlockColumns * kEnvironmentBlockRows;

// 世界坐标 → 分块 id（id = y * columns + x），越界坐标钳制到边缘分块。
int32_t environmentBlockIdAt(float worldX, float worldZ);

struct EnvironmentRenderPlan {
  bool outerRing = true;
  bool centerRift = true;
  bool backdrop = true;
  bool decoration = true;
  StaticTextureTier textureTier = StaticTextureTier::Full;
  // 激活分块集合（升序）：区块批次（blockId≥0）仅在其中时绘制；
  // 全局批次不受影响。由渲染层从流式调度器的活跃分块填充。
  std::vector<int32_t> activeBlocks;

  bool blockActive(int32_t blockId) const {
    return std::binary_search(activeBlocks.begin(), activeBlocks.end(),
                              blockId);
  }
};

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
  EnvironmentRenderPlan evaluate(glm::vec2 cameraTarget,
                                 int32_t perfLevel) const;
};

// 环境批次的世界适配模型矩阵：布局米制 → [0,1] 世界的相似变换，
// 参数与碰撞层 environmentWorldFitForRegion 同源，保证可见建筑
// 与碰撞体严格对齐。index 对应 EnvironmentBatchKind。
EnvironmentWorldFit environmentWorldFitParams(size_t index,
                                              const EnvironmentComposition& composition);
glm::mat4 environmentWorldFitMatrix(size_t index,
                                    const EnvironmentComposition& composition);

// 区块批次（blockId≥0）的世界适配矩阵：统一沿用 OuterRing 参数
//（世界中心锚点，与碰撞层 fromEnvironmentLayout 对区块条目的处理一致）。
glm::mat4 environmentBlockWorldFitMatrix(const EnvironmentComposition& composition);
