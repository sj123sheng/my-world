#include "camera.h"

#include <algorithm>
#include <cmath>

ThirdPersonCamera::ThirdPersonCamera(ThirdPersonCameraConfig config)
    : config_(config) {
  reset();
}

void ThirdPersonCamera::update(Vec2 desiredTarget, Vec2 lookDelta,
                               float dtSeconds) {
  if (!desiredTarget.finite() || !lookDelta.finite() ||
      !std::isfinite(dtSeconds) || !finite()) {
    reset();
    return;
  }

  yaw_ += lookDelta.x;
  pitch_ = std::clamp(pitch_ + lookDelta.y, config_.minPitch,
                      config_.maxPitch);
  const float follow = 1.0f - std::exp(-config_.followSharpness * dtSeconds);
  target_ = target_ + (desiredTarget - target_) * follow;

  if (!finite()) {
    reset();
  }
}

void ThirdPersonCamera::setDistance(float distance) {
  if (!std::isfinite(distance) || !finite()) {
    reset();
    return;
  }
  distance_ = std::clamp(distance, config_.minDistance, config_.maxDistance);
}

void ThirdPersonCamera::reset() {
  yaw_ = config_.defaultYaw;
  pitch_ = config_.defaultPitch;
  distance_ = config_.defaultDistance;
  target_ = {};
}

float ThirdPersonCamera::yaw() const { return yaw_; }

float ThirdPersonCamera::pitch() const { return pitch_; }

float ThirdPersonCamera::distance() const { return distance_; }

Vec2 ThirdPersonCamera::target() const { return target_; }

Vec2 ThirdPersonCamera::position() const {
  const float projectedDistance = distance_ * std::cos(pitch_);
  return {target_.x - std::sin(yaw_) * projectedDistance,
          target_.y - std::cos(yaw_) * projectedDistance};
}

const ThirdPersonCameraConfig& ThirdPersonCamera::config() const {
  return config_;
}

bool ThirdPersonCamera::finite() const {
  return std::isfinite(yaw_) && std::isfinite(pitch_) &&
         std::isfinite(distance_) && target_.finite();
}
