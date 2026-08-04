#pragma once
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <mutex>
#include "fixed_step.h"
#include "../../engine/presentation/vfx_system.h"
#include "../../engine/presentation/performance_guard.h"
#include "../../engine/presentation/damage_numbers.h"
#include "../../platform/harmony/audio_bridge.h"
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
#include "../../gameplay/ai/encounter_controller.h"
#include "../../gameplay/flow/demo_director.h"

struct Loop {
  Loop() {
    const EnvironmentComposition composition =
        EnvironmentController::defaultComposition();
    surface.player.x = composition.spawn.x;
    surface.player.y = composition.spawn.z;
    (void)encounter.start(EncounterMode::Training);
  }

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
  EncounterController encounter{combat};
  DemoDirector demoDirector;
  std::optional<TargetSelection> currentTarget;
  VfxSystem vfxSystem;
  DamageNumberSystem damageNumbers;
  PerformanceGuard performanceGuard;
  AudioBridge audioBridge;
  bool debugHud_ = false;
  FixedStep fixedStep{16, 4};
  SnapshotStore snapshots;
  LifecycleState lifecycle;
  std::mutex inputEnqueueMutex;
  mutable std::mutex combatEventMutex;
  std::atomic<Tick> combatTimeMs_{0};
  CombatEventBatch frameCombatEvents_;
  uint64_t inputSequence = 0;
  int32_t inputEventCount_ = 0;
  std::atomic<bool> running{false};
  std::atomic<bool> shouldStop{false};
  std::atomic<bool> paused{false};
  std::thread runner;
  float fps = 0.0f;
  float particleEmitTimer = 0.0f;
  // 命中卡肉（hitstop）剩余毫秒：>0 时冻结固定步逻辑、渲染继续，
  // 制造命中瞬间的顿帧打击感。仅在玩家命中敌人时触发。
  int64_t hitStopRemainingMs = 0;
  int tickCount = 0;
  std::chrono::steady_clock::time_point lastFpsTime;

  void start();
  void stop();
  // 暂停/恢复帧循环逻辑：暂停期间冻结输入与固定步更新，
  // 画面保持最后一帧，供暂停菜单与结算界面使用。
  void setPaused(bool value);
  bool isPaused() const { return paused.load(); }
  void tickOnce(int64_t elapsedMs);
  void updateFixed(Tick tick, int64_t dtMs);
  void processInput();
  void resetInput();
  void publishRendererStopped();
  bool startEncounter(EncounterMode mode);
  bool advanceLevel();
  bool useSupply();
  bool retryBoss();
  void toggleDebugHud();
  void skipDemoPhase(DemoPhase phase) { demoDirector.skipTo(phase); }

  template <typename Fn>
  decltype(auto) withLifecycle(Fn&& operation) {
    return lifecycle.synchronized(std::forward<Fn>(operation));
  }

  bool enqueueInput(InputAction action, int32_t pointerId, float x, float y) {
    std::lock_guard<std::mutex> lock(inputEnqueueMutex);
    const bool accepted =
        input.push({action, pointerId, x, y, inputSequence});
    if (accepted) ++inputSequence;
    if (accepted) ++inputEventCount_;
    return accepted;
  }

  GameSnapshot snapshot() const { return snapshots.read(); }
  Tick combatTimeMs() const { return combatTimeMs_.load(); }
  CombatEventBatch combatEvents() const {
    std::lock_guard<std::mutex> lock(combatEventMutex);
    return frameCombatEvents_;
  }
};
