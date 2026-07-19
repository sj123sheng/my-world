#pragma once

#include "gameplay/combat/combat_controller.h"

// AudioBridge dispatches placeholder sound effects based on combat events.
// On devices without OHAudioRenderer support it degrades to a silent no-op.
class AudioBridge {
 public:
  AudioBridge() = default;
  ~AudioBridge();

  AudioBridge(const AudioBridge&) = delete;
  AudioBridge& operator=(const AudioBridge&) = delete;

  // Initialize the audio renderer. Silently no-ops if unsupported.
  void start();

  // Stop and release the audio renderer.
  void stop();

  // Dispatch sounds for gameplay and presentation events in the batch.
  void dispatch(const CombatEventBatch& batch);

 private:
  bool initialized_ = false;
};
