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
  // 目标锁定（Plan 2）：循环切换锁定目标 / 解除锁定。只追加，
  // 不改动既有枚举数值，Bridge pushAction 11/12 一一映射。
  CycleTarget,
  ReleaseTargetLock,
};

struct InputEvent {
  InputAction action = InputAction::PointerCancel;
  int32_t pointerId = -1;
  float x = 0.0f;
  float y = 0.0f;
  uint64_t sequence = 0;
};
