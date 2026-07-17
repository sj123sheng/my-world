#include "enemy_agent.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace {

float distanceBetween(Vec2 left, Vec2 right) {
  if (!left.finite() || !right.finite()) return std::numeric_limits<float>::infinity();
  const double deltaX = static_cast<double>(left.x) - right.x;
  const double deltaY = static_cast<double>(left.y) - right.y;
  const double distance = std::hypot(deltaX, deltaY);
  if (!std::isfinite(distance) ||
      distance > static_cast<double>(std::numeric_limits<float>::max())) {
    return std::numeric_limits<float>::max();
  }
  return static_cast<float>(distance);
}

float finiteFloat(double value) {
  const double maximum = static_cast<double>(std::numeric_limits<float>::max());
  if (!std::isfinite(value)) return 0.0f;
  return static_cast<float>(std::max(-maximum, std::min(maximum, value)));
}

Vec2 finiteDifference(Vec2 destination, Vec2 origin) {
  if (!destination.finite() || !origin.finite()) return {};
  return {
      finiteFloat(static_cast<double>(destination.x) - origin.x),
      finiteFloat(static_cast<double>(destination.y) - origin.y),
  };
}

bool allyLess(const EnemyWorldAlly& left, const EnemyWorldAlly& right) {
  if (left.id != right.id) return left.id < right.id;
  if (left.position.x != right.position.x) return left.position.x < right.position.x;
  if (left.position.y != right.position.y) return left.position.y < right.position.y;
  if (left.archetype != right.archetype) {
    return static_cast<std::underlying_type_t<EnemyArchetype>>(left.archetype) <
           static_cast<std::underlying_type_t<EnemyArchetype>>(right.archetype);
  }
  if (left.health != right.health) return left.health < right.health;
  if (left.shield != right.shield) return left.shield < right.shield;
  if (left.alive != right.alive) return left.alive < right.alive;
  return left.insideRegion < right.insideRegion;
}

}  // namespace

EnemyAgent::EnemyAgent(EnemyArchetype archetype, EnemyAiConfig config,
                       EnemyAgentTuning tuning)
    : archetype_(archetype),
      config_(std::move(config)),
      tuning_(sanitizedTuning(tuning)),
      region_(config_.region) {
  abilities_.reserve(config_.abilities.size());
  for (const EnemyAbility& ability : config_.abilities) {
    abilities_.push_back({ability, 0});
  }
}

Tick EnemyAgent::saturatingAdd(Tick tick, Tick duration) {
  const Tick maximum = std::numeric_limits<Tick>::max();
  if (duration > 0 && tick > maximum - duration) return maximum;
  return tick + duration;
}

EnemyAgentTuning EnemyAgent::sanitizedTuning(EnemyAgentTuning tuning) {
  const EnemyAgentTuning defaults;
  if (tuning.decisionPeriodMs <= 0) tuning.decisionPeriodMs = defaults.decisionPeriodMs;
  if (!std::isfinite(tuning.minimumProgress) || tuning.minimumProgress < 0.0f) {
    tuning.minimumProgress = defaults.minimumProgress;
  }
  if (tuning.noProgressDecisionLimit == 0) {
    tuning.noProgressDecisionLimit = defaults.noProgressDecisionLimit;
  }
  if (!std::isfinite(tuning.chaseTolerance) || tuning.chaseTolerance < 0.0f) {
    tuning.chaseTolerance = defaults.chaseTolerance;
  }
  if (!std::isfinite(tuning.safePointTolerance) || tuning.safePointTolerance < 0.0f) {
    tuning.safePointTolerance = defaults.safePointTolerance;
  }
  if (!std::isfinite(tuning.separationDistance) || tuning.separationDistance <= 0.0f) {
    tuning.separationDistance = defaults.separationDistance;
  }
  return tuning;
}

EnemyWorldView EnemyAgent::stableWorldView(const EnemyWorldView& source) const {
  EnemyWorldView world = source;
  world.region = region_.config();
  world.safeReturnPosition = region_.projectInside(world.safeReturnPosition);
  if (!world.selfPosition.finite()) world.selfPosition = world.safeReturnPosition;
  if (!world.spawnPosition.finite()) world.spawnPosition = world.safeReturnPosition;
  if (!world.selfFacing.finite() ||
      (world.selfFacing.x == 0.0f && world.selfFacing.y == 0.0f)) {
    world.selfFacing = {1.0f, 0.0f};
  }
  if (!world.playerPosition.finite()) {
    world.playerId = 0;
    world.playerPosition = world.selfPosition;
    world.playerVisible = false;
    world.playerReachable = true;
  }

  world.allies.erase(
      std::remove_if(world.allies.begin(), world.allies.end(),
                     [&world](const EnemyWorldAlly& ally) {
                       return ally.id == 0 || ally.id == world.selfId ||
                              !ally.position.finite();
                     }),
      world.allies.end());
  std::sort(world.allies.begin(), world.allies.end(), allyLess);

  std::vector<EnemyWorldAlly> uniqueAllies;
  uniqueAllies.reserve(world.allies.size());
  for (std::size_t first = 0; first < world.allies.size();) {
    std::size_t last = first + 1;
    while (last < world.allies.size() && world.allies[last].id == world.allies[first].id) {
      ++last;
    }
    if (last == first + 1) uniqueAllies.push_back(world.allies[first]);
    first = last;
  }
  world.allies = std::move(uniqueAllies);
  return world;
}

Vec2 EnemyAgent::separationFor(const EnemyWorldView& world) const {
  double sumX = 0.0;
  double sumY = 0.0;
  for (const EnemyWorldAlly& ally : world.allies) {
    if (!ally.alive) continue;
    const Vec2 separation = stableSeparation(
        world.selfId, world.selfPosition, ally.id, ally.position,
        tuning_.separationDistance);
    sumX += separation.x;
    sumY += separation.y;
  }
  const Vec2 combined = {finiteFloat(sumX), finiteFloat(sumY)};
  return combined.finite() ? combined : Vec2{};
}

bool EnemyAgent::finishSafePointReturn(Vec2 position, Vec2 safeReturnPosition) {
  if (escapeState_ == EnemyEscapeState::ReturningToSafePoint &&
      distanceBetween(position, safeReturnPosition) <= tuning_.safePointTolerance) {
    clearEscapeTracking();
    executor_.cancel();
    lastPlan_.reset();
    perceptionMemory_.reset();
    return true;
  }
  return false;
}

bool EnemyAgent::updateEscapeTracking(const EnemyActionPlan& plan, Tick tick,
                                      Vec2 position) {
  if (plan.state != EnemyAiState::Moving || !plan.desiredPosition.has_value() ||
      !plan.desiredPosition->finite()) {
    if (escapeState_ == EnemyEscapeState::ReturningToSafePoint) {
      clearProgressTracking();
    } else {
      clearEscapeTracking();
    }
    return false;
  }

  if (plan.intent == EnemyIntent::BreakFree &&
      escapeState_ == EnemyEscapeState::None) {
    escapeState_ = EnemyEscapeState::Repositioning;
  } else if (plan.intent != EnemyIntent::BreakFree &&
             escapeState_ == EnemyEscapeState::Repositioning) {
    escapeState_ = EnemyEscapeState::None;
  }

  const Vec2 destination = *plan.desiredPosition;
  const float remainingDistance = distanceBetween(position, destination);
  const bool goalChanged = !hasProgressSample_ || progressIntent_ != plan.intent ||
                           progressTargetId_ != plan.targetId ||
                           progressDestination_.x != destination.x ||
                           progressDestination_.y != destination.y;
  if (goalChanged) {
    hasProgressSample_ = true;
    progressIntent_ = plan.intent;
    progressTargetId_ = plan.targetId;
    progressDestination_ = destination;
    bestRemainingDistance_ = remainingDistance;
    noProgressDecisions_ = 0;
    lastProgressDecisionTick_ = tick;
    nextProgressDecisionTick_ = saturatingAdd(tick, tuning_.decisionPeriodMs);
    return false;
  }
  if (tick < nextProgressDecisionTick_ || tick <= lastProgressDecisionTick_) return false;

  const float progress = bestRemainingDistance_ - remainingDistance;
  if (progress >= tuning_.minimumProgress) {
    bestRemainingDistance_ = remainingDistance;
    noProgressDecisions_ = 0;
  } else if (noProgressDecisions_ < tuning_.noProgressDecisionLimit) {
    ++noProgressDecisions_;
  }
  lastProgressDecisionTick_ = tick;
  nextProgressDecisionTick_ = saturatingAdd(tick, tuning_.decisionPeriodMs);
  if (noProgressDecisions_ >= tuning_.noProgressDecisionLimit) {
    escapeState_ = EnemyEscapeState::ReturningToSafePoint;
    return true;
  }
  return false;
}

void EnemyAgent::clearProgressTracking() {
  progressIntent_ = EnemyIntent::Idle;
  progressTargetId_.reset();
  progressDestination_ = {};
  bestRemainingDistance_ = 0.0f;
  nextProgressDecisionTick_ = 0;
  lastProgressDecisionTick_ = 0;
  noProgressDecisions_ = 0;
  hasProgressSample_ = false;
}

void EnemyAgent::clearEscapeTracking() {
  escapeState_ = EnemyEscapeState::None;
  clearProgressTracking();
}

EnemyActionPlan EnemyAgent::constrainedPlan(EnemyActionPlan plan, Vec2 selfPosition,
                                            Vec2 separation) const {
  Vec2 destination = plan.desiredPosition.value_or(selfPosition);
  destination = region_.projectInside(destination);
  const Vec2 separatedDestination = {
      finiteFloat(static_cast<double>(destination.x) + separation.x),
      finiteFloat(static_cast<double>(destination.y) + separation.y),
  };
  destination = region_.projectInside(separatedDestination);
  plan.desiredPosition = destination;
  plan.movement = finiteDifference(destination, selfPosition);
  return plan;
}

EnemyUpdateResult EnemyAgent::update(const EnemyUpdateInput& input) {
  EnemyUpdateResult result;
  const EnemyWorldView world = stableWorldView(input.world);
  if (!world.selfAlive) {
    reset();
    result.state = EnemyAiState::Defeated;
    return result;
  }

  PerceptionSnapshot facts = perception_.observe(world);
  facts.selfInsideRegion = region_.contains(world.selfPosition);
  facts.playerInsideRegion = region_.contains(world.playerPosition, tuning_.chaseTolerance);
  perceptionMemory_ = facts;

  if (facts.staggered && !staggerLatched_) {
    executor_.interrupt(world.tick, 0, EnemyInterruptCause::PoiseBreak);
    staggerLatched_ = true;
  }

  const bool arrivedAtSafePoint =
      finishSafePointReturn(world.selfPosition, world.safeReturnPosition);
  EnemyIntent intent = staggerLatched_ ? EnemyIntent::Idle
                                       : policy_.choose(facts, archetype_);
  if (arrivedAtSafePoint) {
    intent = EnemyIntent::Idle;
  } else if (!staggerLatched_ &&
             escapeState_ == EnemyEscapeState::ReturningToSafePoint) {
    intent = EnemyIntent::BreakFree;
  }

  const Vec2 separation = separationFor(world);
  EnemyActionPlan plan;
  if (arrivedAtSafePoint || staggerLatched_) {
    plan = EnemyActionPlan{};
    plan.createdAt = facts.tick;
    plan.intent = EnemyIntent::Idle;
    plan.desiredPosition = facts.selfPosition;
  } else {
    plan = planner_.plan(intent, facts, abilities_);
  }
  plan = constrainedPlan(std::move(plan), world.selfPosition, separation);

  bool enteredSafePointReturn = false;
  if (staggerLatched_) {
    clearProgressTracking();
  } else if (!arrivedAtSafePoint) {
    enteredSafePointReturn =
        updateEscapeTracking(plan, world.tick, world.selfPosition);
  }
  if (enteredSafePointReturn) {
    intent = EnemyIntent::BreakFree;
    plan = constrainedPlan(planner_.plan(intent, facts, abilities_),
                           world.selfPosition, separation);
  }

  if (plan.intent == EnemyIntent::ReturnToArea ||
      plan.intent == EnemyIntent::BreakFree) {
    executor_.cancel();
  }
  lastPlan_ = plan;
  executor_.start(plan, world.tick);
  EnemyExecutionContext executionContext = input.execution;
  executionContext.attacker = world.selfId;
  const EnemyExecutionResult execution =
      executor_.update(world.tick, input.dtMs, executionContext);

  result.intent = intent;
  result.plan = plan;
  result.state = execution.state;
  if (execution.state == EnemyAiState::Idle) result.state = plan.state;
  result.phase = execution.phase;
  result.desiredPosition = plan.desiredPosition;
  result.movement = plan.movement.finite() ? plan.movement : Vec2{};
  result.separation = separation;
  result.hit = execution.hit;
  result.escapeState = escapeState_;
  if (world.selfId != 0 &&
      world.selfId <= static_cast<EntityId>(std::numeric_limits<int32_t>::max())) {
    result.targetCandidate =
        TargetCandidate{static_cast<int32_t>(world.selfId), world.selfPosition};
  }
  return result;
}

void EnemyAgent::releaseStagger() {
  if (!staggerLatched_) return;
  executor_.reset();
  staggerLatched_ = false;
  lastPlan_.reset();
}

void EnemyAgent::reset() {
  executor_.reset();
  perceptionMemory_.reset();
  lastPlan_.reset();
  clearEscapeTracking();
  staggerLatched_ = false;
}
