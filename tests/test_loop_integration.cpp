#include "native/engine/core/loop.h"

#include <cassert>
#include <type_traits>

int main() {
  static_assert(std::is_same_v<decltype(&Loop::tickOnce), void (Loop::*)(int64_t)>);
  static_assert(std::is_same_v<decltype(&Loop::updateFixed), void (Loop::*)(Tick, int64_t)>);

  Loop loop;
  assert(loop.enqueueInput(InputAction::PointerDown, 42, 10.0f, 20.0f));

  InputEvent event{};
  assert(loop.input.pop(event));
  assert(event.action == InputAction::PointerDown);
  assert(event.pointerId == 42);
  assert(event.x == 10.0f);
  assert(event.y == 20.0f);
  assert(event.sequence == 0);

  const GameSnapshot initial = loop.snapshot();
  assert(initial.tick == 0);
  assert(initial.targetId == 0);
  assert(initial.bossPhase == 0);
}
