#include "../native/engine/core/snapshot_store.h"
#include <atomic>
#include <cassert>
#include <thread>

int main() {
  SnapshotStore store;
  store.publish({3, 90, 80, 0.25f, 0.75f, 59.5f, true, 4, 2, true});
  GameSnapshot snapshot = store.read();
  assert(snapshot.tick == 3 && snapshot.hp == 90 && snapshot.bossPhase == 2);

  std::atomic<bool> readerStarted{false};
  std::atomic<bool> writerDone{false};
  std::atomic<Tick> observedTick{3};
  std::thread writer([&]() {
    while (!readerStarted.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    for (Tick tick = 4; tick <= 100; ++tick) {
      store.publish({tick, 90, 80, 0.25f, 0.75f, 59.5f, true, 4, 2, true});
      while (observedTick.load(std::memory_order_acquire) < tick) {
        std::this_thread::yield();
      }
    }
    writerDone.store(true, std::memory_order_release);
  });

  Tick last = 0;
  readerStarted.store(true, std::memory_order_release);
  while (!writerDone.load(std::memory_order_acquire)) {
    Tick current = store.read().tick;
    assert(current >= last);
    last = current;
    observedTick.store(current, std::memory_order_release);
    std::this_thread::yield();
  }
  writer.join();
  assert(observedTick.load(std::memory_order_acquire) == 100);
  assert(store.read().tick == 100);
  return 0;
}
