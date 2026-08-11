#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct TerrainMaterialSet {
  glm::vec3 grassTint{0.42f, 0.62f, 0.30f};
  glm::vec3 soilTint{0.68f, 0.56f, 0.38f};
  glm::vec3 rockTint{0.50f, 0.49f, 0.52f};
  glm::vec3 pathTint{0.48f, 0.38f, 0.27f};
  float macroScale = 6.5f;
  float detailScale = 90.0f;
  float triplanarSharpness = 4.0f;
  float paintedControlStrength = 0.85f;
};

struct TerrainMaterialWeights {
  float grass = 0.0f;
  float soil = 0.0f;
  float rock = 0.0f;
  float path = 0.0f;
};

TerrainMaterialWeights TerrainMaterialWeightsFor(
    float height, float slope, float shoreDistance, float pathMask,
    const glm::vec4& paintedControl, float paintedStrength);
