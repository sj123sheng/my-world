#include "native/engine/world/world_grid.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

bool Contains(const std::vector<ChunkCoord>& chunks, ChunkCoord expected) {
  return std::find(chunks.begin(), chunks.end(), expected) != chunks.end();
}

long double ChebyshevDistance(ChunkCoord lhs, ChunkCoord rhs) {
  const long double dx =
      static_cast<long double>(lhs.x) - static_cast<long double>(rhs.x);
  const long double dy =
      static_cast<long double>(lhs.y) - static_cast<long double>(rhs.y);
  return std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
}

void TestQualityPresetsKeepRequiredActiveAreas() {
  assert(WorldGrid::ActiveRadiusForQuality(0) == 4);
  assert(WorldGrid::ActiveRadiusForQuality(1) == 3);
  assert(WorldGrid::ActiveRadiusForQuality(2) == 2);
  assert(WorldGrid::ActiveRadiusForQuality(-1) == 4);
  assert(WorldGrid::ActiveRadiusForQuality(3) == 2);

  WorldGrid medium({WorldGrid::ActiveRadiusForQuality(1), 2});
  assert(medium.updateStreaming({0, 0}, {}, {}));
  assert(medium.activeChunks().size() == 49);
  assert(medium.cachedChunks().size() == 121);

  WorldGrid low({WorldGrid::ActiveRadiusForQuality(2), 2});
  assert(low.updateStreaming({0, 0}, {}, {}));
  assert(low.activeChunks().size() == 25);
  assert(low.cachedChunks().size() == 81);
}

void TestInitialUpdateBuildsActiveAndCachedSquares() {
  WorldGrid grid({4, 2});
  assert(grid.activeChunks().empty());
  assert(grid.cachedChunks().empty());

  assert(grid.updateStreaming({0, 0}, {1.0f, 0.0f}, {1.0f, 0.0f}));
  assert(grid.activeChunks().size() == 81);
  assert(grid.cachedChunks().size() == 169);
  assert(grid.pendingLoads().size() == 81);
  assert(grid.pendingUnloads().empty());
  assert(grid.pendingLoads().front() == (ChunkCoord{0, 0}));
  assert(grid.pendingLoads()[1] == (ChunkCoord{1, 0}));

  for (const ChunkCoord coord : grid.activeChunks()) {
    assert(ChebyshevDistance(coord, {0, 0}) <= 4);
  }
  for (const ChunkCoord coord : grid.cachedChunks()) {
    assert(ChebyshevDistance(coord, {0, 0}) <= 6);
  }
}

void TestLoadOrderUsesRingThenCombinedForwardThenCoordinates() {
  WorldGrid diagonal({2, 2});
  assert(diagonal.updateStreaming({100, -200}, {0.0f, 1.0f},
                                  {1.0f, 0.0f}));
  const std::vector<ChunkCoord> expectedPrefix{
      {100, -200}, {101, -199}, {101, -200}, {100, -199}};
  assert(diagonal.pendingLoads().size() == 25);
  assert(std::equal(expectedPrefix.begin(), expectedPrefix.end(),
                    diagonal.pendingLoads().begin()));
  for (size_t i = 1; i < 9; ++i) {
    assert(ChebyshevDistance(diagonal.pendingLoads()[i], {100, -200}) ==
           1);
  }
  assert(ChebyshevDistance(diagonal.pendingLoads()[9], {100, -200}) == 2);

  WorldGrid degenerate({1, 2});
  assert(degenerate.updateStreaming({0, 0}, {1.0f, 0.0f},
                                    {-1.0f, 0.0f}));
  const std::vector<ChunkCoord> expectedCoordinateOrder{
      {0, 0}, {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
      {1, 0}, {-1, 1},  {0, 1},  {1, 1}};
  assert(degenerate.pendingLoads() == expectedCoordinateOrder);
}

void TestLoadOrderDoesNotDependOnPriorCandidateHistory() {
  WorldGrid fromWest({2, 2});
  WorldGrid fromSouth({2, 2});
  assert(fromWest.updateStreaming({-1000, 0}, {1.0f, 0.0f}, {}));
  assert(fromSouth.updateStreaming({0, -1000}, {1.0f, 0.0f}, {}));

  const ChunkCoord destination{5000, -5000};
  assert(fromWest.updateStreaming(destination, {0.0f, 1.0f}, {}));
  assert(fromSouth.updateStreaming(destination, {0.0f, 1.0f}, {}));
  assert(fromWest.pendingLoads() == fromSouth.pendingLoads());
  assert(fromWest.activeChunks() == fromSouth.activeChunks());
  assert(fromWest.cachedChunks() == fromSouth.cachedChunks());
}

void TestRepeatedAndAdjacentUpdatesExposeOnlyDeltas() {
  WorldGrid grid({4, 2});
  assert(grid.updateStreaming({0, 0}, {1.0f, 0.0f}, {}));

  assert(!grid.updateStreaming({0, 0}, {-1.0f, 0.0f}, {1.0f, 0.0f}));
  assert(grid.pendingLoads().empty());
  assert(grid.pendingUnloads().empty());

  assert(grid.updateStreaming({1, 0}, {1.0f, 0.0f}, {}));
  assert(grid.activeChunks().size() == 81);
  assert(grid.cachedChunks().size() == 169);
  assert(grid.pendingLoads().size() == 9);
  assert(grid.pendingUnloads().size() == 13);
  assert(!Contains(grid.pendingUnloads(), {-4, 0}));
  for (const ChunkCoord coord : grid.pendingLoads()) {
    assert(ChebyshevDistance(coord, {1, 0}) <= 4);
  }
  for (const ChunkCoord coord : grid.pendingUnloads()) {
    assert(ChebyshevDistance(coord, {1, 0}) > 6);
  }
}

void TestNegativeAndFarCoordinatesRemainDeterministic() {
  WorldGrid grid({4, 2});
  assert(grid.updateStreaming({50, -50}, {-1.0f, 0.0f},
                              {-1.0f, 0.0f}));
  assert(grid.pendingLoads()[1] == (ChunkCoord{49, -50}));

  assert(grid.updateStreaming({-4000000000000LL, 7000000000000LL},
                              {0.0f, -1.0f}, {}));
  assert(grid.activeChunks().size() == 81);
  assert(grid.cachedChunks().size() == 169);
  for (const ChunkCoord coord : grid.pendingUnloads()) {
    assert(ChebyshevDistance(
               coord, {-4000000000000LL, 7000000000000LL}) > 6);
  }

  WorldGrid nearMaximum({4, 2});
  const ChunkCoord maximumSafe{
      std::numeric_limits<int64_t>::max() - 6,
      std::numeric_limits<int64_t>::min() + 6};
  assert(nearMaximum.updateStreaming(maximumSafe, {1.0f, 0.0f}, {}));
  assert(nearMaximum.activeChunks().size() == 81);
  assert(nearMaximum.cachedChunks().size() == 169);
  assert(nearMaximum.pendingLoads().front() == maximumSafe);

  WorldGrid atMaximum({4, 2});
  const ChunkCoord maximum{std::numeric_limits<int64_t>::max(),
                           std::numeric_limits<int64_t>::max()};
  assert(atMaximum.updateStreaming(maximum, {1.0f, 0.0f}, {}));
  assert(atMaximum.activeChunks().size() == 25);
  assert(atMaximum.cachedChunks().size() == 49);
  assert(atMaximum.pendingLoads().front() == maximum);
  assert(!atMaximum.updateStreaming(maximum, {1.0f, 0.0f}, {}));
}

}  // namespace

int main() {
  TestQualityPresetsKeepRequiredActiveAreas();
  TestInitialUpdateBuildsActiveAndCachedSquares();
  TestLoadOrderUsesRingThenCombinedForwardThenCoordinates();
  TestLoadOrderDoesNotDependOnPriorCandidateHistory();
  TestRepeatedAndAdjacentUpdatesExposeOnlyDeltas();
  TestNegativeAndFarCoordinatesRemainDeterministic();
  return 0;
}
