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
  state_ = static_cast<ActionState>(static_cast<uint8_t>(ActionState::Attack1) + comboIndex_ - 1);
  actionActive_ = true;
  waitingForChain_ = false;
  actionElapsedMs_ = 0;
  chainElapsedMs_ = 0;
  actionContext_ = context;
  actionSequence_ = request.sequence;
  return {true, ActionRejectReason::None};
}

std::optional<HitRequest> ActionStateMachine::update(Tick now,
                                                     int64_t dtMs,
                                                     const ActionContext& context) {
  if (context.moving || context.damageTaken) {
    resetCombo();
    return std::nullopt;
  }

  const Tick elapsed = std::max<int64_t>(0, dtMs);
  if (actionActive_) {
    actionElapsedMs_ += elapsed;
    if (actionElapsedMs_ < kAttackHitMs) {
      return std::nullopt;
    }

    actionActive_ = false;
    waitingForChain_ = true;
    chainElapsedMs_ = 0;
    state_ = ActionState::Idle;
    const std::size_t index = comboIndex_ - 1;
    return HitRequest{actionContext_.attacker,
                      actionContext_.target,
                      comboIndex_,
                      std::nullopt,
                      config_.comboDamage[index],
                      config_.comboPoiseDamage[index],
                      now,
                      actionSequence_};
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
  state_ = ActionState::Idle;
}

void ActionStateMachine::reset() {
  resetCombo();
  actionContext_ = {};
  actionSequence_ = 0;
}
