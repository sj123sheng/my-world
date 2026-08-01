#pragma once

#include "native/engine/math/vec2.h"
#include "native/engine/render/camera_render_state.h"

struct ThirdPersonCameraConfig {
  float defaultYaw = 0.0f;
  float defaultPitch = 0.45f;
  float minPitch = -0.2f;
  float maxPitch = 1.1f;
  float defaultDistance = 0.35f;
  float minDistance = 0.2f;
  float maxDistance = 0.6f;
  float followSharpness = 10.0f;
  // yaw 平滑系数：越大转向越快。实际 yaw 以指数插值逼近目标 yaw，
  // 避免视角转动时产生突兀。
  float yawSharpness = 20.0f;
};

class ThirdPersonCamera {
 public:
  explicit ThirdPersonCamera(ThirdPersonCameraConfig config = {});

  void update(Vec2 desiredTarget, Vec2 lookDelta, float dtSeconds);
  void setDistance(float distance);
  void reset();

  float yaw() const;
  float pitch() const;
  float distance() const;
  Vec2 target() const;
  Vec2 position() const;
  CameraRenderState renderState() const;
  const ThirdPersonCameraConfig& config() const;

 private:
  bool finite() const;

  ThirdPersonCameraConfig config_;
  float yaw_ = 0.0f;
  float targetYaw_ = 0.0f;
  float pitch_ = 0.0f;
  float distance_ = 0.0f;
  Vec2 target_;
};
