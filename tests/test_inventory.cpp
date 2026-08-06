#include "native/gameplay/inventory/inventory.h"

#include <cassert>

int main() {
  // 初始背包：携带共鸣之契与金币。
  Inventory inventory = Inventory::defaultInventory();
  assert(inventory.countOf(static_cast<int32_t>(ItemId::Fate)) == 10);
  assert(inventory.countOf(static_cast<int32_t>(ItemId::Gold)) == 500);
  assert(inventory.countOf(static_cast<int32_t>(ItemId::ExpMaterial)) == 0);

  // 物品定义可查。
  assert(Inventory::itemDef(static_cast<int32_t>(ItemId::Fate)) != nullptr);
  assert(Inventory::itemDef(999) == nullptr);

  // 原神式养成扩充：矿石三档与经验书三档的定义、分类与稀有度。
  const ItemDef* oreLow = Inventory::itemDef(static_cast<int32_t>(ItemId::OreLow));
  const ItemDef* oreMid = Inventory::itemDef(static_cast<int32_t>(ItemId::OreMid));
  const ItemDef* oreHigh = Inventory::itemDef(static_cast<int32_t>(ItemId::OreHigh));
  const ItemDef* expSmall = Inventory::itemDef(static_cast<int32_t>(ItemId::ExpSmall));
  const ItemDef* expMedium = Inventory::itemDef(static_cast<int32_t>(ItemId::ExpMedium));
  const ItemDef* expLarge = Inventory::itemDef(static_cast<int32_t>(ItemId::ExpLarge));
  assert(oreLow != nullptr && oreMid != nullptr && oreHigh != nullptr);
  assert(expSmall != nullptr && expMedium != nullptr && expLarge != nullptr);
  assert(oreLow->kind == ItemKind::WeaponMaterial);
  assert(oreMid->kind == ItemKind::WeaponMaterial);
  assert(oreHigh->kind == ItemKind::WeaponMaterial);
  assert(expSmall->kind == ItemKind::Material);
  assert(expLarge->kind == ItemKind::Material);
  // 稀有度与描述字段齐备（供背包 UI 展示）。
  assert(oreLow->rarity == 2 && oreMid->rarity == 3 && oreHigh->rarity == 4);
  assert(expSmall->rarity == 2 && expMedium->rarity == 3 && expLarge->rarity == 4);
  assert(!oreLow->description.empty());
  assert(!expLarge->description.empty());

  // 经验价值映射：角色经验书与矿石；非对应物品返回 0。
  assert(Inventory::characterExpValue(static_cast<int32_t>(ItemId::ExpMaterial)) == 10);
  assert(Inventory::characterExpValue(static_cast<int32_t>(ItemId::ExpSmall)) == 1000);
  assert(Inventory::characterExpValue(static_cast<int32_t>(ItemId::ExpMedium)) == 5000);
  assert(Inventory::characterExpValue(static_cast<int32_t>(ItemId::ExpLarge)) == 20000);
  assert(Inventory::characterExpValue(static_cast<int32_t>(ItemId::OreHigh)) == 0);
  assert(Inventory::weaponExpValue(static_cast<int32_t>(ItemId::OreLow)) == 400);
  assert(Inventory::weaponExpValue(static_cast<int32_t>(ItemId::OreMid)) == 2000);
  assert(Inventory::weaponExpValue(static_cast<int32_t>(ItemId::OreHigh)) == 10000);
  assert(Inventory::weaponExpValue(static_cast<int32_t>(ItemId::ExpLarge)) == 0);

  // 新物品增删与堆叠（打怪掉落入账路径）。
  assert(inventory.addItem(static_cast<int32_t>(ItemId::OreLow), 3));
  assert(inventory.addItem(static_cast<int32_t>(ItemId::ExpSmall), 5));
  assert(inventory.countOf(static_cast<int32_t>(ItemId::OreLow)) == 3);
  assert(inventory.countOf(static_cast<int32_t>(ItemId::ExpSmall)) == 5);
  assert(inventory.removeItem(static_cast<int32_t>(ItemId::OreLow), 3));
  assert(inventory.countOf(static_cast<int32_t>(ItemId::OreLow)) == 0);
  assert(inventory.removeItem(static_cast<int32_t>(ItemId::ExpSmall), 5));

  // 增加与堆叠。
  assert(inventory.addItem(static_cast<int32_t>(ItemId::ExpMaterial), 5));
  assert(inventory.addItem(static_cast<int32_t>(ItemId::ExpMaterial), 3));
  assert(inventory.countOf(static_cast<int32_t>(ItemId::ExpMaterial)) == 8);

  // 堆叠上限：超过 maxStack 截断。
  assert(inventory.addItem(static_cast<int32_t>(ItemId::Fate), 100000));
  assert(inventory.countOf(static_cast<int32_t>(ItemId::Fate)) == 999);

  // 消耗：足量扣除、不足量整体拒绝。
  assert(inventory.removeItem(static_cast<int32_t>(ItemId::Gold), 200));
  assert(inventory.countOf(static_cast<int32_t>(ItemId::Gold)) == 300);
  assert(!inventory.removeItem(static_cast<int32_t>(ItemId::Gold), 400));
  assert(inventory.countOf(static_cast<int32_t>(ItemId::Gold)) == 300);

  // 扣空移除堆叠。
  assert(inventory.removeItem(static_cast<int32_t>(ItemId::ExpMaterial), 8));
  assert(inventory.countOf(static_cast<int32_t>(ItemId::ExpMaterial)) == 0);
  bool found = false;
  for (const ItemStack& stack : inventory.stacks()) {
    if (stack.itemId == static_cast<int32_t>(ItemId::ExpMaterial)) found = true;
  }
  assert(!found);

  // 非法输入拒绝。
  assert(!inventory.addItem(999, 1));
  assert(!inventory.addItem(static_cast<int32_t>(ItemId::Gold), 0));
  assert(!inventory.addItem(static_cast<int32_t>(ItemId::Gold), -1));
  assert(!inventory.removeItem(static_cast<int32_t>(ItemId::Gold), 0));

  // 堆叠列表按物品 id 升序（确定性）。
  inventory.addItem(static_cast<int32_t>(ItemId::AscensionMaterial), 1);
  for (size_t i = 1; i < inventory.stacks().size(); ++i) {
    assert(inventory.stacks()[i].itemId > inventory.stacks()[i - 1].itemId);
  }
  return 0;
}
