#include "gameplay/ai/perception_system.h"

#include <cassert>
#include <type_traits>

namespace {

template <typename T, typename = void>
struct HasCanAttack : std::false_type {};

template <typename T>
struct HasCanAttack<T, std::void_t<decltype(&T::canAttack)>> : std::true_type {};

template <typename T, typename = void>
struct HasShouldSupport : std::false_type {};

template <typename T>
struct HasShouldSupport<T, std::void_t<decltype(&T::shouldSupport)>> : std::true_type {};

bool sameSnapshot(const PerceptionSnapshot& left, const PerceptionSnapshot& right) {
  if (left.tick != right.tick || left.selfId != right.selfId ||
      !(left.selfPosition == right.selfPosition) || left.selfAlive != right.selfAlive ||
      !(left.playerPosition == right.playerPosition) ||
      left.playerDistance != right.playerDistance ||
      left.playerAngleRadians != right.playerAngleRadians ||
      left.playerFacingAngleDeltaRadians != right.playerFacingAngleDeltaRadians ||
      left.playerVisible != right.playerVisible ||
      left.lastPlayerVisibleTick != right.lastPlayerVisibleTick ||
      left.playerThreat != right.playerThreat ||
      left.playerReachable != right.playerReachable ||
      left.selfInsideRegion != right.selfInsideRegion ||
      left.playerInsideRegion != right.playerInsideRegion ||
      !(left.safeReturnPosition == right.safeReturnPosition) ||
      left.distanceToSpawn != right.distanceToSpawn || left.recentlyHit != right.recentlyHit ||
      left.poise != right.poise || left.staggered != right.staggered ||
      left.actionPhase != right.actionPhase || left.targetId != right.targetId ||
      !(left.targetPosition == right.targetPosition) ||
      left.targetDistance != right.targetDistance ||
      left.targetVisible != right.targetVisible || left.allies.size() != right.allies.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.allies.size(); ++index) {
    const AllyPerception& first = left.allies[index];
    const AllyPerception& second = right.allies[index];
    if (first.id != second.id || first.archetype != second.archetype ||
        first.health != second.health || first.shield != second.shield ||
        !(first.position == second.position) || first.distanceToSelf != second.distanceToSelf ||
        first.alive != second.alive || first.insideRegion != second.insideRegion) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  static_assert(!HasCanAttack<PerceptionSnapshot>::value);
  static_assert(!HasShouldSupport<PerceptionSnapshot>::value);

  EnemyWorldView world = EnemyWorldView::testDefaults();
  world.tick = 42;
  world.selfPosition = {0.0f, 0.0f};
  world.selfFacing = {1.0f, 0.0f};
  world.playerId = 7;
  world.playerPosition = {0.3f, 0.4f};
  world.playerVisible = true;
  world.playerReachable = false;
  world.spawnPosition = {3.0f, 4.0f};
  world.allies = {
      {30, EnemyArchetype::Guard, fp(80), 0, {2.0f, 0.0f}, true, true},
      {10, EnemyArchetype::RiftClaw, fp(50), fp(10), {1.0f, 0.0f}, true, true},
      {20, EnemyArchetype::Priest, fp(0), 0, {4.0f, 0.0f}, false, true},
  };

  const PerceptionSnapshot facts = PerceptionSystem{}.observe(world);
  assert(facts.tick == 42);
  assert(facts.playerDistance == 0.5f);
  assert(facts.playerVisible);
  assert(facts.targetId == 7);
  assert(facts.targetPosition == facts.playerPosition);
  assert(facts.targetDistance == facts.playerDistance);
  assert(facts.targetVisible == facts.playerVisible);
  assert(facts.lastPlayerVisibleTick == 42);
  assert(facts.playerAngleRadians > 0.92f && facts.playerAngleRadians < 0.93f);
  assert(facts.playerFacingAngleDeltaRadians > 0.92f &&
         facts.playerFacingAngleDeltaRadians < 0.93f);
  assert(!facts.playerReachable);
  assert(facts.distanceToSpawn == 5.0f);
  assert(facts.selfInsideRegion);
  assert(facts.playerInsideRegion);
  assert(facts.allies.size() == 3);
  assert(facts.allies[0].id == 10);
  assert(facts.allies[1].id == 20);
  assert(facts.allies[2].id == 30);
  assert(facts.allies[0].distanceToSelf == 1.0f);

  for (int iteration = 0; iteration < 100; ++iteration) {
    assert(sameSnapshot(facts, PerceptionSystem{}.observe(world)));
  }

  world.playerVisible = false;
  world.lastPlayerVisibleTick = 17;
  const PerceptionSnapshot unseenFacts = PerceptionSystem{}.observe(world);
  assert(!unseenFacts.playerVisible);
  assert(unseenFacts.lastPlayerVisibleTick == 17);
}
