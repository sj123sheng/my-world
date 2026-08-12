#include "native/gameplay/player/player_controller.h"

#include "native/engine/math/camera_ground_basis.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
// 速度低于此阈值时视为完全停止，避免无限拖尾滑行。
constexpr float kStopSpeedThreshold = 0.005f;

float clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float shortestAngleDelta(float from, float to) {
  return std::remainder(to - from, kTwoPi);
}

}  // namespace

void PlayerController::update(Player& player, Vec2 move, float cameraYaw,
                              float dtSeconds, float speedScale,
                              float turnSpeedScale) const {
  if (!std::isfinite(cameraYaw) || !std::isfinite(dtSeconds) ||
      dtSeconds <= 0.0f) {
    player.moving = false;
    player.velocity = {};
    return;
  }

  // 目标速度：摇杆输入经相机 yaw 映射到世界方向。
  Vec2 targetVelocity{};
  if (move.finite() && move.length() > 0.0f) {
    move = ClampLength(move, 1.0f);
    const CameraGroundBasis cameraBasis =
        CameraGroundBasisForYaw(cameraYaw);
    // speedScale：疾跑等状态的速度倍率（非法值钳为 0）。
    const float effectiveSpeed =
        speed() * std::max(0.0f, std::isfinite(speedScale) ? speedScale : 1.0f);
    targetVelocity =
        (cameraBasis.right * move.x + cameraBasis.forward * move.y) *
        effectiveSpeed;
  }

  // 指数平滑速度大小，但有输入时立即采用当前目标方向。
  // 这样既保留起步/停止的柔和感，也避免旧速度把角色继续推向错误方向。
  const bool hasInput = targetVelocity.length() > 0.0f;
  const float sharpness =
      hasInput ? std::max(0.0f, config_.accelSharpness)
               : std::max(0.0f, config_.decelSharpness);
  const float factor = 1.0f - std::exp(-sharpness * dtSeconds);
  if (hasInput) {
    const float targetSpeed = targetVelocity.length();
    const float currentSpeed = player.velocity.length();
    const float speed = currentSpeed + (targetSpeed - currentSpeed) * factor;
    player.velocity = targetVelocity * (speed / targetSpeed);
  } else {
    player.velocity = player.velocity * (1.0f - factor);
  }

  if (player.velocity.length() < kStopSpeedThreshold) {
    player.velocity = {};
    player.moving = false;
    return;
  }

  player.x = clamp01(player.x + player.velocity.x * dtSeconds);
  player.y = clamp01(player.y + player.velocity.y * dtSeconds);
  player.moving = true;

  // Player::angle 是绕 3D Y 轴的 yaw。模型局部 +Z 为前方，因此世界
  // (x, z) 速度对应 atan2(x, z)，不是二维数学角 atan2(z, x)。
  // turnSpeedScale：转身动画播放期间传 0 冻结朝向插值，由动画驱动
  // 视觉转身；动画结束后传 >1 值快速追回速度方向（与混合转出同步）。
  const float targetAngle =
      std::atan2(player.velocity.x, player.velocity.y);
  const float maxTurn = std::max(0.0f, config_.turnSpeed *
                                           std::max(0.0f, turnSpeedScale) *
                                           dtSeconds);
  const float turn = std::clamp(shortestAngleDelta(player.angle, targetAngle),
                                -maxTurn, maxTurn);
  player.angle += turn;
}
