// render_animation.h: gameplay 快照到渲染动画意图的纯数据映射。

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

enum class RenderAnimation {
  Idle,
  Run,
  Attack,
  Dodge,
  Radiance,
  Current,
  Corruption,
  Ultimate,
  Hit,
  Death,
};

enum class ModelKind {
  Player,
  Enemy,
  Boss,
};

struct ActorRenderState {
  bool alive = true;
  RenderAnimation action = RenderAnimation::Idle;
  bool hit = false;
  bool moving = false;
  // 归一化移动输入幅度（0..1）：驱动跑动步频缩放，与地面移速匹配。
  float moveRatio = 1.0f;
  // 受击/死亡动画变体索引：按奇偶在 hit/Hit_B、death/Death_B 之间
  // 轮换，打破连续受击与群体死亡的重复感。
  uint8_t variant = 0;
};

struct AnimationLogState {
  bool shouldReport(RenderAnimation animation, const std::string& clip) {
    if (initialized && animation == previousAnimation && clip == previousClip) {
      return false;
    }
    initialized = true;
    previousAnimation = animation;
    previousClip = clip;
    return true;
  }

  void reset() {
    initialized = false;
    previousAnimation = RenderAnimation::Idle;
    previousClip.clear();
  }

 private:
  bool initialized = false;
  RenderAnimation previousAnimation = RenderAnimation::Idle;
  std::string previousClip;
};

inline RenderAnimation ChooseAnimation(const ActorRenderState& actor) {
  if (!actor.alive) return RenderAnimation::Death;
  if (actor.action != RenderAnimation::Idle) return actor.action;
  if (actor.hit) return RenderAnimation::Hit;
  if (actor.moving) return RenderAnimation::Run;
  return RenderAnimation::Idle;
}

// 动作转场交叉混合时长（秒）：移动互切 0.15s，进入动作 0.12s，
// 动作恢复到移动/待机 0.2s，死亡转场 0.25s。
inline float AnimationBlendSeconds(RenderAnimation previous,
                                   RenderAnimation requested) {
  if (previous == requested) return 0.0f;
  const bool previousLocomotion =
      previous == RenderAnimation::Idle || previous == RenderAnimation::Run;
  const bool requestedLocomotion =
      requested == RenderAnimation::Idle || requested == RenderAnimation::Run;
  if (requestedLocomotion && previousLocomotion) return 0.15f;
  if (requested == RenderAnimation::Death) return 0.25f;
  if (requestedLocomotion) return 0.2f;
  return 0.12f;
}

// 跑动动画播放速率：按移动输入幅度缩放步频，使脚步节奏与
// 地面移速匹配，消除半推摇杆时的“滑步”；下限保持步态稳定。
inline float RunPlaybackRate(float moveRatio) {
  const float clamped = std::clamp(moveRatio, 0.0f, 1.0f);
  return 0.45f + 0.55f * clamped;
}

// 低速移动步态分层：输入幅度低于阈值时切换行走 clip，而不是
// 只把跑步动画放慢，避免低速时的“慢动作跑”廉价感。
inline bool ShouldUseWalkClip(float moveRatio) {
  constexpr float kWalkThreshold = 0.35f;
  return moveRatio < kWalkThreshold;
}

// clip 播放模式分类：待机/跑动与持续吟唱（Spellcasting）循环播放；
// 攻击、受击、死亡、闪避、单次施法等一次性 clip 播完后必须钳制
// 在尾帧，避免尸体倒地动作或攻击挥砍循环重播。
inline bool IsLoopingClip(const std::string& name) {
  return name == "idle" || name == "run" || name == "Spellcasting";
}

// 死亡尸体淡出曲线：先保持死亡尾帧 0.35s 让玩家看清倒地结果，
// 再用 0.55s 线性淡出到 0 完全移除，避免尸体永久留在场上。
inline float DeathFadeAlpha(float deathSeconds) {
  constexpr float kHoldSeconds = 0.35f;
  constexpr float kFadeSeconds = 0.55f;
  if (deathSeconds <= kHoldSeconds) return 1.0f;
  return std::clamp(1.0f - (deathSeconds - kHoldSeconds) / kFadeSeconds,
                    0.0f, 1.0f);
}

// Boss 出场显现曲线：激活后 0.8s 内从 0 线性升到 1，驱动轮廓光
// 渐入，让 Boss 从黑暗中逐步被勾出，强化登场仪式感。
inline float BossEntranceReveal(float entranceSeconds) {
  constexpr float kRevealSeconds = 0.8f;
  return std::clamp(entranceSeconds / kRevealSeconds, 0.0f, 1.0f);
}

inline const char* RenderAnimationName(RenderAnimation animation) {
  switch (animation) {
    case RenderAnimation::Run:
      return "run";
    case RenderAnimation::Attack:
      return "attack";
    case RenderAnimation::Dodge:
      return "Dodge_Forward";
    case RenderAnimation::Radiance:
      return "Spellcast_Raise";
    case RenderAnimation::Current:
      return "Spellcast_Shoot";
    case RenderAnimation::Corruption:
      return "Spellcasting";
    case RenderAnimation::Ultimate:
      return "Spellcast_Long";
    case RenderAnimation::Hit:
      return "hit";
    case RenderAnimation::Death:
      return "death";
    case RenderAnimation::Idle:
    default:
      return "idle";
  }
}

inline std::string ResolveClip(const std::vector<std::string>& clips,
                               RenderAnimation animation, int variant = 0,
                               float moveRatio = 1.0f) {
  std::vector<std::string> candidates{RenderAnimationName(animation)};
  // 低速步态分层：低幅度输入优先行走 clip（Walking_B），缺失时
  // 自动回退 run；资产无行走 clip 时行为与升级前完全一致。
  if (animation == RenderAnimation::Run && ShouldUseWalkClip(moveRatio)) {
    candidates.insert(candidates.begin(), "Walking_B");
  }
  // 受击/死亡变体轮换：奇数变体优先选用 B 版 clip，缺失时自动
  // 回退主 clip，资产无变体时行为与升级前完全一致。
  if (animation == RenderAnimation::Hit && (variant & 1) == 1) {
    candidates.insert(candidates.begin(), "Hit_B");
  } else if (animation == RenderAnimation::Death && (variant & 1) == 1) {
    candidates.insert(candidates.begin(), "Death_B");
  }
  if (animation == RenderAnimation::Dodge) {
    candidates.push_back("run");
  } else if (animation == RenderAnimation::Radiance ||
             animation == RenderAnimation::Current ||
             animation == RenderAnimation::Corruption ||
             animation == RenderAnimation::Ultimate) {
    candidates.push_back("attack");
  }
  if (animation != RenderAnimation::Idle) candidates.push_back("idle");

  for (const std::string& candidate : candidates) {
    for (const std::string& clip : clips) {
      if (clip == candidate) return clip;
    }
  }
  return clips.empty() ? std::string{} : clips.front();
}
