#pragma once
#include "game_snapshot.h"
#include <mutex>

class SnapshotStore {
 public:
  void publish(const GameSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = snapshot;
  }

  GameSnapshot read() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
  }

 private:
  mutable std::mutex mutex_;
  GameSnapshot snapshot_{};
};
