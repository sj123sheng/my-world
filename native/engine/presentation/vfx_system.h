#pragma once

#include "engine/core/tick_clock.h"
#include "gameplay/combat/combat_controller.h"

#include <cstdint>

// VFX effect bit flags for snapshot consumers.
enum VfxFlag : int32_t {
  VfxNone = 0,
  VfxHitFlash = 1 << 0,
  VfxDodgeFlash = 1 << 1,
  VfxPoiseBreak = 1 << 2,
  VfxResonanceBurst = 1 << 3,
  VfxPhaseTransition = 1 << 4,
  VfxCastBarBroken = 1 << 5,
  VfxCameraShake = 1 << 6,
};

struct VfxSnapshot {
  Tick hitFlashMs = 0;
  Tick dodgeFlashMs = 0;
  Tick poiseBreakMs = 0;
  Tick resonanceBurstMs = 0;
  Tick phaseTransitionMs = 0;
  Tick castBarBrokenMs = 0;
  float cameraShakeX = 0.0f;
  float cameraShakeY = 0.0f;
  int32_t vfxFlags = VfxNone;
};

class VfxSystem {
 public:
  void consume(const CombatEventBatch& batch);
  void update(Tick tick, int64_t dtMs);
  const VfxSnapshot& snapshot() const { return snapshot_; }

 private:
 void refreshFlags();

  Tick cameraShakeRemainingMs_ = 0;
  VfxSnapshot snapshot_;
};
