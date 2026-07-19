#include "native/platform/harmony/audio_bridge.h"

#include <cassert>

namespace {

void testDispatchEmptyBatchDoesNotCrash() {
  AudioBridge audio;
  CombatEventBatch batch{};
  audio.dispatch(batch);
  assert(true);
}

void testDispatchWithEventsDoesNotCrash() {
  AudioBridge audio;
  CombatEventBatch batch{};
  GameplayEvent ge{};
  ge.type = GameplayEventType::Hit;
  batch.gameplay.push_back(ge);
  PresentationEvent pe{};
  pe.type = PresentationEventType::HitFlash;
  batch.presentation.push_back(pe);
  audio.dispatch(batch);
  assert(true);
}

void testStartStopDoesNotCrash() {
  AudioBridge audio;
  audio.start();
  audio.stop();
  assert(true);
}

void testMultipleDispatchesDoesNotCrash() {
  AudioBridge audio;
  audio.start();
  CombatEventBatch batch{};
  GameplayEvent ge{};
  ge.type = GameplayEventType::Dodge;
  batch.gameplay.push_back(ge);
  for (int i = 0; i < 100; i++) audio.dispatch(batch);
  audio.stop();
  assert(true);
}

}  // namespace

int main() {
  testDispatchEmptyBatchDoesNotCrash();
  testDispatchWithEventsDoesNotCrash();
  testStartStopDoesNotCrash();
  testMultipleDispatchesDoesNotCrash();
  return 0;
}
