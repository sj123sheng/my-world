#include "tactical_planner.h"

#include <cmath>
#include <limits>

namespace {

struct TargetChoice {
  EntityId id = 0;
  Vec2 position;
  float distance = 0.0f;
};

std::optional<EnemyAbilityCategory> requiredCategory(EnemyIntent intent) {
  switch (intent) {
    case EnemyIntent::Attack:
      return EnemyAbilityCategory::Attack;
    case EnemyIntent::Support:
      return EnemyAbilityCategory::Support;
    default:
      return std::nullopt;
  }
}

std::optional<FixedPoint> fixedDistance(float distance) {
  if (!std::isfinite(distance) || distance < 0.0f ||
      distance > static_cast<float>(std::numeric_limits<FixedPoint>::max() / FP_ONE)) {
    return std::nullopt;
  }
  return fp(distance);
}

std::optional<TargetChoice> chooseLowestShieldAlly(const EnemyAbility& ability,
                                                    const PerceptionSnapshot& facts) {
  std::optional<TargetChoice> selected;
  FixedPoint selectedShield = 0;
  for (const AllyPerception& ally : facts.allies) {
    if (ally.id == 0 || ally.id == facts.selfId || !ally.alive || !ally.insideRegion ||
        ally.shield > 0 || !ally.position.finite()) {
      continue;
    }
    const std::optional<FixedPoint> distance = fixedDistance(ally.distanceToSelf);
    if (!distance.has_value() || *distance > ability.range) continue;

    if (!selected.has_value() || ally.shield < selectedShield ||
        (ally.shield == selectedShield && ally.id < selected->id)) {
      selected = TargetChoice{ally.id, ally.position, ally.distanceToSelf};
      selectedShield = ally.shield;
    }
  }
  return selected;
}

std::optional<TargetChoice> chooseTarget(const EnemyAbility& ability,
                                         const PerceptionSnapshot& facts) {
  if (ability.category == EnemyAbilityCategory::Support) {
    if (ability.targetPolicy != EnemyTargetPolicy::LowestShieldAlly) return std::nullopt;
    return chooseLowestShieldAlly(ability, facts);
  }

  if (ability.category != EnemyAbilityCategory::Attack) return std::nullopt;
  if (ability.targetPolicy == EnemyTargetPolicy::Self) {
    return facts.selfPosition.finite()
               ? std::optional<TargetChoice>(TargetChoice{facts.selfId, facts.selfPosition, 0.0f})
               : std::nullopt;
  }
  if (ability.targetPolicy != EnemyTargetPolicy::CurrentTarget &&
      ability.targetPolicy != EnemyTargetPolicy::NearestHostile &&
      ability.targetPolicy != EnemyTargetPolicy::LowestHealthHostile) {
    return std::nullopt;
  }
  if (!facts.targetId.has_value() || !facts.targetVisible || !facts.targetPosition.finite() ||
      !std::isfinite(facts.targetDistance) || facts.targetDistance < 0.0f) {
    return std::nullopt;
  }
  return TargetChoice{*facts.targetId, facts.targetPosition, facts.targetDistance};
}

bool isBetter(const EnemyAbility& candidate, FixedPoint candidateExcessRange,
              const EnemyAbility& current, FixedPoint currentExcessRange) {
  if (candidate.weight != current.weight) return candidate.weight > current.weight;
  if (candidateExcessRange != currentExcessRange) {
    return candidateExcessRange < currentExcessRange;
  }
  return candidate.id < current.id;
}

EnemyActionPlan movementPlan(EnemyIntent intent, const PerceptionSnapshot& facts,
                              const Vec2& destination,
                              EnemyPlanFallbackReason fallbackReason) {
  EnemyActionPlan plan;
  plan.createdAt = facts.tick;
  plan.intent = intent;
  plan.state = EnemyAiState::Moving;
  plan.desiredPosition = destination;
  plan.fallbackReason = fallbackReason;
  plan.movement = destination - facts.selfPosition;
  return plan;
}

EnemyActionPlan idlePlan(EnemyIntent intent, const PerceptionSnapshot& facts,
                         EnemyPlanFallbackReason fallbackReason) {
  EnemyActionPlan plan;
  plan.createdAt = facts.tick;
  plan.intent = intent;
  plan.fallbackReason = fallbackReason;
  plan.desiredPosition = facts.selfPosition;
  return plan;
}

Vec2 retreatDestination(const PerceptionSnapshot& facts) {
  if (!facts.targetVisible) return facts.safeReturnPosition;

  const Vec2 away = facts.selfPosition - facts.targetPosition;
  const float distance = away.length();
  if (!away.finite() || !std::isfinite(distance) || distance <= 0.0f) {
    return facts.safeReturnPosition;
  }
  return facts.selfPosition + away * (1.0f / distance);
}

}  // namespace

EnemyActionPlan TacticalPlanner::plan(EnemyIntent intent, const PerceptionSnapshot& facts,
                                      const std::vector<EnemyAbilityState>& abilities) const {
  if (!facts.selfInsideRegion || !facts.playerInsideRegion) {
    return movementPlan(EnemyIntent::ReturnToArea, facts, facts.safeReturnPosition,
                        EnemyPlanFallbackReason::OutsideRegion);
  }

  if (intent == EnemyIntent::ReturnToArea) {
    return movementPlan(intent, facts, facts.safeReturnPosition,
                        EnemyPlanFallbackReason::None);
  }

  const std::optional<EnemyAbilityCategory> category = requiredCategory(intent);
  if (!category.has_value()) {
    if (intent == EnemyIntent::Chase && facts.targetVisible) {
      return movementPlan(intent, facts, facts.targetPosition, EnemyPlanFallbackReason::None);
    }
    if (intent == EnemyIntent::Retreat) {
      return movementPlan(intent, facts, retreatDestination(facts), EnemyPlanFallbackReason::None);
    }
    if (intent == EnemyIntent::Idle) {
      return idlePlan(intent, facts, EnemyPlanFallbackReason::None);
    }
    if (intent == EnemyIntent::BreakFree) {
      return movementPlan(intent, facts, facts.safeReturnPosition,
                          EnemyPlanFallbackReason::None);
    }
    return idlePlan(intent, facts, EnemyPlanFallbackReason::UnsupportedIntent);
  }

  const EnemyAbility* selectedAbility = nullptr;
  std::optional<TargetChoice> selectedTarget;
  FixedPoint selectedExcessRange = 0;
  for (const EnemyAbilityState& state : abilities) {
    const EnemyAbility& ability = state.ability;
    if (state.cooldownRemainingMs > 0 || ability.id == 0 || ability.category != *category ||
        ability.range <= 0) {
      continue;
    }

    const std::optional<TargetChoice> target = chooseTarget(ability, facts);
    if (!target.has_value()) continue;
    const std::optional<FixedPoint> distance = fixedDistance(target->distance);
    if (!distance.has_value() || *distance > ability.range) continue;

    const FixedPoint excessRange = ability.range - *distance;
    if (selectedAbility == nullptr ||
        isBetter(ability, excessRange, *selectedAbility, selectedExcessRange)) {
      selectedAbility = &ability;
      selectedTarget = target;
      selectedExcessRange = excessRange;
    }
  }

  if (selectedAbility == nullptr) {
    if (intent == EnemyIntent::Attack) {
      return facts.targetVisible
                 ? movementPlan(EnemyIntent::Chase, facts, facts.targetPosition,
                                EnemyPlanFallbackReason::NoLegalAbility)
                 : idlePlan(EnemyIntent::Idle, facts, EnemyPlanFallbackReason::NoTarget);
    }
    return movementPlan(EnemyIntent::Retreat, facts, facts.safeReturnPosition,
                        EnemyPlanFallbackReason::NoLegalAbility);
  }

  EnemyActionPlan plan;
  plan.createdAt = facts.tick;
  plan.intent = intent;
  plan.state = EnemyAiState::Acting;
  plan.ability = *selectedAbility;
  plan.targetId = selectedTarget->id;
  plan.desiredPosition = selectedTarget->position;
  plan.movement = selectedTarget->position - facts.selfPosition;
  return plan;
}
