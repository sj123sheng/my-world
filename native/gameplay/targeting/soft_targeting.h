#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "native/engine/math/vec2.h"

struct TargetCandidate {
  int32_t id = 0;
  Vec2 position;

  bool operator==(const TargetCandidate& other) const {
    return id == other.id && position == other.position;
  }
};

struct TargetSelection {
  int32_t id = 0;
  float distance = 0.0f;
  float angle = 0.0f;
  Vec2 direction;
};

struct SoftTargetingConfig {
  float maxDistance = 0.75f;
  float maxAngle = 1.0471976f;
};

class SoftTargeting {
 public:
  explicit SoftTargeting(SoftTargetingConfig config = {});

  std::optional<TargetSelection> select(
      Vec2 player, float cameraYaw,
      const std::vector<TargetCandidate>& candidates,
      std::optional<int32_t> preferredId = std::nullopt) const;

 private:
  SoftTargetingConfig config_;
};
