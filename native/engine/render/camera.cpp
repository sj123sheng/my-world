#include "camera.h"

#include "native/engine/math/camera_ground_basis.h"

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

  if (!std::isfinite(config.minDistance) ||
      !std::isfinite(config.maxDistance) || config.minDistance <= 0.0f ||
      config.maxDistance <= 0.0f || config.minDistance > config.maxDistance) {
    config.minDistance = defaults.minDistance;
    config.maxDistance = defaults.maxDistance;
    config.defaultDistance = defaults.defaultDistance;
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

  if (!std::isfinite(config.yawSharpness) || config.yawSharpness <= 0.0f) {
    config.yawSharpness = defaults.yawSharpness;
  }

  if (!std::isfinite(config.explorationDistance)) {
    config.explorationDistance = defaults.explorationDistance;
  }
  config.explorationDistance = std::clamp(config.explorationDistance,
                                          config.minDistance,
                                          config.maxDistance);
  if (!std::isfinite(config.distanceSharpness) ||
      config.distanceSharpness <= 0.0f) {
    config.distanceSharpness = defaults.distanceSharpness;
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

  targetYaw_ += lookDelta.x;
  pitch_ = std::clamp(pitch_ + lookDelta.y, config_.minPitch,
                      config_.maxPitch);
  // yaw 以指数插值逼近目标，转动视角时平滑过渡，
  // 同时保证移动方向映射与画面一致。
  const float yawFollow =
      1.0f - std::exp(-config_.yawSharpness * dtSeconds);
  yaw_ += (targetYaw_ - yaw_) * yawFollow;
  const float follow = 1.0f - std::exp(-config_.followSharpness * dtSeconds);
  target_ = target_ + (desiredTarget - target_) * follow;

  // 模式距离平滑：探索模式拉远、战斗模式收回，指数插值避免突跳。
  const float modeDistance =
      exploration_ ? config_.explorationDistance : config_.defaultDistance;
  const float distanceFollow =
      1.0f - std::exp(-config_.distanceSharpness * dtSeconds);
  distance_ += (modeDistance - distance_) * distanceFollow;

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
  targetYaw_ = config_.defaultYaw;
  pitch_ = config_.defaultPitch;
  distance_ = config_.defaultDistance;
  target_ = {0.5f, 0.5f};
}

float ThirdPersonCamera::yaw() const { return yaw_; }

float ThirdPersonCamera::pitch() const { return pitch_; }

float ThirdPersonCamera::distance() const { return distance_; }

Vec2 ThirdPersonCamera::target() const { return target_; }

Vec2 ThirdPersonCamera::position() const {
  const float projectedDistance = distance_ * std::cos(pitch_);
  const CameraGroundBasis basis = CameraGroundBasisForYaw(yaw_);
  return target_ - basis.forward * projectedDistance;
}

CameraRenderState ThirdPersonCamera::renderState() const {
  return {target_, yaw_, pitch_, distance_, config_.defaultPitch,
          config_.defaultDistance};
}

const ThirdPersonCameraConfig& ThirdPersonCamera::config() const {
  return config_;
}

bool ThirdPersonCamera::finite() const {
  return std::isfinite(yaw_) && std::isfinite(targetYaw_) &&
         std::isfinite(pitch_) && std::isfinite(distance_) && target_.finite();
}
