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
  // 新原型追加在末尾，不动现有数值（渲染层与 world_layout.gen.h
  // SpawnArchetype 按数值对应：Bruiser/Caster/Elite）。
  Bruiser = 3,  // 重甲近战：高韧性/高伤害/慢速
  Caster = 4,   // 远程施法：保持距离、投射类能力
  Elite = 5,    // 精英：高血量、多能力、霸体窗口
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
  Staggered,
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
  Shield,
};

enum class EnemyAbilityTelegraph : uint8_t {
  Neutral,
  WarningYellow,
};

enum class EnemyAbilityCancelPolicy : uint8_t {
  Uninterruptible,
  WindupOnly,
  WindupAndActive,
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
  FixedPoint effectAmount = 0;
  EnemyAbilityTelegraph telegraph = EnemyAbilityTelegraph::Neutral;
  EnemyAbilityCancelPolicy cancelPolicy = EnemyAbilityCancelPolicy::Uninterruptible;
  FixedPoint interruptThreshold = 0;
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

// 交战区域（留白投影边界）：类型层定义，供快照与配置共享。
struct CombatRegionConfig {
  Vec2 center;
  float radius = 10.0f;
};

// 原型交战留白（Plan 2）：由 engagement_spacing 统一给出，快照携带供
// DecisionPolicy/TacticalPlanner 消费。
struct EngagementRange {
  float minimum = 0.0f;
  float ideal = 0.0f;
  float attack = 0.0f;
  float maxPursuit = 0.0f;
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
  // 交战留白（Plan 2 Task 6）：区域、原型距离、环形槽位与邻居分离。
  CombatRegionConfig region;
  EngagementRange engagementRange;
  Vec2 engagementSlot;
  Vec2 separationOffset;
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
  std::optional<EnemyAbility> ability;
  std::optional<EntityId> targetId;
  std::optional<Vec2> desiredPosition;
  EnemyPlanFallbackReason fallbackReason = EnemyPlanFallbackReason::None;
  Vec2 movement;
};
