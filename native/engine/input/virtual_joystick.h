#pragma once
#include "../math/vec2.h"
#include <cstdint>

struct VirtualJoystickConfig {
  float deadZone = 0.1f;
  float radius = 100.0f;
};

class VirtualJoystick {
 public:
  explicit VirtualJoystick(VirtualJoystickConfig config)
      : config_{std::isfinite(config.deadZone) && config.deadZone >= 0.0f
                    ? config.deadZone
                    : 0.0f,
                std::isfinite(config.radius) && config.radius > 0.0f
                    ? config.radius
                    : 0.0f} {}

  void begin(int32_t pointerId, Vec2 position) {
    pointerId_ = pointerId;
    origin_ = position;
    value_ = {};
  }

  void move(int32_t pointerId, Vec2 position) {
    if (pointerId != pointerId_) return;
    if (config_.radius <= 0.0f || !position.finite()) {
      value_ = {};
      return;
    }
    const Vec2 displacement = position - origin_;
    if (displacement.length() < config_.deadZone * config_.radius) {
      value_ = {};
      return;
    }
    value_ = ClampLength(displacement * (1.0f / config_.radius), 1.0f);
  }

  void end(int32_t pointerId) {
    if (pointerId != pointerId_) return;
    pointerId_ = -1;
    value_ = {};
  }

  void clear() {
    pointerId_ = -1;
    origin_ = {};
    value_ = {};
  }

  Vec2 value() const { return value_; }

 private:
  VirtualJoystickConfig config_;
  int32_t pointerId_ = -1;
  Vec2 origin_;
  Vec2 value_;
};
