#pragma once
#include "pointer_input.h"
#include <cstdint>

template <typename Sink>
bool ForwardChangedPointer(int32_t type, int32_t pointerId, float x, float y,
                           Sink&& sink) {
  InputAction action = InputAction::PointerCancel;
  if (!TryMapPointerAction(type, action)) return false;
  return sink(action, pointerId, x, y);
}
