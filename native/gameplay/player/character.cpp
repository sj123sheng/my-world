#include "character.h"
CombatResult Character::castAbility(AbilityId ab, EntityId target, SourceType s, FixedPoint amt, Tick t, uint32_t seq){
  auras.apply({s, amt, t+100, id});
  return {fp(10), fp(2), ResonanceType::Refraction, {}, {}};
}
