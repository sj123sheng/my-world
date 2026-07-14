#pragma once
#include <thread>
#include <atomic>
#include <chrono>
#include "fixed_step.h"
#include "lifecycle_state.h"
#include "snapshot_store.h"
#include "../render/surface.h"
#include "../input/input_queue.h"

struct Loop {
  Surface surface;
  InputQueue input;
  FixedStep fixedStep{16, 4};
  SnapshotStore snapshots;
  LifecycleState lifecycle;
  std::atomic<uint64_t> inputSequence{0};
  std::atomic<bool> running{false};
  std::atomic<bool> shouldStop{false};
  std::thread runner;
  float fps = 0.0f;
  int tickCount = 0;
  std::chrono::steady_clock::time_point lastFpsTime;

  void start();
  void stop();
  void tickOnce(int64_t elapsedMs);
  void updateFixed(Tick tick, int64_t dtMs);
  void processInput();
  void updatePlayer(float dt);
  void publishRendererStopped();

  template <typename Fn>
  decltype(auto) withLifecycle(Fn&& operation) {
    return lifecycle.synchronized(std::forward<Fn>(operation));
  }

  bool enqueueInput(InputAction action, int32_t pointerId, float x, float y) {
    return input.push({action, pointerId, x, y, inputSequence.fetch_add(1)});
  }

  GameSnapshot snapshot() const { return snapshots.read(); }
};
