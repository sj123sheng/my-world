#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 背包与物品系统（阶段三 + 原神式养成扩充）：
// 物品分货币、角色材料与武器材料三类，按定义的最大堆叠数堆叠；
// 增删操作确定性可测试，供养成消耗与抽卡结算使用。
enum class ItemKind : uint8_t {
  Currency = 0,
  Material = 1,
  WeaponMaterial = 2,
};

// 内置物品 id（单机样板，无外部配置依赖）。
enum class ItemId : int32_t {
  Fate = 1,              // 抽卡道具「共鸣之契」
  Gold = 2,              // 通用货币
  ExpMaterial = 3,       // 角色经验材料（旧档：10 经验/份）
  AscensionMaterial = 4, // 突破材料
  OreLow = 5,            // 武器强化矿石（400 武器经验）
  OreMid = 6,            // 武器强化矿石（2000 武器经验）
  OreHigh = 7,           // 武器强化矿石（10000 武器经验）
  ExpSmall = 8,          // 角色经验书（1000 经验）
  ExpMedium = 9,         // 角色经验书（5000 经验）
  ExpLarge = 10,         // 角色经验书（20000 经验）
};

struct ItemDef {
  int32_t id = 0;
  ItemKind kind = ItemKind::Material;
  std::string name;
  int32_t maxStack = 9999;
  int32_t rarity = 1;        // 稀有度 1..5（边框颜色）。
  std::string description;   // 详情文案。
};

struct ItemStack {
  int32_t itemId = 0;
  int32_t count = 0;
};

class Inventory {
 public:
  // 初始背包：携带少量共鸣之契供抽卡体验。
  static Inventory defaultInventory();
  static const ItemDef* itemDef(int32_t itemId);

  // 增加物品；count 必须为正。返回是否成功。
  bool addItem(int32_t itemId, int32_t count);
  // 消耗物品；数量不足返回 false 且不产生部分扣除。
  bool removeItem(int32_t itemId, int32_t count);
  int32_t countOf(int32_t itemId) const;
  // 按物品 id 升序的堆叠列表（确定性）。
  const std::vector<ItemStack>& stacks() const { return stacks_; }

  // 物品提供的角色经验值；非角色经验物品返回 0。
  static int32_t characterExpValue(int32_t itemId);
  // 物品提供的武器经验值；非矿石物品返回 0。
  static int32_t weaponExpValue(int32_t itemId);

 private:
  std::vector<ItemStack> stacks_;
};
