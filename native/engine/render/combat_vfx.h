#pragma once

// combat_vfx.h: 战斗释放动效的纯函数决策（普攻刀光弧线、技能冲击波环）。
//
// 与 render_animation.h 同样的约定：所有曲线/时长都是纯函数，
// Loop 只负责计时与边沿触发，Surface 只负责按 pose 绘制，
// 行为由 tests/test_combat_vfx.cpp 断言锁定。

#include <algorithm>
#include <cmath>

// 普攻刀光弧线：挥击瞬间在角色身前扫过的新月形刀光。
// 时长 0.26s，略长于命中时刻（kAttackHitMs=160ms），保证刀光
// 覆盖"起手→命中→收招"全过程；扫掠角度以模型局部 +Z 前方为 0。
inline float SlashArcDuration() { return 0.26f; }

struct SlashArcPose {
  float alpha = 0.0f;        // 整体透明度（0..1）
  float sweepRadians = 0.0f; // 相对角色朝向的扫掠角（左后→右前）
  float scale = 1.0f;        // 弧线缩放（随时间略增，终结技额外放大）
  bool visible = false;
};

// 缓出三次方：挥击起始快、收尾慢，符合真实挥砍的速度感。
inline float SlashArcEaseOutCubic(float t) {
  const float clamped = std::clamp(t, 0.0f, 1.0f);
  const float inverse = 1.0f - clamped;
  return 1.0f - inverse * inverse * inverse;
}

// 刀光时间轴：
// - 扫掠：-1.15rad（左后）经缓出曲线到 +1.15rad（右前），完成一次挥砍；
// - 透明度：前 0.05s 快速淡入，随后线性衰减到 0，避免刀光硬消失；
// - 连段第 4 击（终结技）弧线放大 30%、亮度提升，强化收尾仪式感；
// - 窗口外（seconds<0 或 >=时长）返回不可见 pose。
inline SlashArcPose SlashArcPoseAt(float seconds, int comboSegment) {
  SlashArcPose pose;
  const float duration = SlashArcDuration();
  if (seconds < 0.0f || seconds >= duration) return pose;
  const float t = seconds / duration;
  pose.visible = true;
  pose.sweepRadians = -1.15f + 2.3f * SlashArcEaseOutCubic(t);
  const float fadeIn = std::clamp(seconds / 0.05f, 0.0f, 1.0f);
  const float fadeOut =
      1.0f - std::clamp((seconds - 0.05f) / (duration - 0.05f), 0.0f, 1.0f);
  pose.alpha = fadeIn * fadeOut;
  pose.scale = 1.0f + 0.12f * t;
  if (comboSegment >= 4) {
    pose.scale *= 1.3f;
    pose.alpha = std::min(pose.alpha * 1.25f, 1.0f);
  }
  return pose;
}

// 技能释放冲击波环：施法瞬间在施法者脚下扩散的地面光环。
// 0.45s 内从 25% 半径缓出扩张到最大半径并线性淡出。
inline float ShockwaveDuration() { return 0.45f; }

struct ShockwavePose {
  float radiusScale = 0.0f;  // 相对最大半径的扩张进度（0..1）
  float alpha = 0.0f;        // 整体透明度（0..1）
  bool visible = false;
};

inline ShockwavePose ShockwavePoseAt(float seconds) {
  ShockwavePose pose;
  const float duration = ShockwaveDuration();
  if (seconds < 0.0f || seconds >= duration) return pose;
  const float t = seconds / duration;
  pose.visible = true;
  pose.radiusScale = SlashArcEaseOutCubic(t);
  pose.alpha = 1.0f - t;
  return pose;
}

// 火花速度对齐拉伸：把广告牌四边形绕视线旋转到速度投影方向，并按速度
// 拉长，把“圆点”变成原神式“流光”。与 cameraBillboard 的旋转约定
//（RotY(yaw)·RotX(pitch)·RotY(π)）严格互逆，保证屏幕方向一致。
struct SparkStretch {
  float angleRadians = 0.0f;  // 相机平面内绕视线的旋转角
  float stretch = 1.0f;       // 沿速度方向的拉伸倍率（>=1）
};

inline SparkStretch SparkStretchFor(float vx, float vy, float vz,
                                    float cameraYaw, float cameraPitch) {
  SparkStretch result;
  // 世界速度 → 广告牌局部平面：依次施加 billboard 旋转的逆。
  const float cy = std::cos(cameraYaw);
  const float sy = std::sin(cameraYaw);
  const float cp = std::cos(cameraPitch);
  const float sp = std::sin(cameraPitch);
  // RotY(-yaw)
  const float x1 = vx * cy - vz * sy;
  const float z1 = vx * sy + vz * cy;
  // RotX(-pitch)
  const float x2 = x1;
  const float y2 = vy * cp + z1 * sp;
  const float z2 = -vy * sp + z1 * cp;
  // RotY(π)（自逆）
  const float px = -x2;
  const float py = y2;
  (void)z2;
  const float planeSpeed = std::sqrt(px * px + py * py);
  // 速度过低保持圆形广告牌，避免静止火花被拉成细线。
  if (planeSpeed < 0.02f) return result;
  result.angleRadians = std::atan2(py, px);
  result.stretch = std::min(1.0f + planeSpeed * 14.0f, 3.2f);
  return result;
}

// 命中贴地冲击贴花：伤害命中瞬间在受击点地面浮现的源质色光斑，
// 比释放冲击波更短促（0.35s）、半径更小，快速扩张后淡出，
// 与火花/飘字共同构成“打中了”的地面反馈。
inline float ImpactDecalDuration() { return 0.35f; }

struct ImpactDecalPose {
  float radiusScale = 0.0f;  // 相对最大半径的扩张进度（0..1）
  float alpha = 0.0f;        // 整体透明度（0..1）
  bool visible = false;
};

inline ImpactDecalPose ImpactDecalPoseAt(float seconds) {
  ImpactDecalPose pose;
  const float duration = ImpactDecalDuration();
  if (seconds < 0.0f || seconds >= duration) return pose;
  const float t = seconds / duration;
  pose.visible = true;
  // 前 40% 快速扩张到满半径，之后保持半径仅淡出，避免贴花“游走”。
  pose.radiusScale = SlashArcEaseOutCubic(std::min(t / 0.4f, 1.0f));
  pose.alpha = 1.0f - t;
  return pose;
}

// 受击方向性粒子初速度：沿攻击方向（击退方向）喷射，spread ∈ [-1,1]
// 在 ±60° 内横向散布，lift 给出上扬分量。逻辑平面 (x, y) 映射到
// 3D (x, z)，与项目坐标约定一致；方向退化时只保留上扬。
inline void DirectionalSparkVelocity(float dirX, float dirY, float speed,
                                     float spread, float lift, float& vx,
                                     float& vy, float& vz) {
  const float length = std::sqrt(dirX * dirX + dirY * dirY);
  if (length < 1e-5f || speed <= 0.0f) {
    vx = 0.0f;
    vy = lift;
    vz = 0.0f;
    return;
  }
  const float nx = dirX / length;
  const float ny = dirY / length;
  constexpr float kMaxSpreadRadians = 1.04719755f;  // ±60°
  const float angle = std::clamp(spread, -1.0f, 1.0f) * kMaxSpreadRadians;
  const float ca = std::cos(angle);
  const float sa = std::sin(angle);
  vx = (nx * ca - ny * sa) * speed;
  vz = (nx * sa + ny * ca) * speed;
  vy = lift;
}
