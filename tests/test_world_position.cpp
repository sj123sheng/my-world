#include "native/engine/world/world_position.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace {

bool close(float left, float right) {
  return std::abs(left - right) < 0.0001f;
}

void testNormalizationCarriesAcrossPositiveAndNegativeBoundaries() {
  const WorldPosition east = NormalizeWorldPosition({3, -2}, 2.25, -1.5);
  assert(east.chunk == (ChunkCoord{5, -4}));
  assert(close(east.local.x, 0.25f));
  assert(close(east.local.y, 0.5f));

  const WorldPosition exact = NormalizeWorldPosition({0, 0}, -1.0, 1.0);
  assert(exact.chunk == (ChunkCoord{-1, 1}));
  assert(close(exact.local.x, 0.0f));
  assert(close(exact.local.y, 0.0f));
}

void testNormalizationRejectsNonFiniteCoordinates() {
  const double infinity = std::numeric_limits<double>::infinity();
  const WorldPosition invalid = NormalizeWorldPosition({42, -9}, infinity, 0.5);
  assert(invalid.chunk == (ChunkCoord{0, 0}));
  assert(close(invalid.local.x, 0.0f));
  assert(close(invalid.local.y, 0.0f));
}

void testNormalizationRejectsChunkCarryOutsideInt64Range() {
  const WorldPosition invalid = NormalizeWorldPosition(
      {std::numeric_limits<int64_t>::max(), 0}, 1.0, 0.0);
  assert(invalid.chunk == (ChunkCoord{0, 0}));
  assert(close(invalid.local.x, 0.0f));
  assert(close(invalid.local.y, 0.0f));
}

void testRelativePositionPreservesNearbyLargeChunkOffsets() {
  const Vec2 relative = RelativeWorldPosition(
      {{1000000000000LL, -1000000000000LL}, {0.75f, 0.25f}},
      {{999999999999LL, -1000000000001LL}, {0.25f, 0.75f}});
  assert(relative == (Vec2{1.5f, 0.5f}));
}

void testRelativePositionClampsExtremeChunkOffsets() {
  const Vec2 relative = RelativeWorldPosition(
      {{std::numeric_limits<int64_t>::max(), 0}, {0.0f, 0.0f}},
      {{std::numeric_limits<int64_t>::min(), 0}, {0.0f, 0.0f}});
  assert(relative == (Vec2{16777216.0f, 0.0f}));
}

void testStableHashSeparatesSaltAndCoordinates() {
  assert(StableChunkHash(7, {-5, 9}, 11) == StableChunkHash(7, {-5, 9}, 11));
  assert(StableChunkHash(7, {-5, 9}, 11) != StableChunkHash(7, {-5, 9}, 12));
  assert(StableChunkHash(7, {-5, 9}, 11) != StableChunkHash(7, {5, 9}, 11));
  assert(StableChunkHash(7, {-5, 9}, 11) != StableChunkHash(7, {9, -5}, 11));
}

void testChunkCoordinatesSupportOrderedAndHashedContainers() {
  assert((ChunkCoord{-5, 9} < ChunkCoord{-5, 10}));
  assert((ChunkCoord{-5, 9} != ChunkCoord{5, 9}));

  std::unordered_set<ChunkCoord, ChunkCoordHash> chunks;
  chunks.insert({-5, 9});
  chunks.insert({-5, 9});
  chunks.insert({5, 9});
  assert(chunks.size() == 2);
}

}  // namespace

int main() {
  testNormalizationCarriesAcrossPositiveAndNegativeBoundaries();
  testNormalizationRejectsNonFiniteCoordinates();
  testNormalizationRejectsChunkCarryOutsideInt64Range();
  testRelativePositionPreservesNearbyLargeChunkOffsets();
  testRelativePositionClampsExtremeChunkOffsets();
  testStableHashSeparatesSaltAndCoordinates();
  testChunkCoordinatesSupportOrderedAndHashedContainers();
}
