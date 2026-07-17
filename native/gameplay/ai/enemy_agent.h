#pragma once

#include "action_executor.h"
#include "combat_region.h"
#include "decision_policy.h"
#include "perception_system.h"
#include "tactical_planner.h"
#include "gameplay/targeting/soft_targeting.h"

#include <cstddef>
#include <optional>
#include <vector>

enum class EnemyEscapeState : uint8_t {
  None,
  Repositioning,
  ReturningToSafePoint,
};

struct EnemyAgentTuning {
  Tick decisionPeriodMs = 100;
  float minimumProgress = 0.05f;
  std::size_t noProgressDecisionLimit = 3;
  float chaseTolerance = 0.5f;
  float safePointTolerance = 0.1f;
  float separationDistance = 1.0f;
};

struct EnemyUpdateInput {
  EnemyWorldView world;
  int64_t dtMs = 0;
  EnemyExecutionContext execution;
};

struct EnemyUpdateResult {
  std::optional<EnemyIntent> intent;
  std::optional<EnemyActionPlan> plan;
  EnemyAiState state = EnemyAiState::Idle;
  EnemyActionPhase phase = EnemyActionPhase::None;
  std::optional<Vec2> desiredPosition;
  Vec2 movement;
  Vec2 separation;
  std::optional<HitRequest> hit;
  std::optional<TargetCandidate> targetCandidate;
  EnemyEscapeState escapeState = EnemyEscapeState::None;
};

class EnemyAgent {
 public:
  EnemyAgent(EnemyArchetype archetype, EnemyAiConfig config,
             EnemyAgentTuning tuning = {});

  EnemyUpdateResult update(const EnemyUpdateInput& input);
  void releaseStagger();
  void reset();

  EnemyEscapeState escapeState() const noexcept { return escapeState_; }

 private:
  static Tick saturatingAdd(Tick tick, Tick duration);
  static EnemyAgentTuning sanitizedTuning(EnemyAgentTuning tuning);

  EnemyWorldView stableWorldView(const EnemyWorldView& source) const;
  Vec2 separationFor(const EnemyWorldView& world) const;
  bool finishSafePointReturn(Vec2 position, Vec2 safeReturnPosition);
  bool updateEscapeTracking(const EnemyActionPlan& plan, Tick tick, Vec2 position);
  void clearProgressTracking();
  void clearEscapeTracking();
  EnemyActionPlan constrainedPlan(EnemyActionPlan plan, Vec2 selfPosition,
                                  Vec2 separation) const;

  EnemyArchetype archetype_;
  EnemyAiConfig config_;
  EnemyAgentTuning tuning_;
  CombatRegion region_;
  std::vector<EnemyAbilityState> abilities_;
  PerceptionSystem perception_;
  DecisionPolicy policy_;
  TacticalPlanner planner_;
  ActionExecutor executor_;
  std::optional<PerceptionSnapshot> perceptionMemory_;
  std::optional<EnemyActionPlan> lastPlan_;
  EnemyEscapeState escapeState_ = EnemyEscapeState::None;
  EnemyIntent progressIntent_ = EnemyIntent::Idle;
  std::optional<EntityId> progressTargetId_;
  Vec2 progressDestination_;
  float bestRemainingDistance_ = 0.0f;
  Tick nextProgressDecisionTick_ = 0;
  Tick lastProgressDecisionTick_ = 0;
  std::size_t noProgressDecisions_ = 0;
  bool hasProgressSample_ = false;
  bool staggerLatched_ = false;
};
