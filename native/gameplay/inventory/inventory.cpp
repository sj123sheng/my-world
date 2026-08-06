#include "native/gameplay/inventory/inventory.h"

#include <algorithm>

namespace {

const ItemDef kItemDefs[] = {
    {static_cast<int32_t>(ItemId::Fate), ItemKind::Currency, "共鸣之契", 999, 4,
     "用于共鸣抽取的神秘契约。"},
    {static_cast<int32_t>(ItemId::Gold), ItemKind::Currency, "脉金币", 999999, 1,
     "艾瑟兰通用货币，用于强化与突破。"},
    {static_cast<int32_t>(ItemId::ExpMaterial), ItemKind::Material, "辉光经验", 9999, 2,
     "角色经验材料，每份提供 10 点经验。"},
    {static_cast<int32_t>(ItemId::AscensionMaterial), ItemKind::Material, "源晶碎片", 9999, 3,
     "角色与武器突破所需的源能结晶。"},
    {static_cast<int32_t>(ItemId::OreLow), ItemKind::WeaponMaterial, "粗磨矿石", 9999, 2,
     "武器强化材料，提供 400 点武器经验。"},
    {static_cast<int32_t>(ItemId::OreMid), ItemKind::WeaponMaterial, "精锻矿石", 9999, 3,
     "武器强化材料，提供 2000 点武器经验。"},
    {static_cast<int32_t>(ItemId::OreHigh), ItemKind::WeaponMaterial, "魔晶矿石", 9999, 4,
     "武器强化材料，提供 10000 点武器经验。"},
    {static_cast<int32_t>(ItemId::ExpSmall), ItemKind::Material, "流浪者笔记", 9999, 2,
     "角色经验书，提供 1000 点经验。"},
    {static_cast<int32_t>(ItemId::ExpMedium), ItemKind::Material, "冒险家手册", 9999, 3,
     "角色经验书，提供 5000 点经验。"},
    {static_cast<int32_t>(ItemId::ExpLarge), ItemKind::Material, "英雄之证", 9999, 4,
     "角色经验书，提供 20000 点经验。"},
};

}  // namespace

Inventory Inventory::defaultInventory() {
  Inventory inventory;
  inventory.addItem(static_cast<int32_t>(ItemId::Fate), 10);
  inventory.addItem(static_cast<int32_t>(ItemId::Gold), 500);
  return inventory;
}

const ItemDef* Inventory::itemDef(int32_t itemId) {
  for (const ItemDef& def : kItemDefs) {
    if (def.id == itemId) return &def;
  }
  return nullptr;
}

bool Inventory::addItem(int32_t itemId, int32_t count) {
  const ItemDef* def = itemDef(itemId);
  if (def == nullptr || count <= 0) return false;
  auto stack = std::find_if(stacks_.begin(), stacks_.end(),
                            [itemId](const ItemStack& candidate) {
                              return candidate.itemId == itemId;
                            });
  if (stack == stacks_.end()) {
    stacks_.push_back({itemId, std::min(count, def->maxStack)});
    std::sort(stacks_.begin(), stacks_.end(),
              [](const ItemStack& left, const ItemStack& right) {
                return left.itemId < right.itemId;
              });
    return true;
  }
  stack->count = std::min(stack->count + count, def->maxStack);
  return true;
}

bool Inventory::removeItem(int32_t itemId, int32_t count) {
  if (count <= 0) return false;
  auto stack = std::find_if(stacks_.begin(), stacks_.end(),
                            [itemId](const ItemStack& candidate) {
                              return candidate.itemId == itemId;
                            });
  if (stack == stacks_.end() || stack->count < count) return false;
  stack->count -= count;
  if (stack->count == 0) {
    stacks_.erase(stack);
  }
  return true;
}

int32_t Inventory::countOf(int32_t itemId) const {
  for (const ItemStack& stack : stacks_) {
    if (stack.itemId == itemId) return stack.count;
  }
  return 0;
}

int32_t Inventory::characterExpValue(int32_t itemId) {
  switch (static_cast<ItemId>(itemId)) {
    case ItemId::ExpMaterial:
      return 10;
    case ItemId::ExpSmall:
      return 1000;
    case ItemId::ExpMedium:
      return 5000;
    case ItemId::ExpLarge:
      return 20000;
    default:
      return 0;
  }
}

int32_t Inventory::weaponExpValue(int32_t itemId) {
  switch (static_cast<ItemId>(itemId)) {
    case ItemId::OreLow:
      return 400;
    case ItemId::OreMid:
      return 2000;
    case ItemId::OreHigh:
      return 10000;
    default:
      return 0;
  }
}
