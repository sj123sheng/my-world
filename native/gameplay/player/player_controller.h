#pragma once

#include "native/engine/math/vec2.h"

struct Player {
  float x = 0.5f;
  float y = 0.5f;
  float size = 0.05f;
  float angle = 0.0f;
  bool moving = false;
};

struct PlayerControllerConfig {
  float speed = 0.5f;
  float turnSpeed = 8.0f;
};

class PlayerController {
 public:
  explicit PlayerController(PlayerControllerConfig config = {})
      : config_(config) {}

  void update(Player& player, Vec2 move, float cameraYaw,
              float dtSeconds) const;

 private:
  PlayerControllerConfig config_;
};
