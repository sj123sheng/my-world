#include "../native/gameplay/combat/source_reaction_system.h"

#include <array>
#include <cassert>

int main() {
  const CombatConfig config = CombatConfig::defaults();

  TrainingTarget refractionTarget = TrainingTarget::defaults();
  SourceReactionSystem refraction(config);
  assert(!refraction.apply(refractionTarget, SourceType::Radiance, fp(1), 0, 1).type);
  const ReactionOutcome refracted =
      refraction.apply(refractionTarget, SourceType::Current, fp(1), 10, 2);
  assert(refracted.type == ResonanceType::Refraction);
  assert(refracted.hpDamage == fp(12));
  assert(refractionTarget.weakUntil() == 3010);
  assert(refraction.activeAuras().size() == 1);
  assert(refraction.activeAuras()[0].type == SourceType::Current);

  // 组合顺序无关。
  TrainingTarget reverseTarget = TrainingTarget::defaults();
  SourceReactionSystem reverse(config);
  reverse.apply(reverseTarget, SourceType::Current, fp(1), 0, 1);
  assert(reverse.apply(reverseTarget, SourceType::Radiance, fp(1), 1, 2).type ==
         ResonanceType::Refraction);

  // 同源仅刷新，过期附着不参与反应。
  TrainingTarget refreshTarget = TrainingTarget::defaults();
  SourceReactionSystem refresh(config);
  refresh.apply(refreshTarget, SourceType::Radiance, fp(1), 0, 1);
  assert(!refresh.apply(refreshTarget, SourceType::Radiance, fp(2), 5, 2).type);
  assert(refresh.activeAuras().size() == 1);
  assert(refresh.activeAuras()[0].amount == fp(2));
  assert(refresh.activeAuras()[0].expireAt == 6005);
  assert(!refresh.apply(refreshTarget, SourceType::Current, fp(1), 6005, 3).type);

  // 一次命中最多消费一个旧附着，之后保留当前命中源。
  TrainingTarget oneTarget = TrainingTarget::defaults();
  SourceReactionSystem one(config);
  one.apply(oneTarget, SourceType::Radiance, fp(1), 0, 1);
  one.apply(oneTarget, SourceType::Corruption, fp(1), 1, 2);
  const ReactionOutcome only = one.apply(oneTarget, SourceType::Current, fp(1), 2, 3);
  assert(only.type.has_value());
  assert(one.activeAuras().size() == 1);
  assert(one.activeAuras()[0].type == SourceType::Current);

  // 凝滞持续 4 秒，韧性伤害 +50%。
  TrainingTarget stasisTarget = TrainingTarget::defaults();
  SourceReactionSystem stasis(config);
  stasis.apply(stasisTarget, SourceType::Current, fp(1), 100, 1);
  const ReactionOutcome frozen =
      stasis.apply(stasisTarget, SourceType::Corruption, fp(1), 110, 2);
  assert(frozen.type == ResonanceType::Stasis);
  assert(stasisTarget.stagnationUntil() == 4110);
  DamageResolver resolver(config);
  HitRequest poiseHit;
  poiseHit.poiseDamage = fp(10);
  poiseHit.tick = 200;
  assert(resolver.resolve(stasisTarget, poiseHit).poiseDamage == fp(15));
  stasisTarget.advance(5000);
  poiseHit.tick = 5000;
  assert(resolver.resolve(stasisTarget, poiseHit).poiseDamage == fp(10));

  // 崩解造成 30 韧伤；恰好破韧时追加 20 生命伤害。
  TrainingTarget collapseTarget = TrainingTarget::defaults();
  DamageResolver setup(config);
  HitRequest setupHit;
  setupHit.poiseDamage = fp(70);
  setup.resolve(collapseTarget, setupHit);
  SourceReactionSystem collapse(config);
  collapse.apply(collapseTarget, SourceType::Corruption, fp(1), 0, 1);
  const ReactionOutcome collapsed =
      collapse.apply(collapseTarget, SourceType::Radiance, fp(1), 10, 2);
  assert(collapsed.type == ResonanceType::Collapse);
  assert(collapsed.poiseDamage == fp(30));
  assert(collapsed.poiseBroken);
  assert(collapsed.hpDamage == fp(20));
  assert(collapseTarget.weakUntil() == 3010);
  return 0;
}
