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

  // 同一自然目标簇按 POI→自然节点→前置区域→奖励→完成的类别推进。
  ExplorationContent targetSequence = ExplorationContent::verticalSlice();
  ExplorationTarget target = targetSequence.nearestTarget({0.70f, 0.20f}, 0.01f);
  assert(target.kind == ExplorationTargetKind::PointOfInterest);
  assert(target.id == 62);
  assert(targetSequence.discoverPoint(62));
  target = targetSequence.nearestTarget({0.70f, 0.20f}, 0.01f);
  assert(target.kind == ExplorationTargetKind::NaturalNode);
  assert(target.id == 71);
  assert(targetSequence.activateNaturalNode(71, MotionState::Grounded));
  target = targetSequence.nearestTarget({0.70f, 0.20f}, 0.01f);
  assert(target.kind == ExplorationTargetKind::RegionTrigger);
  assert(target.id == 81);
  assert(targetSequence.enterRegion(81, {0.70f, 0.20f}));
  target = targetSequence.nearestTarget({0.70f, 0.20f}, 0.01f);
  assert(target.kind == ExplorationTargetKind::Reward);
  assert(target.id == 91);
  assert(targetSequence.claimReward(91));
  target = targetSequence.nearestTarget({0.70f, 0.20f}, 0.01f);
  assert(target.kind == ExplorationTargetKind::None);

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
  for (int32_t bit = 0; bit < 4; ++bit) {
    ExplorationContent restored = ExplorationContent::verticalSlice();
    restored.restoreMasks(0, 0, 0, 1 << bit, 0);
    for (int32_t regionOffset = 0; regionOffset < 4; ++regionOffset) {
      assert(restored.isRegionCompleted(80 + regionOffset) ==
             (regionOffset == bit));
    }
  }

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
