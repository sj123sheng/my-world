#pragma once

#include "engine/core/tick_clock.h"

#include <cstdint>

// Performance degradation level output by PerformanceGuard.
//   0 = full quality (FPS >= 55)
//   1 = light     (FPS 40-55)
//   2 = medium    (FPS 30-40)
//   3 = heavy     (FPS < 30)
//   4 = critical  (FPS < 24 sustained for the degradation window)
enum class PerfLevel : int32_t {
  Full = 0,
  Light = 1,
  Medium = 2,
  Heavy = 3,
  Critical = 4,
};

class PerformanceGuard {
 public:
  // Record one FPS sample. The guard keeps a 2-second sliding window.
  void sample(Tick tick, int64_t dtMs, float fps);

  // Current degradation level based on the sliding window average.
  int32_t level() const { return static_cast<int32_t>(level_); }

  // 流式场景视距缩放：降级越高视距越短，减少激活分块与绘制量。
  float viewDistanceScale() const {
    switch (level_) {
      case PerfLevel::Full:
        return 1.0f;
      case PerfLevel::Light:
        return 0.9f;
      case PerfLevel::Medium:
        return 0.75f;
      case PerfLevel::Heavy:
        return 0.6f;
      case PerfLevel::Critical:
        return 0.45f;
    }
    return 1.0f;
  }

  // 环境细节 LOD 档位：0=完整 1=中等 2=精简，供环境渲染选择细节层。
  int32_t lodLevel() const {
    if (level_ >= PerfLevel::Heavy) return 2;
    if (level_ >= PerfLevel::Medium) return 1;
    return 0;
  }

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
  Tick sub24Since_ = 0;
  bool sub24Active_ = false;
};
