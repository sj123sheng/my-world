#include "native/gameplay/targeting/soft_targeting.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

bool near(float actual, float expected, float tolerance = 1.0e-5f) {
  return std::abs(actual - expected) <= tolerance;
}

void testFiltersInvalidCandidates() {
  SoftTargeting targeting({1.0f, kPi});
  const float infinity = std::numeric_limits<float>::infinity();
  const std::vector<TargetCandidate> candidates{
      {0, {0.0f, 0.2f}},
      {-1, {0.0f, 0.2f}},
      {1, {infinity, 0.0f}},
      {2, {0.0f, 0.0f}},
      {3, {0.0f, 1.01f}},
  };

  assert(!targeting.select({0.0f, 0.0f}, 0.0f, candidates));
}

void testUsesAngleDistanceThenIdPriority() {
  SoftTargeting targeting({2.0f, kPi});

  auto selected = targeting.select(
      {0.0f, 0.0f}, 0.0f,
      {{1, {0.0f, 1.0f}}, {2, {0.1f, 0.2f}}});
  assert(selected && selected->id == 1);

  selected = targeting.select(
      {0.0f, 0.0f}, 0.0f,
      {{1, {0.0f, 1.0f}}, {2, {0.0f, 0.5f}}});
  assert(selected && selected->id == 2);

  selected = targeting.select(
      {0.0f, 0.0f}, 0.0f,
      {{3, {0.0f, 0.5f}}, {2, {0.0f, 0.5f}}});
  assert(selected && selected->id == 2);
}

void testIsIndependentOfInputOrderAndRejectsDuplicateIds() {
  SoftTargeting targeting({2.0f, kPi});
  std::vector<TargetCandidate> candidates{
      {4, {0.0f, 0.25f}},
      {5, {0.0f, 0.75f}},
      {4, {0.0f, 1.0f}},
  };

  const auto forward = targeting.select({0.0f, 0.0f}, 0.0f, candidates);
  std::reverse(candidates.begin(), candidates.end());
  const auto reversed = targeting.select({0.0f, 0.0f}, 0.0f, candidates);

  assert(forward && reversed);
  assert(forward->id == 5);
  assert(reversed->id == forward->id);
  assert(near(reversed->distance, forward->distance));
  assert(near(reversed->angle, forward->angle));
  assert(reversed->direction == forward->direction);
}

void testIncludesDistanceAndAngleBoundaries() {
  SoftTargeting targeting({1.0f, kPi / 2.0f});
  const auto selected = targeting.select(
      {0.0f, 0.0f}, 0.0f, {{7, {1.0f, 0.0f}}});

  assert(selected && selected->id == 7);
  assert(near(selected->distance, 1.0f));
  assert(near(selected->angle, kPi / 2.0f));
}

void testRejectsNonFinitePlayerAndYaw() {
  SoftTargeting targeting;
  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const std::vector<TargetCandidate> candidates{{1, {0.0f, 0.5f}}};

  assert(!targeting.select({infinity, 0.0f}, 0.0f, candidates));
  assert(!targeting.select({0.0f, nan}, 0.0f, candidates));
  assert(!targeting.select({0.0f, 0.0f}, infinity, candidates));
}

void testNormalizesInvalidConfiguration() {
  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const std::vector<float> invalidDistances{0.0f, -1.0f, infinity, nan};

  for (const float maxDistance : invalidDistances) {
    SoftTargeting targeting({maxDistance, 0.0f});
    assert(targeting.select({0.0f, 0.0f}, 0.0f,
                            {{1, {0.0f, 0.5f}}}));
    assert(!targeting.select({0.0f, 0.0f}, 0.0f,
                             {{1, {0.0f, 0.8f}}}));
  }

  SoftTargeting nonFiniteAngle({1.0f, nan});
  assert(nonFiniteAngle.select({0.0f, 0.0f}, 0.0f,
                               {{1, {0.5f, 0.5f}}}));
  assert(!nonFiniteAngle.select({0.0f, 0.0f}, 0.0f,
                                {{1, {1.0f, 0.0f}}}));

  SoftTargeting negativeAngle({1.0f, -1.0f});
  assert(negativeAngle.select({0.0f, 0.0f}, 0.0f,
                              {{1, {0.0f, 0.5f}}}));
  assert(!negativeAngle.select({0.0f, 0.0f}, 0.0f,
                               {{1, {0.01f, 0.5f}}}));

  SoftTargeting oversizedAngle({1.0f, 2.0f * kPi});
  assert(oversizedAngle.select({0.0f, 0.0f}, 0.0f,
                               {{1, {0.0f, -0.5f}}}));

  SoftTargeting infiniteAngle({1.0f, infinity});
  assert(!infiniteAngle.select({0.0f, 0.0f}, 0.0f,
                               {{1, {1.0f, 0.0f}}}));
}

void testReturnsDistanceAngleAndDirection() {
  SoftTargeting targeting({2.0f, kPi});
  const auto selected = targeting.select(
      {1.0f, 2.0f}, 0.0f, {{9, {1.6f, 2.8f}}});

  assert(selected && selected->id == 9);
  assert(near(selected->distance, 1.0f));
  assert(near(selected->angle, std::acos(0.8f)));
  assert(near(selected->direction.x, 0.6f));
  assert(near(selected->direction.y, 0.8f));
}

void testHandlesLargeAndSmallFiniteOffsets() {
  const float maximum = std::numeric_limits<float>::max();
  SoftTargeting largeRange({maximum, kPi});
  const auto large = largeRange.select(
      {0.0f, 0.0f}, 0.0f, {{1, {maximum / 2.0f, maximum / 2.0f}}});
  assert(large && std::isfinite(large->distance));
  assert(near(large->direction.x, std::sqrt(0.5f)));
  assert(near(large->direction.y, std::sqrt(0.5f)));

  const float smallest = std::numeric_limits<float>::denorm_min();
  SoftTargeting normalRange({1.0f, kPi});
  const auto small = normalRange.select(
      {0.0f, 0.0f}, 0.0f, {{2, {smallest, 0.0f}}});
  assert(small && small->distance == smallest);
  assert(near(small->direction.x, 1.0f));
  assert(near(small->direction.y, 0.0f));
}

void testKeepsPreferredTargetUntilItDisappears() {
  SoftTargeting targeting({2.0f, kPi});
  const std::vector<TargetCandidate> candidates{
      {10, {0.0f, 1.0f}}, {20, {0.0f, 0.5f}}};

  const auto kept = targeting.select({0.0f, 0.0f}, 0.0f, candidates, 10);
  assert(kept && kept->id == 10);

  const auto retargeted = targeting.select(
      {0.0f, 0.0f}, 0.0f, {{20, {0.0f, 0.5f}}}, 10);
  assert(retargeted && retargeted->id == 20);
}

}  // namespace

int main() {
  testFiltersInvalidCandidates();
  testUsesAngleDistanceThenIdPriority();
  testIsIndependentOfInputOrderAndRejectsDuplicateIds();
  testIncludesDistanceAndAngleBoundaries();
  testRejectsNonFinitePlayerAndYaw();
  testNormalizesInvalidConfiguration();
  testReturnsDistanceAngleAndDirection();
  testHandlesLargeAndSmallFiniteOffsets();
  testKeepsPreferredTargetUntilItDisappears();
}
