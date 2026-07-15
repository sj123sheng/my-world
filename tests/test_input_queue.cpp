#include "../native/engine/input/input_queue.h"
#include <cassert>
#include <atomic>
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

  assert(queue.push({InputAction::PointerDown, 8, 1.0f, 2.0f, 4}));
  assert(queue.push({InputAction::PointerMove, 8, 3.0f, 4.0f, 5}));
  queue.clear();
  assert(!queue.pop(out));
  assert(queue.push({InputAction::PointerDown, 9, 5.0f, 6.0f, 6}));
  assert(queue.pop(out));
  assert(out.pointerId == 9 && out.sequence == 6);

  InputQueue concurrent(32);
  std::atomic<bool> producerDone{false};
  std::thread producer([&]() {
    for (uint64_t i = 0; i < 200; ++i) {
      while (!concurrent.push({InputAction::PointerMove, 1, float(i), 0.0f, i})) {
        std::this_thread::yield();
      }
    }
    producerDone = true;
  });
  uint64_t expected = 0;
  while (!producerDone || expected < 200) {
    if (concurrent.pop(out)) {
      assert(out.sequence == expected++);
    } else {
      std::this_thread::yield();
    }
  }
  producer.join();
  assert(expected == 200);
  return 0;
}
