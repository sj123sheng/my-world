#include "../native/engine/core/event_queue.h"
#include "../native/gameplay/combat/event.h"
#include <cassert>

int main() {
  EventQueue<GameplayEvent> gameplay(2);
  assert(gameplay.push({5, 2, 8, GameplayEventType::Hit, fp(10), 1}));
  assert(gameplay.push({5, 2, 8, GameplayEventType::PoiseBreak, fp(2), 2}));
  assert(!gameplay.push({5, 2, 8, GameplayEventType::Death, 0, 3}));
  auto events = gameplay.drain();
  assert(events.size() == 2);
  assert(events[0].sequence == 1 && events[1].sequence == 2);
  assert(gameplay.drain().empty());

  EventQueue<PresentationEvent> presentation;
  assert(presentation.push({5, 2, 8, PresentationEventType::HitFlash, fp(1), 1}));
  assert(presentation.drain()[0].type == PresentationEventType::HitFlash);
  return 0;
}
