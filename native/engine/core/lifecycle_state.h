#pragma once
#include "game_snapshot.h"
#include <mutex>
#include <utility>

class LifecycleState {
 public:
  template <typename Fn>
  decltype(auto) synchronized(Fn&& operation) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return std::forward<Fn>(operation)();
  }

  template <typename Fn>
  bool start(Fn&& operation) {
    return synchronized([&]() {
      if (running_) return false;
      std::forward<Fn>(operation)();
      running_ = true;
      return true;
    });
  }

  template <typename Fn>
  bool stop(Fn&& operation) {
    return synchronized([&]() {
      if (!running_) return false;
      std::forward<Fn>(operation)();
      running_ = false;
      return true;
    });
  }

 private:
  std::recursive_mutex mutex_;
  bool running_ = false;
};

inline GameSnapshot RendererStoppedSnapshot(GameSnapshot snapshot) {
  snapshot.rendererReady = false;
  snapshot.moving = false;
  snapshot.moveX = 0.0f;
  snapshot.moveY = 0.0f;
  snapshot.targetId = 0;
  snapshot.targetDist = 0.0f;
  return snapshot;
}
