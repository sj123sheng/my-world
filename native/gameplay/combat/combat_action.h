#pragma once

#include "../../engine/input/input_event.h"

#include <cstdint>

enum class CombatAction : uint8_t {
  Attack,
  Dodge,
  Radiance,
  Current,
  Corruption,
  Ultimate,
};

enum class ActionState : uint8_t {
  Idle,
  Attack1,
  Attack2,
  Attack3,
  Attack4,
  Dodging,
  CastingSource,
  CastingUltimate,
};

enum class ActionRejectReason : uint8_t {
  None,
  NoTarget,
  TargetDead,
  Cooldown,
  InsufficientStamina,
  InsufficientResonance,
  ActionLocked,
  InvalidAction,
};

struct ActionRequest {
  CombatAction action = CombatAction::Attack;
  uint64_t sequence = 0;
};

inline bool TryMapCombatAction(InputAction input, CombatAction& output) {
  switch (input) {
    case InputAction::Attack:
      output = CombatAction::Attack;
      return true;
    case InputAction::Dodge:
      output = CombatAction::Dodge;
      return true;
    case InputAction::Radiance:
      output = CombatAction::Radiance;
      return true;
    case InputAction::Current:
      output = CombatAction::Current;
      return true;
    case InputAction::Corruption:
      output = CombatAction::Corruption;
      return true;
    case InputAction::Ultimate:
      output = CombatAction::Ultimate;
      return true;
    default:
      return false;
  }
}
