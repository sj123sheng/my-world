#pragma once

#include <glm/vec2.hpp>

struct WaterBody {
  glm::vec2 center{0.5f, 0.5f};
  glm::vec2 halfExtents{0.1f, 0.1f};
  float level = -0.045f;
  float shoreWidth = 0.012f;

  bool contains(glm::vec2 point) const;
  // 0=开阔水面，1=岸线边缘；水体外恒为 0。
  float shoreFactor(glm::vec2 point) const;
};
