#pragma once
#include "../combat/event.h"
struct Enemy { EntityId id; FixedPoint hp, poise; SourceType resist;
  void takeHit(const HitEvent& h){ hp -= h.baseDamage; poise -= fp(2); } };
