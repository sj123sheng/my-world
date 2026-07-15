#pragma once

#include "combat_action.h"
#include "../../engine/core/tick_clock.h"

#include <array>
#include <cstddef>

struct CombatConfig {
  std::array<FixedPoint, 4> comboDamage{fp(8), fp(10), fp(12), fp(18)};
  std::array<FixedPoint, 4> comboPoiseDamage{fp(4), fp(5), fp(6), fp(10)};
  Tick comboWindowMs = 480;

  FixedPoint maxStamina = fp(100);
  FixedPoint dodgeCost = fp(30);
  Tick staminaRecoveryDelayMs = 500;
  FixedPoint staminaRecoveryPerSecond = fp(20);
  Tick dodgeDurationMs = 300;
  Tick dodgeInvulnerabilityMs = 200;
  Tick preciseDodgeWindowMs = 100;
  Tick insightDurationMs = 5000;
  FixedPoint insightDamageMultiplier = fp(1.5);

  std::array<Tick, 3> sourceCooldownMs{3000, 4000, 5000};
  std::array<FixedPoint, 3> sourceDamage{fp(20), fp(16), fp(12)};
  std::array<FixedPoint, 3> sourcePoiseDamage{fp(6), fp(8), fp(18)};
  Tick sourceAuraDurationMs = 6000;

  FixedPoint refractionDamage = fp(12);
  Tick weakDurationMs = 3000;
  FixedPoint weakDamageMultiplier = fp(1.2);
  Tick stagnationDurationMs = 4000;
  FixedPoint stagnationPoiseMultiplier = fp(1.5);
  FixedPoint disintegrationPoiseDamage = fp(30);
  FixedPoint disintegrationBreakDamage = fp(20);
  FixedPoint sourceResonanceGain = fp(10);
  FixedPoint reactionResonanceGain = fp(20);
  FixedPoint maxResonance = fp(100);
  Tick triSourceWindowMs = 8000;
  Tick ultimateWindowMs = 5000;
  FixedPoint ultimateDamage = fp(60);
  FixedPoint ultimatePoiseDamage = fp(40);

  FixedPoint trainingTargetHp = fp(300);
  FixedPoint trainingTargetPoise = fp(100);
  Tick trainingBreakDurationMs = 3000;
  FixedPoint trainingBreakDamageMultiplier = fp(1.25);
  Tick trainingDeathResetMs = 2000;
  FixedPoint trainingPlayerHp = fp(100);
  FixedPoint trainingPlayerPoise = fp(100);
  Tick trainingPulsePeriodMs = 3000;
  Tick trainingPulseWarningMs = 800;
  FixedPoint trainingPulseDamage = fp(10);

  static CombatConfig defaults() { return {}; }

  CombatConfig validated() const {
    CombatConfig safe = *this;
    const CombatConfig fallback = defaults();

    if (!allPositive(comboDamage) || !allPositive(comboPoiseDamage) || comboWindowMs < 0) {
      safe.comboDamage = fallback.comboDamage;
      safe.comboPoiseDamage = fallback.comboPoiseDamage;
      safe.comboWindowMs = fallback.comboWindowMs;
    }

    if (maxStamina <= 0 || dodgeCost < 0 || dodgeCost > maxStamina ||
        staminaRecoveryDelayMs < 0 || staminaRecoveryPerSecond <= 0 || dodgeDurationMs < 0 ||
        dodgeInvulnerabilityMs < 0 || dodgeInvulnerabilityMs > dodgeDurationMs ||
        preciseDodgeWindowMs < 0 || insightDurationMs < 0 || insightDamageMultiplier < FP_ONE) {
      safe.maxStamina = fallback.maxStamina;
      safe.dodgeCost = fallback.dodgeCost;
      safe.staminaRecoveryDelayMs = fallback.staminaRecoveryDelayMs;
      safe.staminaRecoveryPerSecond = fallback.staminaRecoveryPerSecond;
      safe.dodgeDurationMs = fallback.dodgeDurationMs;
      safe.dodgeInvulnerabilityMs = fallback.dodgeInvulnerabilityMs;
      safe.preciseDodgeWindowMs = fallback.preciseDodgeWindowMs;
      safe.insightDurationMs = fallback.insightDurationMs;
      safe.insightDamageMultiplier = fallback.insightDamageMultiplier;
    }

    if (!allNonNegative(sourceCooldownMs) || !allPositive(sourceDamage) ||
        !allPositive(sourcePoiseDamage) || sourceAuraDurationMs < 0) {
      safe.sourceCooldownMs = fallback.sourceCooldownMs;
      safe.sourceDamage = fallback.sourceDamage;
      safe.sourcePoiseDamage = fallback.sourcePoiseDamage;
      safe.sourceAuraDurationMs = fallback.sourceAuraDurationMs;
    }

    if (refractionDamage < 0 || weakDurationMs < 0 || weakDamageMultiplier < FP_ONE ||
        stagnationDurationMs < 0 || stagnationPoiseMultiplier < FP_ONE ||
        disintegrationPoiseDamage < 0 || disintegrationBreakDamage < 0 ||
        sourceResonanceGain < 0 || reactionResonanceGain < 0 || maxResonance <= 0 ||
        sourceResonanceGain > maxResonance || reactionResonanceGain > maxResonance ||
        triSourceWindowMs < 0 || ultimateWindowMs < 0 || ultimateDamage < 0 ||
        ultimatePoiseDamage < 0) {
      safe.refractionDamage = fallback.refractionDamage;
      safe.weakDurationMs = fallback.weakDurationMs;
      safe.weakDamageMultiplier = fallback.weakDamageMultiplier;
      safe.stagnationDurationMs = fallback.stagnationDurationMs;
      safe.stagnationPoiseMultiplier = fallback.stagnationPoiseMultiplier;
      safe.disintegrationPoiseDamage = fallback.disintegrationPoiseDamage;
      safe.disintegrationBreakDamage = fallback.disintegrationBreakDamage;
      safe.sourceResonanceGain = fallback.sourceResonanceGain;
      safe.reactionResonanceGain = fallback.reactionResonanceGain;
      safe.maxResonance = fallback.maxResonance;
      safe.triSourceWindowMs = fallback.triSourceWindowMs;
      safe.ultimateWindowMs = fallback.ultimateWindowMs;
      safe.ultimateDamage = fallback.ultimateDamage;
      safe.ultimatePoiseDamage = fallback.ultimatePoiseDamage;
    }

    if (trainingTargetHp <= 0 || trainingTargetPoise <= 0 || trainingBreakDurationMs < 0 ||
        trainingBreakDamageMultiplier < FP_ONE || trainingDeathResetMs < 0 ||
        trainingPlayerHp <= 0 || trainingPlayerPoise <= 0 || trainingPulsePeriodMs <= 0 ||
        trainingPulseWarningMs < 0 || trainingPulseWarningMs > trainingPulsePeriodMs ||
        trainingPulseDamage < 0) {
      safe.trainingTargetHp = fallback.trainingTargetHp;
      safe.trainingTargetPoise = fallback.trainingTargetPoise;
      safe.trainingBreakDurationMs = fallback.trainingBreakDurationMs;
      safe.trainingBreakDamageMultiplier = fallback.trainingBreakDamageMultiplier;
      safe.trainingDeathResetMs = fallback.trainingDeathResetMs;
      safe.trainingPlayerHp = fallback.trainingPlayerHp;
      safe.trainingPlayerPoise = fallback.trainingPlayerPoise;
      safe.trainingPulsePeriodMs = fallback.trainingPulsePeriodMs;
      safe.trainingPulseWarningMs = fallback.trainingPulseWarningMs;
      safe.trainingPulseDamage = fallback.trainingPulseDamage;
    }
    return safe;
  }

 private:
  template <typename T, std::size_t N>
  static bool allPositive(const std::array<T, N>& values) {
    for (const T value : values) {
      if (value <= 0) return false;
    }
    return true;
  }

  template <typename T, std::size_t N>
  static bool allNonNegative(const std::array<T, N>& values) {
    for (const T value : values) {
      if (value < 0) return false;
    }
    return true;
  }
};
