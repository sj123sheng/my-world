#pragma once

#include "engine/core/tick_clock.h"
#include "engine/math/vec2.h"
#include "gameplay/combat/event.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class EnemyArchetype : uint8_t {
  RiftClaw,
  Priest,
  Guard,
};

enum class EnemyIntent : uint8_t {
  Idle,
  Chase,
  Attack,
  Retreat,
  ReturnToArea,
  BreakFree,
  Support,
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
  LowestShieldAlly,
  Self,
};

enum class EnemyAbilityCategory : uint8_t {
  Attack,
  Support,
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
  FixedPoint range = 0;
  Tick cooldownMs = 0;
  Tick windupMs = 0;
  Tick activeMs = 0;
  Tick recoveryMs = 0;
  FixedPoint weight = 0;
  EnemyAbilityCategory category = EnemyAbilityCategory::Attack;
  EnemyTargetPolicy targetPolicy = EnemyTargetPolicy::CurrentTarget;
  EnemyAbilityEffect effect = EnemyAbilityEffect::Damage;
};

struct EnemyAbilityState {
  EnemyAbility ability;
  Tick cooldownRemainingMs = 0;
};

struct AllyPerception {
  EntityId id = 0;
  EnemyArchetype archetype = EnemyArchetype::RiftClaw;
  FixedPoint health = 0;
  FixedPoint shield = 0;
  Vec2 position;
  float distanceToSelf = 0.0f;
  bool alive = false;
  bool insideRegion = false;
};

struct PerceptionSnapshot {
  Tick tick = 0;
  EntityId selfId = 0;
  Vec2 selfPosition;
  bool selfAlive = true;
  Vec2 playerPosition;
  float playerDistance = 0.0f;
  std::optional<EntityId> targetId;
  Vec2 targetPosition;
  float targetDistance = 0.0f;
  bool targetVisible = false;
  float playerAngleRadians = 0.0f;
  float playerFacingAngleDeltaRadians = 0.0f;
  bool playerVisible = false;
  Tick lastPlayerVisibleTick = 0;
  FixedPoint playerThreat = 0;
  bool playerReachable = true;
  bool selfInsideRegion = true;
  bool playerInsideRegion = true;
  Vec2 safeReturnPosition;
  float distanceToSpawn = 0.0f;
  bool recentlyHit = false;
  FixedPoint poise = 0;
  bool staggered = false;
  EnemyActionPhase actionPhase = EnemyActionPhase::None;
  std::vector<AllyPerception> allies;
};

enum class EnemyPlanFallbackReason : uint8_t {
  None,
  NoLegalAbility,
  OutsideRegion,
  NoTarget,
  UnsupportedIntent,
};

struct EnemyActionPlan {
  Tick createdAt = 0;
  EnemyIntent intent = EnemyIntent::Idle;
  EnemyAiState state = EnemyAiState::Idle;
  EnemyActionPhase phase = EnemyActionPhase::None;
  std::optional<EnemyAbilityId> abilityId;
  std::optional<EntityId> targetId;
  std::optional<Vec2> desiredPosition;
  EnemyPlanFallbackReason fallbackReason = EnemyPlanFallbackReason::None;
  Vec2 movement;
};
