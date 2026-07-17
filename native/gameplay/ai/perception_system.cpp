#include "perception_system.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

float angleOf(Vec2 direction) {
  return std::atan2(direction.y, direction.x);
}

float normalizedAngle(float angle) {
  while (angle > kPi) angle -= 2.0f * kPi;
  while (angle < -kPi) angle += 2.0f * kPi;
  return angle;
}

bool insideRegion(Vec2 point, const CombatRegionConfig& region) {
  return (point - region.center).length() <= region.radius;
}

}  // namespace

EnemyWorldView EnemyWorldView::testDefaults() {
  EnemyWorldView world;
  world.region = {{0.0f, 0.0f}, 10.0f};
  return world;
}

PerceptionSnapshot PerceptionSystem::observe(const EnemyWorldView& world) const {
  const Vec2 playerOffset = world.playerPosition - world.selfPosition;
  const float playerDistance = playerOffset.length();
  const bool playerVisible = world.playerVisible;
  PerceptionSnapshot snapshot;
  snapshot.tick = world.tick;
  snapshot.selfId = world.selfId;
  snapshot.selfPosition = world.selfPosition;
  snapshot.selfAlive = world.selfAlive;
  snapshot.playerPosition = world.playerPosition;
  snapshot.playerDistance = playerDistance;
  snapshot.targetId = world.playerId == 0 ? std::nullopt
                                          : std::optional<EntityId>{world.playerId};
  snapshot.targetPosition = world.playerPosition;
  snapshot.targetDistance = playerDistance;
  snapshot.targetVisible = playerVisible;
  snapshot.playerAngleRadians = angleOf(playerOffset);
  snapshot.playerFacingAngleDeltaRadians =
      normalizedAngle(snapshot.playerAngleRadians - angleOf(world.selfFacing));
  snapshot.playerVisible = playerVisible;
  snapshot.lastPlayerVisibleTick = playerVisible ? world.tick : world.lastPlayerVisibleTick;
  snapshot.playerThreat = world.playerThreat;
  snapshot.playerReachable = world.playerReachable;
  snapshot.selfInsideRegion = insideRegion(world.selfPosition, world.region);
  snapshot.playerInsideRegion = insideRegion(world.playerPosition, world.region);
  snapshot.safeReturnPosition = world.safeReturnPosition;
  snapshot.distanceToSpawn = (world.selfPosition - world.spawnPosition).length();
  snapshot.recentlyHit = world.recentlyHit;
  snapshot.poise = world.poise;
  snapshot.staggered = world.staggered;
  snapshot.actionPhase = world.actionPhase;
  snapshot.allies.reserve(world.allies.size());
  for (const EnemyWorldAlly& ally : world.allies) {
    snapshot.allies.push_back({ally.id, ally.archetype, ally.health, ally.shield, ally.position,
                               (ally.position - world.selfPosition).length(), ally.alive,
                               ally.insideRegion});
  }
  std::sort(snapshot.allies.begin(), snapshot.allies.end(),
            [](const AllyPerception& left, const AllyPerception& right) {
              return left.id < right.id;
            });
  return snapshot;
}
