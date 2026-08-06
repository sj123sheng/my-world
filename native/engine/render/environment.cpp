#include "environment.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

EnvironmentWorldFit environmentWorldFitParams(
    size_t index, const EnvironmentComposition& composition) {
  const EnvironmentFitAnchors anchors{composition.altarAnchor.x,
                                      composition.altarAnchor.z};
  return environmentWorldFitForRegion(static_cast<int>(index), anchors);
}

glm::mat4 environmentWorldFitMatrix(
    size_t index, const EnvironmentComposition& composition) {
  const EnvironmentWorldFit fit = environmentWorldFitParams(index, composition);
  return glm::translate(glm::mat4(1.0f),
                        glm::vec3(fit.centerX, fit.yBias, fit.centerZ)) *
         glm::scale(glm::mat4(1.0f), glm::vec3(fit.scale));
}
