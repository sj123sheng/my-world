#pragma once
#include "../math/vec2.h"
#include <cstdint>

struct CameraGestureConfig {
  float sensitivityX = 0.01f;
  float sensitivityY = 0.01f;
};

class CameraGesture {
 public:
  explicit CameraGesture(CameraGestureConfig config)
      : config_{std::isfinite(config.sensitivityX) ? config.sensitivityX : 0.0f,
                std::isfinite(config.sensitivityY) ? config.sensitivityY
                                                   : 0.0f} {}

  void begin(int32_t pointerId, Vec2 position) {
    pointerId_ = pointerId;
    previous_ = position;
    accumulated_ = {};
  }

  void move(int32_t pointerId, Vec2 position) {
    if (pointerId != pointerId_ || !position.finite()) return;
    const Vec2 delta = position - previous_;
    accumulated_ = accumulated_ +
                   Vec2{delta.x * config_.sensitivityX,
                        delta.y * config_.sensitivityY};
    previous_ = position;
  }

  void end(int32_t pointerId) {
    if (pointerId != pointerId_) return;
    pointerId_ = -1;
  }

  Vec2 consumeDelta() {
    const Vec2 result = accumulated_;
    accumulated_ = {};
    return result;
  }

 private:
  CameraGestureConfig config_;
  int32_t pointerId_ = -1;
  Vec2 previous_;
  Vec2 accumulated_;
};
