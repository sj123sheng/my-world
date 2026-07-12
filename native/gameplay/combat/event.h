#pragma once
#include "../../../native/engine/core/tick_clock.h"
#include <vector>
enum class SourceType { Radiance, Current, Corruption };
enum class ResonanceType { Refraction, Stasis, Collapse, Burst }; // 折光/凝滞/崩解/共鸣爆发
using EntityId = uint32_t;
using AbilityId = uint32_t;
struct SourceAura { SourceType type; FixedPoint amount; Tick expireAt; EntityId applier; };
struct HitEvent { EntityId attacker, target; AbilityId ability; SourceType source;
  FixedPoint sourceAmount, baseDamage; Tick tick; uint32_t sequence;
  bool operator==(const HitEvent& o) const { return attacker==o.attacker && target==o.target
    && ability==o.ability && source==o.source && sourceAmount==o.sourceAmount
    && baseDamage==o.baseDamage && tick==o.tick && sequence==o.sequence; } };
struct CombatResult { FixedPoint damage, poiseDamage; ResonanceType resonance;
  std::vector<int> gameplayEvents; std::vector<int> presentationEvents;
  bool operator==(const CombatResult& o) const { return damage==o.damage
    && poiseDamage==o.poiseDamage && resonance==o.resonance
    && gameplayEvents==o.gameplayEvents && presentationEvents==o.presentationEvents; } };
