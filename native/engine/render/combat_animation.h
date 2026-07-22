// combat_animation.h: 战斗动作状态到渲染动画意图的纯映射。

#pragma once

#include "native/engine/render/render_animation.h"
#include "native/gameplay/combat/combat_action.h"
#include "native/gameplay/entities/boss.h"

inline RenderAnimation PlayerRenderAnimation(ActionState state,
                                             CombatAction activeAction) {
  switch (state) {
    case ActionState::Attack1:
    case ActionState::Attack2:
    case ActionState::Attack3:
    case ActionState::Attack4:
      return RenderAnimation::Attack;
    case ActionState::Dodging:
      return RenderAnimation::Dodge;
    case ActionState::CastingSource:
      switch (activeAction) {
        case CombatAction::Radiance:
          return RenderAnimation::Radiance;
        case CombatAction::Current:
          return RenderAnimation::Current;
        case CombatAction::Corruption:
          return RenderAnimation::Corruption;
        default:
          return RenderAnimation::Idle;
      }
    case ActionState::CastingUltimate:
      return activeAction == CombatAction::Ultimate
                 ? RenderAnimation::Ultimate
                 : RenderAnimation::Idle;
    case ActionState::Idle:
    default:
      return RenderAnimation::Idle;
  }
}

inline RenderAnimation BossRenderAnimation(const BossSnapshot& boss,
                                           FixedPoint previousHp) {
  if (boss.defeated) return RenderAnimation::Death;
  if (boss.castRemainingMs > 0) return RenderAnimation::Ultimate;
  if (previousHp > 0 && boss.hp < previousHp) return RenderAnimation::Hit;
  return RenderAnimation::Idle;
}
