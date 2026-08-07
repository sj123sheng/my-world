#include "native/gameplay/world/exploration_content.h"

#include <cassert>

int main() {
  ExplorationContent content = ExplorationContent::verticalSlice();

  assert(content.pointsOfInterest().size() >= 4);
  assert(content.puzzles().size() >= 3);
  assert(content.gates().size() >= 2);
  assert(content.rewards().size() >= 4);

  const ExplorationTarget first = content.nearestTarget({0.52f, 0.16f}, 0.03f);
  assert(first.kind == ExplorationTargetKind::PointOfInterest);
  assert(first.id == 60);

  assert(content.discoverPoint(60));
  assert(!content.discoverPoint(60));
  assert(content.isPointDiscovered(60));

  // The lake puzzle requires swimming; being nearby is insufficient.
  assert(!content.activatePuzzle(71, MotionState::Grounded));
  assert(content.activatePuzzle(71, MotionState::Swimming));
  assert(content.isPuzzleActivated(71));
  assert(content.isGateOpen(81));

  // The hidden route requires gliding and only rewards the first claim.
  assert(!content.activatePuzzle(72, MotionState::Grounded));
  assert(content.activatePuzzle(72, MotionState::Gliding));
  assert(content.isGateOpen(82));
  assert(content.claimReward(92));
  assert(!content.claimReward(92));

  content.recordTraversal(TraversalAbility::Jump);
  content.recordTraversal(TraversalAbility::Sprint);
  content.recordTraversal(TraversalAbility::Glide);
  content.recordTraversal(TraversalAbility::Climb);
  content.recordTraversal(TraversalAbility::Swim);
  const uint8_t validTraversalMask = content.traversalMask();
  content.recordTraversal(static_cast<TraversalAbility>(255));
  assert(content.traversalMask() == validTraversalMask);
  assert(content.traversalUsed(TraversalAbility::Jump));
  assert(content.traversalUsed(TraversalAbility::Sprint));
  assert(content.traversalUsed(TraversalAbility::Glide));
  assert(content.traversalUsed(TraversalAbility::Climb));
  assert(content.traversalUsed(TraversalAbility::Swim));

  const ExplorationProgress progress = content.progress();
  assert(progress.discoveredPoiCount == 1);
  assert(progress.activatedPuzzleCount == 2);
  assert(progress.claimedRewardCount == 1);
  assert(progress.openGateCount == 2);
  assert(progress.completedTraversalCount == 5);
  return 0;
}
