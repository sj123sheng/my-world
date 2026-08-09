#pragma once

#include "native/engine/math/vec2.h"

struct Player {
  float x = 0.5f;
  float y = 0.5f;
  float size = 0.05f;
  // 绕 3D Y 轴的 yaw；0 表示模型局部 +Z / 逻辑世界 +y。
  float angle = 0.0f;
  bool moving = false;
  // 当前速度（世界单位/秒），由控制器平滑驱动，
  // 保证启动、转向、停止均连续无突跳。
  Vec2 velocity;
};

struct PlayerControllerConfig {
  // 移动速度（世界单位/秒）：真机手感调优，0.5 过快灵敏，降至 0.3。
  float speed = 0.3f;
  float turnSpeed = 8.0f;
  // 加速平滑系数：越大起步越快（指数插值）；降低使起步更可控。
  float accelSharpness = 16.0f;
  // 减速平滑系数：越大停止越快（指数插值）。
  float decelSharpness = 18.0f;
};

class PlayerController {
 public:
  explicit PlayerController(PlayerControllerConfig config = {})
      : config_(config) {}

  // turnSpeedScale：朝向插值速率倍率。转身动画播放期间传 0 冻结
  // yaw（由动画驱动视觉转身），动画结束后传 >1 倍率快速追平目标
  // 朝向，与动画回摆无缝衔接；默认 1 为常规平滑转向。
  void update(Player& player, Vec2 move, float cameraYaw,
              float dtSeconds, float speedScale = 1.0f,
              float turnSpeedScale = 1.0f) const;

 private:
  PlayerControllerConfig config_;
};
