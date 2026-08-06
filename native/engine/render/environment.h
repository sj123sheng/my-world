#pragma once

#include "native/engine/render/static_model.h"
#include "native/engine/world/environment_collision.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <cstdint>
#include <cstddef>

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

struct EnvironmentRenderPlan {
  bool outerRing = true;
  bool centerRift = true;
  bool backdrop = true;
  bool decoration = true;
  StaticTextureTier textureTier = StaticTextureTier::Full;
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
