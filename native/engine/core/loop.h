#pragma once
#include <thread>
#include <atomic>
#include <chrono>
#include "../render/surface.h"
#include "../input/input_queue.h"

struct Loop {
  Surface surface;
  InputQueue input;
  std::atomic<bool> running{false};
  std::atomic<bool> shouldStop{false};
  std::thread runner;
  float fps = 0.0f;
  int tickCount = 0;
  std::chrono::steady_clock::time_point lastFpsTime;

  void start();
  void stop();
  void tickOnce();
  void processInput();
  void updatePlayer(float dt);
};
