#pragma once
#include "engine/core/tick_clock.h"
#include <optional>
#include <vector>
enum class SourceType { Radiance, Current, Corruption };
enum class ResonanceType { Refraction, Stasis, Collapse, Burst }; // 折光/凝滞/崩解/共鸣爆发
using EntityId = uint32_t;
using AbilityId = uint32_t;
struct SourceAura { SourceType type; FixedPoint amount; Tick expireAt; EntityId applier; };
struct HitEvent { EntityId attacker, target; AbilityId ability; SourceType source;
  FixedPoint sourceAmount, baseDamage; Tick tick; uint64_t sequence;
  bool operator==(const HitEvent& o) const { return attacker==o.attacker && target==o.target
    && ability==o.ability && source==o.source && sourceAmount==o.sourceAmount
    && baseDamage==o.baseDamage && tick==o.tick && sequence==o.sequence; } };

struct HitRequest {
  EntityId attacker = 0;
  EntityId target = 0;
  AbilityId ability = 0;
  std::optional<SourceType> source;
  FixedPoint baseDamage = 0;
  FixedPoint poiseDamage = 0;
  Tick tick = 0;
  uint64_t sequence = 0;
  FixedPoint sourceAmount = FP_ONE;
  uint64_t transactionId = 0;
};

enum class CombatEffectType : uint8_t {
  Shield,
};

struct CombatEffectRequest {
  EntityId target = 0;
  CombatEffectType type = CombatEffectType::Shield;
  FixedPoint amount = 0;
  Tick tick = 0;
  uint64_t sequence = 0;
  uint64_t transactionId = 0;
};

enum class GameplayEventType : uint8_t {
  Hit,
  Damage,
  Dodge,
  Interrupt,
  PoiseBreak,
  AuraApplied,
  Resonance,
  PhaseChanged,
  Death,
  EncounterReset,
};

enum class PresentationEventType : uint8_t {
  HitFlash,
  CameraShake,
  DodgeFlash,
  CastBarBroken,
  PoiseBreakBurst,
  ResonanceBurst,
  PhaseTransition,
};

struct GameplayEvent {
  Tick tick = 0;
  EntityId source = 0;
  EntityId target = 0;
  GameplayEventType type = GameplayEventType::Hit;
  FixedPoint value = 0;
  uint64_t sequence = 0;
  bool operator==(const GameplayEvent& other) const {
    return tick == other.tick && source == other.source && target == other.target &&
           type == other.type && value == other.value && sequence == other.sequence;
  }
};

struct PresentationEvent {
  Tick tick = 0;
  EntityId source = 0;
  EntityId target = 0;
  PresentationEventType type = PresentationEventType::HitFlash;
  FixedPoint intensity = 0;
  uint64_t sequence = 0;
  bool operator==(const PresentationEvent& other) const {
    return tick == other.tick && source == other.source && target == other.target &&
           type == other.type && intensity == other.intensity && sequence == other.sequence;
  }
};

struct CombatResult { FixedPoint damage, poiseDamage; ResonanceType resonance;
  std::vector<GameplayEvent> gameplayEvents; std::vector<PresentationEvent> presentationEvents;
  bool operator==(const CombatResult& o) const { return damage==o.damage
    && poiseDamage==o.poiseDamage && resonance==o.resonance
    && gameplayEvents==o.gameplayEvents && presentationEvents==o.presentationEvents; } };
