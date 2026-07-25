#pragma once

#include "native/engine/render/static_model.h"

#include <glm/vec2.hpp>

#include <cstdint>

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

class EnvironmentController {
 public:
  EnvironmentRenderPlan evaluate(glm::vec2 cameraTarget,
                                 int32_t perfLevel) const;
};
