#pragma once

#include "combat_action.h"
#include "combat_config.h"
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
};

class ActionStateMachine {
 public:
  explicit ActionStateMachine(CombatConfig config);

  ActionDecision request(const ActionRequest& request, const ActionContext& context);
  std::optional<HitRequest> update(Tick now, int64_t dtMs, const ActionContext& context);
  void resetCombo();
  void reset();

 private:
  static constexpr Tick kAttackHitMs = 160;

  CombatConfig config_;
  ActionState state_ = ActionState::Idle;
  uint8_t comboIndex_ = 0;
  bool actionActive_ = false;
  bool waitingForChain_ = false;
  Tick actionElapsedMs_ = 0;
  Tick chainElapsedMs_ = 0;
  ActionContext actionContext_;
  uint64_t actionSequence_ = 0;
};
