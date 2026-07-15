#pragma once

#include "combat_action.h"
#include "combat_config.h"
#include "combat_resources.h"
#include "event.h"

#include <cstdint>
#include <optional>

struct ActionContext {
  EntityId attacker = 1;
  EntityId target = 0;
  bool targetAlive = false;
  bool moving = false;
  bool damageTaken = false;
};

struct ActionDecision {
  bool accepted = false;
  ActionRejectReason reason = ActionRejectReason::InvalidAction;
};

struct HitRequest {
  EntityId attacker = 0;
  EntityId target = 0;
  AbilityId ability = 0;
  std::optional<SourceType> source;
  FixedPoint baseDamage = 0;
  FixedPoint poiseDamage = 0;
  Tick tick = 0;
  uint64_t sequence = 0;
  FixedPoint sourceAmount = FP_ONE;
  uint64_t transactionId = 0;
};

class ActionStateMachine {
 public:
  explicit ActionStateMachine(CombatConfig config);

  ActionDecision request(const ActionRequest& request, const ActionContext& context);
  std::optional<HitRequest> update(Tick now, int64_t dtMs, const ActionContext& context);
  bool confirmHit(uint64_t transactionId, bool landed);
  void resetCombo();
  void reset();
  bool isInvulnerable() const;
  bool wasInvulnerableAt(Tick tick) const;
  FixedPoint stamina() const { return resources_.stamina(); }
  void grantInsight(Tick tick) { resources_.grantInsight(tick); }
  bool hasInsight() const { return resources_.hasInsight(); }
  Tick insightRemainingMs() const { return resources_.insightRemainingMs(); }
  FixedPoint resonance() const { return resources_.resonance(); }
  void addResonance(FixedPoint amount) { resources_.addResonance(amount); }
  void recordDistinctSource(SourceType source, Tick tick) {
    resources_.recordDistinctSource(source, tick);
  }
  bool canUltimate(Tick tick) { return resources_.canUltimate(tick); }
  ActionState state() const;
  Tick comboWindowRemainingMs() const;
  Tick sourceCooldownRemainingMs(SourceType source) const {
    return resources_.sourceCooldownRemainingMs(source);
  }
  Tick ultimateWindowRemainingMs() const { return resources_.ultimateWindowRemainingMs(); }

 private:
  static constexpr Tick kAttackHitMs = 160;

  static bool isSourceAction(CombatAction action);
  static std::size_t sourceIndex(CombatAction action);
  static SourceType sourceType(CombatAction action);
  uint64_t allocateTransactionId();

  CombatConfig config_;
  CombatResources resources_;
  CombatAction activeAction_ = CombatAction::Attack;
  uint8_t comboIndex_ = 0;
  bool actionActive_ = false;
  bool waitingForChain_ = false;
  bool hasTimeline_ = false;
  bool actionStartKnown_ = false;
  Tick lastUpdateTick_ = 0;
  Tick actionStartTick_ = 0;
  Tick actionElapsedMs_ = 0;
  Tick chainElapsedMs_ = 0;
  ActionContext actionContext_;
  uint64_t actionSequence_ = 0;

  struct PendingHitTransaction {
    uint64_t transactionId = 0;
    CombatAction action = CombatAction::Attack;
    std::optional<SourceType> source;
    Tick hitTick = 0;
    bool insightApplied = false;
  };
  std::optional<PendingHitTransaction> pendingHit_;
  uint64_t nextTransactionId_ = 1;
  bool transactionIdsExhausted_ = false;
  bool dodgeIntervalKnown_ = false;
  Tick dodgeInvulnerableFrom_ = 0;
  Tick dodgeInvulnerableUntil_ = 0;
};
