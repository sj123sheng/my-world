#pragma once

#include "enemy_ai_types.h"

#include <cstdint>
#include <optional>

enum class EnemyInterruptCause : uint8_t {
  PoiseDamage,
  PoiseBreak,
};

struct EnemyExecutionContext {
  EntityId attacker = 0;
  bool targetAlive = false;
  FixedPoint baseDamage = 0;
  FixedPoint poiseDamage = 0;
  std::optional<SourceType> source;
  FixedPoint sourceAmount = FP_ONE;
  uint64_t sequence = 0;
};

struct EnemyExecutionResult {
  EnemyAiState state = EnemyAiState::Idle;
  EnemyActionPhase phase = EnemyActionPhase::None;
  std::optional<HitRequest> hit;
  std::optional<CombatEffectRequest> effect;
  bool actionCompleted = false;
  bool interrupted = false;
  bool staggered = false;
};

class ActionExecutor {
 public:
  bool start(const EnemyActionPlan& plan, Tick tick);
  EnemyExecutionResult update(Tick tick, int64_t dtMs,
                              const EnemyExecutionContext& context);
  bool cancel();
  bool interrupt(Tick tick, FixedPoint poiseDamage,
                 EnemyInterruptCause cause = EnemyInterruptCause::PoiseDamage);
  void reset();

 private:
  static Tick saturatingAdd(Tick tick, Tick duration);
  static bool validCancelPolicy(EnemyAbilityCancelPolicy policy);

  EnemyActionPhase phaseAt(Tick tick) const;
  bool allowsInterrupt(EnemyActionPhase phase) const;
  void enterInterruptedRecovery(Tick tick);
  void clearAction();

  std::optional<EnemyAbility> ability_;
  std::optional<EntityId> targetId_;
  Tick startTick_ = 0;
  Tick hitTick_ = 0;
  Tick activeEndTick_ = 0;
  Tick recoveryEndTick_ = 0;
  Tick lastTick_ = 0;
  uint64_t transactionId_ = 0;
  uint64_t nextTransactionId_ = 1;
  bool transactionIdsExhausted_ = false;
  bool transactionEmitted_ = false;
  bool interrupted_ = false;
  bool recoveryOnly_ = false;
  bool staggered_ = false;
};
