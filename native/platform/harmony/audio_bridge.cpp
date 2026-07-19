#include "audio_bridge.h"

AudioBridge::~AudioBridge() {
  stop();
}

void AudioBridge::start() {
#ifdef OHOS_PLATFORM
  // TODO: Initialize OHAudioRenderer for 16-bit PCM at 44100 Hz.
  // On failure, silently degrade to no-op (no sound) per design spec.
  initialized_ = true;
#else
  initialized_ = false;
#endif
}

void AudioBridge::stop() {
#ifdef OHOS_PLATFORM
  // TODO: Release OHAudioRenderer stream and buffers.
  initialized_ = false;
#else
  initialized_ = false;
#endif
}

void AudioBridge::dispatch(const CombatEventBatch& batch) {
  if (!initialized_) return;

#ifdef OHOS_PLATFORM
  // TODO: Map events to placeholder sounds:
  //   GameplayEventType::Hit/Damage    -> low-frequency pulse
  //   GameplayEventType::Dodge        -> short dodge swoosh
  //   GameplayEventType::AuraApplied   -> source sine tone
  //   GameplayEventType::Resonance     -> resonance chord
  //   GameplayEventType::PhaseChanged  -> low warning tone
  //   PresentationEventType::HitFlash  -> hit click
  //   PresentationEventType::CameraShake -> impact thud
  //   PresentationEventType::CastBarBroken -> shatter sound
  // All sound generation will use procedural PCM synthesis (square/sine waves)
  // to avoid shipping audio asset files in this stage.
  (void)batch;
#else
  (void)batch;
#endif
}
