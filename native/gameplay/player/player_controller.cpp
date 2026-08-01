#include "native/gameplay/player/player_controller.h"

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
                              float dtSeconds) const {
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
    const Vec2 cameraForward{std::sin(cameraYaw), std::cos(cameraYaw)};
    const Vec2 cameraRight{std::cos(cameraYaw), -std::sin(cameraYaw)};
    targetVelocity =
        (cameraRight * move.x + cameraForward * move.y) * config_.speed;
  }

  // 指数平滑：输入时加速逼近目标速度，松开后快速衰减。
  // 启动、转向、停止均连续，消除速度突跳带来的生硬感。
  const bool hasInput = targetVelocity.length() > 0.0f;
  const float sharpness =
      hasInput ? std::max(0.0f, config_.accelSharpness)
               : std::max(0.0f, config_.decelSharpness);
  const float factor = 1.0f - std::exp(-sharpness * dtSeconds);
  player.velocity = player.velocity +
                    (targetVelocity - player.velocity) * factor;

  if (player.velocity.length() < kStopSpeedThreshold) {
    player.velocity = {};
    player.moving = false;
    return;
  }

  player.x = clamp01(player.x + player.velocity.x * dtSeconds);
  player.y = clamp01(player.y + player.velocity.y * dtSeconds);
  player.moving = true;

  const float targetAngle =
      std::atan2(player.velocity.y, player.velocity.x);
  const float maxTurn = std::max(0.0f, config_.turnSpeed * dtSeconds);
  const float turn = std::clamp(shortestAngleDelta(player.angle, targetAngle),
                                -maxTurn, maxTurn);
  player.angle += turn;
}
