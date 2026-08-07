#pragma once

#include "enemy_agent.h"
#include "gameplay/combat/damage_resolver.h"

namespace enemy_ability_ids {

constexpr EnemyAbilityId kRiftClawSlash = 1001;
constexpr EnemyAbilityId kRadiantPriestShield = 2001;
constexpr EnemyAbilityId kRadiantPriestBolt = 2002;
constexpr EnemyAbilityId kCorrosionGuardBash = 3001;
constexpr EnemyAbilityId kBruiserCrush = 4001;
constexpr EnemyAbilityId kCasterArcaneBolt = 5001;
constexpr EnemyAbilityId kEliteCleave = 6001;
constexpr EnemyAbilityId kEliteShockwave = 6002;

}  // namespace enemy_ability_ids

EnemyAiConfig riftClawDefaults();
EnemyAiConfig radiantPriestDefaults();
EnemyAiConfig corrosionGuardDefaults();
EnemyAiConfig bruiserDefaults();
EnemyAiConfig casterDefaults();
EnemyAiConfig eliteDefaults();
DirectionalDefenseProfile corrosionGuardDefense();
