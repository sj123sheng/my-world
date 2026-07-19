#include "native/engine/presentation/vfx_system.h"

#include <cassert>
#include <cstdint>

namespace {

PresentationEvent makeEvent(PresentationEventType type, Tick tick, FixedPoint intensity = fp(50)) {
  PresentationEvent e{};
  e.type = type;
  e.tick = tick;
  e.intensity = intensity;
  return e;
}

void testHitFlashTriggersAndDecays() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::HitFlash, 100));
  vfx.consume(batch);
  assert(vfx.snapshot().hitFlashMs > 0);
  for (int i = 0; i < 20; i++) vfx.update(116 + 16 * i, 16);
  assert(vfx.snapshot().hitFlashMs == 0);
}

void testDodgeFlashTriggers() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::DodgeFlash, 50));
  vfx.consume(batch);
  assert(vfx.snapshot().dodgeFlashMs > 0);
}

void testPoiseBreakBurstTriggers() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::PoiseBreakBurst, 80));
  vfx.consume(batch);
  assert(vfx.snapshot().poiseBreakMs > 0);
}

void testResonanceBurstTriggers() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::ResonanceBurst, 90));
  vfx.consume(batch);
  assert(vfx.snapshot().resonanceBurstMs > 0);
}

void testPhaseTransitionTriggers() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::PhaseTransition, 120));
  vfx.consume(batch);
  assert(vfx.snapshot().phaseTransitionMs > 0);
}

void testCastBarBrokenTriggers() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::CastBarBroken, 70));
  vfx.consume(batch);
  assert(vfx.snapshot().castBarBrokenMs > 0);
}

void testCameraShakeDecays() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::CameraShake, 0, fp(30)));
  vfx.consume(batch);
  assert(vfx.snapshot().cameraShakeX != 0.0f || vfx.snapshot().cameraShakeY != 0.0f);
  for (int i = 0; i < 30; i++) vfx.update(16 * (i + 1), 16);
  assert(vfx.snapshot().cameraShakeX == 0.0f && vfx.snapshot().cameraShakeY == 0.0f);
}

void testRepeatEventRefreshesNotStacks() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::HitFlash, 100));
  vfx.consume(batch);
  Tick firstMs = vfx.snapshot().hitFlashMs;
  vfx.consume(batch);
  assert(vfx.snapshot().hitFlashMs == firstMs);
}

void testVfxFlagsReflectActiveEffects() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::HitFlash, 100));
  vfx.consume(batch);
  assert(vfx.snapshot().vfxFlags != 0);
  for (int i = 0; i < 20; i++) vfx.update(116 + 16 * i, 16);
  assert(vfx.snapshot().vfxFlags == 0);
}

void testEmptyBatchNoEffect() {
  VfxSystem vfx;
  CombatEventBatch batch{};
  vfx.consume(batch);
  assert(vfx.snapshot().hitFlashMs == 0);
  assert(vfx.snapshot().dodgeFlashMs == 0);
  assert(vfx.snapshot().vfxFlags == 0);
}

}  // namespace

int main() {
  testHitFlashTriggersAndDecays();
  testDodgeFlashTriggers();
  testPoiseBreakBurstTriggers();
  testResonanceBurstTriggers();
  testPhaseTransitionTriggers();
  testCastBarBrokenTriggers();
  testCameraShakeDecays();
  testRepeatEventRefreshesNotStacks();
  testVfxFlagsReflectActiveEffects();
  testEmptyBatchNoEffect();
  return 0;
}
