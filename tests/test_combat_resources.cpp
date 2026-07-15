#include "../native/gameplay/combat/combat_resources.h"

#include <cassert>

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

  resources.grantInsight(1200);
  assert(resources.insightRemainingMs() == 5000);
  assert(resources.consumeInsight(6199));
  assert(!resources.consumeInsight(6200));
  assert(resources.insightRemainingMs() == 0);
  return 0;
}
