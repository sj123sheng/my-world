// render_animation.h: gameplay 快照到渲染动画意图的纯数据映射。

#pragma once

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

inline const char* RenderAnimationName(RenderAnimation animation) {
  switch (animation) {
    case RenderAnimation::Run:
      return "run";
    case RenderAnimation::Attack:
      return "attack";
    case RenderAnimation::Dodge:
      return "Running_Strafe_Right";
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
                               RenderAnimation animation) {
  std::vector<std::string> candidates{RenderAnimationName(animation)};
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
