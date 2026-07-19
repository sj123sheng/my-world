#pragma once

#include "engine/core/tick_clock.h"

#include <cstdint>

// Performance degradation level output by PerformanceGuard.
//   0 = full quality (FPS >= 55)
//   1 = light     (FPS 40-55)
//   2 = medium    (FPS 30-40)
//   3 = heavy     (FPS < 30)
enum class PerfLevel : int32_t {
  Full = 0,
  Light = 1,
  Medium = 2,
  Heavy = 3,
};

class PerformanceGuard {
 public:
  // Record one FPS sample. The guard keeps a 2-second sliding window.
  void sample(Tick tick, int64_t dtMs, float fps);

  // Current degradation level based on the sliding window average.
  int32_t level() const { return static_cast<int32_t>(level_); }

 private:
  void recompute(Tick now);

  // Ring buffer of recent FPS samples within the 2-second window.
  static constexpr int kWindowCapacity = 128;
  struct Sample {
    Tick tick = 0;
    float fps = 60.0f;
  };
  Sample samples_[kWindowCapacity]{};
  int writeIndex_ = 0;
  int sampleCount_ = 0;
  PerfLevel level_ = PerfLevel::Full;
};
