#include "native/gameplay/world/exploration_content.h"

#include "native/generated/world_layout.gen.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

float distanceBetween(Vec2 a, float x, float y) {
  return Vec2{x - a.x, y - a.y}.length();
}

MotionState toMotionState(WorldLayout::TraversalMotion motion) {
  return static_cast<MotionState>(static_cast<uint8_t>(motion));
}

}  // namespace

ExplorationContent ExplorationContent::verticalSlice() {
  std::vector<PointOfInterest> pois;
  pois.reserve(WorldLayout::kPointOfInterestCount);
  for (const WorldLayout::WorldPointOfInterestDef& poi :
       WorldLayout::kPointsOfInterest) {
    pois.push_back({poi.id, poi.x, poi.y, std::string(poi.label),
                    std::string(poi.districtId), poi.mainRoute});
  }
  std::vector<PuzzleNode> puzzles;
  puzzles.reserve(WorldLayout::kPuzzleNodeCount);
  for (const WorldLayout::WorldPuzzleNodeDef& puzzle :
       WorldLayout::kPuzzleNodes) {
    puzzles.push_back({puzzle.id, puzzle.x, puzzle.y, std::string(puzzle.label),
                       toMotionState(puzzle.requiredMotion),
                       puzzle.opensGateId, puzzle.rewardId});
  }
  std::vector<TraversalGate> gates;
  gates.reserve(WorldLayout::kTraversalGateCount);
  for (const WorldLayout::WorldTraversalGateDef& gate :
       WorldLayout::kTraversalGates) {
    gates.push_back({gate.id, gate.x, gate.y, std::string(gate.label),
                     toMotionState(gate.requiredMotion)});
  }
  std::vector<ExplorationReward> rewards;
  rewards.reserve(WorldLayout::kExplorationRewardCount);
  for (const WorldLayout::WorldExplorationRewardDef& reward :
       WorldLayout::kExplorationRewards) {
    rewards.push_back({reward.id, std::string(reward.label), reward.sourceTraces,
                       reward.gold, reward.fate, reward.itemId,
                       reward.itemCount});
  }
  return ExplorationContent(std::move(pois), std::move(puzzles),
                            std::move(gates), std::move(rewards));
}

ExplorationContent::ExplorationContent(
    std::vector<PointOfInterest> pois, std::vector<PuzzleNode> puzzles,
    std::vector<TraversalGate> gates, std::vector<ExplorationReward> rewards)
    : pois_(std::move(pois)),
      puzzles_(std::move(puzzles)),
      gates_(std::move(gates)),
      rewards_(std::move(rewards)),
      discoveredPois_(pois_.size(), false),
      activatedPuzzles_(puzzles_.size(), false),
      claimedRewards_(rewards_.size(), false),
      openGates_(gates_.size(), false) {}

ExplorationTarget ExplorationContent::nearestTarget(Vec2 position,
                                                    float radius) const {
  ExplorationTarget best;
  if (!position.finite() || !std::isfinite(radius) || radius <= 0.0f) {
    return best;
  }
  const auto consider = [&](int32_t id, ExplorationTargetKind kind,
                            float x, float y, const std::string& label,
                            float& bestDistance) {
    const float distance = distanceBetween(position, x, y);
    if (distance <= radius && distance < bestDistance) {
      best = {id, kind, distance, label};
      bestDistance = distance;
    }
  };
  float bestDistance = std::numeric_limits<float>::max();
  for (size_t i = 0; i < pois_.size(); ++i) {
    if (!discoveredPois_[i]) {
      consider(pois_[i].id, ExplorationTargetKind::PointOfInterest, pois_[i].x,
               pois_[i].y, pois_[i].label, bestDistance);
    }
  }
  for (size_t i = 0; i < puzzles_.size(); ++i) {
    if (!activatedPuzzles_[i]) {
      consider(puzzles_[i].id, ExplorationTargetKind::Puzzle, puzzles_[i].x,
               puzzles_[i].y, puzzles_[i].label, bestDistance);
    }
  }
  for (size_t i = 0; i < gates_.size(); ++i) {
    if (!openGates_[i]) {
      consider(gates_[i].id, ExplorationTargetKind::TraversalGate, gates_[i].x,
               gates_[i].y, gates_[i].label, bestDistance);
    }
  }
  for (size_t i = 0; i < rewards_.size(); ++i) {
    if (!claimedRewards_[i]) {
      // Rewards use the matching puzzle location and are surfaced only after
      // their puzzle is active, so the HUD does not reveal every hidden reward.
      const PuzzleNode* source = nullptr;
      for (const PuzzleNode& puzzle : puzzles_) {
        if (puzzle.rewardId == rewards_[i].id) {
          source = &puzzle;
          break;
        }
      }
      if (source != nullptr && isPuzzleActivated(source->id)) {
        consider(rewards_[i].id, ExplorationTargetKind::Reward, source->x,
                 source->y, rewards_[i].label, bestDistance);
      }
    }
  }
  return best;
}

bool ExplorationContent::discoverPoint(int32_t poiId) {
  for (size_t i = 0; i < pois_.size(); ++i) {
    if (pois_[i].id == poiId && !discoveredPois_[i]) {
      discoveredPois_[i] = true;
      return true;
    }
  }
  return false;
}

bool ExplorationContent::activatePuzzle(int32_t puzzleId,
                                         MotionState currentMotion) {
  for (size_t i = 0; i < puzzles_.size(); ++i) {
    const PuzzleNode& puzzle = puzzles_[i];
    if (puzzle.id != puzzleId || activatedPuzzles_[i] ||
        !motionMatches(puzzle.requiredMotion, currentMotion)) {
      continue;
    }
    activatedPuzzles_[i] = true;
    for (size_t gateIndex = 0; gateIndex < gates_.size(); ++gateIndex) {
      if (gates_[gateIndex].id == puzzle.opensGateId) {
        openGates_[gateIndex] = true;
        break;
      }
    }
    return true;
  }
  return false;
}

bool ExplorationContent::claimReward(int32_t rewardId) {
  for (size_t i = 0; i < rewards_.size(); ++i) {
    if (rewards_[i].id != rewardId || claimedRewards_[i]) continue;
    bool available = false;
    for (const PuzzleNode& puzzle : puzzles_) {
      if (puzzle.rewardId == rewardId && isPuzzleActivated(puzzle.id)) {
        available = true;
        break;
      }
    }
    if (!available) return false;
    claimedRewards_[i] = true;
    return true;
  }
  return false;
}

void ExplorationContent::recordTraversal(TraversalAbility ability) {
  const uint8_t value = static_cast<uint8_t>(ability);
  if (value > static_cast<uint8_t>(TraversalAbility::Swim)) return;
  traversalMask_ = static_cast<uint8_t>(traversalMask_ | (1u << value));
}

bool ExplorationContent::isPointDiscovered(int32_t id) const {
  for (size_t i = 0; i < pois_.size(); ++i) {
    if (pois_[i].id == id) return discoveredPois_[i];
  }
  return false;
}

bool ExplorationContent::isPuzzleActivated(int32_t id) const {
  for (size_t i = 0; i < puzzles_.size(); ++i) {
    if (puzzles_[i].id == id) return activatedPuzzles_[i];
  }
  return false;
}

bool ExplorationContent::isGateOpen(int32_t id) const {
  for (size_t i = 0; i < gates_.size(); ++i) {
    if (gates_[i].id == id) return openGates_[i];
  }
  return false;
}

bool ExplorationContent::isRewardClaimed(int32_t id) const {
  for (size_t i = 0; i < rewards_.size(); ++i) {
    if (rewards_[i].id == id) return claimedRewards_[i];
  }
  return false;
}

bool ExplorationContent::traversalUsed(TraversalAbility ability) const {
  if (static_cast<uint8_t>(ability) >
      static_cast<uint8_t>(TraversalAbility::Swim)) {
    return false;
  }
  return (traversalMask_ & (1u << static_cast<uint8_t>(ability))) != 0;
}

ExplorationProgress ExplorationContent::progress() const {
  ExplorationProgress result;
  result.discoveredPoiCount = static_cast<int32_t>(std::count(
      discoveredPois_.begin(), discoveredPois_.end(), true));
  result.activatedPuzzleCount = static_cast<int32_t>(std::count(
      activatedPuzzles_.begin(), activatedPuzzles_.end(), true));
  result.claimedRewardCount = static_cast<int32_t>(std::count(
      claimedRewards_.begin(), claimedRewards_.end(), true));
  result.openGateCount = static_cast<int32_t>(
      std::count(openGates_.begin(), openGates_.end(), true));
  for (uint8_t state = 0; state <= static_cast<uint8_t>(TraversalAbility::Swim);
       ++state) {
    if ((traversalMask_ & (1u << state)) != 0) result.completedTraversalCount += 1;
  }
  return result;
}

bool ExplorationContent::bitSet(int32_t mask, size_t index) {
  return index < 31 && (mask & (1 << static_cast<int32_t>(index))) != 0;
}

int32_t ExplorationContent::maskFrom(const std::vector<bool>& values) {
  int32_t mask = 0;
  for (size_t i = 0; i < values.size() && i < 31; ++i) {
    if (values[i]) mask |= 1 << static_cast<int32_t>(i);
  }
  return mask;
}

int32_t ExplorationContent::discoveredPoiMask() const {
  return maskFrom(discoveredPois_);
}

int32_t ExplorationContent::activatedPuzzleMask() const {
  return maskFrom(activatedPuzzles_);
}

int32_t ExplorationContent::claimedRewardMask() const {
  return maskFrom(claimedRewards_);
}

int32_t ExplorationContent::openGateMask() const {
  return maskFrom(openGates_);
}

void ExplorationContent::restoreMasks(int32_t poiMask, int32_t puzzleMask,
                                       int32_t rewardMask, int32_t gateMask,
                                       uint8_t traversalMask) {
  for (size_t i = 0; i < discoveredPois_.size(); ++i) {
    discoveredPois_[i] = bitSet(poiMask, i);
  }
  for (size_t i = 0; i < activatedPuzzles_.size(); ++i) {
    activatedPuzzles_[i] = bitSet(puzzleMask, i);
  }
  for (size_t i = 0; i < claimedRewards_.size(); ++i) {
    claimedRewards_[i] = bitSet(rewardMask, i);
  }
  for (size_t i = 0; i < openGates_.size(); ++i) {
    openGates_[i] = bitSet(gateMask, i);
  }
  traversalMask_ = traversalMask & 0x1Fu;
}

uint8_t ExplorationContent::traversalMask() const { return traversalMask_; }

bool ExplorationContent::motionMatches(MotionState required,
                                        MotionState current) {
  return required == MotionState::Grounded ? current == MotionState::Grounded
                                           : required == current;
}
