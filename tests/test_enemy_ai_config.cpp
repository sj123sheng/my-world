#include "gameplay/ai/enemy_ai_config.h"

#include <cassert>
#include <type_traits>

namespace {

EnemyAbility validSampleAbility() {
  static_assert(std::is_same_v<decltype(EnemyAbility::range), FixedPoint>);
  static_assert(std::is_same_v<decltype(EnemyAbility::weight), FixedPoint>);

  EnemyAbility ability;
  ability.id = 1;
  ability.tag = "test";
  ability.category = EnemyAbilityCategory::Attack;
  ability.range = fp(1);
  ability.cooldownMs = 100;
  ability.windupMs = 10;
  ability.activeMs = 10;
  ability.recoveryMs = 10;
  ability.weight = fp(1);
  ability.cancelPolicy = EnemyAbilityCancelPolicy::WindupOnly;
  ability.interruptThreshold = fp(5);
  ability.targetPolicy = EnemyTargetPolicy::CurrentTarget;
  ability.effect = EnemyAbilityEffect::Damage;
  return ability;
}

}  // namespace

int main() {
  static_assert(static_cast<unsigned>(EnemyArchetype::RiftClaw) == 0U);
  static_assert(static_cast<unsigned>(EnemyArchetype::Priest) == 1U);
  static_assert(static_cast<unsigned>(EnemyArchetype::Guard) == 2U);

  EnemyAiConfig config = EnemyAiConfig::defaults();
  assert(config.maxEnemies == 3);
  assert(config.abilities.empty());

  assert(config.validated().has_value());
  EnemyAiConfig overCapacity = config;
  overCapacity.maxEnemies = 4;
  assert(!overCapacity.validated().has_value());

  config.abilities.push_back(validSampleAbility());
  assert(config.validated().has_value());

  EnemyAiConfig invalid = config;
  invalid.abilities[0].windupMs = -1;
  assert(!invalid.validated().has_value());

  invalid = config;
  invalid.abilities[0].cancelPolicy = static_cast<EnemyAbilityCancelPolicy>(99);
  assert(!invalid.validated().has_value());

  invalid = config;
  invalid.abilities[0].interruptThreshold = -1;
  assert(!invalid.validated().has_value());

  invalid = config;
  invalid.abilities[0].telegraph = static_cast<EnemyAbilityTelegraph>(99);
  assert(!invalid.validated().has_value());

  invalid = config;
  invalid.staggerRecoveryMs = -1;
  assert(!invalid.validated().has_value());

  invalid = config;
  invalid.region.radius = 0.0f;
  assert(!invalid.validated().has_value());

  invalid = config;
  invalid.abilities[0].category = static_cast<EnemyAbilityCategory>(99);
  assert(!invalid.validated().has_value());

  EnemyAiConfig supportConfig = EnemyAiConfig::defaults();
  EnemyAbility support = validSampleAbility();
  support.category = EnemyAbilityCategory::Support;
  support.targetPolicy = EnemyTargetPolicy::LowestShieldAlly;
  supportConfig.abilities.push_back(support);
  assert(supportConfig.validated().has_value());

  EnemyAiConfig incompatible = supportConfig;
  incompatible.abilities[0].targetPolicy = EnemyTargetPolicy::Self;
  assert(!incompatible.validated().has_value());
  incompatible.abilities[0].targetPolicy = EnemyTargetPolicy::CurrentTarget;
  assert(!incompatible.validated().has_value());
  incompatible.abilities[0].targetPolicy = EnemyTargetPolicy::NearestHostile;
  assert(!incompatible.validated().has_value());

  incompatible = config;
  incompatible.abilities[0].targetPolicy = EnemyTargetPolicy::LowestShieldAlly;
  assert(!incompatible.validated().has_value());

  EnemyAiConfig selfAttackConfig = EnemyAiConfig::defaults();
  EnemyAbility selfAttack = validSampleAbility();
  selfAttack.targetPolicy = EnemyTargetPolicy::Self;
  selfAttackConfig.abilities.push_back(selfAttack);
  assert(selfAttackConfig.validated().has_value());
}
