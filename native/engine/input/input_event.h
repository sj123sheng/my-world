#pragma once
#include <cstdint>

enum class InputAction : uint8_t {
  PointerDown,
  PointerMove,
  PointerUp,
  PointerCancel,
  Attack,
  Dodge,
  Radiance,
  Current,
  Corruption,
  Ultimate,
};

struct InputEvent {
  InputAction action = InputAction::PointerCancel;
  int32_t pointerId = -1;
  float x = 0.0f;
  float y = 0.0f;
  uint64_t sequence = 0;
};
