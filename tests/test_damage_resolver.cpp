#include "../native/gameplay/combat/damage_resolver.h"

#include <cassert>

namespace {
HitRequest basicHit(FixedPoint hp, FixedPoint poise, Tick tick = 0) {
  HitRequest hit;
  hit.baseDamage = hp;
  hit.poiseDamage = poise;
  hit.tick = tick;
  return hit;
}
}

int main() {
  const CombatConfig config = CombatConfig::defaults();
  DamageResolver resolver(config);

  TrainingTarget target = TrainingTarget::defaults();
  const DamageOutcome basic = resolver.resolve(target, basicHit(fp(8), fp(4)));
  assert(basic.hpDamage == fp(8));
  assert(basic.poiseDamage == fp(4));
  assert(target.hp() == fp(292));
  assert(target.poise() == fp(96));

  TrainingTarget broken = TrainingTarget::defaults();
  const DamageOutcome breaking = resolver.resolve(broken, basicHit(fp(1), fp(100), 10));
  assert(breaking.poiseBroken);
  assert(broken.poise() == 0);
  assert(broken.weakUntil() == 3010);
  assert(resolver.resolve(broken, basicHit(fp(8), 0, 20)).hpDamage == fp(10));

  broken.advance(3010);
  assert(broken.poise() == fp(100));
  assert(broken.weakUntil() == 0);
  assert(resolver.resolve(broken, basicHit(fp(8), 0, 3010)).hpDamage == fp(8));

  TrainingTarget refractionOnly = TrainingTarget::defaults();
  refractionOnly.applyPoiseDamage(fp(10), 0);
  refractionOnly.applyWeakness(3000, fp(1.2));
  refractionOnly.advance(3000);
  assert(refractionOnly.poise() == fp(90));

  TrainingTarget dead = TrainingTarget::defaults();
  resolver.resolve(dead, basicHit(fp(300), 0, 100));
  assert(!dead.alive());
  assert(dead.deathResetAt() == 2100);
  dead.advance(2099);
  assert(!dead.alive());
  dead.advance(10000);
  assert(dead.alive());
  assert(dead.hp() == fp(300));
  assert(dead.poise() == fp(100));
  assert(dead.weakUntil() == 0);
  return 0;
}
