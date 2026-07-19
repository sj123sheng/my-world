#include "vfx_system.h"

#include <algorithm>
#include <cmath>

namespace {

// Duration of each VFX effect in milliseconds.
constexpr Tick kHitFlashMs = 200;
constexpr Tick kDodgeFlashMs = 400;
constexpr Tick kPoiseBreakMs = 600;
constexpr Tick kResonanceBurstMs = 800;
constexpr Tick kPhaseTransitionMs = 500;
constexpr Tick kCastBarBrokenMs = 400;
constexpr Tick kCameraShakeMs = 300;

// Camera shake amplitude per unit intensity (scaled to reasonable pixel offset).
constexpr float kShakeScale = 0.01f;

float RandomShake(FixedPoint intensity) {
  const float amp = static_cast<float>(static_cast<double>(intensity) / FP_ONE) * kShakeScale;
  // Deterministic pseudo-random sign: use the tick as a simple hash.
  return amp;
}

}  // namespace

void VfxSystem::consume(const CombatEventBatch& batch) {
  for (const PresentationEvent& e : batch.presentation) {
    switch (e.type) {
      case PresentationEventType::HitFlash:
        snapshot_.hitFlashMs = kHitFlashMs;
        break;
      case PresentationEventType::DodgeFlash:
        snapshot_.dodgeFlashMs = kDodgeFlashMs;
        break;
      case PresentationEventType::PoiseBreakBurst:
        snapshot_.poiseBreakMs = kPoiseBreakMs;
        break;
      case PresentationEventType::ResonanceBurst:
        snapshot_.resonanceBurstMs = kResonanceBurstMs;
        break;
      case PresentationEventType::PhaseTransition:
        snapshot_.phaseTransitionMs = kPhaseTransitionMs;
        break;
      case PresentationEventType::CastBarBroken:
        snapshot_.castBarBrokenMs = kCastBarBrokenMs;
        break;
      case PresentationEventType::CameraShake: {
        const float amp = RandomShake(e.intensity);
        // Apply shake in both axes with opposite signs for a jitter feel.
        if (snapshot_.cameraShakeX == 0.0f && snapshot_.cameraShakeY == 0.0f) {
          snapshot_.cameraShakeX = amp;
          snapshot_.cameraShakeY = -amp;
        }
        // Track remaining shake time via a re-purposed field: we store
        // the remaining ms in cameraShakeX sign by using a separate counter.
        // Simpler: store remaining shake ms in a dedicated approach below.
        cameraShakeRemainingMs_ = kCameraShakeMs;
        break;
      }
    }
  }
  refreshFlags();
}

void VfxSystem::update(Tick /*tick*/, int64_t dtMs) {
  if (dtMs <= 0) return;
  const Tick dt = static_cast<Tick>(dtMs);

  auto decay = [dt](Tick& val) {
    if (val == 0) return;
    val = (val > dt) ? val - dt : 0;
  };

  decay(snapshot_.hitFlashMs);
  decay(snapshot_.dodgeFlashMs);
  decay(snapshot_.poiseBreakMs);
  decay(snapshot_.resonanceBurstMs);
  decay(snapshot_.phaseTransitionMs);
  decay(snapshot_.castBarBrokenMs);

  if (cameraShakeRemainingMs_ > 0) {
    cameraShakeRemainingMs_ = (cameraShakeRemainingMs_ > dt)
        ? cameraShakeRemainingMs_ - dt : 0;
    if (cameraShakeRemainingMs_ == 0) {
      snapshot_.cameraShakeX = 0.0f;
      snapshot_.cameraShakeY = 0.0f;
    } else {
      // Decay amplitude linearly.
      const float ratio = static_cast<float>(cameraShakeRemainingMs_) /
                          static_cast<float>(kCameraShakeMs);
      const float signX = (snapshot_.cameraShakeX >= 0.0f) ? 1.0f : -1.0f;
      const float signY = (snapshot_.cameraShakeY >= 0.0f) ? 1.0f : -1.0f;
      snapshot_.cameraShakeX = std::fabs(snapshot_.cameraShakeX) * ratio * signX;
      snapshot_.cameraShakeY = std::fabs(snapshot_.cameraShakeY) * ratio * signY;
    }
  }

  refreshFlags();
}

void VfxSystem::refreshFlags() {
  int32_t flags = VfxNone;
  if (snapshot_.hitFlashMs > 0) flags |= VfxHitFlash;
  if (snapshot_.dodgeFlashMs > 0) flags |= VfxDodgeFlash;
  if (snapshot_.poiseBreakMs > 0) flags |= VfxPoiseBreak;
  if (snapshot_.resonanceBurstMs > 0) flags |= VfxResonanceBurst;
  if (snapshot_.phaseTransitionMs > 0) flags |= VfxPhaseTransition;
  if (snapshot_.castBarBrokenMs > 0) flags |= VfxCastBarBroken;
  if (cameraShakeRemainingMs_ > 0) flags |= VfxCameraShake;
  snapshot_.vfxFlags = flags;
}
