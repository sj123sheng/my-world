#pragma once
#include "input_event.h"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>

class InputQueue {
 public:
  explicit InputQueue(size_t capacity = 256) : capacity_(capacity) {}

  bool push(const InputEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= capacity_) {
      ++dropped_;
      return false;
    }
    queue_.push(event);
    return true;
  }

  bool pop(InputEvent& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;
    out = queue_.front();
    queue_.pop();
    return true;
  }

  uint64_t droppedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_;
  }

 private:
  const size_t capacity_;
  mutable std::mutex mutex_;
  std::queue<InputEvent> queue_;
  uint64_t dropped_ = 0;
};
