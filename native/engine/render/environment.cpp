#include "environment.h"

#include <glm/geometric.hpp>

EnvironmentComposition EnvironmentController::defaultComposition() {
  return {{0.50f, 0.0f, 0.12f},
          {0.34f, 0.0f, 0.26f},
          {0.50f, 0.0f, 0.48f},
          {0.50f, 0.0f, 0.75f},
          {0.50f, 0.12f, 0.82f}};
}

EnvironmentRenderPlan EnvironmentController::evaluate(
    glm::vec2 cameraTarget, int32_t perfLevel) const {
  EnvironmentRenderPlan plan;
  const glm::vec2 centerDelta = cameraTarget - glm::vec2(0.5f, 0.75f);
  if (glm::dot(centerDelta, centerDelta) <= 0.24f * 0.24f) {
    plan.decoration = false;
  }
  if (perfLevel >= 2) {
    plan.backdrop = false;
  }
  if (perfLevel >= 3) {
    plan.decoration = false;
  }
  if (perfLevel >= 4) {
    plan.textureTier = StaticTextureTier::Half;
  }
  return plan;
}
