// test_combat_vfx.cpp: 普攻刀光与技能冲击波的纯函数曲线断言。

#include <glm/vec2.hpp>
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

void testLightPillarRisesHoldsThenFades() {
  // 窗口外不可见。
  assert(!LightPillarPoseAt(-0.01f).visible);
  assert(!LightPillarPoseAt(LightPillarDuration()).visible);
  assert(!LightPillarPoseAt(LightPillarDuration() + 0.1f).visible);
  // 上升段单调递增，0.12s 到位。
  float previous = -1.0f;
  for (int i = 0; i <= 6; ++i) {
    const LightPillarPose pose =
        LightPillarPoseAt(0.12f * static_cast<float>(i) / 6.0f);
    assert(pose.visible);
    assert(pose.heightScale >= previous);
    previous = pose.heightScale;
  }
  assert(nearlyEqual(LightPillarPoseAt(0.12f).heightScale, 1.0f));
  // 保持段（0.12~0.22s）维持满高。
  assert(nearlyEqual(LightPillarPoseAt(0.17f).heightScale, 1.0f));
  // 衰减段单调递减，终点归零。
  const LightPillarPose mid = LightPillarPoseAt(0.38f);
  const LightPillarPose late = LightPillarPoseAt(0.52f);
  assert(mid.heightScale < 1.0f && mid.heightScale > 0.0f);
  assert(late.heightScale < mid.heightScale);
  // 宽度随高度联动（0.7..1.0），透明度全程不超过 1 且终点趋零。
  for (int i = 0; i < 11; ++i) {
    const LightPillarPose pose = LightPillarPoseAt(
        LightPillarDuration() * static_cast<float>(i) / 11.0f);
    assert(pose.widthScale >= 0.7f && pose.widthScale <= 1.0f + 1e-4f);
    assert(pose.alpha >= 0.0f && pose.alpha <= 1.0f);
  }
  assert(LightPillarPoseAt(LightPillarDuration() - 0.001f).alpha < 0.05f);
}

void testWeaponTrailFollowsSlashArcWindow() {
  // 与刀光同窗口：窗口外不发射。
  assert(!WeaponTrailPoseAt(-0.01f, 1).active);
  assert(!WeaponTrailPoseAt(SlashArcDuration(), 1).active);
  assert(!WeaponTrailPoseAt(SlashArcDuration() + 0.1f, 1).active);
  // 窗口内逐帧：扫掠角与刀光同源，半径为 1.9×刀光缩放。
  for (int i = 0; i <= 8; ++i) {
    const float seconds =
        SlashArcDuration() * static_cast<float>(i) / 8.0f * 0.999f;
    const WeaponTrailPose trail = WeaponTrailPoseAt(seconds, 1);
    const SlashArcPose slash = SlashArcPoseAt(seconds, 1);
    assert(trail.active && slash.visible);
    assert(nearlyEqual(trail.angleRadians, slash.sweepRadians));
    assert(nearlyEqual(trail.radiusFactor, 1.9f * slash.scale));
    assert(trail.heightFactor > 0.0f);
  }
  // 终结段刀光放大时拖尾半径同步放大。
  const WeaponTrailPose finisher =
      WeaponTrailPoseAt(0.1f, 4);
  const WeaponTrailPose normal = WeaponTrailPoseAt(0.1f, 1);
  assert(finisher.radiusFactor > normal.radiusFactor);
}

void testWeaponTrailVelocityTangential() {
  float vx = 0.0f, vy = 0.0f, vz = 0.0f;
  for (const float phi : {0.0f, 0.7f, 1.9f, -1.2f}) {
    WeaponTrailVelocity(phi, 0.03f, vx, vy, vz);
    // 切向与径向（sinφ, cosφ）垂直：点积为 0。
    const float dot = std::sin(phi) * vx + std::cos(phi) * vz;
    assert(std::abs(dot) < 1e-6f);
    // 水平速度模长等于切向速度参数，且固定轻微上扬。
    const float horizontal = std::sqrt(vx * vx + vz * vz);
    assert(nearlyEqual(horizontal, 0.03f));
    assert(nearlyEqual(vy, 0.006f));
  }
}

void testFovPunchDivesThenRecovers() {
  // 窗口外偏移为 0。
  assert(FovPunchOffsetAt(-0.01f, -7.0f) == 0.0f);
  assert(FovPunchOffsetAt(FovPunchDuration(), -7.0f) == 0.0f);
  assert(FovPunchOffsetAt(FovPunchDuration() + 0.1f, -7.0f) == 0.0f);
  // 起点为 0，前 20% 下潜到全量。
  assert(nearlyEqual(FovPunchOffsetAt(0.0f, -7.0f), 0.0f));
  const float full = FovPunchOffsetAt(FovPunchDuration() * 0.2f, -7.0f);
  assert(nearlyEqual(full, -7.0f));
  // 恢复段单调回升（绝对值单调减小），终点趋零。
  float previous = std::abs(full);
  for (int i = 1; i <= 8; ++i) {
    const float seconds = FovPunchDuration() *
                          (0.2f + 0.8f * static_cast<float>(i) / 9.0f);
    const float magnitude = std::abs(FovPunchOffsetAt(seconds, -7.0f));
    assert(magnitude < previous);
    previous = magnitude;
  }
  assert(std::abs(FovPunchOffsetAt(FovPunchDuration() - 0.001f, -7.0f)) <
         0.15f);
  // 符号跟随传入的最大偏移（收窄为负）。
  assert(FovPunchOffsetAt(FovPunchDuration() * 0.2f, -7.0f) < 0.0f);
}

void testSkillRuneSpinsEaseOutAndFades() {
  // 窗口外不可见。
  assert(!SkillRunePoseAt(-0.01f).visible);
  assert(!SkillRunePoseAt(SkillRuneDuration()).visible);
  assert(!SkillRunePoseAt(SkillRuneDuration() + 0.1f).visible);
  // 旋转单调递增（缓出：前段快、后段慢）。
  float previous = -1.0f;
  float previousDelta = 1e9f;
  for (int i = 0; i <= 10; ++i) {
    const SkillRunePose pose = SkillRunePoseAt(
        SkillRuneDuration() * static_cast<float>(i) / 10.0f * 0.999f);
    assert(pose.visible);
    assert(pose.rotationRadians >= previous);
    const float delta = pose.rotationRadians - previous;
    if (i >= 2) assert(delta <= previousDelta + 1e-5f);  // 减速
    previousDelta = delta;
    previous = pose.rotationRadians;
  }
  // 总转角约 240°（4.18879rad）。
  assert(nearlyEqual(previous, 4.18879f, 0.05f));
  // 透明度：起点 0（淡入中），中段接近峰值，终点趋零。
  assert(SkillRunePoseAt(0.0f).alpha < 0.05f);
  const float midAlpha = SkillRunePoseAt(SkillRuneDuration() * 0.2f).alpha;
  assert(midAlpha > 0.6f);
  assert(SkillRunePoseAt(SkillRuneDuration() - 0.001f).alpha < 0.05f);
  // 缩放从 0.85 缓出膨胀到 1.0。
  assert(nearlyEqual(SkillRunePoseAt(0.0f).scale, 0.85f));
  assert(SkillRunePoseAt(SkillRuneDuration() * 0.999f).scale > 0.99f);
}

void testSlashArcColorFollowsInfusion() {
  // 无附魔默认金白；三系附魔颜色互不相同且与源质语义一致。
  const glm::vec3 none = SlashArcColorFor(1, -1);
  const glm::vec3 radiance = SlashArcColorFor(1, 0);
  const glm::vec3 current = SlashArcColorFor(1, 1);
  const glm::vec3 corruption = SlashArcColorFor(1, 2);
  assert(none != radiance);
  assert(radiance != current);
  assert(current != corruption);
  assert(radiance != corruption);
  // 辉印金白（r>b）、脉流青蓝（b>r）、蚀质暗紫（r>g）。
  assert(radiance.r > radiance.b);
  assert(current.b > current.r);
  assert(corruption.r > corruption.g);
  // 终结段固定金橙，不受附魔影响。
  const glm::vec3 finisherPlain = SlashArcColorFor(4, -1);
  assert(SlashArcColorFor(4, 1) == finisherPlain);
  assert(SlashArcColorFor(4, 2) == finisherPlain);
  assert(finisherPlain.r > finisherPlain.b);
  // 未知源质回退默认金白。
  assert(SlashArcColorFor(1, 99) == none);
}

void testWeaponTrailKindFollowsInfusion() {
  // 无附魔/辉印 → 金白拖尾 kind 7；脉流 → 9；蚀质 → 10。
  assert(WeaponTrailKindFor(-1) == 7);
  assert(WeaponTrailKindFor(0) == 7);
  assert(WeaponTrailKindFor(1) == 9);
  assert(WeaponTrailKindFor(2) == 10);
  // 未知源质回退金白拖尾。
  assert(WeaponTrailKindFor(99) == 7);
}

void testBossPhaseVfxDistinctAndEscalating() {
  const BossPhaseVfx lockdown = BossPhaseVfxFor(1);
  const BossPhaseVfx storm = BossPhaseVfxFor(2);
  const BossPhaseVfx collapse = BossPhaseVfxFor(3);
  // 三阶段颜色互不相同，且与阶段源质语义一致：
  // 辉印封锁金白、脉流风暴青蓝、蚀质崩塌暗紫。
  assert(lockdown.color != storm.color);
  assert(storm.color != collapse.color);
  assert(lockdown.color != collapse.color);
  assert(lockdown.color.r > lockdown.color.b);   // 金白：红主导
  assert(storm.color.b > storm.color.r);         // 青蓝：蓝主导
  assert(collapse.color.r > collapse.color.g);   // 暗紫：红主导
  // 火花 kind 在有效范围（0..10）且各阶段不同。
  const BossPhaseVfx all[] = {lockdown, storm, collapse};
  for (const BossPhaseVfx& vfx : all) {
    assert(vfx.sparkKind >= 0 && vfx.sparkKind <= 10);
    assert(vfx.scale >= 1.0f);
  }
  assert(lockdown.sparkKind != storm.sparkKind);
  assert(storm.sparkKind != collapse.sparkKind);
  // 规模随阶段递增（终阶段最猛烈）。
  assert(storm.scale > lockdown.scale);
  assert(collapse.scale > storm.scale);
  // 未知阶段回退辉印配色，不产生黑环。
  const BossPhaseVfx unknown = BossPhaseVfxFor(99);
  assert(unknown.color == lockdown.color);
  assert(unknown.sparkKind == lockdown.sparkKind);
}

void testEnemySkillElementPerArchetype() {
  // 元素原型：Priest=辉印(0)、Caster=脉流(1)、Elite=蚀质(2)。
  assert(EnemyElementFor(1) == 0);
  assert(EnemyElementFor(4) == 1);
  assert(EnemyElementFor(5) == 2);
  // 物理原型：RiftClaw/Guard/Bruiser 无元素(-1)。
  assert(EnemyElementFor(0) == -1);
  assert(EnemyElementFor(2) == -1);
  assert(EnemyElementFor(3) == -1);
  assert(EnemyElementFor(99) == -1);
  // 刀光颜色：元素色与 AuraColorFor 同源；物理红保持原值。
  assert(EnemySkillColorFor(1) == AuraColorFor(0));
  assert(EnemySkillColorFor(4) == AuraColorFor(1));
  assert(EnemySkillColorFor(5) == AuraColorFor(2));
  const glm::vec3 physicalRed{1.0f, 0.42f, 0.36f};
  assert(EnemySkillColorFor(0) == physicalRed);
  assert(EnemySkillColorFor(3) == physicalRed);
  assert(EnemySkillColorFor(99) == physicalRed);
  // 火花 kind：元素 kind 4/5/6；物理红 kind 1。
  assert(EnemySkillSparkKindFor(1) == 4);
  assert(EnemySkillSparkKindFor(4) == 5);
  assert(EnemySkillSparkKindFor(5) == 6);
  assert(EnemySkillSparkKindFor(0) == 1);
  assert(EnemySkillSparkKindFor(2) == 1);
  assert(EnemySkillSparkKindFor(99) == 1);
}

void testWeaponInfusionTintFollowsSource() {
  const glm::vec3 blade{0.72f, 0.78f, 0.88f};
  // 无附魔：原样返回刃色。
  assert(WeaponInfusionTintFor(-1, blade) == blade);
  // 附魔：向元素色混合并提亮，三系结果互不相同。
  const glm::vec3 radiance = WeaponInfusionTintFor(0, blade);
  const glm::vec3 current = WeaponInfusionTintFor(1, blade);
  const glm::vec3 corruption = WeaponInfusionTintFor(2, blade);
  assert(radiance != blade);
  assert(radiance != current);
  assert(current != corruption);
  assert(radiance != corruption);
  // 元素语义：辉印偏金（红绿高）、脉流偏蓝（蓝主导）、蚀质偏紫（红主导）。
  assert(radiance.r > radiance.b);
  assert(current.b > current.r);
  assert(corruption.r > corruption.g);
  // 提亮：附魔后各通道不低于原刃色对应通道的主导分量（整体更亮）。
  const float bladeMax = std::max(blade.r, std::max(blade.g, blade.b));
  assert(std::max(current.r, std::max(current.g, current.b)) > bladeMax);
}

void testWindupWarningColorKeepsDangerAndElement() {
  const glm::vec3 danger{1.0f, 0.32f, 0.22f};
  // 物理原型保持警示红。
  assert(WindupWarningColorFor(0) == danger);
  assert(WindupWarningColorFor(2) == danger);
  assert(WindupWarningColorFor(3) == danger);
  assert(WindupWarningColorFor(99) == danger);
  // 元素原型混入元素色：与纯红不同，且保留红色危险分量（红通道仍高）。
  const glm::vec3 priest = WindupWarningColorFor(1);
  const glm::vec3 caster = WindupWarningColorFor(4);
  const glm::vec3 elite = WindupWarningColorFor(5);
  assert(priest != danger);
  assert(caster != danger);
  assert(elite != danger);
  assert(priest != caster);
  assert(caster != elite);
  assert(priest.r > 0.5f && caster.r > 0.5f && elite.r > 0.5f);
  // 首领预警环随阶段着色：三阶段互不相同，红通道保留危险语义。
  const glm::vec3 bossP1 = BossWindupWarningColorFor(1);
  const glm::vec3 bossP2 = BossWindupWarningColorFor(2);
  const glm::vec3 bossP3 = BossWindupWarningColorFor(3);
  assert(bossP1 != bossP2);
  assert(bossP2 != bossP3);
  assert(bossP1 != bossP3);
  assert(bossP1.r > 0.5f && bossP2.r > 0.5f && bossP3.r > 0.5f);
}

void testTargetMarkerColorBlendsElement() {
  const glm::vec3 marker{0.35f, 0.85f, 0.80f};
  // 无元素（物理/首领/假人）保持青蓝锁定色。
  assert(TargetMarkerColorFor(-1) == marker);
  // 元素目标混入元素色：与纯青蓝不同，三系互不相同。
  const glm::vec3 radiance = TargetMarkerColorFor(0);
  const glm::vec3 current = TargetMarkerColorFor(1);
  const glm::vec3 corruption = TargetMarkerColorFor(2);
  assert(radiance != marker);
  assert(current != marker);
  assert(corruption != marker);
  assert(radiance != current);
  assert(current != corruption);
  assert(radiance != corruption);
}

void testCharacterSwitchVfxFollowsElement() {
  // 三系角色出场配色与附着光环同源：火花 kind 4/5/6，颜色=AuraColorFor。
  const CharacterSwitchVfx radiance = CharacterSwitchVfxFor(1);
  const CharacterSwitchVfx current = CharacterSwitchVfxFor(2);
  const CharacterSwitchVfx corruption = CharacterSwitchVfxFor(3);
  assert(radiance.sparkKind == AuraSparkKindFor(0));
  assert(current.sparkKind == AuraSparkKindFor(1));
  assert(corruption.sparkKind == AuraSparkKindFor(2));
  assert(radiance.color == AuraColorFor(0));
  assert(current.color == AuraColorFor(1));
  assert(corruption.color == AuraColorFor(2));
  assert(radiance.color != current.color);
  assert(current.color != corruption.color);
  // 未知角色回退通用金橙（与火花 kind 0 配色同源），不产生黑环。
  const CharacterSwitchVfx physical = CharacterSwitchVfxFor(0);
  assert(physical.sparkKind == 0);
  assert(physical.color == glm::vec3(1.0f, 0.78f, 0.32f));
  assert(CharacterSwitchVfxFor(99).sparkKind == 0);
}

void testFovPunchTierAmplitudes() {
  // 轻重分层：元素技能轻档 < 切人中档 < 反应/终结/首领重档。
  const float skill = FovPunchMaxOffsetFor(0);
  const float switchChar = FovPunchMaxOffsetFor(1);
  const float heavy = FovPunchMaxOffsetFor(2);
  assert(skill < 0.0f && switchChar < 0.0f && heavy < 0.0f);
  assert(skill > switchChar);
  assert(switchChar > heavy);
  // 未知档位回退重档，避免切人后残留轻振幅削弱反应冲击。
  assert(FovPunchMaxOffsetFor(99) == heavy);
}

void testSkillCastAccentPerSource() {
  // 三系技能剪影差异化：辉印光柱 / 脉流束流 / 蚀质贴花，互不相同。
  assert(SkillCastAccentFor(0) == SkillCastAccent::Pillar);
  assert(SkillCastAccentFor(1) == SkillCastAccent::Stream);
  assert(SkillCastAccentFor(2) == SkillCastAccent::Decal);
  // 未知源质不产生点缀，避免黑环/空特效。
  assert(SkillCastAccentFor(-1) == SkillCastAccent::None);
  assert(SkillCastAccentFor(99) == SkillCastAccent::None);
}

void testPerfectDodgeVfxSharesDodgeLanguage() {
  // 完美闪避与冲刺尘土同淡蓝闪避语言，火花复用移动尾迹 kind。
  const PerfectDodgeVfx dodge = PerfectDodgeVfxFor();
  assert(dodge.color == DodgeDustColor());
  assert(dodge.sparkKind == 3);
  // 闪避语言色与三系元素色区分，避免与元素反馈混淆。
  assert(dodge.color != AuraColorFor(0));
  assert(dodge.color != AuraColorFor(1));
  assert(dodge.color != AuraColorFor(2));
}

void testConvergingSparkMotionArrivesAtCenter() {
  // 出生点在半径圆环上，速度指向圆心，寿命结束恰好抵达圆心。
  constexpr float kRadius = 0.1f;
  constexpr float kLife = 0.24f;
  for (int i = 0; i < 8; ++i) {
    const float angle = static_cast<float>(i) * 0.785398f;
    float ox = 0.0f, oz = 0.0f, vx = 0.0f, vz = 0.0f;
    ConvergingSparkMotion(angle, kRadius, kLife, ox, oz, vx, vz);
    assert(std::fabs(std::sqrt(ox * ox + oz * oz) - kRadius) < 1e-4f);
    // 速度方向与出生偏移相反（指向圆心）。
    assert(ox * vx + oz * vz < 0.0f);
    // 寿命结束抵达圆心：offset + v * life ≈ 0。
    assert(std::fabs(ox + vx * kLife) < 1e-4f);
    assert(std::fabs(oz + vz * kLife) < 1e-4f);
  }
  // 发射间隔为正且与附着粒子间隔同量级（连续前兆不刷屏）。
  assert(WindupConvergeInterval() > 0.02f);
  assert(WindupConvergeInterval() < 0.2f);
}

void testUltimateVfxFollowsCharacterElement() {
  // 元素角色终结技与自身源质同源（与切人出场同语言）。
  for (int id = 1; id <= 3; ++id) {
    const UltimateVfx ult = UltimateVfxFor(id);
    assert(ult.color == AuraColorFor(id - 1));
    assert(ult.sparkKind == AuraSparkKindFor(id - 1));
  }
  // 物理/未知角色保持通用亮金爆发色与金白火花 kind。
  const UltimateVfx physical = UltimateVfxFor(0);
  assert(physical.color == glm::vec3(1.0f, 0.90f, 0.50f));
  assert(physical.sparkKind == 4);
  assert(UltimateVfxFor(99).sparkKind == 4);
}

void testCharacterSourceMapping() {
  // 角色源质归属：三系角色各归其源，其余物理（-1 无元素）。
  assert(CharacterSourceFor(1) == 0);
  assert(CharacterSourceFor(2) == 1);
  assert(CharacterSourceFor(3) == 2);
  assert(CharacterSourceFor(0) == -1);
  assert(CharacterSourceFor(99) == -1);
}

void testUltimateDimAlphaRampsAndCaps() {
  // 非正计时全亮；0.15s 线性淡入到 0.22 上限后封顶。
  assert(UltimateDimAlphaFor(0.0f) == 0.0f);
  assert(UltimateDimAlphaFor(-1.0f) == 0.0f);
  const float half = UltimateDimAlphaFor(0.075f);
  const float full = UltimateDimAlphaFor(0.15f);
  assert(half > 0.0f && half < full);
  assert(std::fabs(full - 0.22f) < 1e-4f);
  assert(UltimateDimAlphaFor(0.3f) == full);
  assert(UltimateDimAlphaFor(10.0f) == full);
}

void testPlayerDeathVfxUsesDangerLanguage() {
  // 死亡爆发用受击红 kind（危险语义），颜色为饱和暗红，
  // 与受击红闪同族但可区分。
  const PlayerDeathVfx death = PlayerDeathVfxFor();
  assert(death.sparkKind == 1);
  assert(death.color.r > 0.6f && death.color.g < 0.4f &&
         death.color.b < 0.4f);
  assert(death.color != glm::vec3(1.0f, 0.45f, 0.40f));
}

void testWindupBodyTintBlendsTowardWarning() {
  const glm::vec3 base{0.60f, 0.30f, 0.20f};
  const glm::vec3 warning{1.0f, 0.32f, 0.22f};
  // 混合比随脉冲在 0.35~0.65 呼吸，结果始终在基色与预警色之间。
  const glm::vec3 low = WindupBodyTintFor(base, warning, 0.0f);
  const glm::vec3 high = WindupBodyTintFor(base, warning, 1.0f);
  assert(low != base && high != base);
  assert(low != high);
  // 脉冲越高越靠近预警色（红通道更高）。
  assert(high.r > low.r);
  // 越界脉冲自动钳制，不产生外插颜色。
  assert(WindupBodyTintFor(base, warning, -2.0f) == low);
  assert(WindupBodyTintFor(base, warning, 5.0f) == high);
}

void testHitRecoilTiltDecaysQuadratically() {
  constexpr float kMax = 0.105f;
  // 窗口外与非法参数归零。
  assert(HitRecoilTiltFor(0.0f, 0.15f, kMax) == 0.0f);
  assert(HitRecoilTiltFor(-1.0f, 0.15f, kMax) == 0.0f);
  assert(HitRecoilTiltFor(0.1f, 0.0f, kMax) == 0.0f);
  assert(HitRecoilTiltFor(0.1f, 0.15f, 0.0f) == 0.0f);
  // 命中瞬间取峰值；remaining 超过 duration 钳制在峰值不外插。
  assert(nearlyEqual(HitRecoilTiltFor(0.15f, 0.15f, kMax), kMax));
  assert(nearlyEqual(HitRecoilTiltFor(9.0f, 0.15f, kMax), kMax));
  // 半程强度平方衰减（0.5² = 0.25），前强后弱。
  assert(nearlyEqual(HitRecoilTiltFor(0.075f, 0.15f, kMax), kMax * 0.25f));
}

void testWindupScaleBreathesWithinBounds() {
  constexpr float kInflate = 0.035f;
  // 呼吸区间 [1, 1+max]，随脉冲单调增大。
  assert(nearlyEqual(WindupScaleFor(0.0f, kInflate), 1.0f));
  assert(nearlyEqual(WindupScaleFor(1.0f, kInflate), 1.0f + kInflate));
  assert(WindupScaleFor(0.5f, kInflate) > WindupScaleFor(0.25f, kInflate));
  // 越界脉冲自动钳制，不产生外插缩放。
  assert(nearlyEqual(WindupScaleFor(-2.0f, kInflate), 1.0f));
  assert(nearlyEqual(WindupScaleFor(5.0f, kInflate), 1.0f + kInflate));
  // maxInflate<=0 恒等返回 1.0。
  assert(nearlyEqual(WindupScaleFor(0.7f, 0.0f), 1.0f));
  assert(nearlyEqual(WindupScaleFor(0.7f, -1.0f), 1.0f));
}

void testDodgeGhostAlphaFadesWithAge() {
  // 窗口边界与非法参数归零。
  assert(DodgeGhostAlphaFor(0.0f, 0.24f) == 0.0f);
  assert(DodgeGhostAlphaFor(0.24f, 0.24f) == 0.0f);
  assert(DodgeGhostAlphaFor(0.5f, 0.24f) == 0.0f);
  assert(DodgeGhostAlphaFor(0.1f, 0.0f) == 0.0f);
  // 窗口内线性衰减：采样越新越亮。
  const float young = DodgeGhostAlphaFor(0.06f, 0.24f);
  const float old = DodgeGhostAlphaFor(0.18f, 0.24f);
  assert(young > old && old > 0.0f);
  assert(nearlyEqual(young, 0.28f * 0.75f));
  assert(nearlyEqual(old, 0.28f * 0.25f));
}

void testInfusedHitSparkFollowsInfusion() {
  // 无附魔返回基础 kind/颜色，与升级前等价。
  assert(InfusedHitSparkKindFor(-1, 0) == 0);
  assert(InfusedHitSparkKindFor(-1, 1) == 1);
  const glm::vec3 physical{1.0f, 0.78f, 0.32f};
  assert(InfusedHitDecalColorFor(-1, physical) == physical);
  // 附魔期间按源质着色：与 AuraSparkKindFor/AuraColorFor 同源。
  assert(InfusedHitSparkKindFor(0, 0) == 4);
  assert(InfusedHitSparkKindFor(1, 0) == 5);
  assert(InfusedHitSparkKindFor(2, 0) == 6);
  assert(InfusedHitDecalColorFor(1, physical) == AuraColorFor(1));
}

void testGlideWindVelocityOpposesMovement() {
  // 逆移动方向掠过，速度按移动速度 1.5 倍。
  const glm::vec3 wind = GlideWindVelocityFor(glm::vec2(0.4f, 0.0f));
  assert(nearlyEqual(wind.x, -0.6f));
  assert(nearlyEqual(wind.z, 0.0f));
  assert(wind.y < 0.0f);  // 轻微下飘
  const glm::vec3 wind2 = GlideWindVelocityFor(glm::vec2(0.0f, -0.3f));
  assert(nearlyEqual(wind2.z, 0.45f));
  assert(nearlyEqual(wind2.x, 0.0f));
  // 零速度退化为纯下飘。
  const glm::vec3 still = GlideWindVelocityFor(glm::vec2(0.0f, 0.0f));
  assert(nearlyEqual(still.x, 0.0f));
  assert(nearlyEqual(still.z, 0.0f));
  assert(still.y < 0.0f);
  // 发射间隔为正。
  assert(GlideWindInterval() > 0.0f);
}

void testBossBerserkRimOnlyInFinalPhase() {
  // 非终段（1/2/未知）不产生狂暴轮廓光。
  assert(BossBerserkRimFor(1, 0.5f).strength == 0.0f);
  assert(BossBerserkRimFor(2, 0.9f).strength == 0.0f);
  assert(BossBerserkRimFor(0, 0.5f).strength == 0.0f);
  // 终段颜色与 BossPhaseVfxFor(3) 蚀质色同源。
  const BossBerserkRim rim = BossBerserkRimFor(3, 0.5f);
  assert(rim.color == BossPhaseVfxFor(3).color);
  // 强度随脉冲在 [0.45, 1.05] 呼吸且单调。
  const float low = BossBerserkRimFor(3, 0.0f).strength;
  const float mid = BossBerserkRimFor(3, 0.5f).strength;
  const float high = BossBerserkRimFor(3, 1.0f).strength;
  assert(nearlyEqual(low, 0.45f));
  assert(nearlyEqual(high, 1.05f));
  assert(low < mid && mid < high);
  // 越界脉冲自动钳制。
  assert(nearlyEqual(BossBerserkRimFor(3, -2.0f).strength, 0.45f));
  assert(nearlyEqual(BossBerserkRimFor(3, 5.0f).strength, 1.05f));
}

void testBossBerserkAuraBoostFinalPhaseOnly() {
  assert(nearlyEqual(BossBerserkAuraBoostFor(1), 1.0f));
  assert(nearlyEqual(BossBerserkAuraBoostFor(2), 1.0f));
  assert(nearlyEqual(BossBerserkAuraBoostFor(0), 1.0f));
  assert(BossBerserkAuraBoostFor(3) > 1.0f);
}

void testBossBerserkEmitIntervalPositive() {
  assert(BossBerserkEmitInterval() > 0.0f);
}


void testBossPhaseAttachmentSetEscalates() {
  // 阶段 1 与未知阶段回退全副武装档。
  assert(BossPhaseAttachmentSetFor(1) == 0);
  assert(BossPhaseAttachmentSetFor(0) == 0);
  assert(BossPhaseAttachmentSetFor(9) == 0);
  // 阶段推进逐档卸甲，三档互不相同。
  assert(BossPhaseAttachmentSetFor(2) == 1);
  assert(BossPhaseAttachmentSetFor(3) == 2);
  assert(BossPhaseAttachmentSetFor(1) != BossPhaseAttachmentSetFor(2));
  assert(BossPhaseAttachmentSetFor(2) != BossPhaseAttachmentSetFor(3));
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
  testLightPillarRisesHoldsThenFades();
  testWeaponTrailFollowsSlashArcWindow();
  testWeaponTrailVelocityTangential();
  testFovPunchDivesThenRecovers();
  testSkillRuneSpinsEaseOutAndFades();
  testSlashArcColorFollowsInfusion();
  testWeaponTrailKindFollowsInfusion();
  testBossPhaseVfxDistinctAndEscalating();
  testEnemySkillElementPerArchetype();
  testWeaponInfusionTintFollowsSource();
  testWindupWarningColorKeepsDangerAndElement();
  testTargetMarkerColorBlendsElement();
  testCharacterSwitchVfxFollowsElement();
  testFovPunchTierAmplitudes();
  testSkillCastAccentPerSource();
  testPerfectDodgeVfxSharesDodgeLanguage();
  testConvergingSparkMotionArrivesAtCenter();
  testUltimateVfxFollowsCharacterElement();
  testCharacterSourceMapping();
  testUltimateDimAlphaRampsAndCaps();
  testPlayerDeathVfxUsesDangerLanguage();
  testWindupBodyTintBlendsTowardWarning();
  testHitRecoilTiltDecaysQuadratically();
  testWindupScaleBreathesWithinBounds();
  testDodgeGhostAlphaFadesWithAge();
  testInfusedHitSparkFollowsInfusion();
  testGlideWindVelocityOpposesMovement();
  testBossBerserkRimOnlyInFinalPhase();
  testBossBerserkAuraBoostFinalPhaseOnly();
  testBossBerserkEmitIntervalPositive();
  testBossPhaseAttachmentSetEscalates();
  return 0;
}
