#pragma once

#include <glm/vec3.hpp>

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
