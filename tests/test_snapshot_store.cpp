#include "../native/engine/core/snapshot_store.h"
#include <atomic>
#include <cassert>
#include <thread>

int main() {
  SnapshotStore store;
  GameSnapshot initial;
  initial.tick = 3;
  initial.hp = 90;
  initial.poise = 80;
  initial.playerX = 0.25f;
  initial.playerY = 0.75f;
  initial.fps = 59.5f;
  initial.moving = true;
  initial.targetId = 4;
  initial.bossPhase = 2;
  initial.rendererReady = true;
  store.publish(initial);
  GameSnapshot snapshot = store.read();
  assert(snapshot.tick == 3 && snapshot.hp == 90 && snapshot.bossPhase == 2);
  assert(snapshot.playerChunkX == 0 && snapshot.playerChunkY == 0);
  assert(snapshot.playerLocalX == 0.5f && snapshot.playerLocalY == 0.5f);
  assert(snapshot.activeChunkCount == 0 && snapshot.cachedChunkCount == 0);
  assert(snapshot.streamingPendingCount == 0);

  std::atomic<bool> readerStarted{false};
  std::atomic<bool> writerDone{false};
  std::atomic<Tick> observedTick{3};
  std::thread writer([&]() {
    while (!readerStarted.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    for (Tick tick = 4; tick <= 100; ++tick) {
      GameSnapshot next;
      next.tick = tick;
      next.hp = static_cast<int32_t>(1000 + tick);
      next.poise = static_cast<int32_t>(2000 + tick);
      next.playerX = static_cast<float>(tick) / 100.0f;
      next.playerY = static_cast<float>(tick) / 200.0f;
      next.playerChunkX = tick;
      next.playerChunkY = -tick;
      next.playerLocalX = 0.25f;
      next.playerLocalY = 0.75f;
      next.activeChunkCount = static_cast<int32_t>(tick % 9);
      next.cachedChunkCount = static_cast<int32_t>(tick % 17);
      next.streamingPendingCount = static_cast<int32_t>(tick % 5);
      next.fps = static_cast<float>(tick) + 0.5f;
      next.moving = tick % 2 == 0;
      next.targetId = static_cast<int32_t>(3000 + tick);
      next.bossPhase = static_cast<int32_t>(tick % 3);
      next.rendererReady = true;
      store.publish(next);
      while (observedTick.load(std::memory_order_acquire) < tick) {
        std::this_thread::yield();
      }
    }
    writerDone.store(true, std::memory_order_release);
  });

  Tick last = 0;
  readerStarted.store(true, std::memory_order_release);
  while (!writerDone.load(std::memory_order_acquire)) {
    const GameSnapshot currentSnapshot = store.read();
    Tick current = currentSnapshot.tick;
    assert(current >= last);
    if (current >= 4) {
      assert(currentSnapshot.hp == static_cast<int32_t>(1000 + current));
      assert(currentSnapshot.poise == static_cast<int32_t>(2000 + current));
      assert(currentSnapshot.playerX == static_cast<float>(current) / 100.0f);
      assert(currentSnapshot.playerChunkX == current);
      assert(currentSnapshot.playerChunkY == -current);
      assert(currentSnapshot.playerLocalX == 0.25f);
      assert(currentSnapshot.playerLocalY == 0.75f);
      assert(currentSnapshot.activeChunkCount == static_cast<int32_t>(current % 9));
      assert(currentSnapshot.cachedChunkCount == static_cast<int32_t>(current % 17));
      assert(currentSnapshot.streamingPendingCount ==
             static_cast<int32_t>(current % 5));
      assert(currentSnapshot.targetId == static_cast<int32_t>(3000 + current));
      assert(currentSnapshot.bossPhase == static_cast<int32_t>(current % 3));
    }
    last = current;
    observedTick.store(current, std::memory_order_release);
    std::this_thread::yield();
  }
  writer.join();
  assert(observedTick.load(std::memory_order_acquire) == 100);
  assert(store.read().tick == 100);
  return 0;
}
