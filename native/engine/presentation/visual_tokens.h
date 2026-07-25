#pragma once

#include "native/gameplay/combat/event.h"

#include <glm/vec3.hpp>

struct EnvironmentPalette {
  glm::vec3 clearColor;
  glm::vec3 ambient;
  glm::vec3 fogColor;
  glm::vec3 altarGlow;
  float fogDensity = 0.0f;
};

struct VisualTokens {
  static glm::vec3 sourceColor(SourceType source) {
    switch (source) {
      case SourceType::Radiance:
        return {0.85f, 0.63f, 0.27f};
      case SourceType::Current:
        return {0.26f, 0.82f, 0.72f};
      case SourceType::Corruption:
        return {0.67f, 0.31f, 0.58f};
    }
    return {1.0f, 1.0f, 1.0f};
  }

  static EnvironmentPalette environmentPalette() {
    return {{0.035f, 0.050f, 0.063f},
            {0.11f, 0.14f, 0.16f},
            {0.09f, 0.13f, 0.16f},
            {0.31f, 0.84f, 0.75f},
            0.18f};
  }
};
