#include "native/engine/presentation/vfx_system.h"

#include <cassert>
#include <cstdint>
#include <cmath>
#include <glm/geometric.hpp>

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
  assert(vfx.snapshot().vfxFlags & VfxCameraShake);
  for (int i = 0; i < 30; i++) vfx.update(16 * (i + 1), 16);
  assert(vfx.snapshot().cameraShakeX == 0.0f && vfx.snapshot().cameraShakeY == 0.0f);
}

void testCameraShakeOscillates() {
  // 抖动必须在窗口内往复振荡（符号翻转），而非单向漂移。
  VfxSystem vfx;
  CombatEventBatch batch{};
  batch.presentation.push_back(makeEvent(PresentationEventType::CameraShake, 0, fp(30)));
  vfx.consume(batch);
  vfx.update(16, 16);
  const float firstX = vfx.snapshot().cameraShakeX;
  assert(firstX != 0.0f);
  bool signChanged = false;
  for (int i = 2; i <= 18; i++) {
    vfx.update(16 * i, 16);
    if (vfx.snapshot().cameraShakeX * firstX < 0.0f) signChanged = true;
  }
  assert(signChanged);
}

void testTriggerCameraShakeMatchesEventPath() {
  // 逻辑层直触（首领砸地）与事件通道同语义：触发后 update 产生
  // 非零偏移，窗口结束归零；更强强度产生更大峰值幅度。
  VfxSystem vfx;
  vfx.triggerCameraShake(FP_ONE);
  assert(vfx.snapshot().vfxFlags & VfxCameraShake);
  vfx.update(16, 16);
  const float weakX = vfx.snapshot().cameraShakeX;
  assert(weakX != 0.0f);
  for (int i = 2; i <= 20; i++) vfx.update(16 * i, 16);
  assert(vfx.snapshot().cameraShakeX == 0.0f &&
         vfx.snapshot().cameraShakeY == 0.0f);
  VfxSystem strong;
  strong.triggerCameraShake(2 * FP_ONE);
  strong.update(16, 16);
  assert(std::fabs(strong.snapshot().cameraShakeX) >= std::fabs(weakX));
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

void testResonanceCueUsesStableSourceColor() {
  const VfxCue cue = VfxCue::resonance(SourceType::Current, 0.8f, 600);
  const glm::vec3 expected{0.26f, 0.82f, 0.72f};
  assert(glm::length(cue.color - expected) < 0.0001f);
  assert(std::fabs(cue.intensity - 0.8f) < 0.0001f);
  assert(cue.durationMs == 600);
  assert(cue.shape == VfxShape::RingWave);
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
  testCameraShakeOscillates();
  testTriggerCameraShakeMatchesEventPath();
  testRepeatEventRefreshesNotStacks();
  testVfxFlagsReflectActiveEffects();
  testEmptyBatchNoEffect();
  testResonanceCueUsesStableSourceColor();
  return 0;
}
