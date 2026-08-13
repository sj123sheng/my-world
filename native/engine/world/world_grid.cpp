#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool TryOffset(int64_t value, int64_t offset, int64_t* result) noexcept {
  if ((offset > 0 &&
       value > std::numeric_limits<int64_t>::max() - offset) ||
      (offset < 0 &&
       value < std::numeric_limits<int64_t>::min() - offset)) {
    return false;
  }
  *result = value + offset;
  return true;
}

std::vector<ChunkCoord> BuildSquare(ChunkCoord center, int64_t radius) {
  std::vector<ChunkCoord> chunks;
  const uint64_t side = static_cast<uint64_t>(radius) * 2U + 1U;
  if (side <= std::numeric_limits<size_t>::max() / side) {
    chunks.reserve(static_cast<size_t>(side * side));
  }

  for (int64_t dy = -radius; dy <= radius; ++dy) {
    int64_t y = 0;
    if (!TryOffset(center.y, dy, &y)) continue;
    for (int64_t dx = -radius; dx <= radius; ++dx) {
      int64_t x = 0;
      if (TryOffset(center.x, dx, &x)) chunks.push_back({x, y});
    }
  }
  std::sort(chunks.begin(), chunks.end());
  return chunks;
}

uint64_t UnsignedDistance(int64_t lhs, int64_t rhs) noexcept {
  return lhs >= rhs ? static_cast<uint64_t>(lhs) - static_cast<uint64_t>(rhs)
                    : static_cast<uint64_t>(rhs) - static_cast<uint64_t>(lhs);
}

long double SignedDelta(int64_t value, int64_t origin) noexcept {
  const long double magnitude =
      static_cast<long double>(UnsignedDistance(value, origin));
  return value < origin ? -magnitude : magnitude;
}

struct LoadOrder {
  ChunkCoord center;
  long double forwardX = 0.0L;
  long double forwardY = 0.0L;

  bool operator()(ChunkCoord lhs, ChunkCoord rhs) const noexcept {
    const uint64_t lhsDistance =
        std::max(UnsignedDistance(lhs.x, center.x),
                 UnsignedDistance(lhs.y, center.y));
    const uint64_t rhsDistance =
        std::max(UnsignedDistance(rhs.x, center.x),
                 UnsignedDistance(rhs.y, center.y));
    if (lhsDistance != rhsDistance) return lhsDistance < rhsDistance;

    const long double lhsDx = SignedDelta(lhs.x, center.x);
    const long double lhsDy = SignedDelta(lhs.y, center.y);
    const long double rhsDx = SignedDelta(rhs.x, center.x);
    const long double rhsDy = SignedDelta(rhs.y, center.y);
    const long double lhsLength = std::hypotl(lhsDx, lhsDy);
    const long double rhsLength = std::hypotl(rhsDx, rhsDy);
    const long double lhsForward =
        lhsLength > 0.0L
            ? (lhsDx * forwardX + lhsDy * forwardY) / lhsLength
            : 0.0L;
    const long double rhsForward =
        rhsLength > 0.0L
            ? (rhsDx * forwardX + rhsDy * forwardY) / rhsLength
            : 0.0L;
    if (lhsForward != rhsForward) return lhsForward > rhsForward;
    if (lhs.y != rhs.y) return lhs.y < rhs.y;
    return lhs.x < rhs.x;
  }
};

LoadOrder MakeLoadOrder(ChunkCoord center, Vec2 cameraForward,
                        Vec2 movement) noexcept {
  LoadOrder order;
  order.center = center;
  const long double x = static_cast<long double>(cameraForward.x) +
                        static_cast<long double>(movement.x);
  const long double y = static_cast<long double>(cameraForward.y) +
                        static_cast<long double>(movement.y);
  const long double length = std::hypotl(x, y);
  if (std::isfinite(length) && length > 0.0L) {
    order.forwardX = x / length;
    order.forwardY = y / length;
  }
  return order;
}

}  // namespace

WorldGrid::WorldGrid(WorldGridConfig config) : config_(config) {
  config_.activeRadius = std::max(config_.activeRadius, 0);
  config_.cacheRings = std::max(config_.cacheRings, 0);
}

int32_t WorldGrid::ActiveRadiusForQuality(int32_t qualityPreset) {
  if (qualityPreset <= 0) return 4;
  return qualityPreset == 1 ? 3 : 2;
}

bool WorldGrid::updateStreaming(ChunkCoord playerChunk, Vec2 cameraForward,
                                Vec2 movement) {
  const int64_t activeRadius = config_.activeRadius;
  const int64_t cachedRadius =
      activeRadius + static_cast<int64_t>(config_.cacheRings);
  std::vector<ChunkCoord> desiredActive =
      BuildSquare(playerChunk, activeRadius);
  std::vector<ChunkCoord> desiredCached =
      BuildSquare(playerChunk, cachedRadius);

  if (desiredActive == active_ && desiredCached == cached_) {
    pendingLoads_.clear();
    pendingUnloads_.clear();
    return false;
  }

  pendingLoads_.clear();
  pendingUnloads_.clear();
  std::set_difference(desiredActive.begin(), desiredActive.end(),
                      active_.begin(), active_.end(),
                      std::back_inserter(pendingLoads_));
  std::set_difference(cached_.begin(), cached_.end(), desiredCached.begin(),
                      desiredCached.end(),
                      std::back_inserter(pendingUnloads_));
  std::sort(pendingLoads_.begin(), pendingLoads_.end(),
            MakeLoadOrder(playerChunk, cameraForward, movement));

  active_ = std::move(desiredActive);
  cached_ = std::move(desiredCached);
  return true;
}
