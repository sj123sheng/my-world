#pragma once

#include "native/engine/math/vec2.h"
#include "native/gameplay/player/exploration_motion.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// 单区垂直切片的自然探索内容模型。定义由 verticalSlice() 集中提供，运行时
// 只维护状态，不解析 JSON；世界布局 JSON 由构建期代码生成器负责校验和生成。
struct PointOfInterest {
  int32_t id = 0;
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
  std::string districtId;
  bool mainRoute = false;
};

struct NaturalNode {
  int32_t id = 0;
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
  MotionState requiredMotion = MotionState::Grounded;
  int32_t rewardId = -1;
};

struct RegionTrigger {
  int32_t id = 0;
  float x = 0.0f;
  float y = 0.0f;
  float radius = 0.0f;
  std::string label;
  int32_t prerequisiteNodeId = -1;
};

struct ExplorationReward {
  int32_t id = 0;
  std::string label;
  int32_t sourceTraces = 0;
  int32_t gold = 0;
  int32_t fate = 0;
  int32_t itemId = 0;
  int32_t itemCount = 0;
};

// 玩家验收口径中的五种移动能力，与 MotionState 的瞬时状态分离。
enum class TraversalAbility : uint8_t {
  Jump = 0,
  Sprint = 1,
  Glide = 2,
  Climb = 3,
  Swim = 4,
};

enum class ExplorationTargetKind : uint8_t {
  None = 0,
  PointOfInterest = 1,
  NaturalNode = 2,
  RegionTrigger = 3,
  Reward = 4,
};

struct ExplorationTarget {
  int32_t id = -1;
  ExplorationTargetKind kind = ExplorationTargetKind::None;
  float distance = 0.0f;
  std::string label;
};

struct ExplorationProgress {
  int32_t discoveredPoiCount = 0;
  // 字段名为快照/Bridge 兼容保留，语义是已激活自然节点数。
  int32_t activatedPuzzleCount = 0;
  int32_t claimedRewardCount = 0;
  // 字段名为快照/Bridge 兼容保留，语义是已完成区域数。
  int32_t openGateCount = 0;
  int32_t completedTraversalCount = 0;
};

class ExplorationContent {
 public:
  static ExplorationContent verticalSlice();

  ExplorationTarget nearestTarget(Vec2 position, float radius) const;
  bool discoverPoint(int32_t poiId);
  bool activateNaturalNode(int32_t id, MotionState currentMotion);
  bool enterRegion(int32_t id, Vec2 playerPosition);
  bool claimReward(int32_t rewardId);
  void recordTraversal(TraversalAbility ability);

  bool isPointDiscovered(int32_t poiId) const;
  bool isNaturalNodeActivated(int32_t id) const;
  bool isRegionCompleted(int32_t id) const;
  const RegionTrigger* regionById(int32_t id) const;
  const NaturalNode* naturalNodeById(int32_t id) const;
  bool isRewardClaimed(int32_t rewardId) const;
  bool traversalUsed(TraversalAbility ability) const;
  ExplorationProgress progress() const;

  const std::vector<PointOfInterest>& pointsOfInterest() const { return pois_; }
  const std::vector<NaturalNode>& naturalNodes() const { return naturalNodes_; }
  const std::vector<RegionTrigger>& regionTriggers() const {
    return regionTriggers_;
  }
  const std::vector<ExplorationReward>& rewards() const { return rewards_; }

  // V9 磁盘顺序保持 POI、自然节点、奖励、完成区域；旧 gate API 名只为
  // 存档/Bridge 兼容，第四个掩码及 openGateCount 的语义均为 completed regions。
  int32_t discoveredPoiMask() const;
  int32_t activatedPuzzleMask() const;
  int32_t claimedRewardMask() const;
  int32_t openGateMask() const;
  void restoreMasks(int32_t poiMask, int32_t puzzleMask, int32_t rewardMask,
                    int32_t gateMask, uint8_t traversalMask);
  uint8_t traversalMask() const;

 private:
  ExplorationContent(std::vector<PointOfInterest> pois,
                     std::vector<NaturalNode> naturalNodes,
                     std::vector<RegionTrigger> regionTriggers,
                     std::vector<ExplorationReward> rewards);

  static bool motionMatches(MotionState required, MotionState current);
  static bool bitSet(int32_t mask, size_t index);
  static int32_t maskFrom(const std::vector<bool>& values);

  std::vector<PointOfInterest> pois_;
  std::vector<NaturalNode> naturalNodes_;
  std::vector<RegionTrigger> regionTriggers_;
  std::vector<ExplorationReward> rewards_;
  std::vector<bool> discoveredPois_;
  std::vector<bool> activatedNaturalNodes_;
  std::vector<bool> claimedRewards_;
  std::vector<bool> completedRegions_;
  uint8_t traversalMask_ = 0;
};
