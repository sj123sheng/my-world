// test_combat_vfx.cpp: 普攻刀光与技能冲击波的纯函数曲线断言。

#include "native/engine/render/combat_vfx.h"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(float left, float right, float epsilon = 0.0001f) {
  return std::fabs(left - right) < epsilon;
}

void testSlashArcInvisibleOutsideWindow() {
  const SlashArcPose before = SlashArcPoseAt(-0.01f, 1);
  assert(!before.visible);
  const SlashArcPose atEnd = SlashArcPoseAt(SlashArcDuration(), 1);
  assert(!atEnd.visible);
  const SlashArcPose after = SlashArcPoseAt(SlashArcDuration() + 0.1f, 1);
  assert(!after.visible);
}

void testSlashArcSweepIsMonotonicLeftToRight() {
  float previous = -10.0f;
  for (int i = 0; i <= 10; ++i) {
    const float seconds =
        SlashArcDuration() * static_cast<float>(i) / 10.0f * 0.999f;
    const SlashArcPose pose = SlashArcPoseAt(seconds, 1);
    assert(pose.visible);
    assert(pose.sweepRadians > previous);
    previous = pose.sweepRadians;
  }
  // 起点左后、终点右前，扫掠总跨度约 2.3rad。
  const SlashArcPose start = SlashArcPoseAt(0.0f, 1);
  const SlashArcPose late = SlashArcPoseAt(SlashArcDuration() * 0.999f, 1);
  assert(start.sweepRadians < 0.0f);
  assert(late.sweepRadians > 0.0f);
  assert(nearlyEqual(late.sweepRadians - start.sweepRadians, 2.3f, 0.05f));
}

void testSlashArcAlphaFadesInThenOut() {
  const SlashArcPose spawn = SlashArcPoseAt(0.0f, 1);
  assert(nearlyEqual(spawn.alpha, 0.0f));
  const SlashArcPose mid = SlashArcPoseAt(0.08f, 1);
  assert(mid.alpha > 0.5f);
  const SlashArcPose end = SlashArcPoseAt(SlashArcDuration() * 0.999f, 1);
  assert(end.alpha < 0.05f);
}

void testSlashArcFinisherIsLargerAndBrighter() {
  const SlashArcPose normal = SlashArcPoseAt(0.08f, 1);
  const SlashArcPose finisher = SlashArcPoseAt(0.08f, 4);
  assert(finisher.scale > normal.scale);
  assert(finisher.alpha >= normal.alpha);
}

void testEaseOutCubicBoundsAndMonotonic() {
  assert(nearlyEqual(SlashArcEaseOutCubic(0.0f), 0.0f));
  assert(nearlyEqual(SlashArcEaseOutCubic(1.0f), 1.0f));
  // 输入越界时夹取，不产生 NaN/越界输出。
  assert(nearlyEqual(SlashArcEaseOutCubic(-1.0f), 0.0f));
  assert(nearlyEqual(SlashArcEaseOutCubic(2.0f), 1.0f));
  float previous = -1.0f;
  for (int i = 0; i <= 8; ++i) {
    const float value = SlashArcEaseOutCubic(static_cast<float>(i) / 8.0f);
    assert(value >= previous);
    previous = value;
  }
}

void testShockwaveExpandsAndFades() {
  const ShockwavePose spawn = ShockwavePoseAt(0.0f);
  assert(spawn.visible);
  assert(nearlyEqual(spawn.radiusScale, 0.0f));
  assert(nearlyEqual(spawn.alpha, 1.0f));
  const ShockwavePose late = ShockwavePoseAt(ShockwaveDuration() * 0.999f);
  assert(late.visible);
  assert(late.radiusScale > 0.98f);
  assert(late.alpha < 0.02f);
  const ShockwavePose done = ShockwavePoseAt(ShockwaveDuration());
  assert(!done.visible);
  // 扩张前快后慢（缓出）：前半程进度过半。
  const ShockwavePose half = ShockwavePoseAt(ShockwaveDuration() * 0.5f);
  assert(half.radiusScale > 0.5f);
}

void testSparkStretchStaysRoundWhenSlow() {
  // 静止/低速火花保持圆形广告牌（stretch=1），不被拉成细线。
  const SparkStretch still = SparkStretchFor(0.0f, 0.0f, 0.0f, 0.4f, 0.3f);
  assert(nearlyEqual(still.stretch, 1.0f));
  assert(nearlyEqual(still.angleRadians, 0.0f));
  const SparkStretch slow = SparkStretchFor(0.01f, 0.0f, 0.0f, 0.0f, 0.0f);
  assert(nearlyEqual(slow.stretch, 1.0f));
}

void testSparkStretchAlignsWithScreenVelocity() {
  // yaw=pitch=0 时 billboard = RotY(π)：世界 +X 速度映射到屏幕 -X，
  // 方向角应为 π（或 -π）；世界 +Y 速度映射到屏幕 +Y，角为 π/2。
  const SparkStretch right = SparkStretchFor(0.1f, 0.0f, 0.0f, 0.0f, 0.0f);
  assert(right.stretch > 1.0f);
  assert(nearlyEqual(std::fabs(right.angleRadians), 3.14159265f, 0.001f));
  const SparkStretch up = SparkStretchFor(0.0f, 0.1f, 0.0f, 0.0f, 0.0f);
  assert(nearlyEqual(up.angleRadians, 3.14159265f * 0.5f, 0.001f));
}

void testSparkStretchGrowsWithSpeedAndClamps() {
  const SparkStretch a = SparkStretchFor(0.05f, 0.0f, 0.0f, 0.0f, 0.0f);
  const SparkStretch b = SparkStretchFor(0.2f, 0.0f, 0.0f, 0.0f, 0.0f);
  const SparkStretch huge = SparkStretchFor(5.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  assert(b.stretch > a.stretch);
  assert(huge.stretch <= 3.2f + 0.0001f);
  // 相机 yaw=π/2 朝向世界 +X：+X 速度沿视线方向，无屏幕分量，保持圆形。
  const SparkStretch alongView =
      SparkStretchFor(0.1f, 0.0f, 0.0f, 3.14159265f * 0.5f, 0.0f);
  assert(nearlyEqual(alongView.stretch, 1.0f));
  // 同一 yaw 下 +Z 速度沿屏幕水平方向（角 0），验证相机平面投影随
  // yaw 正确旋转。
  const SparkStretch turned =
      SparkStretchFor(0.0f, 0.0f, 0.1f, 3.14159265f * 0.5f, 0.0f);
  assert(turned.stretch > 1.0f);
  assert(nearlyEqual(turned.angleRadians, 0.0f, 0.001f));
}

}  // namespace

int main() {
  testSlashArcInvisibleOutsideWindow();
  testSlashArcSweepIsMonotonicLeftToRight();
  testSlashArcAlphaFadesInThenOut();
  testSlashArcFinisherIsLargerAndBrighter();
  testEaseOutCubicBoundsAndMonotonic();
  testShockwaveExpandsAndFades();
  testSparkStretchStaysRoundWhenSlow();
  testSparkStretchAlignsWithScreenVelocity();
  testSparkStretchGrowsWithSpeedAndClamps();
  return 0;
}
