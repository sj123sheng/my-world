#pragma once
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <mutex>
#include "fixed_step.h"
#include "lifecycle_state.h"
#include "snapshot_store.h"
#include "../render/surface.h"
#include "../render/camera.h"
#include "../input/input_queue.h"
#include "../input/touch_router.h"
#include "../input/virtual_joystick.h"
#include "../input/camera_gesture.h"
#include "../input/player_intent.h"
#include "../../gameplay/player/player_controller.h"
#include "../../gameplay/targeting/soft_targeting.h"
#include "../../gameplay/combat/combat_controller.h"

struct Loop {
  Surface surface;
  InputQueue input;
  TouchRouter touchRouter;
  VirtualJoystick joystick{VirtualJoystickConfig{}};
  CameraGesture cameraGesture{CameraGestureConfig{}};
  PlayerIntent intent;
  PlayerController playerController;
  ThirdPersonCamera camera;
  SoftTargeting softTargeting;
  CombatController combat{CombatConfig::defaults()};
  std::optional<TargetSelection> currentTarget;
  FixedStep fixedStep{16, 4};
  SnapshotStore snapshots;
  LifecycleState lifecycle;
  std::mutex inputEnqueueMutex;
  mutable std::mutex combatEventMutex;
  std::atomic<Tick> combatTimeMs_{0};
  CombatEventBatch frameCombatEvents_;
  uint64_t inputSequence = 0;
  std::atomic<bool> running{false};
  std::atomic<bool> shouldStop{false};
  std::thread runner;
  float fps = 0.0f;
  float particleEmitTimer = 0.0f;
  int tickCount = 0;
  std::chrono::steady_clock::time_point lastFpsTime;

  void start();
  void stop();
  void tickOnce(int64_t elapsedMs);
  void updateFixed(Tick tick, int64_t dtMs);
  void processInput();
  void resetInput();
  void publishRendererStopped();

  template <typename Fn>
  decltype(auto) withLifecycle(Fn&& operation) {
    return lifecycle.synchronized(std::forward<Fn>(operation));
  }

  bool enqueueInput(InputAction action, int32_t pointerId, float x, float y) {
    std::lock_guard<std::mutex> lock(inputEnqueueMutex);
    const bool accepted =
        input.push({action, pointerId, x, y, inputSequence});
    if (accepted) ++inputSequence;
    return accepted;
  }

  GameSnapshot snapshot() const { return snapshots.read(); }
  Tick combatTimeMs() const { return combatTimeMs_.load(); }
  CombatEventBatch combatEvents() const {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    return frameCombatEvents_;
  }
};
