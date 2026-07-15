#include "action_state_machine.h"

#include <algorithm>
#include <limits>

namespace {

FixedPoint multiply(FixedPoint value, FixedPoint multiplier) {
  return static_cast<FixedPoint>((static_cast<__int128>(value) * multiplier) / FP_ONE);
}

Tick hitTickFrom(Tick start) {
  const Tick maximum = std::numeric_limits<Tick>::max();
  if (start > maximum - 160) return maximum;
  return start + 160;
}

}  // namespace

ActionStateMachine::ActionStateMachine(CombatConfig config)
    : config_(config.validated()), resources_(config_) {}

ActionDecision ActionStateMachine::request(const ActionRequest& request,
                                           const ActionContext& context) {
  if (request.action != CombatAction::Attack && request.action != CombatAction::Dodge &&
      !isSourceAction(request.action) && request.action != CombatAction::Ultimate) {
    return {false, ActionRejectReason::InvalidAction};
  }
  if (request.action != CombatAction::Dodge && context.target == 0) {
    return {false, ActionRejectReason::NoTarget};
  }
  if (request.action != CombatAction::Dodge && !context.targetAlive) {
    return {false, ActionRejectReason::TargetDead};
  }
  if (actionActive_ || pendingHit_) {
    return {false, ActionRejectReason::ActionLocked};
  }

  if (request.action == CombatAction::Dodge &&
      !resources_.spendStamina(config_.dodgeCost, lastUpdateTick_)) {
    return {false, ActionRejectReason::InsufficientStamina};
  }

  if (isSourceAction(request.action) &&
      !resources_.sourceReady(sourceType(request.action), lastUpdateTick_)) {
    return {false, ActionRejectReason::Cooldown};
  }
  if (request.action == CombatAction::Ultimate && !resources_.canUltimate(lastUpdateTick_)) {
    return {false, ActionRejectReason::InsufficientResonance};
  }

  if (request.action == CombatAction::Dodge) {
    resetCombo();
    actionActive_ = true;
    activeAction_ = CombatAction::Dodge;
    actionStartKnown_ = hasTimeline_;
    actionStartTick_ = lastUpdateTick_;
    actionElapsedMs_ = 0;
    actionContext_ = context;
    actionSequence_ = request.sequence;
    return {true, ActionRejectReason::None};
  }

  if (isSourceAction(request.action) || request.action == CombatAction::Ultimate) {
    resetCombo();
    actionActive_ = true;
    activeAction_ = request.action;
    actionStartKnown_ = hasTimeline_;
    actionStartTick_ = lastUpdateTick_;
    actionElapsedMs_ = 0;
    actionContext_ = context;
    actionSequence_ = request.sequence;
    return {true, ActionRejectReason::None};
  }

  if (!waitingForChain_ || comboIndex_ >= config_.comboDamage.size()) {
    comboIndex_ = 0;
  }
  ++comboIndex_;
  actionActive_ = true;
  activeAction_ = CombatAction::Attack;
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
  resources_.advance(now);
  if ((context.moving || context.damageTaken) &&
      !(actionActive_ && activeAction_ == CombatAction::Dodge)) {
    resetCombo();
    return std::nullopt;
  }

  if (actionActive_) {
    if (!actionStartKnown_) {
      actionStartTick_ = now - elapsed;
      actionStartKnown_ = true;
    }
    actionElapsedMs_ += elapsed;
    if (activeAction_ == CombatAction::Dodge) {
      if (actionElapsedMs_ >= config_.dodgeDurationMs) {
        actionActive_ = false;
        actionStartKnown_ = false;
      }
      return std::nullopt;
    }
    if (actionElapsedMs_ < kAttackHitMs) {
      return std::nullopt;
    }

    if (context.target == 0 || !context.targetAlive || context.target != actionContext_.target) {
      actionActive_ = false;
      actionStartKnown_ = false;
      return std::nullopt;
    }

    actionActive_ = false;
    if (isSourceAction(activeAction_)) {
      const std::size_t index = sourceIndex(activeAction_);
      const SourceType source = sourceType(activeAction_);
      const Tick hitTick = hitTickFrom(actionStartTick_);
      FixedPoint damage = config_.sourceDamage[index];
      FixedPoint amount = FP_ONE;
      const bool insightApplied = resources_.insightAvailableAt(hitTick);
      if (insightApplied) {
        damage = multiply(damage, config_.insightDamageMultiplier);
        amount = multiply(amount, config_.insightDamageMultiplier);
      }
      pendingHit_ = PendingHitTransaction{
          actionSequence_, activeAction_, source, hitTick, insightApplied};
      actionStartKnown_ = false;
      return HitRequest{actionContext_.attacker,
                        actionContext_.target,
                        static_cast<AbilityId>(5 + index),
                        source,
                        damage,
                        config_.sourcePoiseDamage[index],
                        hitTick,
                        actionSequence_,
                        amount};
    }
    if (activeAction_ == CombatAction::Ultimate) {
      const Tick hitTick = hitTickFrom(actionStartTick_);
      pendingHit_ = PendingHitTransaction{
          actionSequence_, activeAction_, std::nullopt, hitTick, false};
      actionStartKnown_ = false;
      return HitRequest{actionContext_.attacker,
                        actionContext_.target,
                        8,
                        std::nullopt,
                        config_.ultimateDamage,
                        config_.ultimatePoiseDamage,
                        hitTick,
                        actionSequence_};
    }

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

bool ActionStateMachine::confirmHit(uint64_t sequence, bool landed) {
  if (!pendingHit_ || pendingHit_->sequence != sequence) return false;
  const PendingHitTransaction transaction = *pendingHit_;
  pendingHit_.reset();
  if (!landed) return true;

  if (transaction.source) {
    if (transaction.insightApplied) {
      (void)resources_.consumeInsight(transaction.hitTick);
    }
    resources_.startSourceCooldown(*transaction.source, transaction.hitTick);
    resources_.addResonance(config_.sourceResonanceGain);
    resources_.recordDistinctSource(*transaction.source, transaction.hitTick);
    return true;
  }
  if (transaction.action == CombatAction::Ultimate) {
    return resources_.spendUltimate(transaction.hitTick);
  }
  return false;
}

bool ActionStateMachine::isSourceAction(CombatAction action) {
  return action == CombatAction::Radiance || action == CombatAction::Current ||
         action == CombatAction::Corruption;
}

std::size_t ActionStateMachine::sourceIndex(CombatAction action) {
  switch (action) {
    case CombatAction::Radiance: return 0;
    case CombatAction::Current: return 1;
    case CombatAction::Corruption: return 2;
    default: return 0;
  }
}

SourceType ActionStateMachine::sourceType(CombatAction action) {
  switch (action) {
    case CombatAction::Radiance: return SourceType::Radiance;
    case CombatAction::Current: return SourceType::Current;
    case CombatAction::Corruption: return SourceType::Corruption;
    default: return SourceType::Radiance;
  }
}

bool ActionStateMachine::isInvulnerable() const {
  return actionActive_ && activeAction_ == CombatAction::Dodge &&
         actionElapsedMs_ < config_.dodgeInvulnerabilityMs;
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
  resources_.reset();
  hasTimeline_ = false;
  lastUpdateTick_ = 0;
  actionStartTick_ = 0;
  actionContext_ = {};
  actionSequence_ = 0;
  pendingHit_.reset();
}
