#include "native/gameplay/growth/weapon_system.h"

#include <cassert>

int main() {
  // 图鉴：5 把武器，五星三把、四星两把。
  const std::vector<WeaponDef>& catalog = WeaponSystem::catalog();
  assert(catalog.size() == 5);
  int fiveStars = 0;
  for (const WeaponDef& def : catalog) {
    if (def.rarity == 5) ++fiveStars;
    assert(!def.name.empty());
    assert(WeaponSystem::weaponDef(def.id) != nullptr);
  }
  assert(fiveStars == 3);
  assert(WeaponSystem::weaponDef(99) == nullptr);

  // 获得武器：新增成功，重复/未知失败。
  WeaponSystem weapons;
  assert(weapons.addWeapon(4));
  assert(!weapons.addWeapon(4));
  assert(!weapons.addWeapon(99));
  assert(weapons.owns(4));
  assert(weapons.find(4)->level == 1);
  assert(weapons.find(4)->ascension == 0);
  assert(weapons.find(4)->refine == 1);
  assert(weapons.find(4)->equippedBy == 0);

  // 攻击公式：五星成长 8，四星成长 5；精炼每阶 +10%。
  assert(WeaponSystem::atkOf(4, 1) == 40);
  assert(WeaponSystem::atkOf(4, 5) == 40 + 4 * 5);
  assert(WeaponSystem::atkOf(1, 5) == 48 + 4 * 8);
  assert(WeaponSystem::atkOf(4, 5, 3) == (40 + 4 * 5) * 120 / 100);
  assert(WeaponSystem::atkOf(4, 5, 5) == (40 + 4 * 5) * 140 / 100);
  assert(WeaponSystem::atkOf(4, 999) == WeaponSystem::atkOf(4, 90));
  assert(WeaponSystem::atkOf(99, 5) == 0);

  // 等级上限与经验曲线。
  assert(WeaponSystem::levelCap(0) == 20);
  assert(WeaponSystem::levelCap(1) == 40);
  assert(WeaponSystem::levelCap(2) == 50);
  assert(WeaponSystem::levelCap(3) == 60);
  assert(WeaponSystem::levelCap(4) == 70);
  assert(WeaponSystem::levelCap(5) == 80);
  assert(WeaponSystem::levelCap(6) == 90);
  assert(WeaponSystem::expRequired(1) == 200);
  assert(WeaponSystem::expRequired(2) == 300);
  assert(WeaponSystem::expRequired(90) == 0);

  // 金币强化（旧路径）：费用随等级递增；受突破上限约束。
  assert(WeaponSystem::upgradeCost(1) == 70);
  assert(WeaponSystem::upgradeCost(10) == 250);
  assert(weapons.upgrade(4));
  assert(weapons.find(4)->level == 2);
  assert(!weapons.upgrade(99));

  // 矿石强化：注入经验级联升级，到突破上限封顶清零。
  assert(weapons.addWeaponExp(4, 200) == 0);  // 2->3 需 300，经验暂存。
  assert(weapons.find(4)->exp == 200);
  assert(weapons.addWeaponExp(4, 100) == 1);  // 凑满升到 3 级。
  assert(weapons.find(4)->level == 3);
  assert(weapons.addWeaponExp(4, 1000000) >= 5);
  assert(weapons.find(4)->level == 20);
  assert(weapons.find(4)->exp == 0);
  // 封顶后继续注入不再升级。
  assert(weapons.addWeaponExp(4, 5000) == 0);
  assert(weapons.find(4)->level == 20);

  // 突破：等级达标才能突破；突破后上限提升。
  assert(weapons.ascend(4));
  assert(weapons.find(4)->ascension == 1);
  assert(WeaponSystem::levelCap(weapons.find(4)->ascension) == 40);
  assert(!weapons.ascend(4));  // 等级未达新上限。
  // 突破消耗配置递增。
  assert(WeaponSystem::ascensionMaterialCost(0) == 2);
  assert(WeaponSystem::ascensionMaterialCost(5) == 7);
  assert(WeaponSystem::ascensionGoldCost(0) == 100);
  assert(WeaponSystem::ascensionGoldCost(5) == 600);

  // 精炼：重复获取累积素材，精炼消耗素材并提升攻击。
  assert(!weapons.refine(4));            // 无素材。
  assert(weapons.addRefineStock(4));
  assert(weapons.addRefineStock(4));
  assert(!weapons.addRefineStock(99));
  assert(weapons.refine(4));
  assert(weapons.find(4)->refine == 2);
  assert(weapons.find(4)->refineStock == 1);
  assert(!weapons.refine(99));

  // 装备：绑定角色；换装自动解绑；加成含精炼。
  assert(weapons.addWeapon(5));
  assert(weapons.equip(4, 1));
  assert(weapons.find(4)->equippedBy == 1);
  assert(weapons.equippedBonusFor(1) ==
         WeaponSystem::atkOf(4, weapons.find(4)->level, 2));
  assert(weapons.equippedBonusFor(2) == 0);
  // 同一角色换装：旧武器卸下。
  assert(weapons.equip(5, 1));
  assert(weapons.find(4)->equippedBy == 0);
  assert(weapons.find(5)->equippedBy == 1);
  // 武器改绑其他角色：原角色解绑。
  assert(weapons.equip(5, 2));
  assert(weapons.find(5)->equippedBy == 2);
  assert(weapons.equippedBonusFor(1) == 0);
  assert(!weapons.equip(99, 1));
  assert(!weapons.equip(4, 0));

  // 存档恢复：旧三元组兼容。
  WeaponSystem restored;
  assert(restored.restoreWeapon(1, 99, 3));
  assert(restored.find(1)->level == 20);  // 突破 0 阶段上限 20。
  assert(restored.find(1)->equippedBy == 3);
  assert(!restored.restoreWeapon(1, 1, 0));
  assert(!restored.restoreWeapon(99, 1, 0));

  // 存档恢复：V7 全字段钳制。
  WeaponSystem full;
  assert(full.restoreWeapon(2, 999, 9, 9, -1, 999999, 1));
  assert(full.find(2)->level == 90);
  assert(full.find(2)->ascension == 6);
  assert(full.find(2)->refine == 5);
  assert(full.find(2)->refineStock == 0);
  assert(full.find(2)->exp == 0);  // 封顶清零。
  assert(full.find(2)->equippedBy == 1);

  // 拥有列表按 id 升序。
  for (size_t i = 1; i < weapons.owned().size(); ++i) {
    assert(weapons.owned()[i].weaponId > weapons.owned()[i - 1].weaponId);
  }

  return 0;
}
