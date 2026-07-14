#pragma once
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

template <typename T>
class EventQueue {
 public:
  explicit EventQueue(size_t capacity = 1024) : capacity_(capacity) {}

  bool push(const T& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.size() >= capacity_) return false;
    events_.push_back(event);
    return true;
  }

  std::vector<T> drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<T> result;
    result.swap(events_);
    return result;
  }

 private:
  size_t capacity_;
  std::mutex mutex_;
  std::vector<T> events_;
};
