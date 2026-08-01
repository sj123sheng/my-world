#pragma once

#include "native/engine/math/vec2.h"

struct Player {
  float x = 0.5f;
  float y = 0.5f;
  float size = 0.05f;
  float angle = 0.0f;
  bool moving = false;
  // 当前速度（世界单位/秒），由控制器平滑驱动，
  // 保证启动、转向、停止均连续无突跳。
  Vec2 velocity;
};

struct PlayerControllerConfig {
  float speed = 0.5f;
  float turnSpeed = 8.0f;
  // 加速平滑系数：越大起步越快（指数插值）。
  float accelSharpness = 25.0f;
  // 减速平滑系数：越大停止越快（指数插值）。
  float decelSharpness = 18.0f;
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
