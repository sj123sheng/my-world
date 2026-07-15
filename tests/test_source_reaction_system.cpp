#include "../native/gameplay/combat/source_reaction_system.h"

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
  assert(refractionTarget.sourceAuras().active().size() == 1);
  assert(refractionTarget.sourceAuras().active()[0].type == SourceType::Current);

  // 附着属于目标，不能通过同一个反应系统跨目标泄漏。
  TrainingTarget isolatedA = TrainingTarget::defaults();
  TrainingTarget isolatedB = TrainingTarget::defaults();
  SourceReactionSystem isolated(config);
  isolated.apply(isolatedA, SourceType::Radiance, fp(1), 0, 1);
  assert(!isolated.apply(isolatedB, SourceType::Current, fp(1), 1, 2).type);
  assert(isolated.apply(isolatedA, SourceType::Current, fp(1), 2, 3).type ==
         ResonanceType::Refraction);

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
  assert(refreshTarget.sourceAuras().active().size() == 1);
  assert(refreshTarget.sourceAuras().active()[0].amount == fp(2));
  assert(refreshTarget.sourceAuras().active()[0].expireAt == 6005);
  assert(!refresh.apply(refreshTarget, SourceType::Current, fp(1), 6005, 3).type);

  // 多候选固定按枚举顺序选择 Radiance+Current=Refraction，且只结算一次。
  TrainingTarget oneTarget = TrainingTarget::defaults();
  SourceReactionSystem one(config);
  oneTarget.sourceAuras().apply({SourceType::Corruption, fp(1), 6000, 1});
  oneTarget.sourceAuras().apply({SourceType::Radiance, fp(1), 6000, 1});
  const ReactionOutcome only = one.apply(oneTarget, SourceType::Current, fp(1), 2, 3);
  assert(only.type == ResonanceType::Refraction);
  assert(only.hpDamage == fp(12));
  assert(only.poiseDamage == 0);
  assert(oneTarget.hp() == fp(288));
  assert(oneTarget.sourceAuras().active().size() == 1);
  assert(oneTarget.sourceAuras().active()[0].type == SourceType::Current);

  // 死亡复位清空未过期附着，复活后不能和旧源反应。
  TrainingTarget revivedTarget = TrainingTarget::defaults();
  SourceReactionSystem revived(config);
  revived.apply(revivedTarget, SourceType::Radiance, fp(1), 0, 1);
  HitRequest lethal;
  lethal.baseDamage = fp(300);
  lethal.tick = 10;
  DamageResolver(config).resolve(revivedTarget, lethal);
  revivedTarget.advance(2010);
  assert(revivedTarget.alive());
  assert(revivedTarget.sourceAuras().active().empty());
  assert(!revived.apply(revivedTarget, SourceType::Current, fp(1), 2011, 2).type);

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

  TrainingTarget corrosionTarget = TrainingTarget::defaults();
  SourceReactionSystem corrosion(config);
  corrosion.apply(corrosionTarget, SourceType::Corruption, fp(1), 100, 1);
  assert(corrosionTarget.corroded());
  corrosion.apply(corrosionTarget, SourceType::Radiance, fp(1), 200, 2);
  assert(corrosionTarget.corroded());
  corrosionTarget.advance(6099);
  assert(corrosionTarget.corroded());
  corrosionTarget.advance(6100);
  assert(!corrosionTarget.corroded());
  corrosionTarget.reset();
  assert(!corrosionTarget.corroded());
  return 0;
}
