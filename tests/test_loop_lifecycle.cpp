#include "native/engine/core/lifecycle_state.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

int main() {
  LifecycleState lifecycle;
  std::atomic<int> active{0};
  std::atomic<int> maxActive{0};
  std::vector<std::thread> workers;
  for (int i = 0; i < 8; ++i) {
    workers.emplace_back([&]() {
      lifecycle.synchronized([&]() {
        const int current = ++active;
        int observed = maxActive.load();
        while (current > observed && !maxActive.compare_exchange_weak(observed, current)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        --active;
      });
    });
  }
  for (auto& worker : workers) worker.join();
  assert(maxActive == 1);

  lifecycle.synchronized([&]() {
    lifecycle.synchronized([]() {});
  });

  LifecycleState loopLifecycle;
  std::atomic<int> starts{0};
  workers.clear();
  for (int i = 0; i < 8; ++i) {
    workers.emplace_back([&]() {
      loopLifecycle.start([&]() { ++starts; });
    });
  }
  for (auto& worker : workers) worker.join();
  assert(starts == 1);
  assert(!loopLifecycle.start([&]() { ++starts; }));

  int stops = 0;
  assert(loopLifecycle.stop([&]() { ++stops; }));
  assert(!loopLifecycle.stop([&]() { ++stops; }));
  assert(stops == 1);
  assert(loopLifecycle.start([&]() { ++starts; }));
  assert(loopLifecycle.stop([&]() { ++stops; }));
  assert(starts == 2);
  assert(stops == 2);

  GameSnapshot running{};
  running.tick = 17;
  running.hp = 73;
  running.playerX = 0.25f;
  running.targetId = 9;
  running.rendererReady = true;
  const GameSnapshot stopped = RendererStoppedSnapshot(running);
  assert(!stopped.rendererReady);
  assert(stopped.tick == running.tick);
  assert(stopped.hp == running.hp);
  assert(stopped.playerX == running.playerX);
  assert(stopped.targetId == running.targetId);
}
