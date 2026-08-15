#include "native/engine/core/game_snapshot.h"
#include "native/gameplay/world/exploration_content.h"
#include "native/gameplay/world/exploration_feedback.h"

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
  content.activateNaturalNode(71, MotionState::Grounded);
  content.enterRegion(81, {0.70f, 0.20f});
  content.claimReward(91);
  content.recordTraversal(TraversalAbility::Swim);

  GameSnapshot snapshot;
  snapshot.explorationPoiCount = content.progress().discoveredPoiCount;
  snapshot.explorationPuzzleCount = content.progress().activatedPuzzleCount;
  snapshot.explorationRewardCount = content.progress().claimedRewardCount;
  snapshot.explorationGateCount = content.progress().openGateCount;
  snapshot.explorationTraversalMask = content.traversalMask();
  snapshot.explorationCurrentPoiId = 62;
  snapshot.explorationCurrentTargetLabel = "辉光湖湾";

  assert(snapshot.explorationPoiCount == 1);
  assert(snapshot.explorationPuzzleCount == 1);
  assert(snapshot.explorationRewardCount == 1);
  assert(snapshot.explorationGateCount == 1);
  assert(snapshot.explorationTraversalMask == (1 << 4));
  assert(snapshot.explorationCurrentPoiId == 62);
  assert(snapshot.explorationCurrentTargetLabel == "辉光湖湾");

  // 自然区域只记录完成，不再生产任何动态阻挡物。
  ExplorationContent regionContent = ExplorationContent::verticalSlice();
  assert(!regionContent.enterRegion(81, {0.70f, 0.20f}));
  assert(regionContent.activateNaturalNode(71, MotionState::Grounded));
  assert(regionContent.enterRegion(81, {0.70f, 0.20f}));
  assert(regionContent.progress().openGateCount == 1);

  ExplorationFeedbackState feedback;
  feedback.publish(ExplorationFeedbackType::PoiDiscovered, 60, "辉光湖畔",
                   "发现新地标", 1200);
  assert(feedback.snapshot().type == ExplorationFeedbackType::PoiDiscovered);
  assert(feedback.snapshot().id == 60);
  feedback.update(1200);
  assert(feedback.snapshot().type == ExplorationFeedbackType::None);
  snapshot.explorationFeedbackType =
      static_cast<int32_t>(ExplorationFeedbackType::PoiDiscovered);
  snapshot.explorationFeedbackId = 60;
  snapshot.explorationFeedbackTitle = "辉光湖畔";
  snapshot.explorationFeedbackSubtitle = "发现新地标";
  snapshot.explorationFeedbackRemainingMs = 1200;
  assert(snapshot.explorationFeedbackId == 60);
  return 0;
}
