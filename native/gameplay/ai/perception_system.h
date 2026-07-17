#pragma once

#include "enemy_ai_config.h"

#include <vector>

struct EnemyWorldAlly {
  EntityId id = 0;
  EnemyArchetype archetype = EnemyArchetype::RiftClaw;
  FixedPoint health = 0;
  FixedPoint shield = 0;
  Vec2 position;
  bool alive = false;
  bool insideRegion = false;
};

struct EnemyWorldView {
  Tick tick = 0;
  EntityId selfId = 0;
  bool selfAlive = true;
  Vec2 selfPosition;
  Vec2 selfFacing = {1.0f, 0.0f};
  Vec2 spawnPosition;
  Vec2 safeReturnPosition;
  CombatRegionConfig region;
  EntityId playerId = 1;
  Vec2 playerPosition;
  bool playerVisible = true;
  Tick lastPlayerVisibleTick = 0;
  FixedPoint playerThreat = 0;
  bool playerReachable = true;
  bool recentlyHit = false;
  FixedPoint poise = 0;
  bool staggered = false;
  EnemyActionPhase actionPhase = EnemyActionPhase::None;
  std::vector<EnemyWorldAlly> allies;

  static EnemyWorldView testDefaults();
};

class PerceptionSystem {
 public:
  PerceptionSnapshot observe(const EnemyWorldView& world) const;
};
