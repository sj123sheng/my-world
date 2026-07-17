#pragma once

#include "enemy_ai_types.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <unordered_set>
#include <vector>

struct CombatRegionConfig {
  Vec2 center;
  float radius = 10.0f;
};

struct EnemyAiConfig {
  static constexpr std::size_t kMaxEnemies = 3;

  std::size_t maxEnemies = 3;
  std::vector<EnemyAbility> abilities;
  CombatRegionConfig region;

  static EnemyAiConfig defaults() { return {}; }

  std::optional<EnemyAiConfig> validated() const {
    if (maxEnemies == 0 || maxEnemies > kMaxEnemies || !validRegion(region)) {
      return std::nullopt;
    }

    std::unordered_set<EnemyAbilityId> abilityIds;
    for (const EnemyAbility& ability : abilities) {
      if (!validAbility(ability) || !abilityIds.insert(ability.id).second) {
        return std::nullopt;
      }
    }
    return *this;
  }

 private:
  static bool validRegion(const CombatRegionConfig& region) {
    return region.center.finite() && std::isfinite(region.radius) && region.radius > 0.0f;
  }

  static bool validAbility(const EnemyAbility& ability) {
    return ability.id != 0 && !ability.tag.empty() && ability.range > 0 &&
           ability.cooldownMs >= 0 && ability.windupMs >= 0 && ability.activeMs >= 0 &&
           ability.recoveryMs >= 0 && ability.weight > 0 && validCategory(ability.category) &&
           validTargetPolicy(ability.targetPolicy) && validCategoryTargetPolicy(ability.category,
                                                                                ability.targetPolicy) &&
           validEffect(ability.effect) && validCancelPolicy(ability.cancelPolicy) &&
           ability.interruptThreshold >= 0;
  }

  static bool validTargetPolicy(EnemyTargetPolicy policy) {
    switch (policy) {
      case EnemyTargetPolicy::CurrentTarget:
      case EnemyTargetPolicy::NearestHostile:
      case EnemyTargetPolicy::LowestHealthHostile:
      case EnemyTargetPolicy::LowestShieldAlly:
      case EnemyTargetPolicy::Self:
        return true;
    }
    return false;
  }

  static bool validCategory(EnemyAbilityCategory category) {
    switch (category) {
      case EnemyAbilityCategory::Attack:
      case EnemyAbilityCategory::Support:
        return true;
    }
    return false;
  }

  static bool validCategoryTargetPolicy(EnemyAbilityCategory category,
                                        EnemyTargetPolicy targetPolicy) {
    switch (category) {
      case EnemyAbilityCategory::Support:
        return targetPolicy == EnemyTargetPolicy::LowestShieldAlly;
      case EnemyAbilityCategory::Attack:
        return targetPolicy != EnemyTargetPolicy::LowestShieldAlly;
    }
    return false;
  }

  static bool validEffect(EnemyAbilityEffect effect) {
    switch (effect) {
      case EnemyAbilityEffect::Damage:
      case EnemyAbilityEffect::AreaDamage:
      case EnemyAbilityEffect::Move:
      case EnemyAbilityEffect::Control:
        return true;
    }
    return false;
  }

  static bool validCancelPolicy(EnemyAbilityCancelPolicy policy) {
    switch (policy) {
      case EnemyAbilityCancelPolicy::Uninterruptible:
      case EnemyAbilityCancelPolicy::WindupOnly:
      case EnemyAbilityCancelPolicy::WindupAndActive:
        return true;
    }
    return false;
  }
};
