#include "engine/world/world_position.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr long double kMaxRelativeChunkDelta = 16777216.0L;

uint64_t SplitMix64(uint64_t value) noexcept {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

uint64_t UnsignedBits(int64_t value) noexcept {
  uint64_t result = 0;
  static_assert(sizeof(result) == sizeof(value));
  std::memcpy(&result, &value, sizeof(result));
  return result;
}

bool NormalizeAxis(int64_t chunk, double local, int64_t* normalizedChunk,
                   float* normalizedLocal) noexcept {
  const double offset = std::floor(local);
  const double minInt64 =
      static_cast<double>(std::numeric_limits<int64_t>::min());
  const double pastMaxInt64 = -minInt64;
  if (!std::isfinite(local) || offset < minInt64 ||
      offset >= pastMaxInt64) {
    return false;
  }

  const int64_t integerOffset = static_cast<int64_t>(offset);
  if ((integerOffset > 0 &&
       chunk > std::numeric_limits<int64_t>::max() - integerOffset) ||
      (integerOffset < 0 &&
       chunk < std::numeric_limits<int64_t>::min() - integerOffset)) {
    return false;
  }

  *normalizedChunk = chunk + integerOffset;
  *normalizedLocal = static_cast<float>(local - offset);
  return true;
}

float ClampedChunkDelta(int64_t target, int64_t origin) noexcept {
  const long double delta = static_cast<long double>(target) -
                            static_cast<long double>(origin);
  if (delta > kMaxRelativeChunkDelta) {
    return static_cast<float>(kMaxRelativeChunkDelta);
  }
  if (delta < -kMaxRelativeChunkDelta) {
    return static_cast<float>(-kMaxRelativeChunkDelta);
  }
  return static_cast<float>(delta);
}

}  // namespace

WorldPosition NormalizeWorldPosition(ChunkCoord chunk, double localX,
                                     double localY) noexcept {
  WorldPosition result{};
  if (!NormalizeAxis(chunk.x, localX, &result.chunk.x, &result.local.x) ||
      !NormalizeAxis(chunk.y, localY, &result.chunk.y, &result.local.y)) {
    return {};
  }
  return result;
}

Vec2 RelativeWorldPosition(const WorldPosition& target,
                           const WorldPosition& origin) noexcept {
  return {ClampedChunkDelta(target.chunk.x, origin.chunk.x) +
              (target.local.x - origin.local.x),
          ClampedChunkDelta(target.chunk.y, origin.chunk.y) +
              (target.local.y - origin.local.y)};
}

uint64_t StableChunkHash(uint64_t worldSeed, ChunkCoord chunk,
                         uint64_t salt) noexcept {
  uint64_t value = SplitMix64(worldSeed);
  value = SplitMix64(value ^ UnsignedBits(chunk.x));
  value = SplitMix64(value ^ UnsignedBits(chunk.y));
  return SplitMix64(value ^ salt);
}

size_t ChunkCoordHash::operator()(ChunkCoord coord) const noexcept {
  return static_cast<size_t>(StableChunkHash(0, coord, 0));
}
