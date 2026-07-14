#include "../native/engine/input/input_queue.h"
#include <cassert>
#include <thread>

int main() {
  InputQueue queue(2);
  assert(queue.push({InputAction::PointerDown, 7, 10.0f, 20.0f, 1}));
  assert(queue.push({InputAction::PointerMove, 7, 11.0f, 21.0f, 2}));
  assert(!queue.push({InputAction::PointerUp, 7, 12.0f, 22.0f, 3}));
  assert(queue.droppedCount() == 1);

  InputEvent out{};
  assert(queue.pop(out));
  assert(out.action == InputAction::PointerDown && out.pointerId == 7 && out.sequence == 1);
  assert(queue.pop(out));
  assert(out.action == InputAction::PointerMove && out.sequence == 2);
  assert(!queue.pop(out));

  InputQueue concurrent(256);
  std::thread producer([&]() {
    for (uint64_t i = 0; i < 200; ++i) {
      assert(concurrent.push({InputAction::PointerMove, 1, float(i), 0.0f, i}));
    }
  });
  producer.join();
  uint64_t expected = 0;
  while (concurrent.pop(out)) assert(out.sequence == expected++);
  assert(expected == 200);
  return 0;
}
