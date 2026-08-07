#pragma once

// combat_vfx.h: 战斗释放动效的纯函数决策（普攻刀光弧线、技能冲击波环）。
//
// 与 render_animation.h 同样的约定：所有曲线/时长都是纯函数，
// Loop 只负责计时与边沿触发，Surface 只负责按 pose 绘制，
// 行为由 tests/test_combat_vfx.cpp 断言锁定。

#include <algorithm>
#include <cmath>

#include <glm/vec3.hpp>

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

// 元素反应（三源共鸣）爆发配色：按 ResonanceType 数值返回冲击波/贴花
// 颜色与火花 kind，四种反应各自独立的元素色（原神式元素反应反馈）。
// ResonanceType：0=折光(辉印+脉流) 1=凝滞(脉流+蚀质)
// 2=崩解(蚀质+辉印) 3=共鸣爆发。
struct ReactionVfx {
  glm::vec3 color{1.0f};  // 冲击波环/贴花颜色
  int sparkKind = 4;      // 命中火花 kind（复用既有配色表）
};

inline ReactionVfx ReactionVfxFor(int resonanceType) {
  switch (resonanceType) {
    case 0:  // 折光：金白折射光
      return {{1.0f, 0.92f, 0.55f}, 4};
    case 1:  // 凝滞：青蓝冰凝光
      return {{0.40f, 0.85f, 1.0f}, 5};
    case 2:  // 崩解：暗紫侵蚀光
      return {{0.75f, 0.42f, 0.95f}, 6};
    case 3:  // 共鸣爆发：亮金全共鸣
      return {{1.0f, 1.0f, 0.85f}, 2};
    default:  // 未知反应回退折光配色，不产生黑环。
      return {{1.0f, 0.92f, 0.55f}, 4};
  }
}

// 元素附着光环（原神式元素附着指示）：目标身上附着源质时，
// 脚下浮现对应元素色的呼吸光环 + 周身上升元素粒子。
// SourceType 数值：0=辉印(Radiance) 1=脉流(Current) 2=蚀质(Corruption)。
// 光环掩码：bit0=辉印 bit1=脉流 bit2=蚀质，多源质可同时附着。
inline int AuraMaskFromFlags(bool radiance, bool current, bool corruption) {
  return (radiance ? 1 : 0) | (current ? 2 : 0) | (corruption ? 4 : 0);
}

inline glm::vec3 AuraColorFor(int sourceType) {
  switch (sourceType) {
    case 0:  // 辉印：金白
      return {1.0f, 0.92f, 0.55f};
    case 1:  // 脉流：青蓝
      return {0.45f, 0.85f, 1.0f};
    case 2:  // 蚀质：暗紫
      return {0.75f, 0.42f, 0.95f};
    default:  // 未知源质回退辉印配色，不产生黑环。
      return {1.0f, 0.92f, 0.55f};
  }
}

// 附着光环上升粒子复用的火花 kind（与火花配色表一致）。
inline int AuraSparkKindFor(int sourceType) {
  switch (sourceType) {
    case 0:
      return 4;  // 辉印金白
    case 1:
      return 5;  // 脉流青蓝
    case 2:
      return 6;  // 蚀质暗紫
    default:
      return 4;
  }
}

struct AuraRingPose {
  float radiusScale = 1.0f;  // 环呼吸缩放（0.92..1.02）
  float alpha = 0.0f;        // 环透明度（0.38..0.68）
};

// 附着光环呼吸周期（秒）：比预警环（0.8s）更缓，传达"持续附着"
// 而非"即将攻击"的语义。
inline float AuraRingPeriod() { return 1.6f; }

// 附着光环脉动 pose：半径与透明度同相位正弦呼吸；ringIndex 为
// 同目标多源质附着的环序号，相位错开 1/3 周期，多环错峰脉动。
inline AuraRingPose AuraRingPoseAt(float seconds, int ringIndex) {
  constexpr float kTau = 6.2831853f;
  const float phase = seconds / AuraRingPeriod() * kTau +
                      static_cast<float>(ringIndex) * (kTau / 3.0f);
  const float wave = 0.5f + 0.5f * std::sin(phase);
  return {0.92f + 0.10f * wave, 0.38f + 0.30f * wave};
}

// 附着粒子发射节奏（秒）：每个附着源质按此间隔各升起一颗粒子。
inline float AuraParticleInterval() { return 0.16f; }

// 附着粒子速度：径向外飘 + 稳定上升，形成"元素能量上涌"；
// kind>=4 的火花不受重力，粒子上升至寿命耗尽自然消散。
inline void AuraParticleVelocity(float angleRadians, float drift, float rise,
                                 float& vx, float& vy, float& vz) {
  vx = std::cos(angleRadians) * drift;
  vz = std::sin(angleRadians) * drift;
  vy = rise;
}

// 共鸣爆发光柱：元素反应触发瞬间从受击点升起的垂直光柱
// （原神元素爆发语言）。时长 0.55s：0~0.12s 缓出上升到满高，
// 0.12~0.22s 保持，随后线性衰减归零；透明度前 0.05s 快速淡入
// 后随全程线性淡出，宽度随高度略膨胀收缩。
inline float LightPillarDuration() { return 0.55f; }

struct LightPillarPose {
  float heightScale = 0.0f;  // 光柱高度进度（0..1）
  float widthScale = 0.7f;   // 宽度系数（随高度 0.7..1.0）
  float alpha = 0.0f;        // 整体透明度（0..1）
  bool visible = false;
};

inline LightPillarPose LightPillarPoseAt(float seconds) {
  LightPillarPose pose;
  const float duration = LightPillarDuration();
  if (seconds < 0.0f || seconds >= duration) return pose;
  pose.visible = true;
  const float t = seconds / duration;
  if (seconds < 0.12f) {
    // 缓出二次方：升起快、到位柔。
    const float rise = seconds / 0.12f;
    pose.heightScale = 1.0f - (1.0f - rise) * (1.0f - rise);
  } else {
    // 0.12~0.22s 保持满高，之后线性衰减归零。
    const float decay =
        std::clamp((seconds - 0.22f) / (duration - 0.22f), 0.0f, 1.0f);
    pose.heightScale = 1.0f - decay;
  }
  pose.widthScale = 0.7f + 0.3f * pose.heightScale;
  const float fadeIn = std::clamp(seconds / 0.05f, 0.0f, 1.0f);
  pose.alpha = fadeIn * (1.0f - t);
  return pose;
}

// 共鸣 FOV 冲击：元素反应触发瞬间相机短暂收窄视场角（zoom-in
// punch）再缓出恢复，强化爆发仪式感（原神元素爆发镜头语言）。
inline float FovPunchDuration() { return 0.45f; }

// 按秒数返回 FOV 偏移（度）：maxOffsetDegrees 传负值表示收窄。
// 前 20% 快速下潜到全量，后 80% 缓出二次方恢复；窗口外返回 0。
inline float FovPunchOffsetAt(float seconds, float maxOffsetDegrees) {
  const float duration = FovPunchDuration();
  if (seconds < 0.0f || seconds >= duration) return 0.0f;
  const float t = seconds / duration;
  const float dive = std::min(t / 0.2f, 1.0f);
  const float recover = t <= 0.2f ? 0.0f : (t - 0.2f) / 0.8f;
  const float ease = 1.0f - (1.0f - recover) * (1.0f - recover);
  return maxOffsetDegrees * dive * (1.0f - ease);
}

// 元素技能符文环：元素技能释放瞬间施法者脚下浮现旋转双新月
// 符阵（原神技能法阵语言），加法混合，缓出旋转 + 淡入淡出。
inline float SkillRuneDuration() { return 0.5f; }

struct SkillRunePose {
  float rotationRadians = 0.0f;  // 符阵旋转角（缓出减速，约 240°）
  float alpha = 0.0f;            // 整体透明度
  float scale = 1.0f;            // 半径微胀（0.85→1.0）
  bool visible = false;
};

inline SkillRunePose SkillRunePoseAt(float seconds) {
  SkillRunePose pose;
  const float duration = SkillRuneDuration();
  if (seconds < 0.0f || seconds >= duration) return pose;
  pose.visible = true;
  const float t = seconds / duration;
  // 缓出三次方旋转：起手快、收尾慢，总转角约 240°。
  const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
  pose.rotationRadians = ease * 4.18879f;
  // 前 10% 淡入，随后随全程线性淡出。
  const float fadeIn = std::clamp(t / 0.1f, 0.0f, 1.0f);
  pose.alpha = fadeIn * (1.0f - t);
  pose.scale = 0.85f + 0.15f * ease;
  return pose;
}

// 武器挥舞粒子拖尾：普攻窗口内发射点沿刀光扫掠角移动（与
// SlashArcPoseAt 同源），形成原神式武器拖尾。发射位置 =
// 角色位置 + 极坐标（朝向+angleRadians, radiusFactor×模型缩放）。
struct WeaponTrailPose {
  float angleRadians = 0.0f;  // 相对角色朝向的极角（与刀光扫掠角一致）
  float radiusFactor = 0.0f;  // 拖尾半径（角色模型缩放倍数）
  float heightFactor = 0.0f;  // 拖尾高度（角色模型缩放倍数）
  bool active = false;
};

inline WeaponTrailPose WeaponTrailPoseAt(float seconds, int comboSegment) {
  const SlashArcPose slash = SlashArcPoseAt(seconds, comboSegment);
  if (!slash.visible) return {};
  // 拖尾贴武器轨迹：半径略小于刀光（刀光 2.4），终结段随刀光同步放大。
  return {slash.sweepRadians, 1.9f * slash.scale, 1.05f, true};
}

// 拖尾粒子速度：沿扫掠切向（随极角增大方向）+ 轻微上扬；
// kind>=3 不受重力，拖尾悬浮在挥击轨迹上消散。
inline void WeaponTrailVelocity(float polarAngle, float tangentSpeed,
                                float& vx, float& vy, float& vz) {
  vx = std::cos(polarAngle) * tangentSpeed;
  vz = -std::sin(polarAngle) * tangentSpeed;
  vy = 0.006f;
}

// 普攻刀光元素染色（原神元素附魔语言）：施放元素技能后武器
// 附着对应源质，普攻刀光随之染色，直到施放另一系源质替换。
// lastSource：0=辉印 1=脉流 2=蚀质，-1/未知=默认金白；
// 终结段（comboSegment>=4）固定金橙不受附魔影响。
inline glm::vec3 SlashArcColorFor(int comboSegment, int lastSource) {
  if (comboSegment >= 4) return {1.0f, 0.78f, 0.38f};
  switch (lastSource) {
    case 0:  // 辉印附魔：金白
      return {1.0f, 0.92f, 0.55f};
    case 1:  // 脉流附魔：青蓝
      return {0.45f, 0.85f, 1.0f};
    case 2:  // 蚀质附魔：暗紫
      return {0.75f, 0.42f, 0.95f};
    default:  // 无附魔：默认金白
      return {1.0f, 0.88f, 0.55f};
  }
}

// 附魔武器拖尾火花 kind：随附魔源质切换配色（kind>=3 不受重力）。
inline int WeaponTrailKindFor(int lastSource) {
  switch (lastSource) {
    case 1:
      return 9;  // 脉流附魔青蓝拖尾
    case 2:
      return 10;  // 蚀质附魔暗紫拖尾
    default:
      return 7;  // 无附魔/辉印金白拖尾
  }
}

// 敌方技能元素（原神式敌方元素可读性）：每类敌人原型的攻击携带
// 专属元素色，玩家凭颜色即可读出敌人系别与威胁类型。
// 原型数值：0=RiftClaw 1=Priest 2=Guard 3=Bruiser 4=Caster 5=Elite。
// 返回源质编号 0=辉印 1=脉流 2=蚀质，-1=无元素（物理红）。
inline int EnemyElementFor(int archetype) {
  switch (archetype) {
    case 1:  // Priest：辉印祭司，金白仪式系
      return 0;
    case 4:  // Caster：脉流法师，青蓝法术系
      return 1;
    case 5:  // Elite：蚀质精英，暗紫侵蚀系
      return 2;
    default:  // RiftClaw/Guard/Bruiser：物理爪击/盾击/重击
      return -1;
  }
}

// 敌方刀光颜色：元素色或物理红（与渲染层原红色一致）。
inline glm::vec3 EnemySkillColorFor(int archetype) {
  const int element = EnemyElementFor(archetype);
  if (element < 0) return {1.0f, 0.42f, 0.36f};
  return AuraColorFor(element);
}

// 敌方蓄力火花/投射物 kind：元素 kind 或物理红 kind 1
//（复用火花配色表，kind>=4 为三系元素色）。
inline int EnemySkillSparkKindFor(int archetype) {
  const int element = EnemyElementFor(archetype);
  if (element < 0) return 1;
  return AuraSparkKindFor(element);
}

// 首领阶段转换爆发配色（原神首领转阶段语言）：阶段切换瞬间按
// 该阶段主导源质着色——1=辉印封锁金白、2=脉流风暴青蓝、
// 3=蚀质崩塌暗紫；终段规模更大，转阶段一阶段比一阶段凶。
struct BossPhaseVfx {
  glm::vec3 color{1.0f};  // 冲击波/光柱/符阵颜色
  int sparkKind = 4;      // 爆发火花 kind（复用既有配色表）
  float scale = 1.0f;     // 整体规模倍率（终段最强）
};

inline BossPhaseVfx BossPhaseVfxFor(int phase) {
  switch (phase) {
    case 1:  // 辉印封锁：金白
      return {{1.0f, 0.92f, 0.55f}, 4, 1.0f};
    case 2:  // 脉流风暴：青蓝
      return {{0.45f, 0.85f, 1.0f}, 5, 1.15f};
    case 3:  // 蚀质崩塌：暗紫（终段）
      return {{0.75f, 0.42f, 0.95f}, 6, 1.3f};
    default:  // 未知阶段回退辉印配色，不产生黑环。
      return {{1.0f, 0.92f, 0.55f}, 4, 1.0f};
  }
}
