#pragma once

#include "../ai/enemy_ai_types.h"

struct Enemy {
  EntityId id = 0;
  FixedPoint hp = 0;
  FixedPoint poise = 0;
  SourceType resist = SourceType::Radiance;
  EnemyArchetype archetype = EnemyArchetype::RiftClaw;
  Vec2 position;
  Vec2 spawnPosition;
  Vec2 safeReturnPosition;
  FixedPoint shield = 0;
  float collisionRadius = 0.5f;

  bool alive() const { return id != 0 && hp > 0; }

  void takeHit(const HitEvent& hit) {
    if (hit.baseDamage > 0) hp = hp > hit.baseDamage ? hp - hit.baseDamage : 0;
    const FixedPoint poiseDamage = fp(2);
    poise = poise > poiseDamage ? poise - poiseDamage : 0;
  }
};
