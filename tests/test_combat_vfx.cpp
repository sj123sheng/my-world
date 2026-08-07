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

}  // namespace

int main() {
  testSlashArcInvisibleOutsideWindow();
  testSlashArcSweepIsMonotonicLeftToRight();
  testSlashArcAlphaFadesInThenOut();
  testSlashArcFinisherIsLargerAndBrighter();
  testEaseOutCubicBoundsAndMonotonic();
  testShockwaveExpandsAndFades();
  return 0;
}
