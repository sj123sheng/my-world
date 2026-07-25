#include "native/engine/render/environment.h"

#include <cassert>
#include <cstdint>

namespace {

static_assert(static_cast<uint8_t>(EnvironmentBatchKind::OuterRing) == 0);
static_assert(static_cast<uint8_t>(EnvironmentBatchKind::CenterRift) == 1);
static_assert(static_cast<uint8_t>(EnvironmentBatchKind::Backdrop) == 2);
static_assert(static_cast<uint8_t>(EnvironmentBatchKind::Decoration) == 3);

static_assert(static_cast<uint8_t>(EnvironmentBatchStatus::Empty) == 0);
static_assert(static_cast<uint8_t>(EnvironmentBatchStatus::Pending) == 1);
static_assert(static_cast<uint8_t>(EnvironmentBatchStatus::Ready) == 2);
static_assert(static_cast<uint8_t>(EnvironmentBatchStatus::Failed) == 3);

void testFullQualityShowsAllBatches() {
  EnvironmentController controller;
  const EnvironmentRenderPlan plan = controller.evaluate({0.5f, 0.5f}, 0);
  assert(plan.outerRing);
  assert(plan.centerRift);
  assert(plan.backdrop);
  assert(plan.decoration);
  assert(plan.textureTier == StaticTextureTier::Full);
}

void testDegradationOrderIsFixed() {
  EnvironmentController controller;
  const auto light = controller.evaluate({0.5f, 0.5f}, 1);
  assert(light.backdrop && light.decoration);

  const auto medium = controller.evaluate({0.5f, 0.5f}, 2);
  assert(!medium.backdrop && medium.decoration);

  const auto heavy = controller.evaluate({0.5f, 0.5f}, 3);
  assert(!heavy.backdrop && !heavy.decoration);
  assert(heavy.textureTier == StaticTextureTier::Full);

  const auto critical = controller.evaluate({0.5f, 0.5f}, 4);
  assert(!critical.backdrop && !critical.decoration);
  assert(critical.outerRing && critical.centerRift);
  assert(critical.textureTier == StaticTextureTier::Half);
}

void testCenterRiftNeverDisappears() {
  EnvironmentController controller;
  for (int level = 0; level <= 4; ++level) {
    assert(controller.evaluate({1.0f, 1.0f}, level).centerRift);
  }
}

void testCameraZoneSuppressesCloseRangeClutter() {
  EnvironmentController controller;
  const auto center = controller.evaluate({0.5f, 0.75f}, 0);
  const auto outer = controller.evaluate({0.15f, 0.15f}, 0);
  assert(!center.decoration);
  assert(outer.decoration);
  assert(center.outerRing && center.centerRift);
}

}  // namespace

int main() {
  testFullQualityShowsAllBatches();
  testDegradationOrderIsFixed();
  testCenterRiftNeverDisappears();
  testCameraZoneSuppressesCloseRangeClutter();
  return 0;
}
