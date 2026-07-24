#include "performance_guard.h"

namespace {

// 2-second window in milliseconds.
constexpr int64_t kWindowMs = 2000;
constexpr float kCriticalFps = 24.0f;

PerfLevel fpsToLevel(float avgFps) {
  if (avgFps >= 55.0f) return PerfLevel::Full;
  if (avgFps >= 40.0f) return PerfLevel::Light;
  if (avgFps >= 30.0f) return PerfLevel::Medium;
  return PerfLevel::Heavy;
}

}  // namespace

void PerformanceGuard::sample(Tick tick, int64_t /*dtMs*/, float fps) {
  samples_[writeIndex_].tick = tick;
  samples_[writeIndex_].fps = fps;
  writeIndex_ = (writeIndex_ + 1) % kWindowCapacity;
  if (sampleCount_ < kWindowCapacity) ++sampleCount_;
  recompute(tick);
}

void PerformanceGuard::recompute(Tick now) {
  if (sampleCount_ == 0) {
    level_ = PerfLevel::Full;
    return;
  }

  // Average FPS over samples within the 2-second window.
  float sum = 0.0f;
  int count = 0;
  for (int i = 0; i < sampleCount_; ++i) {
    const Sample& s = samples_[i];
    if (now > s.tick && (now - s.tick) > kWindowMs) continue;
    sum += s.fps;
    ++count;
  }

  if (count == 0) {
    level_ = PerfLevel::Full;
    return;
  }

  const float avgFps = sum / static_cast<float>(count);
  if (avgFps < kCriticalFps) {
    if (!sub24Active_) {
      sub24Active_ = true;
      sub24Since_ = now;
    }
    if (level_ == PerfLevel::Heavy &&
        now - sub24Since_ >= kWindowMs) {
      level_ = PerfLevel::Critical;
    } else if (level_ != PerfLevel::Critical) {
      level_ = PerfLevel::Heavy;
    }
    return;
  }

  sub24Active_ = false;
  if (level_ == PerfLevel::Critical) {
    level_ = PerfLevel::Heavy;
    return;
  }
  level_ = fpsToLevel(avgFps);
}
