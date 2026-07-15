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
  FixedPoint playerPoise = 0;
  FixedPoint targetHp = 0;
  FixedPoint targetPoise = 0;
  FixedPoint stamina = 0;
  FixedPoint resonance = 0;
  bool hasInsight = false;
  bool invulnerable = false;
  Tick insightMs = 0;
  Tick pulseWarningMs = 0;
  ActionRejectReason lastRejectReason = ActionRejectReason::None;
  bool targetAlive = false;
  uint64_t lastAcceptedSequence = 0;
  uint8_t currentAction = static_cast<uint8_t>(ActionState::Idle);
  Tick comboWindowMs = 0;
  Tick radianceCooldownMs = 0;
  Tick currentCooldownMs = 0;
  Tick corruptionCooldownMs = 0;
  Tick ultimateWindowMs = 0;
  bool targetPoiseBroken = false;
  bool radianceAttached = false;
  bool currentAttached = false;
  bool corruptionAttached = false;
  bool corroded = false;
  int32_t currentReaction = -1;
  uint8_t pulsePhase = static_cast<uint8_t>(PulseEventKind::None);
  AbilityId lastAbility = 0;
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
  void resetTrainingEncounter();
  void sortEvents();

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
  FixedPoint playerPoise_ = 0;
  uint8_t comboSegment_ = 0;
  uint64_t lastAcceptedSequence_ = 0;
  ActionRejectReason lastRejectReason_ = ActionRejectReason::None;
  Tick currentTick_ = 0;
  int32_t currentReaction_ = -1;
  PulseEventKind pulsePhase_ = PulseEventKind::None;
  AbilityId lastAbility_ = 0;
};
