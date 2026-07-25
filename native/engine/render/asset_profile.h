#pragma once

#include "native/engine/render/render_animation.h"

#include <glm/vec3.hpp>

#include <cstdint>

struct AssetProfile {
  float scale = 1.0f;
  float yawOffsetRadians = 0.0f;
  glm::vec3 materialTint{1.0f};
  glm::vec3 outlineColor{0.0f};
  float outlineStrength = 0.0f;
  uint8_t coreMountCount = 0;

  static AssetProfile forModel(ModelKind kind);
};
