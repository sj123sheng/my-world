#include "gameplay/ai/combat_region.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

void testContainsAndProjection() {
  const CombatRegion region({{0.0f, 0.0f}, 5.0f});

  assert(region.contains({3.0f, 4.0f}));
  assert(!region.contains({3.1f, 4.0f}));
  assert(region.contains({5.4f, 0.0f}, 0.5f));
  assert(!region.contains({5.6f, 0.0f}, 0.5f));

  const Vec2 projected = region.projectInside({6.0f, 8.0f});
  assert(projected == (Vec2{3.0f, 4.0f}));
  assert(region.projectInside({1.0f, 2.0f}) == (Vec2{1.0f, 2.0f}));

  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(region.projectInside({infinity, 0.0f}) == (Vec2{0.0f, 0.0f}));
  assert(region.projectInside({0.0f, nan}) == (Vec2{0.0f, 0.0f}));
  assert(!region.contains({infinity, 0.0f}));
}

void testOverlappingEntitiesSeparateByStableIdDirection() {
  const Vec2 lower = stableSeparation(10, {2.0f, 3.0f}, 20, {2.0f, 3.0f}, 1.0f);
  const Vec2 higher = stableSeparation(20, {2.0f, 3.0f}, 10, {2.0f, 3.0f}, 1.0f);

  assert(lower.finite());
  assert(higher.finite());
  assert(lower == (Vec2{-0.5f, 0.0f}));
  assert(higher == (Vec2{0.5f, 0.0f}));
  assert(lower == higher * -1.0f);

  const CombatRegion region({{0.0f, 0.0f}, 5.0f});
  assert(region.stableSeparation(10, {0.0f, 0.0f}, 20, {0.0f, 0.0f}, 1.0f) ==
         lower);
}

void testSeparationRejectsInvalidGeometryWithoutNan() {
  assert(stableSeparation(1, {0.0f, 0.0f}, 2, {2.0f, 0.0f}, 1.0f) ==
         (Vec2{}));
  assert(stableSeparation(1, {0.0f, 0.0f}, 1, {0.0f, 0.0f}, 1.0f) ==
         (Vec2{}));

  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(stableSeparation(1, {infinity, 0.0f}, 2, {0.0f, 0.0f}, 1.0f).finite());
  assert(stableSeparation(1, {0.0f, 0.0f}, 2, {nan, 0.0f}, 1.0f).finite());
  assert(stableSeparation(1, {0.0f, 0.0f}, 2, {0.0f, 0.0f}, nan).finite());

  const float maximum = std::numeric_limits<float>::max();
  assert(stableSeparation(1, {maximum, maximum}, 2, {-maximum, -maximum}, 1.0f)
             .finite());
}

}  // namespace

int main() {
  testContainsAndProjection();
  testOverlappingEntitiesSeparateByStableIdDirection();
  testSeparationRejectsInvalidGeometryWithoutNan();
}
