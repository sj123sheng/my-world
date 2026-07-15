#pragma once
#include "../math/vec2.h"

struct PlayerIntent {
  Vec2 move;
  Vec2 lookDelta;
  bool softLock = true;
};
