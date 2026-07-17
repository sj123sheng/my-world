#pragma once

#include "enemy_ai_config.h"

Vec2 stableSeparation(EntityId selfId, Vec2 selfPosition, EntityId otherId,
                      Vec2 otherPosition, float minimumDistance) noexcept;

class CombatRegion {
 public:
  explicit CombatRegion(CombatRegionConfig config = {});

  bool contains(Vec2 point, float tolerance = 0.0f) const noexcept;
  Vec2 projectInside(Vec2 point, float inset = 0.0f) const noexcept;

  Vec2 stableSeparation(EntityId selfId, Vec2 selfPosition, EntityId otherId,
                        Vec2 otherPosition, float minimumDistance) const noexcept {
    return ::stableSeparation(selfId, selfPosition, otherId, otherPosition,
                              minimumDistance);
  }

  const CombatRegionConfig& config() const noexcept { return config_; }

 private:
  CombatRegionConfig config_;
};
