#pragma once

#include "engine/core/tick_clock.h"
#include "engine/math/vec2.h"
#include "gameplay/combat/event.h"

#include <cstdint>
#include <optional>
#include <string>

enum class EnemyArchetype : uint8_t {
  Melee,
  Ranged,
  Support,
};

enum class EnemyIntent : uint8_t {
  Idle,
  Engage,
  Reposition,
  UseAbility,
  Recover,
};

enum class EnemyAiState : uint8_t {
  Idle,
  Evaluating,
  Moving,
  Acting,
  Recovering,
  Defeated,
};

enum class EnemyActionPhase : uint8_t {
  None,
  Windup,
  Active,
  Recovery,
};

enum class EnemyTargetPolicy : uint8_t {
  CurrentTarget,
  NearestHostile,
  LowestHealthHostile,
  Self,
};

enum class EnemyAbilityEffect : uint8_t {
  Damage,
  AreaDamage,
  Move,
  Control,
};

using EnemyAbilityId = uint32_t;

struct EnemyAbility {
  EnemyAbilityId id = 0;
  std::string tag;
  float range = 0.0f;
  Tick cooldownMs = 0;
  Tick windupMs = 0;
  Tick activeMs = 0;
  Tick recoveryMs = 0;
  float weight = 0.0f;
  EnemyTargetPolicy targetPolicy = EnemyTargetPolicy::CurrentTarget;
  EnemyAbilityEffect effect = EnemyAbilityEffect::Damage;
};

struct PerceptionSnapshot {
  Tick tick = 0;
  EntityId selfId = 0;
  Vec2 selfPosition;
  std::optional<EntityId> targetId;
  Vec2 targetPosition;
  float targetDistance = 0.0f;
  bool targetVisible = false;
};

struct EnemyActionPlan {
  Tick createdAt = 0;
  EnemyIntent intent = EnemyIntent::Idle;
  EnemyAiState state = EnemyAiState::Idle;
  EnemyActionPhase phase = EnemyActionPhase::None;
  std::optional<EnemyAbilityId> abilityId;
  std::optional<EntityId> targetId;
  Vec2 movement;
};
