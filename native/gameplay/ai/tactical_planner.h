#pragma once

#include "enemy_ai_types.h"

#include <vector>

class TacticalPlanner {
 public:
  EnemyActionPlan plan(EnemyIntent intent, const PerceptionSnapshot& facts,
                       const std::vector<EnemyAbilityState>& abilities) const;
};
