#pragma once

#include "action_state_machine.h"
#include "damage_resolver.h"
#include "source_reaction_system.h"
#include "training_pulse.h"

#include <cstdint>
#include <vector>

struct CombatFrameInput {
  Tick tick = 0;
  int64_t dtMs = 0;
  bool moving = false;
  EntityId targetId = 0;
  bool targetAlive = false;
};

struct CombatSnapshot {
  uint8_t comboSegment = 0;
  FixedPoint playerHp = 0;
  FixedPoint targetHp = 0;
  FixedPoint targetPoise = 0;
  FixedPoint stamina = 0;
  FixedPoint resonance = 0;
  bool hasInsight = false;
  bool targetAlive = false;
  uint64_t lastAcceptedSequence = 0;
};

struct CombatEventBatch {
  std::vector<GameplayEvent> gameplay;
  std::vector<PresentationEvent> presentation;
};

class CombatController {
 public:
  static constexpr EntityId kPlayerId = 1;
  static constexpr EntityId kTrainingTargetId = 1001;

  explicit CombatController(CombatConfig config);

  void enqueue(ActionRequest request);
  void update(const CombatFrameInput& input);
  void reset();

  const CombatSnapshot& snapshot() const { return snapshot_; }
  const CombatEventBatch& events() const { return events_; }

 private:
  ActionContext contextFor(const CombatFrameInput& input) const;
  void emitDamageEvents(const HitRequest& hit, const DamageOutcome& damage);
  void refreshSnapshot();

  CombatConfig config_;
  ActionStateMachine actions_;
  TrainingPulse pulse_;
  TrainingTarget target_;
  DamageResolver damage_;
  SourceReactionSystem reactions_;
  std::vector<ActionRequest> pendingActions_;
  CombatSnapshot snapshot_;
  CombatEventBatch events_;
  FixedPoint playerHp_ = 0;
  uint8_t comboSegment_ = 0;
  uint64_t lastAcceptedSequence_ = 0;
};
