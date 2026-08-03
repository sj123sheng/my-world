#pragma once

#include "engine/core/tick_clock.h"
#include "gameplay/combat/combat_controller.h"
#include "native/engine/presentation/visual_tokens.h"

#include <glm/vec3.hpp>

#include <cstdint>

enum class VfxShape : uint8_t {
  RingWave,
  Trail,
  Rune,
  HitFlash,
  OutlinePulse,
};

struct VfxCue {
  VfxShape shape = VfxShape::RingWave;
  SourceType source = SourceType::Radiance;
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float intensity = 0.0f;
  Tick durationMs = 0;

  static VfxCue resonance(SourceType source, float intensity,
                          Tick durationMs) {
    return {VfxShape::RingWave, source, VisualTokens::sourceColor(source),
            intensity, durationMs};
  }
};

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
  VfxCue resonanceCue;
};

class VfxSystem {
 public:
  void consume(const CombatEventBatch& batch);
  void update(Tick tick, int64_t dtMs);
  const VfxSnapshot& snapshot() const { return snapshot_; }

 private:
 void refreshFlags();

  Tick cameraShakeRemainingMs_ = 0;
  // 受击时记录的抖动峰值幅度；振荡衰减期间保持不变，
  // 避免重复事件提前压低幅度。
  float cameraShakeAmplitude_ = 0.0f;
  VfxSnapshot snapshot_;
};
