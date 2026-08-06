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
  // 攻击节奏调优：约 1.4s 一次挥击，前摇 0.28s 给玩家闪避窗口。
  EnemyAbility slash = attackAbility(enemy_ability_ids::kRiftClawSlash,
                                     "rift-claw-slash", fp(1.5), 1400, 280, 80,
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
  shield.cooldownMs = 5200;
  shield.windupMs = 700;
  shield.activeMs = 100;
  shield.recoveryMs = 500;
  shield.weight = fp(2.0);
  shield.category = EnemyAbilityCategory::Support;
  shield.targetPolicy = EnemyTargetPolicy::LowestShieldAlly;
  shield.effect = EnemyAbilityEffect::Shield;
  shield.effectAmount = fp(40);
  shield.telegraph = EnemyAbilityTelegraph::WarningYellow;
  shield.cancelPolicy = EnemyAbilityCancelPolicy::WindupOnly;
  shield.interruptThreshold = fp(10);

  // 远程法术节奏：约 2.4s 一枚法弹，与近战敌人形成交替施压。
  EnemyAbility bolt = attackAbility(enemy_ability_ids::kRadiantPriestBolt,
                                    "radiant-priest-bolt", fp(4.0), 2400, 600,
                                    80, 400, fp(1.0));
  bolt.telegraph = EnemyAbilityTelegraph::WarningYellow;
  bolt.cancelPolicy = EnemyAbilityCancelPolicy::WindupOnly;
  bolt.interruptThreshold = fp(10);
  config.abilities = {shield, bolt};
  return config;
}

EnemyAiConfig corrosionGuardDefaults() {
  EnemyAiConfig config = EnemyAiConfig::defaults();
  // 重击节奏：约 2.2s 一次重砸，前摇加长到 0.48s 凸显霸体威胁感。
  EnemyAbility bash = attackAbility(enemy_ability_ids::kCorrosionGuardBash,
                                    "corrosion-guard-bash", fp(1.5), 2200, 480,
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
