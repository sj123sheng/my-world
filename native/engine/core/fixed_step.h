#pragma once
#include "tick_clock.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

class FixedStep {
 public:
  FixedStep(int64_t stepMs, int maxSteps)
      : stepMs_(stepMs), maxSteps_(maxSteps) {
    assert(stepMs_ > 0);
    assert(maxSteps_ >= 0);
  }

  template <typename Fn>
  int advance(int64_t elapsedMs, Fn&& update) {
    const uint64_t elapsed =
        elapsedMs > 0 ? static_cast<uint64_t>(elapsedMs) : 0;
    accumulatorMs_ += elapsed;
    const uint64_t available = accumulatorMs_ / static_cast<uint64_t>(stepMs_);
    const int count = static_cast<int>(std::min<uint64_t>(
        available, static_cast<uint64_t>(maxSteps_)));
    for (int i = 0; i < count; ++i) {
      ++tick_;
      update(tick_, stepMs_);
      accumulatorMs_ -= stepMs_;
    }
    if (available > static_cast<uint64_t>(maxSteps_)) {
      const uint64_t dropped = available - static_cast<uint64_t>(maxSteps_);
      droppedFrames_ = dropped > std::numeric_limits<uint64_t>::max() - droppedFrames_
                           ? std::numeric_limits<uint64_t>::max()
                           : droppedFrames_ + dropped;
      accumulatorMs_ %= static_cast<uint64_t>(stepMs_);
    }
    return count;
  }

  Tick tick() const { return tick_; }
  uint64_t droppedFrames() const { return droppedFrames_; }

 private:
  int64_t stepMs_;
  int maxSteps_;
  uint64_t accumulatorMs_ = 0;
  Tick tick_ = 0;
  uint64_t droppedFrames_ = 0;
};
