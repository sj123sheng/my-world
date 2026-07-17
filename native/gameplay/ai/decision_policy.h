#pragma once

#include "enemy_ai_types.h"

class DecisionPolicy {
 public:
  EnemyIntent choose(const PerceptionSnapshot& facts, EnemyArchetype archetype) const;
};
