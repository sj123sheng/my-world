// render_animation.h: gameplay 快照到渲染动画意图的纯数据映射。

#pragma once

#include <string>
#include <vector>

enum class RenderAnimation {
  Idle,
  Run,
  Attack,
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
  bool attacking = false;
  bool hit = false;
  bool moving = false;
};

inline RenderAnimation ChooseAnimation(const ActorRenderState& actor) {
  if (!actor.alive) return RenderAnimation::Death;
  if (actor.attacking) return RenderAnimation::Attack;
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
  const std::string desired = RenderAnimationName(animation);
  for (const std::string& clip : clips) {
    if (clip == desired) return clip;
  }
  for (const std::string& clip : clips) {
    if (clip == "idle") return clip;
  }
  return clips.empty() ? std::string{} : clips.front();
}
