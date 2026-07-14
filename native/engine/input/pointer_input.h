#pragma once
#include "input_event.h"
#include <cmath>
#include <cstdint>
#include <limits>

inline bool TryConvertInt32(double value, int32_t& result) {
  if (!std::isfinite(value) || value < std::numeric_limits<int32_t>::min() ||
      value > std::numeric_limits<int32_t>::max()) {
    return false;
  }
  const int32_t converted = static_cast<int32_t>(value);
  if (value != static_cast<double>(converted)) return false;
  result = converted;
  return true;
}

inline bool TryConvertFloat(double value, float& result) {
  if (!std::isfinite(value) || value < -std::numeric_limits<float>::max() ||
      value > std::numeric_limits<float>::max()) {
    return false;
  }
  result = static_cast<float>(value);
  return true;
}

inline bool TryMapPointerAction(int32_t type, InputAction& action) {
  switch (type) {
    case 0: action = InputAction::PointerDown; return true;
    case 1: action = InputAction::PointerUp; return true;
    case 2: action = InputAction::PointerMove; return true;
    case 3: action = InputAction::PointerCancel; return true;
    default: return false;
  }
}
