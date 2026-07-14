#pragma once
#include "tick_clock.h"
#include <algorithm>
#include <cstdint>

class FixedStep {
 public:
  FixedStep(int64_t stepMs, int maxSteps)
      : stepMs_(stepMs), maxSteps_(maxSteps) {}

  template <typename Fn>
  int advance(int64_t elapsedMs, Fn&& update) {
    accumulatorMs_ += std::max<int64_t>(0, elapsedMs);
    int available = static_cast<int>(accumulatorMs_ / stepMs_);
    int count = std::min(available, maxSteps_);
    for (int i = 0; i < count; ++i) {
      ++tick_;
      update(tick_, stepMs_);
      accumulatorMs_ -= stepMs_;
    }
    if (available > maxSteps_) {
      droppedFrames_ += static_cast<uint64_t>(available - maxSteps_);
      accumulatorMs_ %= stepMs_;
    }
    return count;
  }

  Tick tick() const { return tick_; }
  uint64_t droppedFrames() const { return droppedFrames_; }

 private:
  int64_t stepMs_;
  int maxSteps_;
  int64_t accumulatorMs_ = 0;
  Tick tick_ = 0;
  uint64_t droppedFrames_ = 0;
};
