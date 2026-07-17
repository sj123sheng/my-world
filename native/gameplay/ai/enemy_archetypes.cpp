#include "enemy_archetypes.h"

namespace {

EnemyAbility attackAbility(EnemyAbilityId id, const char* tag, FixedPoint range,
                           Tick cooldownMs, Tick windupMs, Tick activeMs,
                           Tick recoveryMs, FixedPoint weight) {
  EnemyAbility ability;
  ability.id = id;
  ability.tag = tag;
  ability.range = range;
  ability.cooldownMs = cooldownMs;
  ability.windupMs = windupMs;
  ability.activeMs = activeMs;
  ability.recoveryMs = recoveryMs;
  ability.weight = weight;
  ability.category = EnemyAbilityCategory::Attack;
  ability.targetPolicy = EnemyTargetPolicy::CurrentTarget;
  ability.effect = EnemyAbilityEffect::Damage;
  return ability;
}

}  // namespace

EnemyAiConfig riftClawDefaults() {
  EnemyAiConfig config = EnemyAiConfig::defaults();
  EnemyAbility slash = attackAbility(enemy_ability_ids::kRiftClawSlash,
                                     "rift-claw-slash", fp(1.5), 700, 180, 80,
                                     320, fp(1.0));
  slash.cancelPolicy = EnemyAbilityCancelPolicy::WindupOnly;
  slash.interruptThreshold = fp(10);
  config.abilities = {slash};
  return config;
}

EnemyAiConfig radiantPriestDefaults() {
  EnemyAiConfig config = EnemyAiConfig::defaults();

  EnemyAbility shield;
  shield.id = enemy_ability_ids::kRadiantPriestShield;
  shield.tag = "radiant-priest-shield";
  shield.range = fp(4.0);
  shield.cooldownMs = 3000;
  shield.windupMs = 700;
  shield.activeMs = 100;
  shield.recoveryMs = 500;
  shield.weight = fp(2.0);
  shield.category = EnemyAbilityCategory::Support;
  shield.targetPolicy = EnemyTargetPolicy::LowestShieldAlly;
  shield.effect = EnemyAbilityEffect::Shield;
  shield.telegraph = EnemyAbilityTelegraph::WarningYellow;
  shield.cancelPolicy = EnemyAbilityCancelPolicy::WindupOnly;
  shield.interruptThreshold = fp(10);

  EnemyAbility bolt = attackAbility(enemy_ability_ids::kRadiantPriestBolt,
                                    "radiant-priest-bolt", fp(4.0), 1500, 600,
                                    80, 400, fp(1.0));
  bolt.telegraph = EnemyAbilityTelegraph::WarningYellow;
  bolt.cancelPolicy = EnemyAbilityCancelPolicy::WindupOnly;
  bolt.interruptThreshold = fp(10);
  config.abilities = {shield, bolt};
  return config;
}

EnemyAiConfig corrosionGuardDefaults() {
  EnemyAiConfig config = EnemyAiConfig::defaults();
  EnemyAbility bash = attackAbility(enemy_ability_ids::kCorrosionGuardBash,
                                    "corrosion-guard-bash", fp(1.5), 1200, 350,
                                    100, 600, fp(1.0));
  bash.cancelPolicy = EnemyAbilityCancelPolicy::Uninterruptible;
  bash.interruptThreshold = 0;
  config.abilities = {bash};
  config.staggerRecoveryMs = 1200;
  return config;
}

DirectionalDefenseProfile corrosionGuardDefense() {
  DirectionalDefenseProfile defense;
  defense.frontalHpDamageMultiplier = fp(0.5);
  defense.minimumFrontDot = 0.0f;
  return defense;
}
