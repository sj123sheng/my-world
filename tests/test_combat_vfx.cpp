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

void testImpactDecalExpandsFastThenFades() {
  const ImpactDecalPose spawn = ImpactDecalPoseAt(0.0f);
  assert(spawn.visible);
  assert(nearlyEqual(spawn.radiusScale, 0.0f));
  assert(nearlyEqual(spawn.alpha, 1.0f));
  // 前 40% 时长内即完成扩张（贴花快速落地成形）。
  const ImpactDecalPose expanded =
      ImpactDecalPoseAt(ImpactDecalDuration() * 0.4f);
  assert(expanded.visible);
  assert(nearlyEqual(expanded.radiusScale, 1.0f, 0.01f));
  // 成形后半径保持，仅透明度继续衰减。
  const ImpactDecalPose late =
      ImpactDecalPoseAt(ImpactDecalDuration() * 0.999f);
  assert(late.visible);
  assert(nearlyEqual(late.radiusScale, 1.0f, 0.01f));
  assert(late.alpha < 0.05f);
  const ImpactDecalPose done = ImpactDecalPoseAt(ImpactDecalDuration());
  assert(!done.visible);
}

void testDirectionalSparkVelocityFollowsAttackDirection() {
  float vx = 0.0f, vy = 0.0f, vz = 0.0f;
  // 零方向回退为纯上扬，不产生 NaN。
  DirectionalSparkVelocity(0.0f, 0.0f, 0.1f, 0.5f, 0.04f, vx, vy, vz);
  assert(nearlyEqual(vx, 0.0f));
  assert(nearlyEqual(vz, 0.0f));
  assert(nearlyEqual(vy, 0.04f));
  // 逻辑 +X 攻击方向、零散布：水平速度沿 3D +X，大小守恒。
  DirectionalSparkVelocity(1.0f, 0.0f, 0.1f, 0.0f, 0.04f, vx, vy, vz);
  assert(nearlyEqual(vx, 0.1f));
  assert(nearlyEqual(vz, 0.0f));
  assert(nearlyEqual(vy, 0.04f));
  // 非单位方向先归一化：结果速度大小不受输入长度影响。
  DirectionalSparkVelocity(10.0f, 0.0f, 0.1f, 0.0f, 0.04f, vx, vy, vz);
  assert(nearlyEqual(vx, 0.1f));
  // 满散布 = ±60° 旋转：+1 把 +X 方向转到 (cos60°, sin60°)。
  DirectionalSparkVelocity(1.0f, 0.0f, 0.1f, 1.0f, 0.0f, vx, vy, vz);
  assert(nearlyEqual(vx, 0.1f * 0.5f, 0.001f));
  assert(nearlyEqual(vz, 0.1f * 0.8660254f, 0.001f));
  // 任意散布下水平速度大小守恒（只旋转不缩放）。
  DirectionalSparkVelocity(0.3f, -0.7f, 0.13f, -0.42f, 0.02f, vx, vy, vz);
  assert(nearlyEqual(std::sqrt(vx * vx + vz * vz), 0.13f, 0.001f));
  assert(nearlyEqual(vy, 0.02f));
}

void testReactionVfxDistinctPerResonanceType() {
  const ReactionVfx refraction = ReactionVfxFor(0);
  const ReactionVfx stasis = ReactionVfxFor(1);
  const ReactionVfx collapse = ReactionVfxFor(2);
  const ReactionVfx burst = ReactionVfxFor(3);
  // 四种反应颜色互不相同，火花 kind 也在有效范围内（0..6）。
  assert(refraction.color != stasis.color);
  assert(stasis.color != collapse.color);
  assert(collapse.color != burst.color);
  assert(refraction.color != burst.color);
  const ReactionVfx all[] = {refraction, stasis, collapse, burst};
  for (const ReactionVfx& vfx : all) {
    assert(vfx.sparkKind >= 0 && vfx.sparkKind <= 6);
    assert(vfx.color.r > 0.0f || vfx.color.g > 0.0f || vfx.color.b > 0.0f);
  }
  // 折光金白、凝滞青蓝、崩解暗紫：主通道符合元素语义。
  assert(refraction.color.r > refraction.color.b);
  assert(stasis.color.b > stasis.color.r);
  assert(collapse.color.r > collapse.color.g);
  // 未知反应回退折光配色，不产生黑环。
  const ReactionVfx unknown = ReactionVfxFor(99);
  assert(unknown.color == refraction.color);
  assert(unknown.sparkKind == refraction.sparkKind);
}

void testAuraColorAndSparkKindPerSource() {
  const glm::vec3 radiance = AuraColorFor(0);
  const glm::vec3 current = AuraColorFor(1);
  const glm::vec3 corruption = AuraColorFor(2);
  // 三种源质颜色互不相同，且与元素反应同色系语义一致。
  assert(radiance != current);
  assert(current != corruption);
  assert(radiance != corruption);
  // 辉印金白（r>b）、脉流青蓝（b>r）、蚀质暗紫（r>g）。
  assert(radiance.r > radiance.b);
  assert(current.b > current.r);
  assert(corruption.r > corruption.g);
  // 火花 kind 映射到既有元素配色表 4/5/6。
  assert(AuraSparkKindFor(0) == 4);
  assert(AuraSparkKindFor(1) == 5);
  assert(AuraSparkKindFor(2) == 6);
  // 未知源质回退辉印配色，不产生黑环/黑粒子。
  assert(AuraColorFor(99) == radiance);
  assert(AuraSparkKindFor(99) == 4);
}

void testAuraRingPoseBreathesWithinBounds() {
  // 呼吸 pose 始终可见：半径/透明度落在设计区间内。
  for (int i = 0; i < 64; ++i) {
    const float seconds = static_cast<float>(i) * 0.05f;
    const AuraRingPose pose = AuraRingPoseAt(seconds, 0);
    assert(pose.radiusScale >= 0.92f && pose.radiusScale <= 1.02f);
    assert(pose.alpha >= 0.38f && pose.alpha <= 0.68f);
  }
  // 周期性：整周期后 pose 复原（浮点容差）。
  const AuraRingPose start = AuraRingPoseAt(0.0f, 0);
  const AuraRingPose loop = AuraRingPoseAt(AuraRingPeriod(), 0);
  assert(std::abs(start.radiusScale - loop.radiusScale) < 1e-3f);
  assert(std::abs(start.alpha - loop.alpha) < 1e-3f);
  // 多源质附着环相位错开：同一时刻不同 ringIndex 的 pose 不同。
  const AuraRingPose ring0 = AuraRingPoseAt(0.3f, 0);
  const AuraRingPose ring1 = AuraRingPoseAt(0.3f, 1);
  const AuraRingPose ring2 = AuraRingPoseAt(0.3f, 2);
  assert(std::abs(ring0.alpha - ring1.alpha) > 1e-3f);
  assert(std::abs(ring1.alpha - ring2.alpha) > 1e-3f);
}

void testAuraParticleVelocityRisesAndDrifts() {
  float vx = 0.0f, vy = 0.0f, vz = 0.0f;
  AuraParticleVelocity(0.0f, 0.01f, 0.06f, vx, vy, vz);
  // 角度 0：沿 +X 外飘，无 Z 分量，上升速度原样保留。
  assert(nearlyEqual(vx, 0.01f));
  assert(nearlyEqual(vz, 0.0f));
  assert(nearlyEqual(vy, 0.06f));
  AuraParticleVelocity(3.14159265f * 0.5f, 0.02f, 0.05f, vx, vy, vz);
  // 角度 π/2：沿 +Z 外飘。
  assert(std::abs(vx) < 1e-4f);
  assert(nearlyEqual(vz, 0.02f));
  assert(nearlyEqual(vy, 0.05f));
  // 水平漂移模长恒等于 drift（任意角度）。
  AuraParticleVelocity(1.3f, 0.015f, 0.04f, vx, vy, vz);
  const float horizontal = std::sqrt(vx * vx + vz * vz);
  assert(std::abs(horizontal - 0.015f) < 1e-5f);
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
  testImpactDecalExpandsFastThenFades();
  testDirectionalSparkVelocityFollowsAttackDirection();
  testReactionVfxDistinctPerResonanceType();
  testAuraColorAndSparkKindPerSource();
  testAuraRingPoseBreathesWithinBounds();
  testAuraParticleVelocityRisesAndDrifts();
  return 0;
}
