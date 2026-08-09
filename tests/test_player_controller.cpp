#include "native/gameplay/player/player_controller.h"

#include <cassert>
#include <cmath>

namespace {

bool close(float actual, float expected, float tolerance = 0.0001f) {
  return std::abs(actual - expected) < tolerance;
}

}  // namespace

int main() {
  // 平滑加速：第一帧只达到部分速度，位移小于瞬时满速值。
  Player player;
  PlayerController controller({1.0f, 8.0f, 25.0f, 18.0f});
  controller.update(player, {0, 1}, 0.0f, 0.016f);
  assert(player.moving);
  assert(player.velocity.length() > 0.0f);
  assert(player.velocity.length() < 1.0f);
  const float firstStepDisplacement = player.y - 0.5f;
  assert(firstStepDisplacement > 0.0f);
  assert(firstStepDisplacement < 1.0f * 0.016f);

  // 持续输入后速度收敛到满速，每帧位移趋近 speed * dt。
  for (int frame = 0; frame < 24; ++frame) {
    controller.update(player, {0, 1}, 0.0f, 0.016f);
  }
  assert(close(player.velocity.length(), 1.0f, 0.001f));
  assert(player.y < 1.0f);  // 未触及边界
  const float before = player.y;
  controller.update(player, {0, 1}, 0.0f, 0.016f);
  assert(close(player.y - before, 1.0f * 0.016f));

  // 相机相对方向映射：yaw=pi/2 时推"上"应沿世界 +x 移动。
  player = {};
  for (int frame = 0; frame < 25; ++frame) {
    controller.update(player, {0, 1}, 1.5707963f, 0.016f);
  }
  assert(player.x > 0.8f);
  assert(close(player.y, 0.5f));
  assert(close(player.velocity.x, player.velocity.length(), 0.001f));
  assert(std::abs(player.velocity.y) < 0.001f);

  // 松开输入后平滑减速：速度连续衰减并最终停止。
  player = {};
  for (int frame = 0; frame < 25; ++frame) {
    controller.update(player, {0, 1}, 0.0f, 0.016f);
  }
  assert(player.moving);
  const float speedBeforeRelease = player.velocity.length();
  controller.update(player, {}, 0.0f, 0.016f);
  assert(player.velocity.length() < speedBeforeRelease);
  int stopFrames = 1;
  while (player.moving && stopFrames < 1000) {
    controller.update(player, {}, 0.0f, 0.016f);
    ++stopFrames;
  }
  assert(!player.moving);
  assert(player.velocity == Vec2{});
  // 减速应在合理时间内完成（约 0.5 秒内），不会无限滑行。
  assert(stopFrames < 40);

  // 转向平滑：速度方向渐变而非瞬切，角度连续追踪运动方向。
  player = {};
  for (int frame = 0; frame < 25; ++frame) {
    controller.update(player, {0, 1}, 0.0f, 0.016f);
  }
  const float angleBeforeTurn = player.angle;
  controller.update(player, {1, 0}, 0.0f, 0.016f);
  assert(player.moving);
  const float angleDelta = std::abs(player.angle - angleBeforeTurn);
  assert(angleDelta > 0.0f);
  assert(angleDelta < 1.0f);  // 单帧不会瞬切 90 度

  // 输入方向必须立即决定实际位移方向：反向输入后不能继续沿旧方向滑行。
  player = {};
  for (int frame = 0; frame < 25; ++frame) {
    controller.update(player, {0, 1}, 0.0f, 0.016f);
  }
  const float yBeforeReverse = player.y;
  controller.update(player, {0, -1}, 0.0f, 0.016f);
  assert(player.y < yBeforeReverse);
  assert(player.velocity.y < 0.0f);

  // angle 使用 3D 绕 Y 轴 yaw：模型局部 +Z 前轴旋转后必须与世界速度一致。
  const Vec2 directions[] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0},
                             {0.6f, 0.8f}};
  PlayerController fastTurnController({1.0f, 1000.0f, 25.0f, 18.0f});
  for (const Vec2 direction : directions) {
    player = {};
    fastTurnController.update(player, direction, 0.0f, 0.016f);
    const Vec2 modelForward{std::sin(player.angle), std::cos(player.angle)};
    const Vec2 velocityDirection =
        player.velocity * (1.0f / player.velocity.length());
    assert(close(modelForward.x, velocityDirection.x));
    assert(close(modelForward.y, velocityDirection.y));
  }

  // 无效输入防护。
  player = {};
  controller.update(player, {}, 1.5707963f, 0.25f);
  assert(!player.moving);
  controller.update(player, {0, 1}, 0.0f, 0.0f);
  assert(!player.moving);
  assert(player.velocity == Vec2{});

  // turnSpeedScale：转身动画播放期传 0 冻结朝向插值，位移照常；
  // 恢复默认后角度继续追踪速度方向。
  player = {};
  for (int frame = 0; frame < 25; ++frame) {
    controller.update(player, {0, 1}, 0.0f, 0.016f);
  }
  const float frozenAngle = player.angle;
  // 反向输入 + 冻结朝向：位移方向立即反转，但 angle 保持不变。
  controller.update(player, {0, -1}, 0.0f, 0.016f, 1.0f, 0.0f);
  assert(player.velocity.y < 0.0f);
  assert(close(player.angle, frozenAngle));
  // 解冻后角度朝新速度方向追赶。
  controller.update(player, {0, -1}, 0.0f, 0.016f);
  assert(std::abs(player.angle - frozenAngle) > 0.0f);
  return 0;
}
