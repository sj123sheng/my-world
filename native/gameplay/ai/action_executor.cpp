#include "action_executor.h"

#include <algorithm>
#include <limits>

Tick ActionExecutor::saturatingAdd(Tick tick, Tick duration) {
  const Tick maximum = std::numeric_limits<Tick>::max();
  if (duration > 0 && tick > maximum - duration) return maximum;
  return tick + duration;
}

bool ActionExecutor::validCancelPolicy(EnemyAbilityCancelPolicy policy) {
  switch (policy) {
    case EnemyAbilityCancelPolicy::Uninterruptible:
    case EnemyAbilityCancelPolicy::WindupOnly:
    case EnemyAbilityCancelPolicy::WindupAndActive:
      return true;
  }
  return false;
}

bool ActionExecutor::start(const EnemyActionPlan& plan, Tick tick) {
  if (ability_.has_value() || staggered_ || transactionIdsExhausted_ ||
      !plan.ability.has_value() || !plan.targetId.has_value() || *plan.targetId == 0) {
    return false;
  }

  const EnemyAbility& ability = *plan.ability;
  if (ability.id == 0 || ability.windupMs < 0 || ability.activeMs < 0 ||
      ability.recoveryMs < 0 || ability.interruptThreshold < 0 ||
      !validCancelPolicy(ability.cancelPolicy)) {
    return false;
  }

  ability_ = ability;
  targetId_ = plan.targetId;
  startTick_ = tick;
  hitTick_ = saturatingAdd(startTick_, ability_->windupMs);
  activeEndTick_ = saturatingAdd(hitTick_, ability_->activeMs);
  recoveryEndTick_ = saturatingAdd(activeEndTick_, ability_->recoveryMs);
  lastTick_ = tick;
  hitEmitted_ = false;
  interrupted_ = false;
  recoveryOnly_ = false;
  transactionId_ = nextTransactionId_;
  if (nextTransactionId_ == std::numeric_limits<uint64_t>::max()) {
    transactionIdsExhausted_ = true;
  } else {
    ++nextTransactionId_;
  }
  return true;
}

EnemyActionPhase ActionExecutor::phaseAt(Tick tick) const {
  if (!ability_.has_value()) return EnemyActionPhase::None;
  if (recoveryOnly_) {
    return tick < recoveryEndTick_ ? EnemyActionPhase::Recovery
                                   : EnemyActionPhase::None;
  }
  if (tick < hitTick_) return EnemyActionPhase::Windup;
  if (tick < activeEndTick_) return EnemyActionPhase::Active;
  if (tick < recoveryEndTick_) return EnemyActionPhase::Recovery;
  return EnemyActionPhase::None;
}

bool ActionExecutor::allowsInterrupt(EnemyActionPhase phase) const {
  if (!ability_.has_value()) return false;
  switch (ability_->cancelPolicy) {
    case EnemyAbilityCancelPolicy::Uninterruptible:
      return false;
    case EnemyAbilityCancelPolicy::WindupOnly:
      return phase == EnemyActionPhase::Windup;
    case EnemyAbilityCancelPolicy::WindupAndActive:
      return phase == EnemyActionPhase::Windup || phase == EnemyActionPhase::Active;
  }
  return false;
}

void ActionExecutor::enterInterruptedRecovery(Tick tick) {
  hitEmitted_ = true;
  interrupted_ = true;
  recoveryOnly_ = true;
  recoveryEndTick_ = saturatingAdd(tick, ability_->recoveryMs);
}

bool ActionExecutor::interrupt(Tick tick, FixedPoint poiseDamage,
                               EnemyInterruptCause cause) {
  if (cause == EnemyInterruptCause::PoiseBreak) {
    clearAction();
    staggered_ = true;
    return true;
  }
  if (cause != EnemyInterruptCause::PoiseDamage || !ability_.has_value() ||
      tick < startTick_ || tick < lastTick_) {
    return false;
  }

  const EnemyActionPhase phase = phaseAt(tick);
  if (!allowsInterrupt(phase) || poiseDamage < ability_->interruptThreshold) {
    return false;
  }
  enterInterruptedRecovery(tick);
  lastTick_ = tick;
  return true;
}

EnemyExecutionResult ActionExecutor::update(
    Tick tick, int64_t dtMs, const EnemyExecutionContext& context) {
  static_cast<void>(dtMs);
  EnemyExecutionResult result;
  if (staggered_) {
    result.state = EnemyAiState::Staggered;
    result.staggered = true;
    return result;
  }
  if (!ability_.has_value()) return result;

  const Tick effectiveTick = std::max(tick, lastTick_);
  lastTick_ = effectiveTick;

  if (!context.targetAlive && !hitEmitted_ && !recoveryOnly_) {
    enterInterruptedRecovery(effectiveTick);
  }

  if (!hitEmitted_ && effectiveTick >= hitTick_) {
    hitEmitted_ = true;
    HitRequest hit;
    hit.attacker = context.attacker;
    hit.target = *targetId_;
    hit.ability = ability_->id;
    hit.source = context.source;
    hit.baseDamage = context.baseDamage;
    hit.poiseDamage = context.poiseDamage;
    hit.tick = hitTick_;
    hit.sequence = context.sequence;
    hit.sourceAmount = context.sourceAmount;
    hit.transactionId = transactionId_;
    result.hit = hit;
  }

  result.phase = phaseAt(effectiveTick);
  result.interrupted = interrupted_;
  if (result.phase == EnemyActionPhase::None) {
    result.state = EnemyAiState::Idle;
    result.actionCompleted = true;
    clearAction();
  } else if (result.phase == EnemyActionPhase::Recovery) {
    result.state = EnemyAiState::Recovering;
  } else {
    result.state = EnemyAiState::Acting;
  }
  return result;
}

void ActionExecutor::clearAction() {
  ability_.reset();
  targetId_.reset();
  transactionId_ = 0;
  hitEmitted_ = false;
  interrupted_ = false;
  recoveryOnly_ = false;
}

void ActionExecutor::reset() {
  clearAction();
  staggered_ = false;
  startTick_ = 0;
  hitTick_ = 0;
  activeEndTick_ = 0;
  recoveryEndTick_ = 0;
  lastTick_ = 0;
}
