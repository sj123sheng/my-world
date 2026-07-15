#include "action_state_machine.h"

#include <algorithm>

ActionStateMachine::ActionStateMachine(CombatConfig config) : config_(config.validated()) {}

ActionDecision ActionStateMachine::request(const ActionRequest& request,
                                           const ActionContext& context) {
  if (request.action != CombatAction::Attack) {
    return {false, ActionRejectReason::InvalidAction};
  }
  if (context.target == 0) {
    return {false, ActionRejectReason::NoTarget};
  }
  if (!context.targetAlive) {
    return {false, ActionRejectReason::TargetDead};
  }
  if (actionActive_) {
    return {false, ActionRejectReason::ActionLocked};
  }

  if (!waitingForChain_ || comboIndex_ >= config_.comboDamage.size()) {
    comboIndex_ = 0;
  }
  ++comboIndex_;
  actionActive_ = true;
  waitingForChain_ = false;
  actionStartKnown_ = hasTimeline_;
  actionStartTick_ = lastUpdateTick_;
  actionElapsedMs_ = 0;
  chainElapsedMs_ = 0;
  actionContext_ = context;
  actionSequence_ = request.sequence;
  return {true, ActionRejectReason::None};
}

std::optional<HitRequest> ActionStateMachine::update(Tick now,
                                                     int64_t dtMs,
                                                     const ActionContext& context) {
  const Tick elapsed = std::max<int64_t>(0, dtMs);
  lastUpdateTick_ = now;
  hasTimeline_ = true;
  if (context.moving || context.damageTaken) {
    resetCombo();
    return std::nullopt;
  }

  if (actionActive_) {
    if (!actionStartKnown_) {
      actionStartTick_ = now - elapsed;
      actionStartKnown_ = true;
    }
    actionElapsedMs_ += elapsed;
    if (actionElapsedMs_ < kAttackHitMs) {
      return std::nullopt;
    }

    actionActive_ = false;
    waitingForChain_ = true;
    chainElapsedMs_ = actionElapsedMs_ - kAttackHitMs;
    const std::size_t index = comboIndex_ - 1;
    HitRequest hit{actionContext_.attacker,
                   actionContext_.target,
                   comboIndex_,
                   std::nullopt,
                   config_.comboDamage[index],
                   config_.comboPoiseDamage[index],
                   actionStartTick_ + kAttackHitMs,
                   actionSequence_};
    if (chainElapsedMs_ > config_.comboWindowMs) {
      resetCombo();
    }
    return hit;
  }

  if (waitingForChain_) {
    chainElapsedMs_ += elapsed;
    if (chainElapsedMs_ > config_.comboWindowMs) {
      resetCombo();
    }
  }
  return std::nullopt;
}

void ActionStateMachine::resetCombo() {
  comboIndex_ = 0;
  actionActive_ = false;
  waitingForChain_ = false;
  actionElapsedMs_ = 0;
  chainElapsedMs_ = 0;
  actionStartKnown_ = false;
}

void ActionStateMachine::reset() {
  resetCombo();
  hasTimeline_ = false;
  lastUpdateTick_ = 0;
  actionStartTick_ = 0;
  actionContext_ = {};
  actionSequence_ = 0;
}
