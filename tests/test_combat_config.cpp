#include "native/gameplay/combat/combat_config.h"
#include "native/gameplay/combat/combat_action.h"

#include <array>
#include <cassert>

int main() {
  const CombatConfig defaults = CombatConfig::defaults();
  assert(defaults.comboDamage ==
         (std::array<FixedPoint, 4>{fp(8), fp(10), fp(12), fp(18)}));
  assert(defaults.comboPoiseDamage ==
         (std::array<FixedPoint, 4>{fp(4), fp(5), fp(6), fp(10)}));
  assert(defaults.comboWindowMs == 480);
  assert(defaults.dodgeCost == fp(30));
  assert(defaults.preciseDodgeWindowMinMs == 100);
  assert(defaults.preciseDodgeWindowMaxMs == 500);
  assert(defaults.insightDurationMs == 15000);
  assert(defaults.sourceCooldownMs == (std::array<Tick, 3>{3000, 4000, 5000}));
  assert(defaults.sourceDamage ==
         (std::array<FixedPoint, 3>{fp(20), fp(16), fp(12)}));
  assert(defaults.sourcePoiseDamage ==
         (std::array<FixedPoint, 3>{fp(6), fp(8), fp(18)}));
  assert(defaults.refractionDamage == fp(12));
  assert(defaults.maxResonance == fp(100));
  assert(defaults.trainingTargetHp == fp(300));
  assert(defaults.trainingPulsePeriodMs == 3000);

  CombatConfig invalid = defaults;
  invalid.maxStamina = 0;
  invalid.dodgeCost = fp(-1);
  const CombatConfig safe = invalid.validated();
  assert(safe.maxStamina == fp(100) && safe.dodgeCost == fp(30));

  invalid = defaults;
  invalid.preciseDodgeWindowMinMs = 501;
  invalid.preciseDodgeWindowMaxMs = 500;
  const CombatConfig safeDodgeWindow = invalid.validated();
  assert(safeDodgeWindow.preciseDodgeWindowMinMs == 100);
  assert(safeDodgeWindow.preciseDodgeWindowMaxMs == 500);

  invalid = defaults;
  invalid.comboWindowMs = -1;
  invalid.comboDamage[0] = fp(99);
  const CombatConfig safeCombo = invalid.validated();
  assert(safeCombo.comboWindowMs == 480);
  assert(safeCombo.comboDamage[0] == fp(8));

  invalid = defaults;
  invalid.sourceCooldownMs[1] = -1;
  invalid.sourceDamage[0] = fp(99);
  const CombatConfig safeSources = invalid.validated();
  assert(safeSources.sourceCooldownMs[1] == 4000);
  assert(safeSources.sourceDamage[0] == fp(20));

  invalid = defaults;
  invalid.weakDamageMultiplier = 0;
  invalid.refractionDamage = fp(99);
  const CombatConfig safeReaction = invalid.validated();
  assert(safeReaction.weakDamageMultiplier == fp(1.2));
  assert(safeReaction.refractionDamage == fp(12));

  invalid = defaults;
  invalid.trainingTargetHp = 0;
  invalid.trainingPulseDamage = fp(99);
  const CombatConfig safeTraining = invalid.validated();
  assert(safeTraining.trainingTargetHp == fp(300));
  assert(safeTraining.trainingPulseDamage == fp(10));

  CombatAction action{};
  assert(TryMapCombatAction(InputAction::Ultimate, action));
  assert(action == CombatAction::Ultimate);
  assert(!TryMapCombatAction(InputAction::PointerDown, action));
}
