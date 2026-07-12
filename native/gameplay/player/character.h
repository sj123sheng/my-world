#pragma once
#include "../combat/source_aura.h"
#include "../combat/event.h"
struct Character { EntityId id; FixedPoint hp, poise; SourceAuraContainer auras;
  CombatResult castAbility(AbilityId ab, EntityId target, SourceType s, FixedPoint amt, Tick t, uint32_t seq); };
