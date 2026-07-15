#pragma once
#include "input_event.h"
#include <cmath>
#include <cstdint>
#include <unordered_map>

enum class TouchRole {
  None,
  Movement,
  Camera,
  Ignored,
};

class TouchRouter {
 public:
  bool handle(const InputEvent& event, float width, float height) {
    if (width <= 0.0f || height <= 0.0f || !std::isfinite(width) ||
        !std::isfinite(height) || !std::isfinite(event.x) ||
        !std::isfinite(event.y)) {
      return false;
    }

    switch (event.action) {
      case InputAction::PointerDown:
        if (roles_.find(event.pointerId) != roles_.end()) return false;
        roles_[event.pointerId] = assignRole(event.x < width * 0.5f);
        return true;
      case InputAction::PointerMove:
        return roles_.find(event.pointerId) != roles_.end();
      case InputAction::PointerUp:
      case InputAction::PointerCancel:
        return roles_.erase(event.pointerId) > 0;
      default:
        return false;
    }
  }

  TouchRole role(int32_t pointerId) const {
    const auto found = roles_.find(pointerId);
    return found == roles_.end() ? TouchRole::None : found->second;
  }

  void clear() { roles_.clear(); }

 private:
  TouchRole assignRole(bool left) const {
    const TouchRole desired = left ? TouchRole::Movement : TouchRole::Camera;
    for (const auto& entry : roles_) {
      if (entry.second == desired) return TouchRole::Ignored;
    }
    return desired;
  }

  std::unordered_map<int32_t, TouchRole> roles_;
};
