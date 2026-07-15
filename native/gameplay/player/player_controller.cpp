#include "native/gameplay/player/player_controller.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

float clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float shortestAngleDelta(float from, float to) {
  return std::remainder(to - from, kTwoPi);
}

}  // namespace

void PlayerController::update(Player& player, Vec2 move, float cameraYaw,
                              float dtSeconds) const {
  if (!move.finite() || move.length() == 0.0f || !std::isfinite(cameraYaw) ||
      !std::isfinite(dtSeconds) || dtSeconds <= 0.0f) {
    player.moving = false;
    return;
  }

  move = ClampLength(move, 1.0f);
  const Vec2 cameraForward{std::sin(cameraYaw), std::cos(cameraYaw)};
  const Vec2 cameraRight{std::cos(cameraYaw), -std::sin(cameraYaw)};
  const Vec2 worldMove = cameraRight * move.x + cameraForward * move.y;
  const float distance = config_.speed * dtSeconds;

  player.x = clamp01(player.x + worldMove.x * distance);
  player.y = clamp01(player.y + worldMove.y * distance);
  player.moving = true;

  const float targetAngle = std::atan2(worldMove.y, worldMove.x);
  const float maxTurn = std::max(0.0f, config_.turnSpeed * dtSeconds);
  const float turn = std::clamp(shortestAngleDelta(player.angle, targetAngle),
                                -maxTurn, maxTurn);
  player.angle += turn;
}
