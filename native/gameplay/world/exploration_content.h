#pragma once

#include "native/engine/math/vec2.h"
#include "native/gameplay/player/exploration_motion.h"

#include <cstdint>
#include <string>
#include <vector>

// 单区垂直切片的探索内容模型。定义由 verticalSlice() 集中提供，运行时只维护
// 内容状态，不解析 JSON；世界布局 JSON 仍由构建期代码生成器负责校验和生成。
struct PointOfInterest {
  int32_t id = 0;
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
  std::string districtId;
  bool mainRoute = false;
};

struct PuzzleNode {
  int32_t id = 0;
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
  MotionState requiredMotion = MotionState::Grounded;
  int32_t opensGateId = -1;
  int32_t rewardId = -1;
};

struct TraversalGate {
  int32_t id = 0;
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
  MotionState requiredMotion = MotionState::Grounded;
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
  Puzzle = 2,
  TraversalGate = 3,
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
  int32_t activatedPuzzleCount = 0;
  int32_t claimedRewardCount = 0;
  int32_t openGateCount = 0;
  int32_t completedTraversalCount = 0;
};

class ExplorationContent {
 public:
  static ExplorationContent verticalSlice();

  ExplorationTarget nearestTarget(Vec2 position, float radius) const;
  bool discoverPoint(int32_t poiId);
  bool activatePuzzle(int32_t puzzleId, MotionState currentMotion);
  bool claimReward(int32_t rewardId);
  void recordTraversal(TraversalAbility ability);

  bool isPointDiscovered(int32_t poiId) const;
  bool isPuzzleActivated(int32_t puzzleId) const;
  bool isGateOpen(int32_t gateId) const;
  bool isRewardClaimed(int32_t rewardId) const;
  bool traversalUsed(TraversalAbility ability) const;
  ExplorationProgress progress() const;

  const std::vector<PointOfInterest>& pointsOfInterest() const { return pois_; }
  const std::vector<PuzzleNode>& puzzles() const { return puzzles_; }
  const std::vector<TraversalGate>& gates() const { return gates_; }
  const std::vector<ExplorationReward>& rewards() const { return rewards_; }

  // 存档使用稳定的 bit mask：声明顺序分别对应 POI、机关、奖励和路径门。
  int32_t discoveredPoiMask() const;
  int32_t activatedPuzzleMask() const;
  int32_t claimedRewardMask() const;
  int32_t openGateMask() const;
  void restoreMasks(int32_t poiMask, int32_t puzzleMask, int32_t rewardMask,
                    int32_t gateMask, uint8_t traversalMask);
  uint8_t traversalMask() const;

 private:
  ExplorationContent(std::vector<PointOfInterest> pois,
                     std::vector<PuzzleNode> puzzles,
                     std::vector<TraversalGate> gates,
                     std::vector<ExplorationReward> rewards);

  template <typename T>
  static bool hasId(const std::vector<T>& values, int32_t id) {
    for (const T& value : values) {
      if (value.id == id) return true;
    }
    return false;
  }

  static bool motionMatches(MotionState required, MotionState current);
  static bool bitSet(int32_t mask, size_t index);
  static int32_t maskFrom(const std::vector<bool>& values);

  std::vector<PointOfInterest> pois_;
  std::vector<PuzzleNode> puzzles_;
  std::vector<TraversalGate> gates_;
  std::vector<ExplorationReward> rewards_;
  std::vector<bool> discoveredPois_;
  std::vector<bool> activatedPuzzles_;
  std::vector<bool> claimedRewards_;
  std::vector<bool> openGates_;
  uint8_t traversalMask_ = 0;
};
