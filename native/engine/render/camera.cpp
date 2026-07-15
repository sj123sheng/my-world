#include "camera.h"

#include <algorithm>
#include <cmath>

namespace {

ThirdPersonCameraConfig sanitizeConfig(ThirdPersonCameraConfig config) {
  const ThirdPersonCameraConfig defaults;

  if (!std::isfinite(config.defaultYaw)) {
    config.defaultYaw = defaults.defaultYaw;
  }
  if (!std::isfinite(config.minPitch)) {
    config.minPitch = defaults.minPitch;
  }
  if (!std::isfinite(config.maxPitch)) {
    config.maxPitch = defaults.maxPitch;
  }
  if (config.minPitch > config.maxPitch) {
    config.minPitch = defaults.minPitch;
    config.maxPitch = defaults.maxPitch;
  }
  if (!std::isfinite(config.defaultPitch)) {
    config.defaultPitch = defaults.defaultPitch;
  }
  config.defaultPitch =
      std::clamp(config.defaultPitch, config.minPitch, config.maxPitch);

  if (!std::isfinite(config.minDistance)) {
    config.minDistance = defaults.minDistance;
  }
  if (!std::isfinite(config.maxDistance)) {
    config.maxDistance = defaults.maxDistance;
  }
  if (config.minDistance > config.maxDistance) {
    config.minDistance = defaults.minDistance;
    config.maxDistance = defaults.maxDistance;
  }
  if (!std::isfinite(config.defaultDistance)) {
    config.defaultDistance = defaults.defaultDistance;
  }
  config.defaultDistance = std::clamp(config.defaultDistance,
                                      config.minDistance,
                                      config.maxDistance);

  if (!std::isfinite(config.followSharpness) ||
      config.followSharpness < 0.0f) {
    config.followSharpness = defaults.followSharpness;
  }

  return config;
}

}  // namespace

ThirdPersonCamera::ThirdPersonCamera(ThirdPersonCameraConfig config)
    : config_(sanitizeConfig(config)) {
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
