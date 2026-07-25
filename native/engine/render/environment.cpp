#include "environment.h"

#include <glm/geometric.hpp>

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
