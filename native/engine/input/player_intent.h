#pragma once
#include "../math/vec2.h"
#include "../../gameplay/combat/combat_action.h"

#include <vector>

struct PlayerIntent {
  Vec2 move;
  Vec2 lookDelta;
  bool softLock = true;
  std::vector<ActionRequest> actions;
};
