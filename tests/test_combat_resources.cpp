#include "../native/gameplay/combat/combat_resources.h"

#include <cassert>
#include <limits>

int main() {
  CombatResources resources(CombatConfig::defaults());
  assert(resources.stamina() == fp(100));
  assert(resources.spendStamina(fp(30), 0));
  assert(resources.stamina() == fp(70));
  resources.advance(499);
  assert(resources.stamina() == fp(70));
  resources.advance(999);
  assert(resources.stamina() == fp(80));

  CombatResources insufficient(CombatConfig::defaults());
  assert(insufficient.spendStamina(fp(30), 0));
  assert(insufficient.spendStamina(fp(30), 0));
  assert(insufficient.spendStamina(fp(30), 0));
  assert(!insufficient.spendStamina(fp(30), 0));
  assert(insufficient.stamina() == fp(10));

  CombatResources insightBoundary(CombatConfig::defaults());
  insightBoundary.grantInsight(1200);
  assert(insightBoundary.consumeInsight(16199));
  CombatResources insightExpired(CombatConfig::defaults());
  insightExpired.grantInsight(1200);
  assert(!insightExpired.consumeInsight(16200));

  CombatResources insightOnce(CombatConfig::defaults());
  insightOnce.grantInsight(50);
  assert(insightOnce.consumeInsight(50));
  assert(!insightOnce.consumeInsight(50));

  insufficient.grantInsight(0);
  insufficient.reset();
  assert(insufficient.stamina() == fp(100));
  assert(insufficient.insightRemainingMs() == 0);
  insufficient.advance(999);
  assert(insufficient.stamina() == fp(100));

  CombatConfig zeroDelay = CombatConfig::defaults();
  zeroDelay.staminaRecoveryDelayMs = 0;
  CombatResources atMaximum(zeroDelay);
  const Tick maximum = std::numeric_limits<Tick>::max();
  assert(atMaximum.spendStamina(fp(30), maximum));
  atMaximum.advance(maximum);
  assert(atMaximum.stamina() >= fp(70));
  assert(atMaximum.stamina() <= fp(100));
  return 0;
}
