#include "native/engine/core/game_snapshot.h"
#include "native/gameplay/world/exploration_content.h"
#include "native/gameplay/world/exploration_gate_collision.h"

#include <cassert>
#include <string>

int main() {
  static_assert(static_cast<int32_t>(TraversalAbility::Jump) == 0);
  static_assert(static_cast<int32_t>(TraversalAbility::Sprint) == 1);
  static_assert(static_cast<int32_t>(TraversalAbility::Glide) == 2);
  static_assert(static_cast<int32_t>(TraversalAbility::Climb) == 3);
  static_assert(static_cast<int32_t>(TraversalAbility::Swim) == 4);

  ExplorationContent content = ExplorationContent::verticalSlice();
  content.discoverPoint(60);
  content.activatePuzzle(71, MotionState::Swimming);
  content.claimReward(91);
  content.recordTraversal(TraversalAbility::Swim);

  GameSnapshot snapshot;
  snapshot.explorationPoiCount = content.progress().discoveredPoiCount;
  snapshot.explorationPuzzleCount = content.progress().activatedPuzzleCount;
  snapshot.explorationRewardCount = content.progress().claimedRewardCount;
  snapshot.explorationGateCount = content.progress().openGateCount;
  snapshot.explorationTraversalMask = content.traversalMask();
  snapshot.explorationCurrentPoiId = 62;
  snapshot.explorationCurrentTargetLabel = "辉光湖畔渡口";

  assert(snapshot.explorationPoiCount == 1);
  assert(snapshot.explorationPuzzleCount == 1);
  assert(snapshot.explorationRewardCount == 1);
  assert(snapshot.explorationGateCount == 1);
  assert(snapshot.explorationTraversalMask == (1 << 4));
  assert(snapshot.explorationCurrentPoiId == 62);
  assert(snapshot.explorationCurrentTargetLabel == "辉光湖畔渡口");

  ExplorationContent gateContent = ExplorationContent::verticalSlice();
  ExplorationGateCollision gates =
      ExplorationGateCollision::fromContent(gateContent);
  float gateX = 0.78f;
  float gateY = 0.28f;
  assert(gates.resolve(gateX, gateY, 0.012f, 0.0f).touching);
  assert(gateContent.activatePuzzle(71, MotionState::Swimming));
  gates = ExplorationGateCollision::fromContent(gateContent);
  gateX = 0.78f;
  gateY = 0.28f;
  assert(!gates.resolve(gateX, gateY, 0.012f, 0.0f).touching);
  return 0;
}
