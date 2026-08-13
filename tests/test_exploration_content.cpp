#include "native/gameplay/world/exploration_content.h"

#include <cassert>

int main() {
  ExplorationContent content = ExplorationContent::verticalSlice();

  assert(content.pointsOfInterest().size() >= 4);
  assert(content.naturalNodes().size() == 4);
  assert(content.regionTriggers().size() == 4);
  assert(content.rewards().size() >= 4);

  const ExplorationTarget first = content.nearestTarget({0.52f, 0.16f}, 0.03f);
  assert(first.kind == ExplorationTargetKind::PointOfInterest);
  assert(first.id == 60);

  assert(content.discoverPoint(60));
  assert(!content.discoverPoint(60));
  assert(content.isPointDiscovered(60));

  // 区域只能在前置自然节点已激活且玩家真正进入半径后完成。
  assert(!content.enterRegion(81, {0.70f, 0.20f}));
  assert(!content.activateNaturalNode(71, MotionState::Swimming));
  assert(content.activateNaturalNode(71, MotionState::Grounded));
  assert(content.isNaturalNodeActivated(71));
  assert(!content.enterRegion(81, {0.90f, 0.20f}));
  assert(content.enterRegion(81, {0.70f, 0.20f}));
  assert(content.isRegionCompleted(81));
  assert(content.regionById(81) != nullptr);
  assert(content.regionById(999) == nullptr);
  assert(content.openGateMask() == (1 << 1));

  // 岩台风脉需要滑翔，奖励仍只可领取一次。
  assert(!content.activateNaturalNode(72, MotionState::Grounded));
  assert(content.activateNaturalNode(72, MotionState::Gliding));
  assert(content.claimReward(92));
  assert(!content.claimReward(92));

  // V9 第四个磁盘掩码槽继续按原 80..83 声明位序恢复区域状态。
  ExplorationContent restored = ExplorationContent::verticalSlice();
  restored.restoreMasks(0, 0, 0, content.openGateMask(), 0);
  assert(restored.isRegionCompleted(81));
  assert(!restored.isRegionCompleted(80));
  assert(!restored.isRegionCompleted(82));

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
  assert(progress.openGateCount == 1);
  assert(progress.completedTraversalCount == 5);
  return 0;
}
