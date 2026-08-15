#include "native/gameplay/world/exploration_content.h"

#include "native/generated/world_layout.gen.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

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

  std::vector<NaturalNode> naturalNodes;
  naturalNodes.reserve(WorldLayout::kNaturalNodeCount);
  for (const WorldLayout::WorldNaturalNodeDef& node :
       WorldLayout::kNaturalNodes) {
    naturalNodes.push_back({node.id, node.x, node.y, std::string(node.label),
                            toMotionState(node.requiredMotion), node.rewardId});
  }

  std::vector<RegionTrigger> regionTriggers;
  regionTriggers.reserve(WorldLayout::kRegionTriggerCount);
  for (const WorldLayout::WorldRegionTriggerDef& region :
       WorldLayout::kRegionTriggers) {
    regionTriggers.push_back({region.id, region.x, region.y, region.radius,
                              std::string(region.label),
                              region.prerequisiteNodeId});
  }

  std::vector<ExplorationReward> rewards;
  rewards.reserve(WorldLayout::kExplorationRewardCount);
  for (const WorldLayout::WorldExplorationRewardDef& reward :
       WorldLayout::kExplorationRewards) {
    rewards.push_back({reward.id, std::string(reward.label), reward.sourceTraces,
                       reward.gold, reward.fate, reward.itemId,
                       reward.itemCount});
  }
  return ExplorationContent(std::move(pois), std::move(naturalNodes),
                            std::move(regionTriggers), std::move(rewards));
}

ExplorationContent::ExplorationContent(
    std::vector<PointOfInterest> pois, std::vector<NaturalNode> naturalNodes,
    std::vector<RegionTrigger> regionTriggers,
    std::vector<ExplorationReward> rewards)
    : pois_(std::move(pois)),
      naturalNodes_(std::move(naturalNodes)),
      regionTriggers_(std::move(regionTriggers)),
      rewards_(std::move(rewards)),
      discoveredPois_(pois_.size(), false),
      activatedNaturalNodes_(naturalNodes_.size(), false),
      claimedRewards_(rewards_.size(), false),
      completedRegions_(regionTriggers_.size(), false) {}

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
  for (size_t i = 0; i < naturalNodes_.size(); ++i) {
    if (!activatedNaturalNodes_[i]) {
      consider(naturalNodes_[i].id, ExplorationTargetKind::NaturalNode,
               naturalNodes_[i].x, naturalNodes_[i].y, naturalNodes_[i].label,
               bestDistance);
    }
  }
  for (size_t i = 0; i < regionTriggers_.size(); ++i) {
    if (!completedRegions_[i] &&
        isNaturalNodeActivated(regionTriggers_[i].prerequisiteNodeId)) {
      consider(regionTriggers_[i].id, ExplorationTargetKind::RegionTrigger,
               regionTriggers_[i].x, regionTriggers_[i].y,
               regionTriggers_[i].label, bestDistance);
    }
  }
  for (size_t i = 0; i < rewards_.size(); ++i) {
    if (claimedRewards_[i]) continue;
    for (const NaturalNode& node : naturalNodes_) {
      if (node.rewardId == rewards_[i].id &&
          isNaturalNodeActivated(node.id)) {
        consider(rewards_[i].id, ExplorationTargetKind::Reward, node.x, node.y,
                 rewards_[i].label, bestDistance);
        break;
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

bool ExplorationContent::activateNaturalNode(int32_t id,
                                             MotionState currentMotion) {
  for (size_t i = 0; i < naturalNodes_.size(); ++i) {
    const NaturalNode& node = naturalNodes_[i];
    if (node.id == id && !activatedNaturalNodes_[i] &&
        motionMatches(node.requiredMotion, currentMotion)) {
      activatedNaturalNodes_[i] = true;
      return true;
    }
  }
  return false;
}

bool ExplorationContent::enterRegion(int32_t id, Vec2 playerPosition) {
  if (!playerPosition.finite()) return false;
  for (size_t i = 0; i < regionTriggers_.size(); ++i) {
    const RegionTrigger& region = regionTriggers_[i];
    if (region.id != id || completedRegions_[i] ||
        !isNaturalNodeActivated(region.prerequisiteNodeId)) {
      continue;
    }
    if (distanceBetween(playerPosition, region.x, region.y) > region.radius) {
      return false;
    }
    completedRegions_[i] = true;
    return true;
  }
  return false;
}

bool ExplorationContent::claimReward(int32_t rewardId) {
  for (size_t i = 0; i < rewards_.size(); ++i) {
    if (rewards_[i].id != rewardId || claimedRewards_[i]) continue;
    for (const NaturalNode& node : naturalNodes_) {
      if (node.rewardId == rewardId && isNaturalNodeActivated(node.id)) {
        claimedRewards_[i] = true;
        return true;
      }
    }
    return false;
  }
  return false;
}

void ExplorationContent::recordTraversal(TraversalAbility ability) {
  const uint8_t value = static_cast<uint8_t>(ability);
  if (value <= static_cast<uint8_t>(TraversalAbility::Swim)) {
    traversalMask_ = static_cast<uint8_t>(traversalMask_ | (1u << value));
  }
}

bool ExplorationContent::isPointDiscovered(int32_t id) const {
  for (size_t i = 0; i < pois_.size(); ++i) {
    if (pois_[i].id == id) return discoveredPois_[i];
  }
  return false;
}

bool ExplorationContent::isNaturalNodeActivated(int32_t id) const {
  for (size_t i = 0; i < naturalNodes_.size(); ++i) {
    if (naturalNodes_[i].id == id) return activatedNaturalNodes_[i];
  }
  return false;
}

bool ExplorationContent::isRegionCompleted(int32_t id) const {
  for (size_t i = 0; i < regionTriggers_.size(); ++i) {
    if (regionTriggers_[i].id == id) return completedRegions_[i];
  }
  return false;
}

const RegionTrigger* ExplorationContent::regionById(int32_t id) const {
  for (const RegionTrigger& region : regionTriggers_) {
    if (region.id == id) return &region;
  }
  return nullptr;
}

const NaturalNode* ExplorationContent::naturalNodeById(int32_t id) const {
  for (const NaturalNode& node : naturalNodes_) {
    if (node.id == id) return &node;
  }
  return nullptr;
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
      activatedNaturalNodes_.begin(), activatedNaturalNodes_.end(), true));
  result.claimedRewardCount = static_cast<int32_t>(std::count(
      claimedRewards_.begin(), claimedRewards_.end(), true));
  result.openGateCount = static_cast<int32_t>(std::count(
      completedRegions_.begin(), completedRegions_.end(), true));
  for (uint8_t state = 0; state <= static_cast<uint8_t>(TraversalAbility::Swim);
       ++state) {
    if ((traversalMask_ & (1u << state)) != 0) {
      result.completedTraversalCount += 1;
    }
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
  return maskFrom(activatedNaturalNodes_);
}

int32_t ExplorationContent::claimedRewardMask() const {
  return maskFrom(claimedRewards_);
}

int32_t ExplorationContent::openGateMask() const {
  return maskFrom(completedRegions_);
}

void ExplorationContent::restoreMasks(int32_t poiMask, int32_t puzzleMask,
                                      int32_t rewardMask, int32_t gateMask,
                                      uint8_t traversalMask) {
  for (size_t i = 0; i < discoveredPois_.size(); ++i) {
    discoveredPois_[i] = bitSet(poiMask, i);
  }
  for (size_t i = 0; i < activatedNaturalNodes_.size(); ++i) {
    activatedNaturalNodes_[i] = bitSet(puzzleMask, i);
  }
  for (size_t i = 0; i < claimedRewards_.size(); ++i) {
    claimedRewards_[i] = bitSet(rewardMask, i);
  }
  for (size_t i = 0; i < completedRegions_.size(); ++i) {
    completedRegions_[i] = bitSet(gateMask, i);
  }
  traversalMask_ = traversalMask & 0x1Fu;
}

uint8_t ExplorationContent::traversalMask() const { return traversalMask_; }

bool ExplorationContent::motionMatches(MotionState required,
                                       MotionState current) {
  return required == MotionState::Grounded ? current == MotionState::Grounded
                                           : required == current;
}
