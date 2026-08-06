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
  // 探索动作：跳跃、交互（锚点/物件）、滑翔按下与松开。
  Jump,
  Interact,
  GlidePress,
  GlideRelease,
  // 队伍动作：循环切换出战角色。
  SwitchCharacter,
};

struct InputEvent {
  InputAction action = InputAction::PointerCancel;
  int32_t pointerId = -1;
  float x = 0.0f;
  float y = 0.0f;
  uint64_t sequence = 0;
};
