#include "native/gameplay/growth/character_growth.h"

#include <cassert>

int main() {
  // 图鉴：6 名角色，五星三名、四星三名。
  const std::vector<CharacterDef>& roster = CharacterGrowth::roster();
  assert(roster.size() == 6);
  int fiveStars = 0;
  for (const CharacterDef& def : roster) {
    if (def.rarity == 5) ++fiveStars;
    assert(!def.name.empty());
    assert(CharacterGrowth::characterDef(def.id) != nullptr);
  }
  assert(fiveStars == 3);
  assert(CharacterGrowth::characterDef(999) == nullptr);

  // 获得角色：新增成功，重复失败，未知 id 失败。
  CharacterGrowth growth;
  assert(growth.addCharacter(1));
  assert(!growth.addCharacter(1));
  assert(!growth.addCharacter(999));
  assert(growth.owns(1));
  assert(!growth.owns(2));

  // 初始状态：1 级、0 突破。
  const OwnedCharacter* hero = growth.find(1);
  assert(hero != nullptr);
  assert(hero->level == 1);
  assert(hero->ascension == 0);
  assert(hero->exp == 0);

  // 经验曲线与等级上限。
  assert(CharacterGrowth::expRequired(1) == 30);
  assert(CharacterGrowth::expRequired(2) == 40);
  assert(CharacterGrowth::levelCap(0) == 20);
  assert(CharacterGrowth::levelCap(3) == 80);
  assert(CharacterGrowth::levelCap(99) == 80);

  // 升级：30 经验恰好升到 2 级。
  assert(growth.addExp(1, 30) == 1);
  hero = growth.find(1);
  assert(hero->level == 2);
  assert(hero->exp == 0);

  // 级联升级：一次注入大量经验（2→20 级需 2250）。
  const int32_t levels = growth.addExp(1, 3000);
  assert(levels >= 5);
  hero = growth.find(1);
  // 未突破时封顶 20 级且经验清零。
  assert(hero->level == 20);
  assert(hero->exp == 0);

  // 封顶后继续注入不再升级。
  assert(growth.addExp(1, 1000) == 0);
  assert(growth.find(1)->level == 20);

  // 突破：等级达标才能突破；突破后上限提升。
  assert(growth.ascend(1));
  hero = growth.find(1);
  assert(hero->ascension == 1);
  assert(CharacterGrowth::levelCap(hero->ascension) == 40);
  // 未达上限不能再突破。
  assert(!growth.ascend(1));

  // 未拥有角色无法养成。
  assert(growth.addExp(2, 100) == 0);
  assert(!growth.ascend(2));

  // 突破到最高阶段后不可再突破。
  growth.addCharacter(3);
  for (int32_t ascension = 0; ascension < CharacterGrowth::kMaxAscension;
       ++ascension) {
    growth.addExp(3, 100000);
    assert(growth.ascend(3));
  }
  growth.addExp(3, 100000);
  assert(growth.find(3)->level == 80);
  assert(!growth.ascend(3));

  // 拥有列表按 id 升序。
  for (size_t i = 1; i < growth.owned().size(); ++i) {
    assert(growth.owned()[i].characterId > growth.owned()[i - 1].characterId);
  }

  // 打怪升级闭环注入路径（与 loop.cpp 击杀掉落一致）：
  // 普通敌人每只 20 经验、Boss 300 经验，经验书 1000/5000/20000，
  // 均可经 addExp 级联升级；世界等级倍率按整数折算后注入。
  CharacterGrowth combatGrowth;
  assert(combatGrowth.addCharacter(1));
  for (int kill = 0; kill < 5; ++kill) {
    const int32_t dropExp = 20 * 125 / 100;  // 世界等级 1 倍率折算。
    (void)combatGrowth.addExp(1, dropExp);
  }
  assert(combatGrowth.find(1)->level >= 2);
  (void)combatGrowth.addExp(1, 300);  // Boss 击杀。
  const int32_t levelBeforeBook = combatGrowth.find(1)->level;
  (void)combatGrowth.addExp(1, 1000);   // 流浪者笔记。
  (void)combatGrowth.addExp(1, 5000);   // 冒险家手册。
  assert(combatGrowth.find(1)->level > levelBeforeBook);

  // 派生属性（优化）：确定性公式，五星加成、随等级/突破增长、突破钳制。
  assert(CharacterGrowth::hpFor(1, 1, 0) == 800 + 40 + 200);
  assert(CharacterGrowth::atkFor(1, 1, 0) == 60 + 6 + 15);
  assert(CharacterGrowth::hpFor(4, 20, 1) == 800 + 800 + 300);   // 四星无加成。
  assert(CharacterGrowth::atkFor(4, 20, 1) == 60 + 120 + 25);
  assert(CharacterGrowth::hpFor(1, 10, 1) > CharacterGrowth::hpFor(1, 1, 0));
  assert(CharacterGrowth::atkFor(1, 10, 1) > CharacterGrowth::atkFor(1, 1, 0));
  // 突破超限钳制到最高阶段。
  assert(CharacterGrowth::hpFor(1, 1, 99) == CharacterGrowth::hpFor(1, 1, 3));

  // 命之座（抽卡闭环）：重复抽取转化，最高 6 层，提供属性加成。
  CharacterGrowth cons;
  cons.addCharacter(2);
  assert(cons.find(2)->constellation == 0);
  assert(cons.boostConstellation(2));
  assert(cons.find(2)->constellation == 1);
  assert(!cons.boostConstellation(99));
  assert(CharacterGrowth::hpFor(2, 1, 0, 1) ==
         CharacterGrowth::hpFor(2, 1, 0, 0) + 60);
  assert(CharacterGrowth::atkFor(2, 1, 0, 1) ==
         CharacterGrowth::atkFor(2, 1, 0, 0) + 8);
  assert(CharacterGrowth::hpFor(2, 1, 0, 99) ==
         CharacterGrowth::hpFor(2, 1, 0, 6));
  for (int i = 0; i < 5; ++i) {
    assert(cons.boostConstellation(2));
  }
  assert(cons.find(2)->constellation == 6);
  assert(!cons.boostConstellation(2));
  return 0;
}
